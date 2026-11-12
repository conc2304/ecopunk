#pragma once

// ============================================================================
// BECollage — Blueprint Emergence "EDITORIAL_COLLAGE" visual grammar.
//
// A composition grammar, not a scene: owned and driven by BlueprintSceneCore,
// drawing into the scene's own output. It does not touch the HUD compositor,
// SceneManager, or any shared runtime contract; reusable drawing lives in
// scene-agnostic primitives (shared/src/hud/graphics).
//
// Model
//   * Source sampling and destination geometry are independent. Each video
//     element crops the one shared video texture anywhere and is drawn as ONE
//     quad with permuted UVs (quarter-turn + flips) — no per-element FBOs.
//   * A persistent registry of elements (stable ids), each with its OWN
//     lifecycle: Pending -> Entering -> Active -> (mutate xN) -> Exiting -> retired.
//   * A seeded, deterministic director (single RNG, dt-driven) schedules
//     micro / local / swell / recomposition events through an activity envelope
//     (QUIET / ACTIVE / SWELL / SETTLE). Dwell means reduced activity, never a
//     frozen screen. Anchors persist through ordinary events.
//   * Templates (generateLayout) only SEED roles; mutation keeps editing the
//     registry in the same grammar and recomposition morphs between grammars
//     while retaining an anchor.
// ============================================================================

#include "BECollageTransform.h"
#include "BECollageTuning.h"
#include "hud/graphics/GraphicPrimitives.h"
#include "ofRectangle.h"
#include "ofTexture.h"
#include "ofMesh.h"
#include "ofTrueTypeFont.h"
#include "glm/vec2.hpp"
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <random>
#include <string>
#include <vector>

enum class CollageRole { Hero,
	Secondary,
	Detail,
	Strip,
	Masked };

enum class CollageBlockType {
	SOLID_BLOCK,
	TRANSLUCENT_BLOCK,
	OUTLINE_FRAME,
	PARTIAL_FRAME,
	GRID_BLOCK,
	TICK_STRIP,
	DATA_STRIP,
	CIRCLE_SAMPLE_BACKPLATE,
	REGISTRATION_MARK,
	DIVIDER_BAR,
	CORNER_TAB,
	INDEX_PLATE
};

enum class CollagePhase { Placement,
	Density,
	Dissolve };

enum class CollageActivity { Quiet,
	Active,
	Swell,
	Settle };

// ── Template output (seed specification, not runtime state) ─────────────────

struct CollageFragment {
	ofRectangle sourceUvRect;
	ofRectangle destinationRect;
	QuarterTurn rotation = QuarterTurn::Deg0;
	bool flipX = false;
	bool flipY = false;
	float opacity = 1.0f;
	int zLayer = 0;
	bool hasFrame = false;
	float frameWidth = 0.0f;
	int paletteIndex = 0;

	CollageRole role = CollageRole::Secondary;
	bool circular = false;
	bool allowOverlap = false;
	bool showLabel = false;
	std::string labelTag;
	std::vector<glm::vec2> srcCenters;
	float srcScale = 0.5f;
	float swapPeriod = 0.0f;
	int stage = 0; // 0 = establish, 1 = accumulate
	float delay = 0.0f;
	float dur = 1.0f;
	hud::RevealDir revealDir = hud::RevealDir::LeftToRight;
	float dissolveDelay = 0.0f;
	float dissolveDur = 1.3f;
	bool hasOverlay = false;
	ofRectangle overlayRect;
	hud::PaletteToken overlayColor = hud::PaletteToken::Secondary;
	float overlayAlpha = 0.35f;
};

struct CollageGraphic {
	CollageBlockType type = CollageBlockType::SOLID_BLOCK;
	ofRectangle rect;
	hud::PaletteToken color = hud::PaletteToken::Primary;
	hud::PaletteToken textColor = hud::PaletteToken::Light;
	float alpha = 1.0f;
	int pass = 0; // 0 under-blocks, 1 grids, 3 over-video, 5 small primitives, 6 labels
	hud::RevealDir revealDir = hud::RevealDir::None;
	int stage = 0;
	float delay = 0.0f;
	float dur = 0.8f;
	float dissolveDelay = 5.0f;
	float dissolveDur = 1.0f;
	bool linger = false;
	bool vertical = false;
	bool filled = true;
	bool segmented = true;
	bool pulse = false;
	int markKind = 0;
	float value = 0.5f;
	float strokeWidth = 2.0f;
	std::string text;
};

