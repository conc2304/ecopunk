#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "TFPattern.h"
#include "TFFragmentTransition.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofRectangle.h"
#include "ofVec2f.h"

// A uniform grid of patches that age independently. Each patch's own
// maturity (age / Maturity Time, clamped 0..1) drives three things at once:
// how often it reassigns its playhead (young/"pioneer" patches churn fast,
// old/"climax" patches churn slowly — see update()), which transition style
// it favors (young -> hard cut, climax -> erosion, a linear blend per the
// brief), and how large it renders (inset within its cell, growing from
// ~25% at the sprout threshold to 100% at full climax).
//
// Patches below Sprout Threshold don't draw at all — the Section 0
// transparency requirement — skipping the draw call outright rather than
// drawing at alpha 0 (same reasoning as TFPatternBlobGrid's
// MASK_COVERAGE_THRESHOLD skip).
//
// Disturbance events periodically reset all patches within a random radius
// back to age zero, flashing the existing Quarantine Hatch visual
// (TFQuarantineHatch.h) over the affected area rather than inventing a
// second "flagged for change" visual language.
class TFPatternEcologicalSuccession : public TFPattern {
	public:
		struct Params {
			int gridResolution = 12; // uniform patch grid cols/rows

			float maturityTime = 45.0f; // seconds to reach full climax
			float youngTurnoverInterval = 2.0f; // reassignment interval at maturity 0
			float climaxTurnoverInterval = 20.0f; // reassignment interval at maturity 1

			// Transition style bias: hard-cut weight lerps youngHardCutWeight -> 0
			// and erosion weight lerps 0 -> climaxErosionWeight as maturity goes
			// 0 -> 1; crossfade weight stays constant across maturity.
			float youngHardCutWeight = 70.0f;
			float climaxErosionWeight = 70.0f;
			float crossfadeWeight = 30.0f;
			float transitionDuration = 1.2f;

			float sproutThreshold = 0.08f; // below this maturity, a patch doesn't draw at all
			float minRenderScale = 0.25f; // inset size just past the sprout threshold; grows to 1.0 at climax

			float disturbanceInterval = 12.0f; // seconds between disturbance events
			float disturbanceRadius = 0.18f; // normalized, fraction of the shorter canvas edge
			float disturbanceFlashDuration = 0.6f; // Quarantine Hatch flash length at reset
		};

		void setup(TimeOffsetVideoBuffer* videoBuffer, int canvasW, int canvasH, const Params& params);

		// Cheap POD copy — safe to call every frame so a live GUI panel can
		// push tuned values through without needing this class to know
		// anything about ofParameter/ofxGui.
		void setParams(const Params& p) { params = p; }

		void reset(int seed) override;
		void update(float dt) override;
		void draw() override;
		void resizeCanvas(int canvasW, int canvasH) override;

		void setOnFragmentReassigned(std::function<void(float nx, float ny)> cb) override { onFragmentReassignedCb = std::move(cb); }
		std::vector<ofVec2f> getActiveFragmentCenters() const override;

	private:
		struct Patch {
			ofRectangle bounds;
			float age = 0.0f;
			float reassignTimer = 0.0f;
			float committedOffset = 0.0f;
			int playheadIndex = -1;
			int lastPlayheadIndex = -1; // -1 means "never assigned" — no transition on first assignment
			std::unique_ptr<TFFragmentTransition> transition; // lazily allocated; nullptr until first needed

			// Quarantine hatch flash at disturbance reset — a fixed-duration
			// fade-out, independent of the sprout-threshold draw gate (the
			// flash reads over bare ground too, since it's the visible marker
			// of the disturbance itself, not of the patch's own content).
			float flashElapsed = -1.0f; // -1 = inactive
		};

		void rebuildGrid();
		void triggerDisturbance();

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		int canvasW = 0;
		int canvasH = 0;
		Params params{};

		std::vector<Patch> patches;
		int gridCols = 1;
		int gridRows = 1;

		float elapsedTime = 0.0f;
		float noiseTime = 0.0f;
		float disturbanceTimer = 0.0f;

		std::function<void(float nx, float ny)> onFragmentReassignedCb;
};
