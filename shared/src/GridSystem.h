#pragma once

#include <vector>
#include "ofRectangle.h"

// Fixed reference grid that fragment placement snaps to, plus the
// occupancy tracking a placement algorithm needs to find free space.
class GridSystem {
	public:
		void setup(int cols, int rows, int canvasW, int canvasH);

		ofRectangle cellRect(int col, int row) const;

		bool isOccupied(int col, int row) const;
		void reserve(int col, int row, int w, int h);
		void clear();

		int getCols() const { return cols; }
		int getRows() const { return rows; }
		int getCellWidth() const { return cellW; }
		int getCellHeight() const { return cellH; }

	private:
		int cols = 0;
		int rows = 0;
		int cellW = 0;
		int cellH = 0;
		std::vector<bool> occupied; // row-major, cols*rows
};
