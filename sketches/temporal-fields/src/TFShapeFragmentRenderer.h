#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "TFFragmentShape.h"
#include "TFFragmentTransition.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofMesh.h"
#include "ofVec2f.h"

// One fragment, shared by the three "persistent cell, reassign in place"
// patterns (Bands, Column Grid, Telescoping Frames) — the
// RECT/WEDGE counterpart of TFPatternBSP::Node / TFPatternBlobGrid::Fragment,
// factored out here because those patterns are otherwise structurally
// identical (same playhead-pool assignment, same transition lifecycle),
// where BSP and Blob Grid were each distinct enough not to share one.
//
// Particle Field does NOT use this — its fragments spawn/age/die rather than
// persist and reassign in place, so it owns a separate lifecycle entirely
// (see TFPatternParticleField).
struct TFPatternFragment {
	TFFragmentShape shape;

	// Canvas-space position representative of this fragment, used both for
	// noise-based playhead sampling and as this fragment's reported
	// "center" (getActiveFragmentCenters(), for HUD ripple origins). Must
	// be set explicitly by whatever builds the fragment — for a RECT this
	// is naturally its own bounds' center, but a WEDGE's bounding-box
	// center (shape.bounds) is always the canvas center regardless of
	// angle, which would make every wedge sample identical noise; a WEDGE
	// builder must instead set this to a point inside the actual wedge
	// (e.g. mid-angle, mean radius). Defaults to the origin — every pattern
	// that builds fragments must assign this itself.
	ofVec2f samplePos;

	int playheadIndex = -1;
	int lastPlayheadIndex = -1; // -1 means "never assigned" — no transition on first assignment, matches BSP/Blob Grid
	std::unique_ptr<TFFragmentTransition> transition; // lazily allocated; nullptr until first needed
	ofMesh wedgeMesh; // scratch geometry rebuilt each draw when shape.kind == WEDGE; unused for RECT
};

// Reassigns every fragment's playhead from the shared noise field (the same
// pool-sharing tfAssignPlayheadsByNoise() both BSP and Blob Grid use), and
// starts a TFFragmentTransition on whichever fragments actually changed —
// factoring out the near-identical block BSP's assignPlayheads() and Blob
// Grid's updateFragmentAlphasAndPlayheads() each wrote independently.
void tfAssignFragmentPlayheadsAndTransitions(
	TimeOffsetVideoBuffer& videoBuffer,
	int canvasW, int canvasH,
	float noiseTime, float noiseScale,
	float transitionDuration, float hardCutWeight, float crossfadeWeight, float erosionWeight,
	std::vector<TFPatternFragment>& fragments,
	const std::function<void(float nx, float ny)>& onReassigned);

// Same transition-triggering behavior as above, but takes each fragment's
// desired offset directly instead of computing one from noise — used by
// Bands' Strata offset mode (see TFPatternBands.cpp), where each band's
// offset is fixed and index-proportional (offset = bandIndex / (bandCount -
// 1)) rather than reassigned from the noise field like every other pattern.
// `desiredOffsets` must be the same length and order as `fragments`.
void tfAssignFragmentPlayheadsAndTransitionsWithOffsets(
	TimeOffsetVideoBuffer& videoBuffer,
	int canvasW, int canvasH,
	const std::vector<float>& desiredOffsets,
	float transitionDuration, float hardCutWeight, float crossfadeWeight, float erosionWeight,
	std::vector<TFPatternFragment>& fragments,
	const std::function<void(float nx, float ny)>& onReassigned);

// Advances every fragment's in-flight transition. Call once per frame from
// the owning pattern's update().
void tfUpdateFragmentTransitions(std::vector<TFPatternFragment>& fragments, float dt);

// Draws one fragment. Mid-transition, always a plain rect dissolve/crossfade
// over shape.bounds — for WEDGE shapes this is an accepted simplification
// (see TFFragmentShape.h's comment): the ~1.6s transition briefly renders
// the wedge's full bounding square rather than being clipped to the true
// pie-slice/ring silhouette. At rest (no active transition), RECT draws its
// own bounds directly and WEDGE draws a texture-mapped mesh clipped to the
// real silhouette.
void tfDrawPatternFragment(TFPatternFragment& f, TimeOffsetVideoBuffer& videoBuffer, int canvasW, int canvasH, float alpha = 1.0f);

// Every visible fragment's normalized [0,1] bounding-box center — the shared
// body every new pattern's getActiveFragmentCenters() delegates to.
std::vector<ofVec2f> tfCollectFragmentCenters(const std::vector<TFPatternFragment>& fragments, int canvasW, int canvasH);