struct CollageLayout {
	uint32_t seed = 0;
	int templateIndex = 0;
	const char * templateName = "";
	hud::HudCollagePalette palette;
	std::vector<CollageFragment> fragments;
	std::vector<CollageGraphic> graphics;
};

// ── Runtime registry ────────────────────────────────────────────────────────

enum class ElemState { Pending,
	Entering,
	Active,
	Exiting };

struct CollageCrop {
	glm::vec2 center { 0.5f, 0.5f }; // normalized source position
	float scale = 0.5f; // fraction of the maximal aspect-correct crop
	QuarterTurn rot = QuarterTurn::Deg0;
	bool flipX = false;
	bool flipY = false;
};

// Eased scalar animation (opacity, frame/grid reveal, bar value, overlay).
struct CollageTween {
	float cur = 0.0f, from = 0.0f, to = 0.0f, t = 1.0f, dur = 1.0f;
	void snap(float v) { cur = from = to = v; t = 1.0f; }
	void set(float target, float d) {
		from = cur;
		to = target;
		t = 0.0f;
		dur = d > 0.01f ? d : 0.01f;
	}
	bool running() const { return t < 1.0f; }
	void update(float dt) {
		if (t >= 1.0f) return;
		t += dt / dur;
		if (t >= 1.0f) {
			t = 1.0f;
			cur = to;
			return;
		}
		const float u = t < 0.5f ? 4.0f * t * t * t : 1.0f - 4.0f * (1.0f - t) * (1.0f - t) * (1.0f - t);
		cur = from + (to - from) * u;
	}
};

struct CollageElement {
	uint32_t id = 0;
	bool video = false;
	CollageRole role = CollageRole::Secondary;
	CollageBlockType type = CollageBlockType::SOLID_BLOCK;

	// lifecycle
	ElemState state = ElemState::Pending;
	float stateT = 0.0f;
	float enterDur = 0.8f, exitDur = 0.6f;
	float spawnAt = 0.0f; // director time
	float born = 0.0f;
	float minHold = 3.0f;
	float exitAt = -1.0f; // scheduled retirement (director time), <0 = none
	int zLayer = 0;
	int stage = 0; // 0 = establishing layer, 1 = accumulating layer (initial build only)
	int group = 0; // related elements (split children, montage)
	bool anchor = false;
	bool linger = false;
	hud::RevealDir revealDir = hud::RevealDir::None;

	// geometry (animated)
	ofRectangle rect, rectFrom, rectTo;
	float moveT = 1.0f, moveDur = 1.0f;

	// video
	CollageCrop crop, prevCrop;
	float cropT = 1.0f, cropDur = 1.0f;
	hud::RevealDir cropWipe = hud::RevealDir::LeftToRight;
	bool circular = false;
	bool hasFrame = false;
	float frameWidth = 4.0f;
	int paletteIndex = 0;
	CollageTween frame;
	bool hasOverlay = false;
	ofRectangle overlayFrac; // overlay region as fractions of the fragment rect
	int overlayAxis = 0; // 0: coverage scales width from the left, 1: height from the top
	hud::PaletteToken overlayColor = hud::PaletteToken::Secondary;
	float overlayAlphaMax = 0.35f;
	CollageTween overlay; // opacity 0..1
	CollageTween cover; // coverage 0..1
	bool showLabel = false;
	std::string labelTag;
	bool pendingDiversify = false;

	// graphic
	hud::PaletteToken color = hud::PaletteToken::Primary;
	hud::PaletteToken textColor = hud::PaletteToken::Light;
	float alphaMax = 1.0f;
	CollageTween alpha; // opacity multiplier
	CollageTween ext; // grid / tick reveal extension
	CollageTween value; // data bar
	int bind = 0; // 0 decorative, 1 = video coverage (honest binding)
	int pass = 0;
	bool vertical = false, filled = true, segmented = true, pulse = false;
	int markKind = 0;
	float flash = 0.0f;
	float strokeWidth = 2.0f;
	std::string text;

	bool inTransition() const {
		return state == ElemState::Entering || state == ElemState::Exiting || moveT < 1.0f || cropT < 1.0f
			|| frame.running() || overlay.running() || cover.running() || alpha.running() || ext.running();
	}
	const ofRectangle & finalRect() const { return moveT < 1.0f ? rectTo : rect; }
};

class BECollage {
public:
	static constexpr int TEMPLATE_COUNT = 5;
	// After DISSOLVE starts, the selective clear-out takes this long.
	static constexpr float DISSOLVE_TOTAL_SECONDS = 4.5f;

	void setup(int canvasW, int canvasH, const std::string & fontPath, int fontSize);

