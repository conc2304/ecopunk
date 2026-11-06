#pragma once

// OC-004 production publication state for the scene visual-activity measurement. OF-free.
//
// ExperienceRuntime-owned. The SceneActivityProbe (GL half) feeds it the measurement events; this class
// turns them into the immutable scenevisual::SceneVisualActivitySnapshot and caches it so an unchanged
// state republishes the same shared_ptr (a refcount bump per frame). It performs no sampling and no
// smoothing (smoothing and segment mapping are HUD-owned).

#include "SceneVisualActivitySnapshot.h"
#include "SceneActivityCore.h"

#include <cmath>
#include <memory>
#include <string>

namespace sceneactivity {

// level = clamp(log10(raw / floor) / log10(ceiling / floor), 0, 1). A raw of exactly 0 (or below the
// floor) is a valid level 0; non-finite or negative input is not a measurement and returns 0 only because
// the tracker never feeds it (see onSample).
inline double fixedLogLevel(double raw) {
	using scenevisual::kLogCeiling;
	using scenevisual::kLogFloor;
	if (!(raw > kLogFloor)) return 0.0;
	return std::min(1.0, std::log10(raw / kLogFloor) / std::log10(kLogCeiling / kLogFloor));
}

class VisualActivityTracker {
public:
	void setPeriod(double seconds) { if (seconds > 0.0) period_ = seconds; }
	double period() const { return period_; }

	// New scene generation: every prior observation is invalid.
	void beginGeneration(uint64_t generation, const std::string& sceneId) {
		generation_ = generation; sceneId_ = sceneId;
		haveValid_ = false; transition_ = false; raw_ = 0.0; rawT_ = 0.0;
		++revision_;
	}
	// A valid (or wide-but-valid) raw difference. Non-finite or negative raw is rejected, not stored as 0.
	void onSample(double t, double raw) {
		if (!std::isfinite(raw) || raw < 0.0) return;
		raw_ = raw; rawT_ = t; haveValid_ = true; transition_ = false;
		++revision_;
	}
	// A live grid with nothing to difference against (first of a generation / after a static run).
	void onLiveNoMeasurement() {
		if (transition_) { transition_ = false; ++revision_; }
	}
	// A transition static frame: the retained frame is not a measurement, and an observation from before
	// the transition no longer describes what is on screen.
	void onStatic() {
		if (transition_ && !haveValid_) return;
		transition_ = true; haveValid_ = false;
		++revision_;
	}
	// The GPU path cannot measure (format/GLES/shader failure): honest Unavailable.
	void onUnsupported() {
		if (!haveValid_ && !transition_) return;
		haveValid_ = false; transition_ = false;
		++revision_;
	}

	scenevisual::State evaluate(double now) const {
		if (transition_) return scenevisual::State::Transition;
		if (!haveValid_) return scenevisual::State::Unavailable;
		return (now - rawT_) > kStalePeriods * period_ ? scenevisual::State::Stale : scenevisual::State::Valid;
	}

	std::shared_ptr<const scenevisual::SceneVisualActivitySnapshot> build(double now) const {
		auto s = std::make_shared<scenevisual::SceneVisualActivitySnapshot>();
		s->revision = revision_;
		s->generation = generation_;
		s->sceneId = sceneId_;
		s->state = evaluate(now);
		if (s->state == scenevisual::State::Valid || s->state == scenevisual::State::Stale) {
			s->raw = static_cast<float>(raw_);
			s->rawT = rawT_;
			s->level = static_cast<float>(fixedLogLevel(raw_));
		}
		s->nominalPeriodSeconds = period_;
		s->stalePeriods = kStalePeriods;
		return s;
	}
	uint64_t revision() const { return revision_; }

private:
	double period_ = kNominalPeriodSeconds;
	uint64_t generation_ = 0, revision_ = 0;
	std::string sceneId_;
	bool haveValid_ = false, transition_ = false;
	double raw_ = 0.0, rawT_ = 0.0;
};

// Rebuilds the snapshot only when the tracker revision or the time-evaluated state changed.
class VisualActivityPublisher {
public:
	const std::shared_ptr<const scenevisual::SceneVisualActivitySnapshot>& publish(const VisualActivityTracker& t, double now) {
		const auto st = t.evaluate(now);
		if (!cached_ || cachedRevision_ != t.revision() || cachedState_ != st) {
			cached_ = t.build(now);
			cachedRevision_ = t.revision();
			cachedState_ = st;
		}
		return cached_;
	}
private:
	std::shared_ptr<const scenevisual::SceneVisualActivitySnapshot> cached_;
	uint64_t cachedRevision_ = 0;
	scenevisual::State cachedState_ = scenevisual::State::Unavailable;
};

} // namespace sceneactivity
