#pragma once

#include <string>
#include <vector>
#include "ofTrueTypeFont.h"
#include "GridSystem.h"
#include "Fragment.h"

// Draws the canvas's institutional furniture: grid lines, the zone divider,
// small text labels, measurement lines between fragments, and the
// autonomous code-fragment text overlay.
//
// Timing/tuning values (measurement-line draw speed/fade, code-text spawn
// interval/opacity) are passed in by the sketch rather than hardcoded here,
// same reasoning as Fragment's drift amplitude — keeps this class reusable
// by sketches with different BESettings.h-style constants.
class AnnotationRenderer {
	public:
		void setup(const GridSystem* grid, int dividerCol, int canvasW, int canvasH);
		void loadCodeFont(const std::string& path, int size);
		void setMeasurementLineTiming(float drawSpeed, float fadeDelay, float fadeOpacity);
		void setCodeTextTiming(float intervalMin, float intervalMax, float opacityMin, float opacityMax);
		void setCodeFragments(std::vector<std::string> fragments);

		void onFragmentPlaced(Fragment* newFragment, Fragment* nearest);
		void update(float dt);
		void reset(); // clears measurement lines + code text; called on cycle restart

		void drawGrid(float alpha) const;
		void drawDivider(float progress) const; // progress 0..1, grows top->bottom
		void drawCornerLabel(const std::string& label) const;
		void drawMeasurementLines() const;
		void drawCodeText() const;

	private:
		struct MeasurementLine {
			glm::vec2 p1, p2;
			std::string label;
			float age = 0;
			float totalDist = 0;
		};

		struct CodeTextEntry {
			glm::vec2 pos;
			std::string text;
			float opacity = 1.0f;
		};

		bool findFreeCellNear(int anchorCol, int anchorRow, int& outCol, int& outRow) const;
		void trySpawnCodeText();

		const GridSystem* grid = nullptr;
		int dividerCol = 0;
		int canvasW = 0;
		int canvasH = 0;

		ofTrueTypeFont codeFont;

		float mlineDrawSpeed = 400.0f;
		float mlineFadeDelay = 4.0f;
		float mlineFadeOpacity = 0.20f;
		std::vector<MeasurementLine> measurementLines;

		float codeTextIntervalMin = 3.0f;
		float codeTextIntervalMax = 8.0f;
		float codeTextOpacityMin = 0.5f;
		float codeTextOpacityMax = 0.7f;
		float codeTextTimer = 0;
		std::vector<std::string> codeFragments;
		std::vector<CodeTextEntry> codeTexts;
		std::vector<std::pair<int,int>> usedTextCells; // (col,row) cells that already have code text
		Fragment* lastPlacedFragment = nullptr;
};
