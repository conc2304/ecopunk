#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "TFPattern.h"
#include "TFFragmentTransition.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofFbo.h"
#include "ofRectangle.h"
#include "ofVec2f.h"

// A square grid masked by a drifting metaball field. Cells outside the
// field aren't drawn at all (true transparent holes); cells near the
// boundary feather in opacity; cells deep inside the field merge into
// larger blocks (an "adaptive quadtree" in effect, built here as a greedy
// block-merge scan rather than a literal recursive tree structure — same
// visual outcome, since the brief's own description is behavioral: bigger
// merged blocks deep inside, fine detail at the boundary).
//
// Each active fragment (whether a single base cell or a merged block)
// crops its own proportional region of a shared playhead's buffered frame,
// exactly like TFPatternBSP — see that class for why (all fragments
// together must reassemble one intact image, not tiled thumbnails).
class TFPatternBlobGrid : public TFPattern {
	public:
		struct Params {
			int gridResolution = 18; // base grid cols/rows
			int blobCenters = 3; // 1-8
			float driftSpeed = 1.0f;
			float blobRadius = 0.28f; // normalized, fraction of the shorter canvas edge
			float edgeSoftness = 0.30f;
			float sizeVariation = 0.50f; // 0 = uniform grid, 1 = ~7x base cell at max merge
			float fragmentRefreshRate = 3.0f; // seconds between quadtree re-quantizations

			// Transition system (Phase 5) — shared in spirit with BSP, each
			// pattern just owns its own copy of the same tunables. Note:
			// this only covers per-fragment playhead reassignment, not the
			// quadtree's own shape changes at a refresh tick — see
			// TFPatternBlobGrid.cpp's rebuildQuadtree() comment.
			float transitionDuration = 0.8f;
			float hardCutWeight = 33.0f;
			float crossfadeWeight = 34.0f;
			float erosionWeight = 33.0f;

			// Fragment masking & background compositing (Section 6a)
			bool maskToBlob = true; // false = debug view, draws every grid cell regardless of coverage
			bool transparentBackground = false; // Solid Ground (false) vs Transparent (true)
		};

		void setup(TimeOffsetVideoBuffer* videoBuffer, int canvasW, int canvasH, const Params& params);

		// Cheap POD copy — safe to call every frame so a live GUI panel can
		// push tuned values through without needing this class to know
		// anything about ofParameter/ofxGui.
		void setParams(const Params& p) { params = p; }

		void reset(int seed) override;
		void update(float dt) override;
		void draw() override;
		void resizeCanvas(int canvasW, int canvasH) override;

		void setOnFragmentReassigned(std::function<void(float nx, float ny)> cb) override { onFragmentReassignedCb = std::move(cb); }
		std::vector<ofVec2f> getActiveFragmentCenters() const override;

	private:
		struct Blob {
			ofVec2f anchor;
			float orbitRadius = 0.0f;
			float angularSpeed = 0.0f;
			float phase = 0.0f;
			ofVec2f pos;
		};

		struct Fragment {
			ofRectangle bounds;
			int playheadIndex = -1;
			int lastPlayheadIndex = -1; // -1 means "never assigned" — no transition on first assignment
			float alpha = 0.0f;
			std::unique_ptr<TFFragmentTransition> transition; // lazily allocated; nullptr until first needed

			// Hysteresis for the noise-derived offset — see
			// updateFragmentAlphasAndPlayheads(). -1 means "not yet committed".
			float lastCommittedOffset = -1.0f;
		};

		float fieldAt(float px, float py) const;
		bool isBlockSafelyInterior(int gx, int gy, int k) const;
		void updateBlobPositions();
		void rebuildQuadtree();
		void updateFragmentAlphasAndPlayheads();

		// The actual per-cell draw loop, shared by both background modes —
		// Solid Ground calls it directly against the screen; Transparent
		// calls it once into compositeFbo instead. See TFPatternBlobGrid.cpp.
		void drawFragments();

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		int canvasW = 0;
		int canvasH = 0;
		Params params{};

		std::vector<Blob> blobs;
		std::vector<Fragment> fragments;

		int gridCols = 1;
		int gridRows = 1;
		float baseCellW = 1.0f;
		float baseCellH = 1.0f;

		float elapsedTime = 0.0f;
		float refreshTimer = 0.0f;

		// Only allocated/used when params.transparentBackground is true.
		ofFbo compositeFbo;

		std::function<void(float nx, float ny)> onFragmentReassignedCb;
};
