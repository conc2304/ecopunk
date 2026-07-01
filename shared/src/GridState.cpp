#include "GridState.h"
#include "ofMath.h"

void GridState::setup(int gridW_, int gridH_, float decayRate_) {
	gridW = gridW_;
	gridH = gridH_;
	decayRate = decayRate_;
	grid.assign(static_cast<size_t>(gridW) * gridH, 0.0f);
	gridPixels.allocate(gridW, gridH, OF_PIXELS_GRAY);
	gridTex.allocate(gridPixels);
}

void GridState::update() {
	for (auto & v : grid) {
		v *= decayRate;
	}

	for (int i = 0; i < static_cast<int>(grid.size()); i++) {
		gridPixels[i] = static_cast<unsigned char>(ofClamp(grid[i], 0.0f, 1.0f) * 255.0f);
	}
	gridTex.loadData(gridPixels);
}

void GridState::accumulate(int col, int row, float amount) {
	if (col < 0 || row < 0 || col >= gridW || row >= gridH) {
		return;
	}
	float & cell = grid[row * gridW + col];
	cell = ofClamp(cell + amount, 0.0f, 1.0f);
}

void GridState::accumulateRect(int col, int row, int wCells, int hCells, float amount) {
	for (int r = row; r < row + hCells; r++) {
		for (int c = col; c < col + wCells; c++) {
			accumulate(c, r, amount);
		}
	}
}

float GridState::get(int col, int row) const {
	if (col < 0 || row < 0 || col >= gridW || row >= gridH) {
		return 0.0f;
	}
	return grid[row * gridW + col];
}

void GridState::clear() {
	std::fill(grid.begin(), grid.end(), 0.0f);
}

float GridState::getAverageActivity() const {
	if (grid.empty()) {
		return 0.0f;
	}
	float sum = 0.0f;
	for (float v : grid) {
		sum += v;
	}
	return ofClamp(sum / static_cast<float>(grid.size()), 0.0f, 1.0f);
}
