#include "TFPatternNetworkGrowth.h"
#include "TFRandom.h"
#include "TFPlayheadAssignment.h"
#include "TFTextureCropFill.h"
#include "ofMath.h"
#include "ofGraphics.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
	constexpr float NOISE_SCALE = 2.2f;
	constexpr float NOISE_TIME_SPEED = 0.0167f; // matches Blob Grid/Succession's "airy and calm" pass
}

void TFPatternNetworkGrowth::setup(TimeOffsetVideoBuffer* videoBuffer_, int canvasW_, int canvasH_, const Params& params_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	params = params_;
}

void TFPatternNetworkGrowth::resizeCanvas(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
}

void TFPatternNetworkGrowth::reset(int seed) {
	std::srand(seed);
	elapsedTime = tfRandRangeF(0.0f, 1000.0f);
	noiseTime = elapsedTime;
	growthTimer = 0.0f;
	nodes.clear();
	spawnSeedNodes();
}

void TFPatternNetworkGrowth::rollNodeAppearance(Node& n) const {
	float size = tfRandRangeF(params.nodeMinSize, params.nodeMaxSize);
	bool rectangular = tfRandRangeF(0.0f, 100.0f) < params.rectangularChance;

	if (rectangular) {
		float minAR = std::max(1.0f, params.minAspectRatio);
		float maxAR = std::max(minAR, params.maxAspectRatio);
		float aspect = tfRandRangeF(minAR, maxAR);
		// Coin flip per node so rectangular nodes aren't all oriented the
		// same way (long side isn't always width, or always height).
		if (tfRandRangeI(0, 1) == 0) {
			n.widthFrac = size * aspect;
			n.heightFrac = size;
		} else {
			n.widthFrac = size;
			n.heightFrac = size * aspect;
		}
		n.useCroppedContent = true; // hard rule (Section 4): rectangular nodes always crop
	} else {
		n.widthFrac = size;
		n.heightFrac = size;
		n.useCroppedContent = tfRandRangeF(0.0f, 100.0f) < params.cropChance;
	}
}

ofRectangle TFPatternNetworkGrowth::nodeLocalRect(const Node& n) const {
	float shorterEdge = static_cast<float>(std::min(canvasW, canvasH));
	float w = n.widthFrac * shorterEdge;
	float h = n.heightFrac * shorterEdge;
	return ofRectangle(n.pos.x - w * 0.5f, n.pos.y - h * 0.5f, w, h);
}

ofRectangle TFPatternNetworkGrowth::nodeSrcRect(const Node& n, const ofRectangle& localRect, const ofTexture& tex) const {
	if (n.useCroppedContent) {
		// Same proportional-crop mechanism BSP/Blob Grid use (Section 0): a
		// fragment's crop region is its own on-screen bounds mapped into the
		// buffer's pixel space. Built from `localRect`, which the caller
		// derives fresh from the node's live pos every frame — nothing here
		// is cached from spawn time.
		float bufW = static_cast<float>(videoBuffer->getBufferWidth());
		float bufH = static_cast<float>(videoBuffer->getBufferHeight());
		return ofRectangle(
			(localRect.x / canvasW) * bufW,
			(localRect.y / canvasH) * bufH,
			(localRect.width / canvasW) * bufW,
			(localRect.height / canvasH) * bufH);
	}
	return tfComputeCropFillSrcRect(tex.getWidth(), tex.getHeight(), localRect);
}

