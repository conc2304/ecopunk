#pragma once

#include "BlobAnalysisDirector.h"
#include "VideoRegion.h"

#include "ofMesh.h"
#include "ofRectangle.h"
#include "ofTrueTypeFont.h"

#include <string>
#include <vector>

// BlobAnalysisOverlay — the "scientific instrument" annotation layer for
// Specimen Track's Analysis Overlay mode.
//
// Draws ONLY lines, points and bitmap text over whatever the scene already
// rendered (no FBOs, no shaders, no textures — Pi 3B friendly). Every line
// of the frame is batched into one vertex-coloured ofMesh, so a busy frame
// is a handful of draw calls. What is visible at any moment is decided by
// blobanalysis::Director (per-element intensities); this class owns only
// per-track bookkeeping (lock-on timers, trails, acquire/lost pings, track
// count history) and the drawing itself.
//
// Scene-owned annotation, not a HUD: it is part of this scene's own picture
// (BlobProductionScene only instantiates it in the analysis mode), so it
// does not conflict with the runtime HudCompositor.
class BlobAnalysisOverlay {
public:
	// What the overlay prints; filled by BlobProductionScene from real,
	// already-computed scene state (nothing here is invented).
	struct Readout {
		int tracks = 0;
		int fragments = 0;
		int maxFragments = 0;
		float coverage = 0.0f; // 0..1, BlobSceneCore::occupiedAreaFraction()
		std::string compositionName;
		std::string paletteName;
		std::string lookName;
		int cycle = 0;
		int stage = 0;
		int stageCount = 1;
		float growth = 0.0f;
	};

	void reset();

	// regions: this frame's tracked regions (normalized source space);
	// srcW/srcH: native video size; dest: the cover-fit destination rect
	// (the same one the background used).
	void update(float dt, const std::vector<VideoRegion> & regions, float srcW, float srcH, const ofRectangle & dest);

	void draw(const blobanalysis::State & state, const Readout & readout, const ofRectangle & dest);

	int nodeCount() const { return static_cast<int>(nodes_.size()); }

private:
	struct Node {
		int id = -1;
		ofRectangle rect; // screen space
		glm::vec2 c { 0.0f, 0.0f };
		glm::vec2 normC { 0.0f, 0.0f };
		float conf = 0.0f;
		float area = 0.0f; // normalized box area (fraction of the source frame)
		float age = 0.0f;
		float missing = 0.0f;
		float lockT = 0.0f; // seconds since this overlay first saw the track
		float trailTimer = 0.0f;
		std::vector<glm::vec2> trail;
		bool seen = false;
	};
	struct Ping {
		int id = -1;
		ofRectangle rect;
		glm::vec2 c { 0.0f, 0.0f };
		float age = 0.0f;
		float life = 1.0f;
		bool lost = false;
	};

	static constexpr int kMaxNodes = 16;
	static constexpr int kMaxPings = 12;
	static constexpr int kHistory = 48;

	// -- Batched primitives (all append to lines_) --
	void line(glm::vec2 a, glm::vec2 b, float alphaA, float alphaB, bool alert = false);
	void line(glm::vec2 a, glm::vec2 b, float alpha, bool alert = false) { line(a, b, alpha, alpha, alert); }
	void dashed(glm::vec2 a, glm::vec2 b, float alpha, float dash, float gap, float offset, bool alert = false);
	void ring(glm::vec2 c, float r, float alpha, int segments, float startAngle, float arc, bool dashedRing, bool alert = false);
	void cross(glm::vec2 c, float r, float alpha, bool alert = false);
	void diamond(glm::vec2 c, float r, float alpha, bool alert = false);
	void cornerBracket(const ofRectangle & r, float arm, float alpha, bool alert);
	void text(const std::string & s, float x, float y, float scale, float alpha, bool alert = false);

	// -- Elements --
	void drawFrame(float i, const ofRectangle & dest, float pulse);
	void drawScan(float i, const ofRectangle & dest, float scanX);
	void drawTrails(float i);
	void drawRings(float i);
	void drawHull(float i);
	void drawPlexus(float i, int linksPerNode, const ofRectangle & dest);
	void drawBrackets(float i, float scanX, float scanI);
	void drawEvents(float i);
	void drawTags(float i, const ofRectangle & dest);
	void drawReadout(float i, const blobanalysis::State & state, const Readout & readout, const ofRectangle & dest);
	void drawBanner(const blobanalysis::State & state, const Readout & readout, const ofRectangle & dest);

	Node * find(int id);
	void ensureFont();

	std::vector<Node> nodes_;
	std::vector<Ping> pings_;
	std::vector<int> history_;
	float historyTimer_ = 0.0f;
	float time_ = 0.0f;
	ofMesh lines_;
	ofTrueTypeFont font_;
	bool fontTried_ = false;
	struct PendingText {
		std::string s;
		float x, y, scale, alpha;
		bool alert;
	};
	std::vector<PendingText> texts_;
};
