#include "TFShapeFragmentRenderer.h"
#include "TFPlayheadAssignment.h"
#include "TFRandom.h"
#include "ofGraphics.h"
#include "ofMath.h"
#include <algorithm>
#include <cmath>

namespace {
	ofRectangle proportionalSrcRect(const ofRectangle& bounds, int canvasW, int canvasH, float bufW, float bufH) {
		return ofRectangle(
			(bounds.x / canvasW) * bufW,
			(bounds.y / canvasH) * bufH,
			(bounds.width / canvasW) * bufW,
			(bounds.height / canvasH) * bufH);
	}

	// One arc step every ~6 degrees, clamped to a sane range — smooth
	// circles without excess triangles. Independent of any wedge-count
	// dial a WEDGE-producing pattern might have, which controls how many
	// *wedges* exist, not this per-wedge tessellation resolution.
	int arcSteps(float angleBeginDeg, float angleEndDeg) {
		float span = std::abs(angleEndDeg - angleBeginDeg);
		return ofClamp(static_cast<int>(std::ceil(span / 6.0f)), 2, 64);
	}

	void buildWedgeMesh(const TFFragmentShape& shape, int canvasW, int canvasH, float bufW, float bufH, float texW, float texH, ofMesh& mesh) {
		mesh.clear();

		float beginRad = ofDegToRad(shape.angleBeginDeg);
		float endRad = ofDegToRad(shape.angleEndDeg);
		int steps = arcSteps(shape.angleBeginDeg, shape.angleEndDeg);

		auto addVertex = [&](float radius, float angle) {
			float px = shape.center.x + radius * std::cos(angle);
			float py = shape.center.y + radius * std::sin(angle);
			float bufX = (px / canvasW) * bufW;
			float bufY = (py / canvasH) * bufH;
			mesh.addVertex(ofVec3f(px, py, 0.0f));
			// Normalized [0,1] texcoords, not pixel-space — ofDisableArbTex()
			// (ofApp::setup()) means textures are plain GL_TEXTURE_2D, which
			// only the convenience draw()/drawSubsection() wrappers convert
			// from pixel-space automatically; a hand-built mesh must do it
			// itself, same as TFFragmentTransition's dissolve shader already
			// does for its toUVRect uniform.
			mesh.addTexCoord(ofVec2f(bufX / texW, bufY / texH));
		};

		if (shape.innerRadius <= 0.0001f) {
			// Sunburst pie slice: triangle fan from the shape's center.
			mesh.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
			addVertex(0.0f, beginRad); // radius 0 collapses to center regardless of angle
			for (int i = 0; i <= steps; i++) {
				float t = static_cast<float>(i) / static_cast<float>(steps);
				addVertex(shape.outerRadius, ofLerp(beginRad, endRad, t));
			}
		} else {
			// Concentric ring segment: triangle strip alternating inner/outer arc points.
			mesh.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
			for (int i = 0; i <= steps; i++) {
				float t = static_cast<float>(i) / static_cast<float>(steps);
				float angle = ofLerp(beginRad, endRad, t);
				addVertex(shape.innerRadius, angle);
				addVertex(shape.outerRadius, angle);
			}
		}
	}
}

void tfAssignFragmentPlayheadsAndTransitions(
	TimeOffsetVideoBuffer& videoBuffer,
	int canvasW, int canvasH,
	float noiseTime, float noiseScale,
	float transitionDuration, float hardCutWeight, float crossfadeWeight, float erosionWeight,
	std::vector<TFPatternFragment>& fragments,
	const std::function<void(float nx, float ny)>& onReassigned) {
	std::vector<ofVec2f> normalizedCenters(fragments.size());
	for (size_t i = 0; i < fragments.size(); i++) {
		const ofVec2f& s = fragments[i].samplePos;
		normalizedCenters[i] = ofVec2f(s.x / canvasW, s.y / canvasH);
	}

	std::vector<float> desiredOffsets(normalizedCenters.size());
	for (size_t i = 0; i < normalizedCenters.size(); i++) {
		float gray = ofNoise(normalizedCenters[i].x * noiseScale, normalizedCenters[i].y * noiseScale, noiseTime);
		desiredOffsets[i] = videoBuffer.quantize(gray);
	}

	tfAssignFragmentPlayheadsAndTransitionsWithOffsets(
		videoBuffer, canvasW, canvasH, desiredOffsets, transitionDuration, hardCutWeight, crossfadeWeight, erosionWeight,
		fragments, onReassigned);
}

