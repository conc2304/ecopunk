#include "MediaViewportMesh.h"

#include "ofGraphics.h"
#include "ofMath.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace hudpresent {

namespace {

// Arc tessellation resolution, matching TFShapeFragmentRenderer's own
// documented convention (arcSteps(): one step per ~6°, clamped to a sane
// range) — reused as algorithmic precedent per this file's header
// comment, not by including that class.
int arcSteps(float spanDeg, float degreesPerStep) {
	return std::clamp(static_cast<int>(std::ceil(spanDeg / std::max(0.5f, degreesPerStep))), 2, 64);
}

// "Cover"-fit UV mapping: scales so the source texture fully covers
// `bounds` (cropping excess) rather than stretching or letterboxing —
// the standard technique for showing a native-resolution scene render
// inside a differently-shaped chrome viewport without distortion. Baked
// once per rebuild(), not recomputed per draw() call.
glm::vec2 coverUV(float normalizedX, float normalizedY, float boundsAspect, float sourceAspect) {
	float scaleX = 1.0f, scaleY = 1.0f;
	if (sourceAspect > 0.0f) {
		// "Cover" means the source image effectively scales UP until it
		// fully covers the (differently-shaped) destination, cropping
		// whatever sticks out — in UV terms that means the SAMPLED range
		// shrinks (a scale factor < 1 pulling the edges in toward 0.5),
		// never expands past [0,1]. Comparing the two aspect ratios
		// directly (not the earlier, inverted `boundsAspect > sourceAspect`
		// check this had before a test caught it): whichever axis the
		// source is relatively LONGER on is the axis that gets cropped.
		if (sourceAspect > boundsAspect) {
			// Source relatively wider than bounds -> crop its left/right -> shrink U range.
			scaleX = boundsAspect / sourceAspect;
		} else {
			// Source relatively taller than bounds -> crop its top/bottom -> shrink V range.
			scaleY = sourceAspect / boundsAspect;
		}
	}
	float u = 0.5f + (normalizedX - 0.5f) * scaleX;
	float v = 0.5f + (normalizedY - 0.5f) * scaleY;
	return glm::vec2(u, v);
}

} // namespace

void MediaViewportMesh::updateGeometry(const MediaViewportGeometryParams& params, glm::ivec2 sourceSize) {
	lastUpdateRebuilt_ = false;
	if (hasBuilt_ && params == lastParams_ && sourceSize == lastSourceSize_) {
		return; // exact-match early-out — "no unnecessary per-frame rebuild"
	}
	rebuild(params, sourceSize);
	lastParams_ = params;
	lastSourceSize_ = sourceSize;
	hasBuilt_ = true;
	lastUpdateRebuilt_ = true;
}

