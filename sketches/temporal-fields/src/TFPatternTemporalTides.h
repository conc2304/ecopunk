#pragma once

#include <functional>
#include <vector>
#include "TFPattern.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofRectangle.h"
#include "ofVec2f.h"

// No discrete transitions at all — a cell's playhead offset is a continuous
// function of its position and time, like a wave sweeping across the
// canvas: offset = clamp(0.5 + 0.5 * sin(t*tideSpeed - phaseCoord*waveLength*2pi) * amplitude, 0, 1).
// This bypasses TFFragmentTransition/the hard-cut/crossfade/erosion system
// entirely (confirmed fine per the brief's Section 0/3 — cheap, stateless,
// evaluated fresh every frame, no transition object needed) rather than
// routing a continuous value through machinery built for discrete "before ->
// after" cuts.
//
// The same scalar field also gates presence: cells whose wave value falls
// below Exposed Threshold are exposed/dry ground and are skipped entirely
// (Section 0's transparency requirement — a true hole, not a fade), while
// cells above it are submerged and draw normally. Hard cutoff at the
// threshold, confirmed against the browser prototype rather than guessed —
// no crossfade at the boundary.
class TFPatternTemporalTides : public TFPattern {
	public:
		enum class WaveDirection { HORIZONTAL, VERTICAL, DIAGONAL };

		struct Params {
			int gridResolution = 16; // uniform cell grid cols/rows
			float tideSpeed = 0.3f;
			float waveLength = 1.0f;
			float amplitude = 1.0f;
			WaveDirection waveDirection = WaveDirection::HORIZONTAL;
			float exposedThreshold = 0.35f; // below this wave value, a cell is exposed/dry and doesn't draw
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

		// Tides has no discrete reassignment moment to hook into — offset is
		// continuously recomputed every frame, so this callback is stored but
		// deliberately never fired (no HUD ripple/connection-thread/tick-stamp
		// cadence for this pattern). See TFPatternTemporalTides.cpp.
		void setOnFragmentReassigned(std::function<void(float nx, float ny)> cb) override { onFragmentReassignedCb = std::move(cb); }
		std::vector<ofVec2f> getActiveFragmentCenters() const override;

	private:
		struct Cell {
			ofRectangle bounds;
			int playheadIndex = -1; // recomputed fresh every frame; -1 = exposed/not drawn this frame
		};

		void rebuildGrid();

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		int canvasW = 0;
		int canvasH = 0;
		Params params{};

		std::vector<Cell> cells;
		int gridCols = 1;
		int gridRows = 1;

		float elapsedTime = 0.0f;

		std::function<void(float nx, float ny)> onFragmentReassignedCb;
};