	// Pure + deterministic: same (seed, template) -> same layout.
	static CollageLayout generateLayout(uint32_t seed, int canvasW, int canvasH, int forcedTemplate = -1);

	// New cycle. The first call builds the initial plate; later calls RECOMPOSE:
	// an anchor is retained, the rest of the registry is retired in a stagger and
	// the new grammar's roles enter around it. Ignored while locked unless force.
	void beginLayout(uint32_t seed, bool force = false);
	void setPhase(CollagePhase phase);
	void update(float dt);
	void draw(const ofTexture * video) const;
	bool dissolveFinished() const;

	// Developer tuning (the one tuning surface — see BECollageTuning.h).
	void setTuning(const CollageTuning & t);
	void setProfile(const char * name) { setTuning(CollageTuning::byName(name)); }
	const CollageTuning & tuning() const { return tuning_; }

	// Dev / capture.
	void lock(bool on) { locked_ = on; }
	// Fast-forwards the current layout by `seconds` of establish-only time
	// (director events off), then freezes it. densityStart < 0 => placement only.
	void debugSetTimeline(float seconds, float densityStart);
	void forceTemplate(int index) { forcedTemplate_ = index; }
	// Morphs to another grammar now (anchor retained). No-op before the first layout.
	bool swapTemplate(int index);
	int templateIndex() const { return hasLayout_ ? layout_.templateIndex : 0; }

	struct FragmentStats {
		int visible = 0, left = 0, right = 0, total = 0;
	};
	FragmentStats fragmentStats() const;

	// Introspection (tests, capture, logs).
	const CollageLayout & layout() const { return layout_; }
	const std::vector<CollageElement> & elements() const { return elements_; }
	// Dev: 4x4 chart (rows = 0/90/180/270 deg, cols = none/flipX/flipY/both) drawn through the
	// real fragment-quad path from an asymmetric test texture, to verify UV orientation.
	void debugDrawOrientationChart(const ofTexture * tex) const;
	ofRectangle cropUv(const CollageElement & e, float texAspect) const { return resolveSourceUv(e.crop, e.rect, texAspect); }
	const char * displayName() const { return "EDITORIAL COLLAGE"; }
	const char * templateName() const { return layout_.templateName; }
	bool hasLayout() const { return hasLayout_; }
	CollageActivity activity() const { return activity_; }
	float activityLevel() const { return level_; }
	float directorTime() const { return time_; }
	size_t queueSize() const { return queue_.size(); }
	int inTransitionCount() const;
	uint64_t stateHash() const;
	struct LogEntry {
		float t;
		CollageEvent type;
		uint32_t id;
		int fragments, graphics;
	};
	const std::deque<LogEntry> & eventLog() const { return log_; }
	uint64_t eventCount() const { return eventCount_; }
	uint64_t eventCountOf(CollageEvent e) const { return perEventCount_[static_cast<int>(e)]; }
	int swellCount() const { return swellCount_; }
	int recomposeCount() const { return recomposeCount_; }

private:
	struct QueuedEvent {
		float at;
		CollageEvent type;
		uint32_t target;
		int param;
	};
	struct Vis {
		float reveal = 0.0f;
		float fade = 1.0f;
		bool visible() const { return reveal > 0.001f && fade > 0.001f; }
	};

