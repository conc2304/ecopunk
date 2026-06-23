#pragma once

#include <string>
#include "GridSystem.h"

// Draws the canvas's institutional furniture: grid lines, the zone divider,
// and small text labels. Measurement lines and code-fragment text are added
// here in a later phase.
class AnnotationRenderer {
	public:
		void setup(const GridSystem* grid, int dividerCol, int canvasW, int canvasH);

		void drawGrid(float alpha) const;
		void drawDivider(float progress) const; // progress 0..1, grows top->bottom
		void drawCornerLabel(const std::string& label) const;

	private:
		const GridSystem* grid = nullptr;
		int dividerCol = 0;
		int canvasW = 0;
		int canvasH = 0;
};
