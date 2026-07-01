#pragma once

#include "ofRectangle.h"
#include <vector>

// A single grid line. Structural lines are generated at cycle start;
// fragment-derived lines are appended when a fragment places itself near
// no existing line; subdivision lines grow in mid-cycle inside an
// over-wide zone. See docs/blueprint_emergence "Grid System Handoff".
struct GridLine {
	float position = 0.0f; // canvas coordinate (x for vertical, y for horizontal)
	float opacity = 0.0f;
	float targetOpacity = 0.0f;
	float extentStart = 0.0f; // normalized 0..1 along the canvas
	float extentEnd = 1.0f;
	float homePosition = 0.0f; // structural-line drift center
	float driftPhase = 0.0f;
	bool isStructural = false;
	bool isSubdivision = false;
	int fragmentId = -1; // owning fragment, if fragment-derived; -1 otherwise

	// Subdivision growth-in-progress (extentStart or extentEnd animates toward
	// a target while opacity fades in alongside it).
	bool growing = false;
	bool growsFromRight = false; // true: extentStart shrinks toward target; false: extentEnd grows toward target
	float growTargetExtentStart = 0.0f;
	float growTargetExtentEnd = 1.0f;

	enum class Anim { WAITING, FADING_IN, STEADY, DISSOLVING };
	Anim anim = Anim::STEADY;
	float animTimer = 0.0f; // counts down while WAITING, counts up while FADING_IN/DISSOLVING
	float fadeDuration = 1.0f;
};

// Dynamic, content-derived grid: a handful of structural lines generated at
// cycle start (golden-ratio vertical subdivision, weighted-random
// horizontal subdivision), extended over the cycle by lines fragments
// contribute at their own edges, and occasionally subdivided mid-cycle.
// Replaces the old fixed-column/fixed-row equal grid; occupancy is tracked
// as actual placed rectangles since cells are no longer uniform.
class GridSystem {
	public:
		void setup(int canvasW, int canvasH, float dividerX);

		// Generates this cycle's structural lines, blending each with the
		// previous cycle's home position (INHERITANCE_WEIGHT/FRESH_WEIGHT).
		// Call once per cycle, after the cycle's RNG seed is set.
		void startNewCycle();

		void update(float dt);

		// Clears occupancy and fragment-derived/subdivision lines. Structural
		// line home positions survive this call, for startNewCycle()'s blend.
		void clear();

		// Occupancy, tracked as actual fragment rectangles rather than cell
		// indices (cells are no longer uniform). Each reservation is tagged
		// with the fragment that made it, so a long-running composition that
		// respawns the same slot many times within one cycle (see BEComposition's
		// quadrant-style slots) can release just that fragment's rect instead
		// of accumulating phantom occupancy for the rest of the cycle.
		bool isRectFree(const ofRectangle & bounds, float maxOverlapFraction) const;
		void reserve(int fragmentId, const ofRectangle & bounds);
		void releaseFragment(int fragmentId);
		std::vector<ofRectangle> getOccupiedRects() const;

		// Snap candidates for placement (Section 06): line positions + canvas
		// edges + the divider.
		std::vector<float> getSnapXPositions() const;
		std::vector<float> getSnapYPositions() const;

		// Manufactures a new structural position when no existing pair of
		// snap positions can satisfy a requested span ("the first fragment in
		// a cycle always generates its own grid").
		float addStructuralX(float pos);
		float addStructuralY(float pos);

		// Fragment-derived lines (Section 03) — call once per placed fragment,
		// after reserve(). `otherBounds` should exclude this fragment.
		void contributeFragmentEdges(int fragmentId, const ofRectangle & bounds,
			const std::vector<ofRectangle> & otherBounds);

		// Subdivision events (Section 04.2) — call after each placement.
		void maybeSubdivide();

		// Dissolve choreography (Section 04.3).
		void beginLineDissolveForFragment(int fragmentId, float fadeDuration);
		void beginStructuralDissolve();
		bool isDissolveFadeComplete() const;
		float getDividerOpacity() const { return dividerOpacity; }

		const std::vector<GridLine> & getVLines() const { return vLines; }
		const std::vector<GridLine> & getHLines() const { return hLines; }
		float getDividerX() const { return dividerX; }
		void setDividerX(float x) { dividerX = x; }
		int getCanvasW() const { return canvasW; }
		int getCanvasH() const { return canvasH; }

	private:
		void updateLine(GridLine & line, float dt, float driftAmplitude, float driftSpeed);
		std::vector<float> previousHomePositions(const std::vector<GridLine> & lines) const;
		float nearestLineDistance(float pos, const std::vector<GridLine> & lines) const;

		int canvasW = 0;
		int canvasH = 0;
		float dividerX = 0.0f;

		struct OccupiedRect {
			int fragmentId = -1;
			ofRectangle bounds;
		};

		std::vector<GridLine> vLines;
		std::vector<GridLine> hLines;
		std::vector<OccupiedRect> occupied;

		std::vector<float> previousVHome;
		std::vector<float> previousHHome;

		int subdivisionEventsThisCycle = 0;

		bool structuralDissolving = false;
		float structuralDissolveElapsed = 0.0f;
		float dividerOpacity = 1.0f;
		bool dividerDissolving = false;
		float dividerDissolveElapsed = 0.0f;
};
