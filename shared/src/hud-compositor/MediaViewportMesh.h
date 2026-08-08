#pragma once

// ============================================================================
// MediaViewportMesh.h — the HUD-owned media-viewport clip shape.
//
// Per Scene/HUD Contract v1 §11 ("Mesh Clip Ownership"): "build a new
// HUD-owned viewport mesh class first... Reuse the algorithmic precedent
// (the rounded-corner + 45°-bevel-as-a-vertex-cut technique, arc-stepping
// approach) not the class itself" — i.e. explicitly NOT an extraction of
// sketches/temporal-fields/src/TFShapeFragmentRenderer.*. This class
// reuses that precedent's two techniques (arc-stepped rounded corners,
// vertex-level corner cutting for the bevel) as a fresh, HUD-owned
// implementation — no #include of, or dependency on, TFShapeFragmentRenderer
// or anything else under sketches/temporal-fields/.
//
// docs/probes/hud-runtime-validation-studio-probe.md §7 recommended this
// exact approach (direct triangulated mesh, shaped boundary, manually
// mapped UVs) over rectangular-texture-plus-mask, stencil clipping, or
// shader-discard clipping — citing shared/src/Fragment.cpp's own
// documented history of abandoning shader-discard clipping for live video
// after a real bug (see that file's drawTexturedCircleMesh() comment).
// "No shader-discard masking as default" (this task's own §12) matches
// that finding directly.
//
// Scene-independent: this class has never seen an IEcopunkScene, a
// SceneServices, or a scene ID — it only ever consumes a plain pixel-space
// bounds rectangle, a corner/bevel geometry configuration, and (at draw
// time only) an optional texture pointer + its pixel dimensions.
// ============================================================================

#include "ofMesh.h"
#include "ofRectangle.h"
#include "ofTexture.h"

#include "glm/vec2.hpp"

#include <vector>

namespace hudpresent {

struct MediaViewportGeometryParams {
	ofRectangle bounds; // pixel-space destination rectangle

	// Both in pixels, clamped internally to sane fractions of
	// min(bounds.width, bounds.height) so a small viewport can never
	// request a corner/bevel larger than the shape itself.
	float cornerRadius = 18.0f;
	float bevelSize = 42.0f;

	// Arc tessellation resolution matches TFShapeFragmentRenderer's own
	// documented convention (arcSteps(): ~6° per step, clamped [2,64]) —
	// reused as precedent, per this file's header comment, not by
	// including that class.
	float degreesPerArcStep = 6.0f;

	bool operator==(const MediaViewportGeometryParams& other) const {
		return bounds.x == other.bounds.x && bounds.y == other.bounds.y &&
			bounds.width == other.bounds.width && bounds.height == other.bounds.height &&
			cornerRadius == other.cornerRadius && bevelSize == other.bevelSize &&
			degreesPerArcStep == other.degreesPerArcStep;
	}
	bool operator!=(const MediaViewportGeometryParams& other) const { return !(*this == other); }
};

class MediaViewportMesh {
public:
	// Rebuilds the triangulated mesh + baked UVs ONLY if `params` or
	// `sourceSize` differ from the last call (exact equality check, no
	// epsilon/fuzz — this task's "no unnecessary per-frame rebuild"
	// requirement) — safe to call every frame; the common case is a cheap
	// early-out. `sourceSize` is the SceneFrame texture's native pixel
	// size (glm::ivec2(0,0) is a legal "no source yet" value — UVs are
	// still computed, just with a 1:1 assumed aspect ratio, since no
	// meaningful cover-fit can be computed without a real source size).
	void updateGeometry(const MediaViewportGeometryParams& params, glm::ivec2 sourceSize);

	// Draws the mesh. If `texture` is non-null and allocated, draws
	// textured (cover-fit UVs, baked at the last updateGeometry() call —
	// see that method's own comment on why texture-coordinate baking
	// happens there, not here). If `texture` is null/unallocated, draws a
	// flat `placeholderColor` fill at the exact same silhouette — the
	// wireframe-mode path this task's "No production textures in this
	// task" non-goal expects; both paths exercise identical geometry.
	// Restores GL state it touches (bound texture, fill color) before
	// returning — see this task's "GL state clean" requirement.
	void draw(const ofTexture* texture, ofColor placeholderColor) const;

	// Inspection surface for the Validation Studio and tests.
	const ofMesh& mesh() const { return mesh_; }
	size_t vertexCount() const { return mesh_.getNumVertices(); }
	size_t triangleCount() const { return mesh_.getNumVertices() >= 3 ? mesh_.getNumVertices() - 2 : 0; } // triangle fan

	// True only for the updateGeometry() call that actually rebuilt the
	// mesh (false on every early-out call) — performance-instrumentation
	// surface (this task's "MediaViewportMesh timing and vertex count").
	bool lastUpdateRebuilt() const { return lastUpdateRebuilt_; }

	const MediaViewportGeometryParams& currentParams() const { return lastParams_; }
	glm::ivec2 currentSourceSize() const { return lastSourceSize_; }

private:
	void rebuild(const MediaViewportGeometryParams& params, glm::ivec2 sourceSize);

	// mesh_'s baked texcoords are always normalized/percent [0,1] cover-fit
	// UVs — the class's public, texture-convention-agnostic contract (see
	// mesh()'s own doc comment and hud_media_viewport_tests.cpp, which
	// asserts directly against getTexCoord() in this range). `mutable`
	// because draw() (const) temporarily rewrites these into whatever
	// native coordinate convention the actually-bound ofTexture expects
	// (see draw()'s own .cpp comment for why that conversion is required
	// at all), then restores them back to the canonical percent values
	// before returning — so mesh_'s texcoords are always back to the
	// documented percent convention by the time any other const caller
	// (mesh(), a test, the Validation Studio inspector) can observe them.
	mutable ofMesh mesh_;
	// Parallel to mesh_'s vertex order — the canonical percent UVs, kept
	// separately so draw() can restore mesh_ exactly after its temporary
	// native-coordinate conversion, without recomputing coverUV() (which
	// would need params/sourceSize again) or risking float drift from a
	// convert-then-convert-back round trip.
	std::vector<glm::vec2> canonicalUV_;
	MediaViewportGeometryParams lastParams_{};
	glm::ivec2 lastSourceSize_{-1, -1};
	bool hasBuilt_ = false;
	bool lastUpdateRebuilt_ = false;
};

} // namespace hudpresent
