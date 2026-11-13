#include "BlobAnalysisDirector.h"

#include <algorithm>
#include <cmath>

namespace blobanalysis {

namespace {

// Element order: Brackets, Plexus, Tags, Scan, Frame, Readout, Trails, Hull,
// Rings, Events. Unlock stages make the scene grow: early cycles only know
// the simple compositions, later stages add the denser ones.
const Composition kCompositions[] = {
	{ "acquisition", "ACQUISITION", 0, { 1.00f, 0.00f, 0.00f, 0.00f, 0.80f, 0.35f, 0.00f, 0.00f, 0.00f, 1.00f } },
	{ "lattice", "LATTICE", 0, { 0.60f, 1.00f, 0.00f, 0.00f, 0.30f, 0.00f, 0.00f, 0.00f, 0.00f, 0.80f } },
	{ "survey", "SURVEY", 1, { 0.80f, 0.00f, 0.80f, 1.00f, 0.90f, 0.00f, 0.00f, 0.00f, 0.00f, 0.60f } },
	{ "telemetry", "TELEMETRY", 1, { 0.70f, 0.00f, 1.00f, 0.00f, 0.40f, 1.00f, 0.70f, 0.00f, 0.00f, 0.50f } },
	{ "constellation", "CONSTELLATION", 2, { 0.30f, 1.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.80f, 1.00f, 0.60f, 0.60f } },
	{ "radar", "RADAR SWEEP", 3, { 0.60f, 0.00f, 0.50f, 0.80f, 0.60f, 0.00f, 0.00f, 0.00f, 1.00f, 1.00f } },
	{ "full_spectrum", "FULL SPECTRUM", 4, { 0.80f, 0.80f, 0.70f, 0.50f, 0.80f, 0.70f, 0.60f, 0.70f, 0.50f, 0.90f } },
};
constexpr int kCompositionCount = static_cast<int>(sizeof(kCompositions) / sizeof(kCompositions[0]));

// Effects here must be ones the Blob production path already treats as
// eligible and cheap (BlobEffectBudget.cpp defaultTable(): recolor and
// heatmap_recolor are pointwise, ~1.0-1.25 cost). No neighbour-sampling
// effects for the full-frame background on a Pi 3B.
const Look kLooks[] = {
	{ "desaturated", 2, "desaturate", 0.45f },
	{ "void", 0, "desaturate", 0.00f },
	{ "natural", 1, "desaturate", 0.50f },
	{ "recolor", 2, "recolor", 0.55f },
	{ "heatmap", 2, "heatmap_recolor", 0.50f },
};
constexpr int kLookCount = static_cast<int>(sizeof(kLooks) / sizeof(kLooks[0]));

const Palette kPalettes[] = {
	{ "observe", "original:6,recolor:1,heatmap_recolor:0" },
	{ "spectral", "original:2,recolor:5,heatmap_recolor:0" },
	{ "thermal", "original:2,recolor:1,heatmap_recolor:5" },
	{ "mixed", "original:4,recolor:3,heatmap_recolor:2" },
};
constexpr int kPaletteCount = static_cast<int>(sizeof(kPalettes) / sizeof(kPalettes[0]));

const char * const kElementNames[kElementCount] = {
	"brackets", "plexus", "tags", "scan", "frame", "readout", "trails", "hull", "rings", "events"
};

float smooth01(float x) {
	x = std::min(1.0f, std::max(0.0f, x));
	return x * x * (3.0f - 2.0f * x);
}

} // namespace

const char * elementName(int e) {
	return (e >= 0 && e < kElementCount) ? kElementNames[e] : "?";
}

int compositionCount() { return kCompositionCount; }
const Composition & composition(int index) { return kCompositions[std::max(0, std::min(kCompositionCount - 1, index))]; }
int lookCount() { return kLookCount; }
const Look & look(int index) { return kLooks[std::max(0, std::min(kLookCount - 1, index))]; }
int paletteCount() { return kPaletteCount; }
const Palette & palette(int index) { return kPalettes[std::max(0, std::min(kPaletteCount - 1, index))]; }

Director::Director(const Config & config)
	: config_(config) {
	reset();
}

void Director::reset() {
	rng_ = config_.seed != 0 ? config_.seed : 0x5eed1234u;
	state_ = State {};
	changes_ = Changes {};
	dip_ = Dip::Idle;
	dipT_ = 0.0f;
	dipLevel_ = 0.0f;
	pendingLook_ = -1;
	pendingCollapse_ = false;
	growthEased_ = 0.0f;
	lookTimer_ = 0.0f;
	paletteTimer_ = 0.0f;
	compositionsThisStage_ = 0;
	beginCycle(true);
}

uint32_t Director::nextRand() {
	// xorshift32: tiny, stable across platforms.
	uint32_t x = rng_;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	rng_ = x;
	return x;
}

float Director::rand01() {
	return static_cast<float>(nextRand() & 0xffffffu) / static_cast<float>(0x1000000u);
}

int Director::pickComposition(int stage, int avoid) {
	// Newly unlocked compositions are favoured so the scene visibly gains
	// vocabulary as it grows; nothing repeats back-to-back.
	float weights[kCompositionCount];
	float total = 0.0f;
	for (int i = 0; i < kCompositionCount; i++) {
		weights[i] = 0.0f;
		if (i == avoid || kCompositions[i].unlockStage > stage) {
			continue;
		}
		weights[i] = (kCompositions[i].unlockStage == stage && stage > 0) ? 3.0f : 1.0f;
		total += weights[i];
	}
	if (total <= 0.0f) {
		return avoid == 0 ? std::min(1, kCompositionCount - 1) : 0;
	}
	float r = rand01() * total;
	for (int i = 0; i < kCompositionCount; i++) {
		r -= weights[i];
		if (weights[i] > 0.0f && r <= 0.0f) {
			return i;
		}
	}
	return 0;
}

float Director::compositionDuration() {
	return config_.compositionSeconds * (0.75f + 0.5f * rand01());
}

void Director::beginComposition(int index) {
	state_.previousCompositionIndex = state_.compositionIndex;
	state_.compositionIndex = index;
	state_.compositionAge = 0.0f;
	state_.transitionPulse = 1.0f;
	compositionTimer_ = 0.0f;
	compositionLength_ = compositionDuration();
	changes_.compositionChanged = true;
}

void Director::applyLook(int index) {
	const Look & l = kLooks[index];
	state_.lookIndex = index;
	state_.backgroundMode = l.drawMode;
	state_.backgroundEffect = l.effectName;
	changes_.lookChanged = true;
}

void Director::beginCycle(bool first) {
	state_.stage = 0;
	compositionsThisStage_ = 0;
	growthEased_ = 0.0f;
	if (!first) {
		state_.cycle++;
		changes_.cycleChanged = true;
		// New cycle -> next palette epoch and a different look, so it does
		// not replay the previous cycle.
		state_.paletteIndex = (state_.paletteIndex + 1) % kPaletteCount;
		changes_.paletteChanged = true;
		paletteTimer_ = 0.0f;
		int next = static_cast<int>(nextRand() % static_cast<uint32_t>(kLookCount));
		if (next == state_.lookIndex) {
			next = (next + 1) % kLookCount;
		}
		applyLook(next);
		lookTimer_ = 0.0f;
	} else {
		state_.paletteIndex = 0;
		applyLook(0);
		changes_.paletteChanged = true;
	}
	// Seed the new cycle with a stage-0 composition; on a collapse keep the
	// old element mix so it eases out rather than snapping.
	int first0 = pickComposition(0, first ? -1 : state_.compositionIndex);
	beginComposition(first0);
	if (first) {
		state_.previousCompositionIndex = -1;
		state_.elements = kCompositions[first0].targets;
	}
	updateGrowthOutputs();
	state_.backgroundDim = kLooks[state_.lookIndex].baseDim;
}

void Director::updateGrowthOutputs() {
	state_.growth = smooth01(growthEased_);
	int span = std::max(0, config_.maxFragments - config_.minFragments);
	state_.maxFragments = config_.minFragments + static_cast<int>(std::lround(state_.growth * static_cast<float>(span)));
	state_.maxFragments = std::min(config_.maxFragments, std::max(config_.minFragments, state_.maxFragments));
	state_.linksPerNode = 1 + (state_.growth > 0.35f ? 1 : 0) + (state_.growth > 0.70f ? 1 : 0);
}

Changes Director::takeChanges() {
	Changes c = changes_;
	changes_ = Changes {};
	return c;
}

void Director::update(float dt) {
	if (dt <= 0.0f) {
		return;
	}
	dt = std::min(dt, 0.25f); // a media-load stall must not skip whole compositions

	const int stageCount = std::max(1, config_.stageCount);
	const int perStage = std::max(1, config_.compositionsPerStage);

	// -- Growth easing toward the position inside the cycle ---------------
	{
		float target = (static_cast<float>(state_.stage) + static_cast<float>(compositionsThisStage_) / static_cast<float>(perStage))
			/ static_cast<float>(stageCount);
		target = std::min(1.0f, std::max(0.0f, target));
		// Slow follow (~6 s) so a stage change reads as growth, not a jump.
		growthEased_ += (target - growthEased_) * (1.0f - std::exp(-dt / 6.0f));
		updateGrowthOutputs();
	}

	// -- Composition clock -------------------------------------------------
	state_.compositionAge += dt;
	compositionTimer_ += dt;
	state_.transitionPulse *= std::exp(-dt / 0.6f);
	if (compositionTimer_ >= compositionLength_) {
		compositionsThisStage_++;
		bool collapse = false;
		if (compositionsThisStage_ >= perStage) {
			compositionsThisStage_ = 0;
			if (state_.stage + 1 >= stageCount) {
				collapse = true;
			} else {
				state_.stage++;
			}
		}
		if (collapse) {
			// End of a full growth cycle: collapse behind a dip, then start
			// over sparse. If a look dip is mid-flight, wait for it.
			if (dip_ == Dip::Idle) {
				dip_ = Dip::Out;
				dipT_ = 0.0f;
				pendingCollapse_ = true;
				// Undo the wrap so the stage stays on the final one until the swap.
				compositionsThisStage_ = perStage - 1;
				compositionTimer_ = 0.0f;
				compositionLength_ = config_.dipSeconds * 2.0f; // hold the last composition across the dip
			} else {
				compositionsThisStage_ = perStage - 1;
				compositionTimer_ = compositionLength_; // retry next frame
			}
		} else {
			beginComposition(pickComposition(std::min(state_.stage, stageCount - 1), state_.compositionIndex));
		}
	}

	// -- Look clock --------------------------------------------------------
	lookTimer_ += dt;
	if (lookTimer_ >= config_.lookSeconds && dip_ == Dip::Idle && !pendingCollapse_) {
		int next = static_cast<int>(nextRand() % static_cast<uint32_t>(kLookCount));
		if (next == state_.lookIndex) {
			next = (next + 1) % kLookCount;
		}
		pendingLook_ = next;
		dip_ = Dip::Out;
		dipT_ = 0.0f;
		lookTimer_ = 0.0f;
	}

	// -- Palette epoch clock ----------------------------------------------
	paletteTimer_ += dt;
	if (paletteTimer_ >= config_.paletteSeconds) {
		paletteTimer_ = 0.0f;
		state_.paletteIndex = (state_.paletteIndex + 1) % kPaletteCount;
		changes_.paletteChanged = true;
	}

	// -- Dip-to-black choreography ----------------------------------------
	if (dip_ != Dip::Idle) {
		float half = std::max(0.05f, config_.dipSeconds * 0.5f);
		dipT_ += dt;
		if (dip_ == Dip::Out) {
			dipLevel_ = smooth01(dipT_ / half);
			if (dipT_ >= half) {
				dipLevel_ = 1.0f;
				// Fully black: swap what is hidden behind it.
				if (pendingCollapse_) {
					pendingCollapse_ = false;
					beginCycle(false);
				} else if (pendingLook_ >= 0) {
					applyLook(pendingLook_);
					pendingLook_ = -1;
				}
				dip_ = Dip::In;
				dipT_ = 0.0f;
			}
		} else {
			dipLevel_ = 1.0f - smooth01(dipT_ / half);
			if (dipT_ >= half) {
				dipLevel_ = 0.0f;
				dip_ = Dip::Idle;
				dipT_ = 0.0f;
			}
		}
	}
	float base = kLooks[state_.lookIndex].baseDim;
	state_.backgroundDim = base + (1.0f - base) * dipLevel_;

	// -- Element cross-fade -----------------------------------------------
	const ElementSet & target = kCompositions[state_.compositionIndex].targets;
	float k = 1.0f - std::exp(-dt / std::max(0.05f, config_.elementEaseSeconds));
	for (int i = 0; i < kElementCount; i++) {
		state_.elements[i] += (target[i] - state_.elements[i]) * k;
	}
}

} // namespace blobanalysis