void TFPatternNetworkGrowth::spawnSeedNodes() {
	int count = ofClamp(params.seedNodeCount, 1, 3);
	ofVec2f center(canvasW * 0.5f, canvasH * 0.5f);
	float shorterEdge = static_cast<float>(std::min(canvasW, canvasH));

	for (int i = 0; i < count; i++) {
		Node n;
		// Evenly spaced initial directions (with jitter) so seeds branch
		// outward in different directions from the start, rather than all
		// growing the same way.
		float baseAngle = TWO_PI * static_cast<float>(i) / static_cast<float>(count);
		n.outwardAngleRad = baseAngle + tfRandRangeF(-PI * 0.125f, PI * 0.125f);
		n.pos = center + ofVec2f(tfRandRangeF(-1.0f, 1.0f), tfRandRangeF(-1.0f, 1.0f)) * (shorterEdge * 0.02f);
		n.age = 0.0f;
		n.reassignTimer = tfRandRangeF(0.0f, std::max(0.01f, params.nodeReassignInterval));
		rollNodeAppearance(n);

		float nx = n.pos.x / canvasW;
		float ny = n.pos.y / canvasH;
		float gray = ofNoise(nx * NOISE_SCALE, ny * NOISE_SCALE, noiseTime);
		n.committedOffset = videoBuffer->quantize(gray);

		nodes.push_back(std::move(n));
	}
}

int TFPatternNetworkGrowth::countAlive() const {
	int count = 0;
	for (const auto& n : nodes) {
		if (n.alive) count++;
	}
	return count;
}

void TFPatternNetworkGrowth::spawnChildNode() {
	std::vector<int> aliveIndices;
	aliveIndices.reserve(nodes.size());
	for (size_t i = 0; i < nodes.size(); i++) {
		if (nodes[i].alive) aliveIndices.push_back(static_cast<int>(i));
	}
	if (aliveIndices.empty()) {
		return;
	}

	int parentIdx = aliveIndices[tfRandRangeI(0, static_cast<int>(aliveIndices.size()) - 1)];
	const Node& parent = nodes[parentIdx];

	float jitterRad = ofDegToRad(params.branchAngleJitterDeg);
	float angle = parent.outwardAngleRad + tfRandRangeF(-jitterRad, jitterRad);

	float shorterEdge = static_cast<float>(std::min(canvasW, canvasH));
	float dist = params.branchDistance * shorterEdge * (1.0f + tfRandRangeF(-params.branchDistanceJitter, params.branchDistanceJitter));
	dist = std::max(1.0f, dist);

	Node child;
	child.pos = parent.pos + ofVec2f(std::cos(angle), std::sin(angle)) * dist;
	child.parentIndex = parentIdx;
	child.outwardAngleRad = angle;
	child.age = 0.0f;
	child.reassignTimer = 0.0f;
	rollNodeAppearance(child);

	float nx = child.pos.x / canvasW;
	float ny = child.pos.y / canvasH;
	float gray = ofNoise(nx * NOISE_SCALE, ny * NOISE_SCALE, noiseTime);
	child.committedOffset = videoBuffer->quantize(gray);

	nodes.push_back(std::move(child));
}