	// rng helpers (single pathway)
	float rnd(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng_); }
	float rnd(const CollageRange & r) { return rnd(r.lo, r.hi); }
	int irnd(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng_); }
	bool chance(float p) { return rnd(0.0f, 1.0f) < p; }

	// registry
	CollageElement & addElement(CollageElement e);
	CollageElement * find(uint32_t id);
	const CollageElement * find(uint32_t id) const;
	void startExit(CollageElement & e, float dur = -1.0f);
	void updateElement(CollageElement & e, float dt, float cover);
	Vis visOf(const CollageElement & e) const;
	void spawnFromLayout(const CollageLayout & L, float baseT, float stageOffset, const CollageElement * keepAnchor, float delayScale);
	void scheduleTimers();
	int regionOf(const ofRectangle & r) const;
	void pickAnchors();
	int fragmentCount() const; // non-exiting video elements
	int graphicCount() const; // non-exiting graphics
	float videoCover() const;
	float sideBalance() const; // -1 left heavy .. +1 right heavy (video area)
	bool placementFree(const ofRectangle & r, uint32_t ignoreA, uint32_t ignoreB, float maxOverlap) const;
	bool findPlacement(float w, float h, ofRectangle & out, uint32_t ignoreA = 0, uint32_t ignoreB = 0);
	CollageCrop chooseCrop(const ofRectangle & dest, QuarterTurn rot, CollageRole role);
	void pickTransform(bool forceOdd, QuarterTurn & t, bool & fx, bool & fy);
	CollageElement makeVideoElement(CollageRole role, const ofRectangle & dest, bool forceOdd, bool circular);

	// director
	void directorUpdate(float dt);
	void updateActivity(float dt);
	bool fireTier(bool micro);
	bool runWeighted(bool micro);
	bool runEvent(CollageEvent e, uint32_t target, int param);
	void enqueue(float delay, CollageEvent e, uint32_t target = 0, int param = 0);
	void processQueue();
	void startSwell();
	void startSettle();
	void recompose(uint32_t seed, int templateIndex, bool immediate);
	void startDissolveClear();
	void logEvent(CollageEvent e, uint32_t id);
	void applyPaletteTarget(const hud::HudCollagePalette & p);

	// events (each returns false when rejected by layout constraints)
	bool evOverprint(uint32_t target);
	bool evFrameEdge(uint32_t target);
	bool evReveal(uint32_t target, CollageBlockType type);
	bool evMarkPulse(uint32_t target);
	bool evMarkMove(uint32_t target);
	bool evDataUpdate(uint32_t target);
	bool evAddMicro(uint32_t target, int param);
	bool evRemoveMicro(uint32_t target, bool excess);
	bool evCropSwap(uint32_t target);
	bool evMoveResize(uint32_t target);
	bool evReplace(uint32_t target, bool strip);
	bool evTransform(uint32_t target);
	bool evSplit(uint32_t target);
	bool evMerge(uint32_t target);
	bool evBlockSlide(uint32_t target);
	bool evMontage(uint32_t target);
	bool evSwellMass(uint32_t target);
	CollageElement * pickFragment(uint32_t target, bool allowAnchor, bool needHold);
	CollageElement * pickGraphic(uint32_t target, const std::function<bool(const CollageElement &)> & pred, bool needHold);

	// drawing
	ofRectangle resolveSourceUv(const CollageCrop & c, const ofRectangle & dest, float texAspect) const;
	void drawFragmentQuad(const ofRectangle & dest, const CollageCrop & c, const ofTexture * tex, hud::RevealDir wipe, float wipeReveal, float alpha, bool circular) const;
	void drawGraphic(const CollageElement & g, const Vis & v) const;
	void drawLabelPlate(const ofRectangle & r, const std::string & text, float alpha) const;
	void drawText(const std::string & s, float x, float y) const;
	ofColor col(hud::PaletteToken t) const { return activePal_.resolve(t); }

	int W_ = 1280, H_ = 720;
	float g_ = 1280.0f / 24.0f, y0_ = 0.0f;
	ofTrueTypeFont font_;
	bool fontLoaded_ = false;
	mutable ofMesh mesh_;
	mutable std::vector<const CollageElement *> drawOrder_;

	CollageLayout layout_;
	bool hasLayout_ = false;
	hud::HudCollagePalette activePal_, fromPal_;
	float paletteMix_ = 1.0f;

	std::vector<CollageElement> elements_;
	uint32_t nextId_ = 1;
	int nextGroup_ = 1;
	int zCounter_ = 0;
	uint32_t anchorId_ = 0;

	// director state
	CollageTuning tuning_;
	std::mt19937 rng_ { 1u };
	float time_ = 0.0f;
	CollageActivity activity_ = CollageActivity::Active;
	float level_ = 1.0f, levelTarget_ = 1.0f, activityLeft_ = 12.0f;
	float accMicro_ = 0.0f, nextMicro_ = 2.0f;
	float accLocal_ = 0.0f, nextLocal_ = 5.0f;
	float swellAcc_ = 0.0f, nextSwell_ = 25.0f;
	float recomposeAcc_ = 0.0f, nextRecompose_ = 80.0f;
	float anchorAcc_ = 0.0f, nextAnchorRotate_ = 35.0f;
	std::vector<QueuedEvent> queue_;
	std::deque<LogEntry> log_;
	uint64_t eventCount_ = 0;
	uint64_t perEventCount_[COLLAGE_EVENT_COUNT] = {};
	float lastEventTime_[COLLAGE_EVENT_COUNT] = {};
	int lastRegion_ = -1;
	int swellCount_ = 0, recomposeCount_ = 0;
	bool dissolving_ = false;
	float dissolveT0_ = -1.0f;
	bool directorEnabled_ = true;
	bool locked_ = false;
	int forcedTemplate_ = -1;
	uint32_t lastSeed_ = 0;
	uint32_t touched_ = 0;
	int labelCounter_ = 0;
};
