#include "TFPatternBSP.h"
#include "TFRandom.h"
#include "TFPlayheadAssignment.h"
#include "TFQuarantineHatch.h"
#include "Settings.h"
#include "ofMath.h"
#include "ofGraphics.h"
#include "ofVec2f.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
	constexpr float NOISE_SCALE = 2.2f;
	// Slowed further for a calmer feel (was 0.05, then 0.02) — ~3x slower
	// still — offset/transparency noise drifts far more gradually across
	// the canvas.
	constexpr float NOISE_TIME_SPEED = 0.0067f;
	constexpr int MAX_DEPTH = 8;
}

void TFPatternBSP::setup(TimeOffsetVideoBuffer* videoBuffer_, int canvasW_, int canvasH_, const Params& params_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	params = params_;
}

void TFPatternBSP::resizeCanvas(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
}

void TFPatternBSP::reset(int seed) {
	// Redundant with TFComposition's own srand(seed) call right before this,
	// but kept here too so this pattern's tree generation stays reproducible
	// even if exercised standalone.
	std::srand(seed);
	noiseTime = tfRandRangeF(0.0f, 1000.0f);

	root = Node{};
	root.bounds = ofRectangle(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH));
	root.depth = 0;
	split(root);

	reshuffleTimer = 0.0f;
	assignPlayheads();
}

void TFPatternBSP::update(float dt) {
	noiseTime += dt * NOISE_TIME_SPEED;

	reshuffleTimer += dt;
	if (reshuffleTimer >= params.geometryReshuffleRate) {
		reshuffleTimer -= params.geometryReshuffleRate;
		reshuffleRegions();
	}

	assignPlayheads();

	std::vector<Node*> leaves;
	collectLeaves(root, leaves);
	for (Node* leaf : leaves) {
		if (leaf->transition) {
			leaf->transition->update(dt);
		}
		// Quarantine hold: advance elapsed time, and actually split once the
		// hold duration has passed. Splitting here (mid-iteration) is safe —
		// it only grows *this* leaf's own children vector, never touches any
		// other Node's address or the `leaves` pointer list itself.
		if (leaf->quarantined) {
			leaf->quarantineElapsed += dt;
			if (leaf->quarantineElapsed >= params.quarantineHoldDuration) {
				leaf->playheadIndex = -1;
				split(*leaf);
			}
		}
	}
}

void TFPatternBSP::draw() {
	std::vector<Node*> leaves;
	collectLeaves(root, leaves);

	float bufW = static_cast<float>(videoBuffer->getBufferWidth());
	float bufH = static_cast<float>(videoBuffer->getBufferHeight());

	for (Node* leaf : leaves) {
		if (leaf->playheadIndex < 0) {
			continue;
		}

		// Crop the same proportional region of the source frame that this
		// leaf occupies on screen — not the whole frame stretched into the
		// leaf's rectangle. Every leaf together reassembles one intact
		// image; each just shows its own region frozen at whatever point
		// in the last 10s its playhead is set to.
		ofRectangle srcRect(
			(leaf->bounds.x / canvasW) * bufW,
			(leaf->bounds.y / canvasH) * bufH,
			(leaf->bounds.width / canvasW) * bufW,
			(leaf->bounds.height / canvasH) * bufH);

		const ofTexture& tex = videoBuffer->getPlayheadTexture(leaf->playheadIndex);
		if (!tex.isAllocated()) {
			// History is still empty (first frame or two after launch,
			// before the video decoder has produced anything) — nothing to
			// draw yet, and drawing an unallocated texture just spams an
			// ofGLRenderer warning.
			continue;
		}

		if (leaf->transition) {
			leaf->transition->draw(leaf->bounds, tex, srcRect);
		} else {
			ofSetColor(255);
			tex.drawSubsection(leaf->bounds.x, leaf->bounds.y, leaf->bounds.width, leaf->bounds.height,
				srcRect.x, srcRect.y, srcRect.width, srcRect.height);
		}

		if (leaf->quarantined) {
			// Full opacity for the whole hold — this is a discrete flag
			// (quarantined or not), unlike Blob Grid's continuous
			// coverage-scaled version.
			tfDrawQuarantineHatch(leaf->bounds, HATCH_WARNING, 1.0f);
		}
	}
}

void TFPatternBSP::split(Node& node) {
	if (shouldStopSubdividing(node)) {
		node.isLeaf = true;
		return;
	}

	node.isLeaf = false;
	node.children.clear();
	node.transition.reset(); // no longer a leaf — its transition (if any) is no longer relevant

	bool splitVertical = node.bounds.width >= node.bounds.height; // split axis follows the longer dimension
	float ratio = ofClamp(0.5f + tfRandRangeF(-params.irregularity, params.irregularity) * 0.5f, 0.15f, 0.85f);

	Node a;
	Node b;
	a.depth = b.depth = node.depth + 1;

	if (splitVertical) {
		float splitX = node.bounds.width * ratio;
		a.bounds = ofRectangle(node.bounds.x, node.bounds.y, splitX, node.bounds.height);
		b.bounds = ofRectangle(node.bounds.x + splitX, node.bounds.y, node.bounds.width - splitX, node.bounds.height);
	} else {
		float splitY = node.bounds.height * ratio;
		a.bounds = ofRectangle(node.bounds.x, node.bounds.y, node.bounds.width, splitY);
		b.bounds = ofRectangle(node.bounds.x, node.bounds.y + splitY, node.bounds.width, node.bounds.height - splitY);
	}

	node.children.push_back(std::move(a));
	node.children.push_back(std::move(b));
	split(node.children[0]);
	split(node.children[1]);
}

