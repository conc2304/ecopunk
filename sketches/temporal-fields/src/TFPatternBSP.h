#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "TFPattern.h"
#include "TFFragmentTransition.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofRectangle.h"
#include "ofVec2f.h"

// Recursive rectangular subdivision. Every leaf cell references a playhead
// from the shared TimeOffsetVideoBuffer's pool (Phase 2) — several leaves
// with similar noise-derived offsets deliberately share the same playhead
// rather than each getting their own, since the pool is a hard, fixed size
// (5-8) while the tree can easily produce more leaves than that.
//
// The brief specifies the offset->gray mapping (linear, quantized) but not
// how a leaf's own gray value is derived spatially — Perlin noise sampled
// at each leaf's center, drifting slowly over time, is this implementation's
// answer: it gives the "amorphic" quality named in the sketch's own title,
// and naturally clusters neighboring cells onto the same playhead (since
// nearby noise samples are similar), which is exactly what the playhead
// pool needs to stay usable.
class TFPatternBSP : public TFPattern {
	public:
		struct Params {
			float irregularity = 0.55f; // 0..1, spread of split ratio around center
			float cellDensity = 0.06f; // min leaf size, as a fraction of the shorter canvas edge
			float geometryReshuffleRate = 3.0f; // seconds between piecemeal re-partitions
			int regionsTouchedPerTick = 1;

			// Fraction-ish of leaves suppressed via a second, decorrelated
			// noise channel — lets a background layer drawn beneath this
			// pattern show through. See assignPlayheads().
			float transparencyAmount = 0.15f;

			// Transition system (Phase 5) — shared in spirit with Blob Grid,
			// each pattern just owns its own copy of the same tunables.
			float transitionDuration = 0.8f;
			float hardCutWeight = 33.0f;
			float crossfadeWeight = 34.0f;
			float erosionWeight = 33.0f;

			// Quarantine hatch (Event Layer phase) — how long a leaf selected
			// for re-partitioning is held, hatch overlaid, before the actual
			// split happens. See reshuffleRegions()/update().
			float quarantineHoldDuration = 0.52f;
		};

		void setup(TimeOffsetVideoBuffer* videoBuffer, int canvasW, int canvasH, const Params& params);

		// Cheap POD copy — safe to call every frame so a live GUI panel can
		// push tuned values through without needing this class to know
		// anything about ofParameter/ofxGui.
		void setParams(const Params& p) { params = p; }

		void reset(int seed) override;
		void update(float dt) override;
		void draw() override;

		void setOnFragmentReassigned(std::function<void(float nx, float ny)> cb) override { onFragmentReassignedCb = std::move(cb); }
		std::vector<ofVec2f> getActiveFragmentCenters() const override;

	private:
		struct Node {
			ofRectangle bounds;
			bool isLeaf = true;
			int depth = 0;
			int playheadIndex = -1;
			int lastPlayheadIndex = -1; // -1 means "never assigned" — no transition on first assignment
			std::unique_ptr<TFFragmentTransition> transition; // lazily allocated; nullptr until first needed
			std::vector<Node> children; // empty, or exactly 2

			// Quarantine hatch: true while this leaf has been selected for
			// re-partitioning but hasn't actually split yet (held for
			// Params::quarantineHoldDuration with a hatch overlay drawn over
			// its bounds). Becomes moot once the node stops being a leaf —
			// split() replaces it with two fresh children.
			bool quarantined = false;
			float quarantineElapsed = 0.0f;
		};

		void split(Node& node);
		bool shouldStopSubdividing(const Node& node) const;
		void collectLeaves(Node& node, std::vector<Node*>& outLeaves);
		// Const, read-only counterpart to collectLeaves() — getActiveFragmentCenters()
		// can't use collectLeaves()'s non-const Node* pointers from a const method.
		void collectLeafCentersConst(const Node& node, std::vector<ofVec2f>& out) const;
		void assignPlayheads();
		void reshuffleRegions();

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		int canvasW = 0;
		int canvasH = 0;
		Params params{};

		Node root;
		float reshuffleTimer = 0.0f;
		float noiseTime = 0.0f;

		std::function<void(float nx, float ny)> onFragmentReassignedCb;
};
