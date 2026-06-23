#pragma once

#include "ofRectangle.h"

// Fixed reference grid that fragment placement snaps to. Cell math only —
// occupancy tracking (isOccupied/reserve/clear) is added once a composition
// system exists to call it.
class GridSystem {
	public:
		void setup(int cols, int rows, int canvasW, int canvasH);

		ofRectangle cellRect(int col, int row) const;

		int getCols() const { return cols; }
		int getRows() const { return rows; }
		int getCellWidth() const { return cellW; }
		int getCellHeight() const { return cellH; }

	private:
		int cols = 0;
		int rows = 0;
		int cellW = 0;
		int cellH = 0;
};
