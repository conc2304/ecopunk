#pragma once

#include <functional>
#include <vector>
#include "ofVec2f.h"

// Interface both pattern implementations (TFPatternBSP, TFPatternBlobGrid —
// Phase 3/4) conform to, so TFComposition can hold and swap between them
// without knowing which one is active.
class TFPattern {
	public:
		virtual ~TFPattern() = default;

		// Re-seeds and restarts this pattern's internal geometry/state for a
		// fresh cycle.
		virtual void reset(int seed) = 0;
		virtual void update(float dt) = 0;
		virtual void draw() = 0;

		// Updates the canvas dimensions this pattern lays geometry out
		// against (e.g. on a window resize). Does not itself regenerate
		// existing geometry -- callers that need the visible pattern to
		// reflect the new size immediately should follow up with reset();
		// an inactive pattern will pick up the new size for free the next
		// time TFComposition::switchToPattern() calls reset() on it.
		virtual void resizeCanvas(int canvasW, int canvasH) = 0;

		// Fired whenever a fragment's playhead offset actually changes — the
		// same moment that today triggers a hard cut/crossfade/erosion
		// transition internally (see TFPatternBSP::assignPlayheads() and
		// TFPatternBlobGrid::updateFragmentAlphasAndPlayheads()). nx/ny are
		// the fragment's center in normalized [0,1] canvas coordinates.
		virtual void setOnFragmentReassigned(std::function<void(float nx, float ny)> cb) = 0;

		// This frame's active (visible, currently drawn) fragment centers,
		// normalized [0,1]. Lets an outside consumer (e.g. the connection-
		// thread HUD effect) find fragments near an event's origin without
		// this pattern exposing its internal leaf/fragment representation.
		virtual std::vector<ofVec2f> getActiveFragmentCenters() const = 0;
};