void TFPatternNetworkGrowth::update(float dt) {
	elapsedTime += dt;
	noiseTime += dt * NOISE_TIME_SPEED;

	growthTimer += dt;
	if (growthTimer >= params.growthInterval) {
		growthTimer -= params.growthInterval;
		if (countAlive() < std::max(1, params.maxNodeCount)) {
			spawnChildNode();
		}
	}

	if (params.nodeLifespanEnabled) {
		for (auto& n : nodes) {
			if (n.alive && n.age >= params.maxNodeAge) {
				// Removed, not cascaded: this node stops drawing (and its own
				// edge to ITS parent disappears with it), but any children
				// that named it as their parent simply lose their parent-edge
				// and keep drawing normally — see draw()'s edge pass.
				n.alive = false;
				n.playheadIndex = -1;
				n.transition.reset();
			}
		}
	}

	std::vector<int> liveIndices;
	std::vector<ofVec2f> normalizedCenters;
	std::vector<float> desiredOffsets;
	liveIndices.reserve(nodes.size());
	normalizedCenters.reserve(nodes.size());
	desiredOffsets.reserve(nodes.size());

	for (size_t i = 0; i < nodes.size(); i++) {
		Node& n = nodes[i];
		if (!n.alive) {
			continue;
		}
		n.age += dt;

		float nx = n.pos.x / canvasW;
		float ny = n.pos.y / canvasH;

		n.reassignTimer += dt;
		if (n.reassignTimer >= params.nodeReassignInterval) {
			n.reassignTimer -= params.nodeReassignInterval;
			float gray = ofNoise(nx * NOISE_SCALE, ny * NOISE_SCALE, noiseTime);
			n.committedOffset = videoBuffer->quantize(gray);
		}

		liveIndices.push_back(static_cast<int>(i));
		normalizedCenters.push_back(ofVec2f(nx, ny));
		desiredOffsets.push_back(n.committedOffset);

		if (n.transition) {
			n.transition->update(dt);
		}
	}

	std::vector<int> playheadIndices;
	tfAssignPlayheadsByDesiredOffsets(*videoBuffer, desiredOffsets, playheadIndices);

	for (size_t k = 0; k < liveIndices.size(); k++) {
		Node& n = nodes[liveIndices[k]];
		int newIndex = playheadIndices[k];

		if (n.lastPlayheadIndex >= 0 && n.lastPlayheadIndex != newIndex) {
			ofRectangle localRect = nodeLocalRect(n);
			const ofTexture& oldTex = videoBuffer->getPlayheadTexture(n.lastPlayheadIndex);
			ofRectangle oldSrcRect = nodeSrcRect(n, localRect, oldTex);

			if (!n.transition) {
				n.transition = std::make_unique<TFFragmentTransition>();
			}
			n.transition->begin(
				tfPickTransitionStyle(params.hardCutWeight, params.crossfadeWeight, params.erosionWeight),
				params.transitionDuration,
				localRect,
				oldTex,
				oldSrcRect);

			if (onFragmentReassignedCb) {
				onFragmentReassignedCb(normalizedCenters[k].x, normalizedCenters[k].y);
			}
		}

		n.playheadIndex = newIndex;
		n.lastPlayheadIndex = newIndex;
	}
}

void TFPatternNetworkGrowth::draw() {
	ofEnableAlphaBlending();

	// Edges first, so node fragments composite on top of them. Only drawn
	// when BOTH endpoints are alive — a dead node's own edge disappears with
	// it, and a live child whose parent just died simply loses its edge
	// (the child itself is untouched) rather than cascading.
	ofSetLineWidth(params.edgeThickness);
	for (const auto& n : nodes) {
		if (!n.alive || n.parentIndex < 0) {
			continue;
		}
		const Node& parent = nodes[n.parentIndex];
		if (!parent.alive) {
			continue;
		}
		ofSetColor(255, 255, 255, 200);
		ofDrawLine(parent.pos.x, parent.pos.y, n.pos.x, n.pos.y);
	}

	for (const auto& n : nodes) {
		if (!n.alive || n.playheadIndex < 0) {
			// Most of the canvas simply has no node here at all — a true
			// hole by construction (Section 0), not something masked/faded.
			continue;
		}
		const ofTexture& tex = videoBuffer->getPlayheadTexture(n.playheadIndex);
		if (!tex.isAllocated()) {
			continue;
		}

		ofRectangle localRect = nodeLocalRect(n);
		ofRectangle srcRect = nodeSrcRect(n, localRect, tex);

		if (n.transition) {
			n.transition->draw(localRect, tex, srcRect);
		} else {
			ofSetColor(255);
			tex.drawSubsection(localRect.x, localRect.y, localRect.width, localRect.height,
				srcRect.x, srcRect.y, srcRect.width, srcRect.height);
		}
	}
}

std::vector<ofVec2f> TFPatternNetworkGrowth::getActiveFragmentCenters() const {
	std::vector<ofVec2f> centers;
	centers.reserve(nodes.size());
	for (const auto& n : nodes) {
		if (!n.alive || n.playheadIndex < 0) {
			continue;
		}
		centers.push_back(ofVec2f(n.pos.x / canvasW, n.pos.y / canvasH));
	}
	return centers;
}
