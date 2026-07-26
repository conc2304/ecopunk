#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "TFPattern.h"
#include "TFFragmentTransition.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofRectangle.h"
#include "ofVec2f.h"

// A tree-like branching structure: nodes are small fragments, edges are
// persistent lines connecting each node to its parent. Starts from 1-3 seed
// nodes near canvas center; on a growth clock, a random living node spawns a
// child at a set distance/angle (jittered around the parent's own outward-
// facing direction) from it.
//
// Nodes are never erased from the backing vector, only marked !alive — their
// index is their stable identity, which lets children reference a parent by
// plain index for the pattern's entire lifetime (a std::vector<Node> that
// shrank on removal would invalidate every other node's parentIndex). This
// is bounded and cheap: reset() (every pattern-cycle switch, ~90s per
// CYCLE_DURATION) clears the vector, so it can't grow unbounded across
// sessions.
//
// Deliberately NOT the same mechanism as TFHudLayer's connection-thread
// ripple effect (a transient, event-triggered, self-fading line spawned on
// fragment reassignment) — these edges are structural and permanent for the
// life of both endpoints, drawn by this class alone. See Section 0/2 of the
// brief.
//
// Transparency is close to automatic here: fragments only exist at node
// positions, not tessellating full canvas coverage, so most of the canvas
// is already a true hole "by construction" — see the class's draw().
class TFPatternNetworkGrowth : public TFPattern {
	public:
		struct Params {
			int seedNodeCount = 2; // 1-3
			float growthInterval = 2.0f; // seconds between branch attempts
			int maxNodeCount = 60; // cap on currently-alive nodes; spawning pauses above this

			float branchDistance = 0.08f; // normalized, fraction of the shorter canvas edge
			float branchDistanceJitter = 0.35f; // +/- fraction of branchDistance
			float branchAngleJitterDeg = 35.0f; // spread around the parent's outward-facing direction

			float nodeMinSize = 0.04f; // normalized, fraction of the shorter canvas edge
			float nodeMaxSize = 0.09f;
			float nodeReassignInterval = 6.0f; // each node's own offset-reassignment clock

			float rectangularChance = 20.0f; // 0-100%; chance a node spawns rectangular instead of square
			float minAspectRatio = 1.2f; // long-side:short-side ratio, rectangular nodes only
			float maxAspectRatio = 2.5f;

			// 0-100%; square-node-only chance of using a positional crop of
			// the source frame (matching every tessellating pattern) instead
			// of the whole buffered frame scaled to fit. Rectangular nodes
			// ignore this entirely — they always crop (Section 4 hard rule):
			// scaling a whole frame into a non-square box would distort it.
			float cropChance = 50.0f;

			bool nodeLifespanEnabled = true;
			float maxNodeAge = 30.0f; // seconds; only used when lifespan is enabled — does not cascade to children

			float edgeThickness = 1.5f;

			float transitionDuration = 1.0f;
			float hardCutWeight = 33.0f;
			float crossfadeWeight = 34.0f;
			float erosionWeight = 33.0f;
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
		struct Node {
			ofVec2f pos;
			int parentIndex = -1; // -1 = seed node, no parent edge
			float outwardAngleRad = 0.0f; // direction children jitter around when branching off this node
			float age = 0.0f;
			bool alive = true;

			// Rolled once at spawn, fixed for the node's lifetime (Section 2/3).
			// Fractions of the shorter canvas edge, like every other size in
			// this file, so they track resizeCanvas() the same way nodeSize
			// used to.
			float widthFrac = 0.0f;
			float heightFrac = 0.0f;
			// Rolled once at spawn, fixed for the node's lifetime (Section 4).
			// The specific crop rectangle is NOT cached here — draw()/update()
			// recompute it from the node's live pos every frame.
			bool useCroppedContent = false;

			float reassignTimer = 0.0f;
			float committedOffset = 0.0f;
			int playheadIndex = -1;
			int lastPlayheadIndex = -1; // -1 means "never assigned" — no transition on first assignment
			std::unique_ptr<TFFragmentTransition> transition; // lazily allocated; nullptr until first needed
		};

		void spawnSeedNodes();
		void spawnChildNode();
		int countAlive() const;

		// Rolls size, shape (square vs. rectangular), and content mode
		// (cropped vs. scaled-full-frame) for a freshly-created node — shared
		// by spawnSeedNodes()/spawnChildNode() so both roll the same way.
		void rollNodeAppearance(Node& n) const;
		ofRectangle nodeLocalRect(const Node& n) const;
		// Cropped mode reuses BSP/Blob Grid's proportional-crop mechanism
		// (Section 0), recomputed from `localRect` every call — never cached
		// — so it tracks the node's current position. Scaled-full-frame mode
		// is unchanged: tfComputeCropFillSrcRect() against the whole texture.
		ofRectangle nodeSrcRect(const Node& n, const ofRectangle& localRect, const ofTexture& tex) const;

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		int canvasW = 0;
		int canvasH = 0;
		Params params{};

		std::vector<Node> nodes;
		float elapsedTime = 0.0f;
		float noiseTime = 0.0f;
		float growthTimer = 0.0f;

		std::function<void(float nx, float ny)> onFragmentReassignedCb;
};
