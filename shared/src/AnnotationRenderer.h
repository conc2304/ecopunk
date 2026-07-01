#pragma once

#include <map>
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
		void setup(const GridSystem* grid, int canvasW, int canvasH);
		void loadCodeFont(const std::string& path, int size);
		void setMeasurementLineTiming(float drawSpeed, float fadeDelay, float fadeOpacity);
		void setCodeTextTiming(float intervalMin, float intervalMax, float opacityMin, float opacityMax);
		void setCodeFragments(std::vector<std::string> fragments);

		void onFragmentPlaced(Fragment* newFragment, Fragment* nearest);

		// Must be called whenever a Fragment this renderer may be tracking by
		// raw pointer (hub highlight, last-placed-for-code-text anchor,
		// connection counts) is about to be destroyed outside of a full
		// reset() — e.g. a composition slot respawning mid-cycle. Drops any
		// stale references to it so draw calls never dereference freed memory.
		void onFragmentRemoved(Fragment* frag);
		void update(float dt);
		void reset(); // clears measurement lines + code text; called on cycle restart

		void drawGrid(float alpha) const;
		void drawDivider(float progress, float brightness = 1.0f) const; // progress 0..1, grows top->bottom
		void drawCornerLabel(const std::string& label) const;
		void drawMeasurementLines() const;
		void drawCodeText() const;
		void drawHubHighlight() const;

		// LFO-driven tuning, set every frame by the sketch; both default to
		// "no change from baseline" so this stays optional.
		void setCodeTextWeight(float weight01) { codeTextLfoWeight = weight01; hasCodeTextLfoWeight = true; }

		// Pulses every active measurement line's opacity up, then lets it
		// decay back to the normal fade-delay behavior (CIRCLE_PLACED response).
		void pulseMeasurementLines(float peakOpacity, float decaySeconds);

		// Forces one autonomous code-text spawn outside the normal timer
		// (LONG_SILENCE / ZONE_IMBALANCE responses). Anchors near the most
		// recently placed fragment, same as the timer-driven spawn.
		void triggerCodeTextSpawn() { trySpawnCodeText(); }

		int getMaxConnectionCount() const { return maxConnectionCount; }

	private:
		struct MeasurementLine {
			Fragment* a = nullptr;
			Fragment* b = nullptr;
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

		bool findFreeTextSlot(glm::vec2 anchorPos, ofRectangle& outRect) const;
		void trySpawnCodeText();
		void recomputeHub();

		const GridSystem* grid = nullptr;
		int canvasW = 0;
		int canvasH = 0;

		ofTrueTypeFont codeFont;

		float mlineDrawSpeed = 400.0f;
		float mlineFadeDelay = 4.0f;
		float mlineFadeOpacity = 0.20f;
		std::vector<MeasurementLine> measurementLines;
		float mlinePulseOpacity = 0.0f;
		float mlinePulseDecay = 1.0f; // seconds, set per-pulse by pulseMeasurementLines()

		float codeTextIntervalMin = 3.0f;
		float codeTextIntervalMax = 8.0f;
		float codeTextOpacityMin = 0.5f;
		float codeTextOpacityMax = 0.7f;
		float codeTextTimer = 0;
		float codeTextLfoWeight = 0.5f;
		bool hasCodeTextLfoWeight = false;
		std::vector<std::string> codeFragments;
		std::vector<CodeTextEntry> codeTexts;
		std::vector<ofRectangle> usedTextRects; // slots that already have code text
		Fragment* lastPlacedFragment = nullptr;

		std::map<Fragment*, int> connectionCounts;
		Fragment* hubFragment = nullptr;
		int maxConnectionCount = 0;
		float hubHighlightT = 0.0f; // 0..1 ramp once a fragment becomes the hub
};
