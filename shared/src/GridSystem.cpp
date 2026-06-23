#include "GridSystem.h"

void GridSystem::setup(int cols_, int rows_, int canvasW, int canvasH){
	cols = cols_;
	rows = rows_;
	cellW = canvasW / cols; // integer math, no sub-pixel drift
	cellH = canvasH / rows;
}

ofRectangle GridSystem::cellRect(int col, int row) const{
	return ofRectangle(col * cellW, row * cellH, cellW, cellH);
}