void MediaViewportMesh::rebuild(const MediaViewportGeometryParams& params, glm::ivec2 sourceSize) {
	mesh_.clear();
	mesh_.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
	canonicalUV_.clear();

	const ofRectangle& b = params.bounds;
	float minDim = std::min(b.width, b.height);
	// Clamp so a small viewport can never request geometry larger than
	// itself — corner radius and bevel additionally share the available
	// space fairly (each capped to 40% of the shorter side).
	float radius = std::clamp(params.cornerRadius, 0.0f, minDim * 0.4f);
	float bevel = std::clamp(params.bevelSize, 0.0f, minDim * 0.4f);

	float boundsAspect = (b.height > 0.0f) ? (b.width / b.height) : 1.0f;
	float sourceAspect = (sourceSize.x > 0 && sourceSize.y > 0)
		? (static_cast<float>(sourceSize.x) / static_cast<float>(sourceSize.y))
		: boundsAspect; // no real source yet -> assume matching aspect (identity UV mapping)

	auto addOutlineVertex = [&](float px, float py) {
		// px,py in absolute pixel space, within `bounds`.
		mesh_.addVertex(glm::vec3(px, py, 0.0f));
		float nx = (b.width > 0.0f) ? (px - b.x) / b.width : 0.0f;
		float ny = (b.height > 0.0f) ? (py - b.y) / b.height : 0.0f;
		glm::vec2 uv = coverUV(nx, ny, boundsAspect, sourceAspect);
		mesh_.addTexCoord(uv); // canonical, normalized/percent — see mesh_'s own doc comment
		canonicalUV_.push_back(uv);
	};

	// Center vertex first — triangle fan origin. The shape (rounded rect
	// with one beveled corner) is convex, so a fan from the bounds'
	// center is a correct, minimal triangulation — no ear-clipping or
	// general polygon tessellation needed.
	glm::vec2 center(b.x + b.width * 0.5f, b.y + b.height * 0.5f);
	addOutlineVertex(center.x, center.y);

	std::vector<glm::vec2> outline;
	outline.reserve(4 * 17 + 4);

	auto arc = [&](glm::vec2 c, float r, float fromDeg, float toDeg) {
		if (r <= 0.001f) {
			outline.push_back(c);
			return;
		}
		int steps = arcSteps(std::abs(toDeg - fromDeg), params.degreesPerArcStep);
		for (int i = 0; i <= steps; ++i) {
			float t = static_cast<float>(i) / static_cast<float>(steps);
			float deg = ofLerp(fromDeg, toDeg, t);
			float rad = ofDegToRad(deg);
			outline.emplace_back(c.x + std::cos(rad) * r, c.y + std::sin(rad) * r);
		}
	};

	// Clockwise from top-left, in screen space (y down): angles measured
	// with 0deg = +x, 90deg = +y (down), matching ofDegToRad/std::cos/sin
	// convention used elsewhere in this codebase (e.g.
	// TFShapeFragmentRenderer.cpp's own wedge builder).
	//
	// Top-left rounded corner: 180deg -> 270deg.
	arc({b.x + radius, b.y + radius}, radius, 180.0f, 270.0f);
	// Top edge.
	outline.emplace_back(b.x + b.width - radius, b.y);
	// Top-right rounded corner: 270deg -> 360deg.
	arc({b.x + b.width - radius, b.y + radius}, radius, 270.0f, 360.0f);
	// Right edge, shortened to leave room for the lower-right bevel.
	outline.emplace_back(b.x + b.width, b.y + b.height - bevel);
	// 45-degree bevel cut (a single straight vertex-to-vertex edge — the
	// "vertex-cut" technique Scene/HUD Contract v1 §11 names).
	outline.emplace_back(b.x + b.width - bevel, b.y + b.height);
	// Bottom edge, shortened by the bevel and the rounded corner.
	outline.emplace_back(b.x + radius, b.y + b.height);
	// Bottom-left rounded corner: 90deg -> 180deg.
	arc({b.x + radius, b.y + b.height - radius}, radius, 90.0f, 180.0f);
	// Left edge closes back to the top-left arc's start implicitly (the
	// fan's last triangle connects back to outline[0] — see below).

	for (const auto& p : outline) addOutlineVertex(p.x, p.y);

	// Close the fan: OF_PRIMITIVE_TRIANGLE_FAN needs the FIRST non-center
	// vertex repeated at the end to close the loop back to the start.
	if (!outline.empty()) addOutlineVertex(outline.front().x, outline.front().y);
}

void MediaViewportMesh::draw(const ofTexture* texture, ofColor placeholderColor) const {
	if (mesh_.getNumVertices() < 3) return;

	ofPushStyle();
	ofEnableAlphaBlending();

	bool textured = texture != nullptr && texture->isAllocated();
	if (textured) {
		// mesh_'s baked texcoords are normalized/percent [0,1] cover-fit
		// UVs (this class's canonical, texture-convention-agnostic
		// representation — see mesh_'s own doc comment). Raw OpenGL does
		// NOT know that convention on its own: ofTexture defaults to
		// GL_TEXTURE_RECTANGLE_ARB on desktop (ofGetUsingArbTex()'s
		// documented default — see ofTexture.cpp/ofFbo.cpp, which BOTH
		// use it, so this is not specific to any one texture source),
		// whose native texture coordinates are PIXEL-space
		// ([0,width]x[0,height]), not [0,1]. Submitting [0,1] UVs
		// directly to an ARB-rectangle-bound texture samples only the
		// texture's extreme top-left texel for the whole mesh — a real
		// bug this class had until a Validation Studio screenshot (a
		// textured MediaViewportMesh rendering as one flat, un-cropped
		// solid color instead of a visibly cover-fit-cropped image)
		// caught it. getCoordFromPercent() is ofTexture's own, texture-
		// specific [0,1]->native conversion — correct for BOTH ARB
		// rectangle and normalized (GL_TEXTURE_2D / GLES) textures
		// without this class needing to know which one it was handed.
		//
		// mesh_ is temporarily rewritten in place (no heap allocation:
		// setTexCoord() only ever writes an already-sized slot, same
		// vertex count as canonicalUV_ from the last rebuild()) then
		// restored to the canonical percent values before returning, so
		// mesh() and every other const observer always sees the
		// documented percent convention, never this draw call's
		// transient native-coordinate version.
		for (size_t i = 0; i < canonicalUV_.size(); ++i) {
			mesh_.setTexCoord(i, texture->getCoordFromPercent(canonicalUV_[i].x, canonicalUV_[i].y));
		}

		ofSetColor(255, 255, 255, 255);
		texture->bind();
		mesh_.draw();
		texture->unbind();

		for (size_t i = 0; i < canonicalUV_.size(); ++i) {
			mesh_.setTexCoord(i, canonicalUV_[i]);
		}
	} else {
		ofSetColor(placeholderColor);
		mesh_.draw();
	}

	ofPopStyle();
}

} // namespace hudpresent
