#pragma once

#include <array>
#include <cstdint>
#include <string>

// BlobAnalysisDirector — the "evolving scene" brain for Specimen Track's
// Analysis Overlay mode (BlobProductionScene, mode "analysis_overlay").
//
// Pure and openFrameworks-free (test/blob_analysis_director_tests.cpp).
// It owns TIME and CHOICE only; it draws nothing and touches no scene state.
// The scene reads its State each frame and applies it:
//   - overlay element intensities  -> BlobAnalysisOverlay
//   - background look (mode/effect/dim) -> BlobSceneCore::BackgroundConfig
//   - fragment cap (growth)        -> VideoRegionController::Params
//   - effect palette weights       -> blobbudget::EffectBudget::setTable
//
// Three independent clocks drive the evolution:
//   1. Compositions: which HUD elements are alive. Each lasts ~20-30 s and
//      cross-fades (element intensities ease toward the target set).
//   2. Looks: the background treatment (desaturated / dark void / natural /
//      recolor / heatmap). A look swap is hidden behind a short dip-to-black
//      of the BACKGROUND only, so tracked fragments and the overlay stay up.
//   3. Palette epochs: the per-fragment effect weights used for NEW
//      fragments (existing fragments keep theirs, exactly like a media
//      change in BlobEffectPolicy.h).
//
// Growth makes the scene accumulate: each cycle starts sparse (few
// fragments, only the simplest compositions unlocked, one plexus link per
// node) and grows stage by stage (fragment cap up to the production max,
// all compositions unlocked, denser links) until a full cycle completes;
// it then "collapses" behind a dip-to-black and a new cycle begins with a
// different composition order and palette epoch.
//
// Deterministic: same seed + same dt sequence => identical output.
namespace blobanalysis {

enum Element {
	kBrackets = 0, // corner-bracket reticles + lock-on contraction
	kPlexus,       // nearest-neighbour centroid links + travelling pulses
	kTags,         // per-blob leader line + data readout
	kScan,         // sweeping scan line that lights frames as it passes
	kFrame,        // edge ticks, centre crosshair, corner registration marks
	kReadout,      // global counters + track-count sparkline
	kTrails,       // centroid motion trails
	kHull,         // convex-ish outline around all centroids
	kRings,        // range rings pulsing out from centroids
	kEvents,       // acquire / lost pings
	kElementCount
};

const char * elementName(int e);

// One entry per element; 0 = absent, 1 = full.
using ElementSet = std::array<float, kElementCount>;

struct Composition {
	const char * id;
	const char * name; // shown in the overlay readout
	int unlockStage;   // first growth stage at which this composition may be picked
	ElementSet targets;
};

int compositionCount();
const Composition & composition(int index);

// Background look: what BlobSceneCore::BackgroundConfig should be.
struct Look {
	const char * id;
	int drawMode;             // BackgroundConfig::mode (0 off, 1 natural, 2 effect)
	const char * effectName;  // used when drawMode == 2
	float baseDim;            // 0 = untouched video, 1 = black
};
int lookCount();
const Look & look(int index);

// Per-fragment effect palette ("epoch"): a withWeights() spec applied over
// blobbudget::defaultTable(), e.g. "original:5,recolor:1,heatmap_recolor:0".
struct Palette {
	const char * id;
	const char * weightsSpec;
};
int paletteCount();
const Palette & palette(int index);

struct Config {
	uint32_t seed = 0x5eed1234u;
	float compositionSeconds = 24.0f; // +/- ~25% per composition, deterministic
	float lookSeconds = 38.0f;
	float paletteSeconds = 61.0f;
	float elementEaseSeconds = 2.5f; // time constant for intensity cross-fades
	float dipSeconds = 1.6f;         // total dip-to-black (half out, half in)
	int compositionsPerStage = 2;
	int stageCount = 5;              // growth stages per cycle
	int minFragments = 3;
	int maxFragments = 10;           // never above VideoRegionController's production cap
};

struct State {
	// Compositions
	int compositionIndex = 0;
	int previousCompositionIndex = -1;
	float compositionAge = 0.0f; // seconds since the current composition began
	ElementSet elements{};       // smoothed, ready to draw with

	// Background look (apply to BackgroundConfig)
	int lookIndex = 0;
	int backgroundMode = 2;
	std::string backgroundEffect = "desaturate";
	float backgroundDim = 0.0f; // 0..1, dip-to-black already folded in

	// Growth
	int cycle = 0;
	int stage = 0;
	float growth = 0.0f; // 0..1 across the cycle, eased
	int maxFragments = 3;
	int linksPerNode = 1; // plexus neighbours, 1..3

	// Palette epoch
	int paletteIndex = 0;

	// Presentation helpers
	float transitionPulse = 0.0f; // 1 at a composition change, decays to 0 (flash cue)
};

// One-shot notifications since the last takeChanges() call.
struct Changes {
	bool lookChanged = false;
	bool paletteChanged = false;
	bool compositionChanged = false;
	bool cycleChanged = false;
};

class Director {
public:
	explicit Director(const Config & config = Config {});

	// Restart from a clean sparse cycle 0 (scene activation).
	void reset();

	void update(float dt);

	const State & state() const { return state_; }
	const Config & config() const { return config_; }

	// Returns and clears the pending change flags.
	Changes takeChanges();

private:
	uint32_t nextRand();
	float rand01();
	int pickComposition(int stage, int avoid);
	float compositionDuration();
	void beginComposition(int index);
	void beginCycle(bool first);
	void applyLook(int index);
	void updateGrowthOutputs();

	Config config_;
	State state_;
	Changes changes_;
	uint32_t rng_ = 0;

	float compositionTimer_ = 0.0f;
	float compositionLength_ = 0.0f;
	int compositionsThisStage_ = 0;
	float lookTimer_ = 0.0f;
	float paletteTimer_ = 0.0f;

	// Dip-to-black choreography. kIdle -> kFadingOut -> (swap) -> kFadingIn.
	enum class Dip { Idle, Out, In };
	Dip dip_ = Dip::Idle;
	float dipT_ = 0.0f;
	int pendingLook_ = -1;
	bool pendingCollapse_ = false;
	float dipLevel_ = 0.0f;
	float growthEased_ = 0.0f;
};

} // namespace blobanalysis