void tfAssignFragmentPlayheadsAndTransitionsWithOffsets(
	TimeOffsetVideoBuffer& videoBuffer,
	int canvasW, int canvasH,
	const std::vector<float>& desiredOffsets,
	float transitionDuration, float hardCutWeight, float crossfadeWeight, float erosionWeight,
	std::vector<TFPatternFragment>& fragments,
	const std::function<void(float nx, float ny)>& onReassigned) {
	std::vector<int> playheadIndices;
	tfAssignPlayheadsByDesiredOffsets(videoBuffer, desiredOffsets, playheadIndices);

	float bufW = static_cast<float>(videoBuffer.getBufferWidth());
	float bufH = static_cast<float>(videoBuffer.getBufferHeight());

	for (size_t i = 0; i < fragments.size(); i++) {
		TFPatternFragment& f = fragments[i];
		int newIndex = playheadIndices[i];
		const ofRectangle& b = f.shape.bounds;
		float nx = f.samplePos.x / canvasW;
		float ny = f.samplePos.y / canvasH;

		if (f.lastPlayheadIndex >= 0 && f.lastPlayheadIndex != newIndex) {
			ofRectangle srcRect = proportionalSrcRect(b, canvasW, canvasH, bufW, bufH);

			if (!f.transition) {
				f.transition = std::make_unique<TFFragmentTransition>();
			}
			f.transition->begin(
				tfPickTransitionStyle(hardCutWeight, crossfadeWeight, erosionWeight),
				transitionDuration,
				b,
				videoBuffer.getPlayheadTexture(f.lastPlayheadIndex),
				srcRect);

			if (onReassigned) {
				onReassigned(nx, ny);
			}
		}

		f.playheadIndex = newIndex;
		f.lastPlayheadIndex = newIndex;
	}
}

void tfUpdateFragmentTransitions(std::vector<TFPatternFragment>& fragments, float dt) {
	for (auto& f : fragments) {
		if (f.transition) {
			f.transition->update(dt);
		}
	}
}

void tfDrawPatternFragment(TFPatternFragment& f, TimeOffsetVideoBuffer& videoBuffer, int canvasW, int canvasH, float alpha) {
	if (f.playheadIndex < 0) {
		return;
	}

	const ofTexture& tex = videoBuffer.getPlayheadTexture(f.playheadIndex);
	if (!tex.isAllocated()) {
		// History still empty (first frame or two after launch) — nothing to draw yet.
		return;
	}

	float bufW = static_cast<float>(videoBuffer.getBufferWidth());
	float bufH = static_cast<float>(videoBuffer.getBufferHeight());
	ofRectangle srcRect = proportionalSrcRect(f.shape.bounds, canvasW, canvasH, bufW, bufH);

	if (f.transition && f.transition->isActive()) {
		f.transition->draw(f.shape.bounds, tex, srcRect, alpha);
		return;
	}

	if (f.shape.kind == TFShapeKind::RECT) {
		ofSetColor(255, 255, 255, static_cast<int>(ofClamp(alpha, 0.0f, 1.0f) * 255));
		tex.drawSubsection(f.shape.bounds.x, f.shape.bounds.y, f.shape.bounds.width, f.shape.bounds.height,
			srcRect.x, srcRect.y, srcRect.width, srcRect.height);
		ofSetColor(255);
		return;
	}

	// WEDGE, steady state — a real texture-mapped mesh clipped to the
	// pie-slice/ring silhouette (unlike the transition branch above, which
	// intentionally falls back to the bounding-box rect).
	buildWedgeMesh(f.shape, canvasW, canvasH, bufW, bufH, tex.getWidth(), tex.getHeight(), f.wedgeMesh);
	ofSetColor(255, 255, 255, static_cast<int>(ofClamp(alpha, 0.0f, 1.0f) * 255));
	tex.bind();
	f.wedgeMesh.draw();
	tex.unbind();
	ofSetColor(255);
}

std::vector<ofVec2f> tfCollectFragmentCenters(const std::vector<TFPatternFragment>& fragments, int canvasW, int canvasH) {
	std::vector<ofVec2f> centers;
	centers.reserve(fragments.size());
	for (const auto& f : fragments) {
		if (f.playheadIndex < 0) {
			continue;
		}
		centers.push_back(ofVec2f(f.samplePos.x / canvasW, f.samplePos.y / canvasH));
	}
	return centers;
}