bool TFPatternBSP::shouldStopSubdividing(const Node& node) const {
	if (node.depth >= MAX_DEPTH) {
		return true;
	}

	float minEdge = std::min(canvasW, canvasH) * params.cellDensity;
	if (std::min(node.bounds.width, node.bounds.height) < minEdge * 2.2f) {
		return true;
	}

	// Depth-based probability: deeper branches increasingly likely to stop
	// early even above the size floor, so leaf sizes vary organically
	// instead of every branch subdividing to the same depth.
	float stopProbability = ofClamp(node.depth * 0.12f, 0.0f, 0.75f);
	return tfRandRangeF(0.0f, 1.0f) < stopProbability;
}

void TFPatternBSP::collectLeaves(Node& node, std::vector<Node*>& outLeaves) {
	if (node.isLeaf) {
		outLeaves.push_back(&node);
	} else {
		for (auto& child : node.children) {
			collectLeaves(child, outLeaves);
		}
	}
}

void TFPatternBSP::assignPlayheads() {
	std::vector<Node*> leaves;
	collectLeaves(root, leaves);

	// Second, decorrelated noise channel (different domain offset + time
	// scale from the offset-assignment noise below) decides which leaves
	// are suppressed this frame — a real hole a background layer shows
	// through, not an alpha-0 draw. Suppressed leaves are excluded before
	// the pool assignment below entirely, so they don't consume a shared
	// playhead slot that a visible leaf could use instead.
	std::vector<Node*> visibleLeaves;
	visibleLeaves.reserve(leaves.size());
	std::vector<ofVec2f> normalizedCenters;
	normalizedCenters.reserve(leaves.size());

	for (Node* leaf : leaves) {
		float cx = (leaf->bounds.x + leaf->bounds.width * 0.5f) / canvasW;
		float cy = (leaf->bounds.y + leaf->bounds.height * 0.5f) / canvasH;

		float transparencyNoise = ofNoise(cx * NOISE_SCALE + 743.21f, cy * NOISE_SCALE + 743.21f, noiseTime * 0.6f + 91.7f);
		if (transparencyNoise < params.transparencyAmount) {
			leaf->playheadIndex = -1;
			leaf->lastPlayheadIndex = -1; // pops back in at full opacity, no transition — matches the "no transition on first assignment" behavior elsewhere
			continue;
		}

		visibleLeaves.push_back(leaf);
		normalizedCenters.push_back(ofVec2f(cx, cy));
	}

	std::vector<int> playheadIndices;
	tfAssignPlayheadsByNoise(*videoBuffer, noiseTime, NOISE_SCALE, normalizedCenters, playheadIndices);

	float bufW = static_cast<float>(videoBuffer->getBufferWidth());
	float bufH = static_cast<float>(videoBuffer->getBufferHeight());

	for (size_t i = 0; i < visibleLeaves.size(); i++) {
		Node* leaf = visibleLeaves[i];
		int newIndex = playheadIndices[i];

		if (leaf->lastPlayheadIndex >= 0 && leaf->lastPlayheadIndex != newIndex) {
			ofRectangle srcRect(
				(leaf->bounds.x / canvasW) * bufW,
				(leaf->bounds.y / canvasH) * bufH,
				(leaf->bounds.width / canvasW) * bufW,
				(leaf->bounds.height / canvasH) * bufH);

			if (!leaf->transition) {
				leaf->transition = std::make_unique<TFFragmentTransition>();
			}
			leaf->transition->begin(
				tfPickTransitionStyle(params.hardCutWeight, params.crossfadeWeight, params.erosionWeight),
				params.transitionDuration,
				leaf->bounds,
				videoBuffer->getPlayheadTexture(leaf->lastPlayheadIndex),
				srcRect);

			if (onFragmentReassignedCb) {
				onFragmentReassignedCb(normalizedCenters[i].x, normalizedCenters[i].y);
			}
		}

		leaf->playheadIndex = newIndex;
		leaf->lastPlayheadIndex = newIndex;
	}
}

void TFPatternBSP::reshuffleRegions() {
	std::vector<Node*> leaves;
	collectLeaves(root, leaves);

	// Exclude leaves already quarantined from a previous tick so the same
	// leaf isn't queued twice while it's holding.
	std::vector<Node*> eligible;
	eligible.reserve(leaves.size());
	for (Node* l : leaves) {
		if (!l->quarantined) {
			eligible.push_back(l);
		}
	}
	if (eligible.empty()) {
		return;
	}

	// Flag for quarantine rather than splitting immediately — update()
	// holds each flagged leaf (hatch overlaid) for params.quarantineHoldDuration
	// before actually re-partitioning it.
	int count = std::min(params.regionsTouchedPerTick, static_cast<int>(eligible.size()));
	for (int i = 0; i < count; i++) {
		int idx = tfRandRangeI(0, static_cast<int>(eligible.size()) - 1);
		Node* leaf = eligible[idx];
		leaf->quarantined = true;
		leaf->quarantineElapsed = 0.0f;
		eligible.erase(eligible.begin() + idx);
	}
}

void TFPatternBSP::collectLeafCentersConst(const Node& node, std::vector<ofVec2f>& out) const {
	if (node.isLeaf) {
		if (node.playheadIndex >= 0) {
			out.push_back(ofVec2f(
				(node.bounds.x + node.bounds.width * 0.5f) / canvasW,
				(node.bounds.y + node.bounds.height * 0.5f) / canvasH));
		}
	} else {
		for (const auto& child : node.children) {
			collectLeafCentersConst(child, out);
		}
	}
}

std::vector<ofVec2f> TFPatternBSP::getActiveFragmentCenters() const {
	std::vector<ofVec2f> centers;
	collectLeafCentersConst(root, centers);
	return centers;
}
