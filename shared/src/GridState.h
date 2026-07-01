#pragma once

#include "ofPixels.h"
#include "ofTexture.h"
#include <vector>

// A float grid that accumulates per-cell activity and decays every frame,
// uploaded as a grayscale texture shaders/placement logic can sample.
//
// Adapted from quadrant-crosshair/src/GridState.h, which accumulated at a
// fixed 24x18 resolution under a moving cursor position. This version takes
// its dimensions in setup() and accumulates by cell index instead of pixel
// position, since composition-cycle sketches place fragments on a grid
// rather than tracking a cursor.
class GridState {
	public:
		void setup(int gridW, int gridH, float decayRate = 0.995f);
		void update(); // applies decay, re-uploads texture

		void accumulate(int col, int row, float amount = 1.0f);
		void accumulateRect(int col, int row, int wCells, int hCells, float amount = 1.0f);

		// Returns [0.0, 1.0] — normalized activity for a cell
		float get(int col, int row) const;

		// Returns [0.0, 1.0] — mean activity across every cell. A cheap
		// "how busy is the whole composition right now" signal for anything
		// that wants to react to overall activity without sampling per-cell
		// (e.g. driving a motion-overlay's intensity).
		float getAverageActivity() const;

		const ofTexture & getTexture() const { return gridTex; }

		void clear();

	private:
		int gridW = 0;
		int gridH = 0;
		float decayRate = 0.995f;
		std::vector<float> grid;
		ofTexture gridTex;
		ofPixels gridPixels;
};
