#pragma once

// RawRevealController — scene-local, effect-independent, OF-free autonomous "raw video window".
//
// One full-height vertical rectangle that now and then reveals the UNPROCESSED frame inside the
// effected output, for visual A/B comparison. Not an effect mode and not a visitor control: it has
// no knowledge of effect ids and nothing here touches the Shared Effects schema or any contract.
//
//   HIDDEN_DWELL -> GROW -> DRIFT -> RETRACT -> HIDDEN_DWELL ...
//
// * HIDDEN_DWELL is a true mask-off state: width is EXACTLY 0 (the compositor draws no raw pixels).
// * Width and x follow smoothstep curves; the rectangle never leaves [0,1] and never moves vertically.
// * Per-cycle values (dwell, durations, max width, anchor, direction) come from a small deterministic
//   seeded sequence advanced ONCE per cycle (never per frame) - fixed seed => reproducible.
// * Time-driven (seconds): phase time carries over transitions exactly, so the result is a function
//   of total elapsed time (time-step independent within float tolerance). Pathological dt spikes are
//   clamped (kMaxStepSeconds).
// * The scene starts in (and reset() returns to) HIDDEN_DWELL - never a flash of raw frame.
//
// Art-direction defaults live in RevealTuning (scene-level constants, not user controls).

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace scenefx {

struct RevealTuning {
	float dwellMin = 7.f, dwellMax = 15.f;       // seconds fully effected
	float growMin = 3.f, growMax = 5.f;
	float driftMin = 2.f, driftMax = 5.f;
	float retractMin = 3.f, retractMax = 5.f;
	float widthMin = 0.20f, widthMax = 0.40f;    // of scene width
	float travelMin = 0.05f, travelMax = 0.18f;  // horizontal drift over the visible period, of scene width
	float edgeSoftPx = 4.f;                      // near-crisp edge (2..6 px)
	float firstDwellScale = 0.6f;                // the first dwell after (re)start is a little shorter
};

enum class RevealPhase { HiddenDwell, Grow, Drift, Retract };

class RawRevealController {
public:
	static constexpr float kMaxStepSeconds = 0.1f; // dt clamp: a hitch must not jump states

	explicit RawRevealController(uint32_t seed = 0xEC0F1u, RevealTuning tuning = RevealTuning{}) : seed_(seed), tuning_(tuning) { reset(); }

	// Back to a fresh HIDDEN_DWELL (activation, effect-mode change). Does not touch playback.
	void reset() {
		rng_ = seed_ ? seed_ : 1u;
		phase_ = RevealPhase::HiddenDwell;
		phaseT_ = 0.f;
		cycle_ = 0;
		beginCycle(true);
	}

	// `timeScale` is a QA/curation multiplier applied AFTER the dt clamp (1 = real time).
	void update(float dt, float timeScale = 1.f) {
		if (!(dt > 0.f) || !(timeScale > 0.f)) return;
		float remaining = std::min(dt, kMaxStepSeconds) * timeScale;
		for (int guard = 0; remaining > 0.f && guard < 64; ++guard) {
			const float left = phaseDuration() - phaseT_;
			if (remaining < left) { phaseT_ += remaining; remaining = 0.f; break; }
			remaining -= left; // finish the phase exactly, carry the rest into the next one
			advancePhase();
		}
	}

	RevealPhase phase() const { return phase_; }
	int cycleIndex() const { return cycle_; }
	bool visible() const { return width() > 0.f; }

	// Normalised [0,1] geometry of the rectangle (full height implied). width()==0 <=> hidden.
	float width() const {
		switch (phase_) {
			case RevealPhase::HiddenDwell: return 0.f;
			case RevealPhase::Grow: return maxWidth_ * smooth(progress());
			case RevealPhase::Drift: return maxWidth_;
			case RevealPhase::Retract: return maxWidth_ * (1.f - smooth(progress()));
		}
		return 0.f;
	}
	float left() const {
		const float w = width();
		if (w <= 0.f) return std::clamp(anchor_, 0.f, 1.f);                     // settles at the next anchor, invisible
		const float x = anchor_ + dir_ * travel_ * smooth(visibleProgress());  // slow bounded drift
		return std::clamp(x, 0.f, 1.f - w);                                     // whole visible width stays inside the scene
	}
	float right() const { return left() + width(); }

private:
	static float smooth(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }

	float progress() const { const float d = phaseDuration(); return d > 0.f ? phaseT_ / d : 1.f; }
	float visibleProgress() const {
		const float total = growS_ + driftS_ + retractS_;
		float t = phaseT_;
		if (phase_ == RevealPhase::Drift) t += growS_;
		else if (phase_ == RevealPhase::Retract) t += growS_ + driftS_;
		else if (phase_ == RevealPhase::HiddenDwell) t = 0.f;
		return total > 0.f ? t / total : 1.f;
	}
	float phaseDuration() const {
		switch (phase_) {
			case RevealPhase::HiddenDwell: return dwellS_;
			case RevealPhase::Grow: return growS_;
			case RevealPhase::Drift: return driftS_;
			case RevealPhase::Retract: return retractS_;
		}
		return 1.f;
	}
	void advancePhase() {
		phaseT_ = 0.f;
		switch (phase_) {
			case RevealPhase::HiddenDwell: phase_ = RevealPhase::Grow; break;
			case RevealPhase::Grow: phase_ = RevealPhase::Drift; break;
			case RevealPhase::Drift: phase_ = RevealPhase::Retract; break;
			case RevealPhase::Retract: phase_ = RevealPhase::HiddenDwell; ++cycle_; beginCycle(false); break;
		}
	}

	// Small deterministic sequence (xorshift32), advanced only when a new cycle is planned.
	float rand01() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return float(rng_ & 0xFFFFFFu) / float(0x1000000u); }
	float range(float a, float b) { return a + (b - a) * rand01(); }

	void beginCycle(bool first) {
		dwellS_ = range(tuning_.dwellMin, tuning_.dwellMax) * (first ? tuning_.firstDwellScale : 1.f);
		growS_ = range(tuning_.growMin, tuning_.growMax);
		driftS_ = range(tuning_.driftMin, tuning_.driftMax);
		retractS_ = range(tuning_.retractMin, tuning_.retractMax);
		maxWidth_ = range(tuning_.widthMin, tuning_.widthMax);
		travel_ = range(tuning_.travelMin, tuning_.travelMax);
		dir_ = rand01() < 0.5f ? -1.f : 1.f;
		const float room = std::max(0.f, 1.f - maxWidth_ - travel_ * 1.f);
		// anchor so that BOTH ends of the drift stay inside [0, 1 - width]
		anchor_ = (dir_ > 0.f ? 0.f : travel_) + room * rand01();
	}

	uint32_t seed_, rng_ = 1u;
	RevealTuning tuning_;
	RevealPhase phase_ = RevealPhase::HiddenDwell;
	float phaseT_ = 0.f;
	int cycle_ = 0;
	float dwellS_ = 10.f, growS_ = 4.f, driftS_ = 3.f, retractS_ = 4.f, maxWidth_ = 0.3f, travel_ = 0.1f, anchor_ = 0.3f, dir_ = 1.f;
};

// Pixel span of the raw window for a render width. Empty (x1 <= x0) when hidden or narrower than a
// pixel, so the compositor draws NO raw pixels at all - a hidden dwell can never leak a 1 px line.
struct RevealSpan { int x0 = 0, x1 = 0; bool empty() const { return x1 <= x0; } };

inline RevealSpan revealSpanPx(float left01, float right01, int widthPx) {
	RevealSpan s;
	if (!(right01 > left01) || widthPx <= 0) return s;
	s.x0 = std::clamp(int(std::lround(left01 * widthPx)), 0, widthPx);
	s.x1 = std::clamp(int(std::lround(right01 * widthPx)), 0, widthPx);
	return s;
}

} // namespace scenefx
