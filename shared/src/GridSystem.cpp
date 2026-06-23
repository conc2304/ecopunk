#include "GridSystem.h"
#include <algorithm>

void GridSystem::setup(int cols_, int rows_, int canvasW, int canvasH){
	cols = cols_;
	rows = rows_;
	cellW = canvasW / cols; // integer math, no sub-pixel drift
	cellH = canvasH / rows;
	occupied.assign(cols * rows, false);
}

ofRectangle GridSystem::cellRect(int col, int row) const{
	return ofRectangle(col * cellW, row * cellH, cellW, cellH);
}

bool GridSystem::isOccupied(int col, int row) const{
	if(col < 0 || row < 0 || col >= cols || row >= rows){
		return true; // out of bounds counts as occupied so callers can't place off-grid
	}
	return occupied[row * cols + col];
}

void GridSystem::reserve(int col, int row, int w, int h){
	for(int r = row; r < row + h; r++){
		for(int c = col; c < col + w; c++){
			if(c >= 0 && r >= 0 && c < cols && r < rows){
				occupied[r * cols + c] = true;
			}
		}
	}
}

void GridSystem::clear(){
	std::fill(occupied.begin(), occupied.end(), false);
}
