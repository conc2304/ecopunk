// OC-004 -- Scene Visual Activity Measurement Probe: OpenFrameworks-free core.
//
// DIAGNOSTIC / opt-in. ExperienceRuntime-private. Not published to the HUD, and it does not define or
// feed `scene.activity.overall` (that stays scene-semantic; see SceneSemanticTypes.h).
//
// Measurement: successive 64x36 luminance grids of the rendered SceneFBO (pre-HUD, composited over
// black). RAW = mean absolute per-cell difference between two successive valid grids, in 0..1 luminance
// units. Raw is preserved separately from every derived value in this file: normalisers, the smoother and
// the segment mapping are pure functions of the raw series, so they can be swapped or re-run offline.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>

namespace sceneactivity {

constexpr int kGridW = 64;
constexpr int kGridH = 36;
constexpr int kGridCells = kGridW * kGridH;
constexpr double kNominalPeriodSeconds = 0.1; // ~10 Hz

// Why a sample slot has (or has not) a raw value.
enum class State : uint8_t {
	Valid,        // raw is the difference between this grid and the previous one, one nominal period apart
	Wide,         // raw is valid but spans > kWidePeriods nominal periods (late sample / missed frames)
	First,        // first grid of a generation (or after a gap): nothing to difference against yet
	Static,       // transition static frame: SceneFBO holds a retained frame, no new measurement exists
	SceneChange,  // generation boundary row (history reset)
	Unsupported,  // SceneFBO format not measurable / GPU path unavailable
	Missing       // live scene, but no sample arrived for > kStalePeriods nominal periods
};
constexpr int kStalePeriods = 3;
constexpr double kWidePeriods = 1.5;

inline const char* stateName(State s) {
	switch (s) {
		case State::Valid: return "valid";
		case State::Wide: return "wide";
		case State::First: return "first";
		case State::Static: return "static";
		case State::SceneChange: return "scenechange";
		case State::Unsupported: return "unsupported";
		case State::Missing: return "missing";
	}
	return "?";
}
inline bool hasRaw(State s) { return s == State::Valid || s == State::Wide; }

// Mean absolute difference of two equally sized grids (CPU reference for the GPU reduction).
inline double meanAbsDiff(const float* a, const float* b, std::size_t n, std::size_t strideA = 1, std::size_t strideB = 1) {
	if (!a || !b || n == 0) return std::numeric_limits<double>::quiet_NaN();
	double acc = 0.0;
	for (std::size_t i = 0; i < n; ++i) acc += std::fabs(static_cast<double>(a[i * strideA]) - static_cast<double>(b[i * strideB]));
	return acc / static_cast<double>(n);
}

// ---- Normalisation -------------------------------------------------------------------------------------
// All three map raw (>= 0) to a 0..1 level through the same perceptual-ish curve  n = (raw / ref)^gamma
// clamped to [0,1], differing only in how `ref` (the raw value that fills the meter) is chosen.
struct NormParams {
	double fixedRef = 0.04;     // raw that fills the meter under the fixed strategy
	double gamma = 0.5;         // <1 expands small motion (raw spans ~3 decades)
	double floorRaw = 0.002;    // rolling strategies never use a ref below this (stillness must not self-amplify)
	double windowSeconds = 60.0;
	double adaptLo = 0.5, adaptHi = 2.0; // bounded adaptive: ref stays within [adaptLo, adaptHi] * fixedRef
};

inline double applyCurve(double raw, double ref, double gamma) {
	if (!(ref > 0.0) || !(raw >= 0.0)) return 0.0;
	return std::min(1.0, std::pow(raw / ref, gamma));
}

// Rolling maximum of raw over a time window (monotonic deque, O(1) amortised).
class RollingMax {
public:
	void reset() { q_.clear(); }
	void push(double t, double v, double window) {
		while (!q_.empty() && q_.back().v <= v) q_.pop_back();
		q_.push_back({t, v});
		while (!q_.empty() && q_.front().t < t - window) q_.pop_front();
	}
	double value() const { return q_.empty() ? 0.0 : q_.front().v; }
private:
	struct E { double t, v; };
	std::deque<E> q_;
};

struct Normalised {
	double fixed = 0.0;     // strategy 1
	double rolling = 0.0;   // strategy 2
	double bounded = 0.0;   // strategy 3
	double rollingRef = 0.0, boundedRef = 0.0;
};

class Normalizer {
public:
	explicit Normalizer(NormParams p = {}) : p_(p) {}
	const NormParams& params() const { return p_; }
	void setParams(const NormParams& p) { p_ = p; }
	// A scene change resets the rolling window: another scene's dynamic range is not this scene's.
	void reset() { roll_.reset(); }
	Normalised push(double t, double raw) {
		roll_.push(t, raw, p_.windowSeconds);
		Normalised n;
		n.fixed = applyCurve(raw, p_.fixedRef, p_.gamma);
		n.rollingRef = std::max(roll_.value(), p_.floorRaw);
		n.rolling = applyCurve(raw, n.rollingRef, p_.gamma);
		n.boundedRef = std::clamp(roll_.value(), p_.fixedRef * p_.adaptLo, p_.fixedRef * p_.adaptHi);
		n.bounded = applyCurve(raw, n.boundedRef, p_.gamma);
		return n;
	}
private:
	NormParams p_;
	RollingMax roll_;
};

// ---- Fast-attack / slow-decay smoothing (time-constant based, cadence independent) ---------------------
struct SmootherParams {
	double attackSeconds = 0.08;
	double decaySeconds = 0.9;
};
class Smoother {
public:
	explicit Smoother(SmootherParams p = {}) : p_(p) {}
	void reset() { have_ = false; v_ = 0.0; }
	// A missing measurement (static / gap) holds nothing: it decays toward 0 at the slow rate so a stale
	// bar never stays lit. `dt` is the time since the previous push.
	double push(double x, double dt) {
		if (!have_) { v_ = x; have_ = true; t_ = 0.0; return v_; }
		const double tau = x > v_ ? p_.attackSeconds : p_.decaySeconds;
		const double a = tau > 0.0 ? 1.0 - std::exp(-std::max(0.0, dt) / tau) : 1.0;
		v_ += a * (x - v_);
		return v_;
	}
	double value() const { return v_; }
	bool has() const { return have_; }
private:
	SmootherParams p_;
	bool have_ = false;
	double v_ = 0.0, t_ = 0.0;
};

// Illuminated segment count for an N-segment meter: 0 only when the level is exactly 0 (no lit segment
// is shown for "no information"; callers decide missing/stale separately), otherwise ceil so any
// real activity lights at least one segment.
inline int segmentCount(double level, int segments) {
	if (!(level > 0.0)) return 0;
	return std::clamp(static_cast<int>(std::ceil(level * segments - 1e-9)), 1, segments);
}

} // namespace sceneactivity
