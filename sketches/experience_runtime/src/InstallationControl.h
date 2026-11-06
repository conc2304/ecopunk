#pragma once

// ============================================================================
// InstallationControl.h — Installation Controls & ENERGY v0.1.
//
// The ONE authoritative installation-control model, owned by
// ExperienceRuntime. OF-free and header-only so test/installation_control_
// tests.cpp exercises it headless.
//
// It owns settings (validated), the scene auto-cycle schedule, the ENERGY
// target/smoothing, and a request interface. It never touches scenes,
// decoders or GL itself: ExperienceRuntime asks it what to do each frame and
// performs the work through the existing paths (RuntimeCommand::NextScene via
// SceneManager, Shared Video's automatic-advance policy, ofSetFullscreen).
//
// Threading: submit()/tick*() are runtime-thread only. enqueue() is the one
// thread-safe entry, for a future local REST server: requests are queued and
// applied by the runtime thread in drainQueue() — no other thread mutates
// runtime state.
// ============================================================================

#include <algorithm>
#include <cmath>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace installation {

struct AutoCycle {
	bool enabled = false;
	float durationSeconds = 120.0f;
};

struct Settings {
	AutoCycle sceneAutoCycle{false, 120.0f}; // off by default: existing behavior
	AutoCycle mediaAutoCycle{true, 30.0f};   // = Shared Video's existing automatic dwell
	bool launchFullscreen = false;
	float energyTarget = 0.5f;
	bool clock24Hour = true; // app-owned time display preference (Experience Settings page); never changes the OS clock
	// System Mode (Experience Settings page): "auto" (day/night skin schedule), a skin
	// package id, or "minimal.light.<hue>" / "minimal.dark.<hue>" (see MinimalTheme.h),
	// plus the selected background preset for each minimal group.
	std::string systemMode = "auto";
	int minimalBackgroundLight = 0;
	int minimalBackgroundDark = 0;
	// Navigation-button labels: "text" (default), "icon" (icon only) or "both" (icon above text).
	std::string buttonLabels = "text";
	// OC-005 Stage 2: the two committed Orbit observations (global to the installation). Stored as the
	// persisted tokens (source "fx"|"raw", metric "avg"|"dom"|"vib"); the HUD maps them to its own enums.
	// Defaults reproduce the Stage 1 console: A = FX/AVG, B = RAW/AVG.
	struct OrbitSelection {
		std::string source;
		std::string metric = "avg";
		bool operator==(const OrbitSelection& o) const { return source == o.source && metric == o.metric; }
	};
	OrbitSelection orbitA{"fx", "avg"};
	OrbitSelection orbitB{"raw", "avg"};
};
inline bool validOrbitSource(const std::string& v) { return v == "fx" || v == "raw"; }
inline bool validOrbitMetric(const std::string& v) { return v == "avg" || v == "dom" || v == "vib"; }
// "<source>.<metric>" (the Request payload), e.g. "raw.vib".
inline bool parseOrbitSelection(const std::string& text, Settings::OrbitSelection& out) {
	const size_t dot = text.find('.');
	if (dot == std::string::npos) return false;
	const std::string src = text.substr(0, dot), met = text.substr(dot + 1);
	if (!validOrbitSource(src) || !validOrbitMetric(met)) return false;
	out.source = src;
	out.metric = met;
	return true;
}
inline bool validButtonLabels(const std::string& v) { return v == "text" || v == "icon" || v == "both"; }

constexpr int kMaxMinimalBackgrounds = 16;
inline bool validSystemModeId(const std::string& id) {
	if (id.empty() || id.size() > 64) return false;
	for (char c : id) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_')) return false;
	return true;
}
inline bool validBackgroundIndex(int i) { return i >= 0 && i < kMaxMinimalBackgrounds; }

constexpr float kMinSceneCycleSeconds = 10.0f;
constexpr float kMaxSceneCycleSeconds = 3600.0f;
constexpr float kMinMediaCycleSeconds = 5.0f;
constexpr float kMaxMediaCycleSeconds = 3600.0f;
// A stalled frame contributes at most this much schedule time, so a long
// hitch never fires automation early or in bursts.
constexpr float kMaxScheduleStepSeconds = 1.0f;
constexpr float kEnergyStep = 0.05f;          // one encoder detent
constexpr float kEnergySmoothingTau = 0.5f;   // seconds, exponential approach

inline bool validSceneDuration(float s) { return std::isfinite(s) && s >= kMinSceneCycleSeconds && s <= kMaxSceneCycleSeconds; }
inline bool validMediaDuration(float s) { return std::isfinite(s) && s >= kMinMediaCycleSeconds && s <= kMaxMediaCycleSeconds; }
inline bool validEnergy(float e) { return std::isfinite(e) && e >= 0.0f && e <= 1.0f; }

struct Request {
	enum class Kind {
		SetSceneAutoCycle, // flag (+ optional duration)
		SetMediaAutoCycle, // flag (+ optional duration)
		SetLaunchFullscreen,
		SetClock24Hour,    // flag
		SetSystemMode,     // text = mode id
		SetMinimalBackground, // flag = dark group (else light), steps = preset index
		SetButtonLabels,   // text = "text" | "icon" | "both"
		SetOrbitObservation, // flag = observation B (else A), text = "<source>.<metric>" (OC-005 Stage 2)
		ToggleFullscreen,
		SetEnergyTarget,   // value
		StepEnergy,        // steps (encoder detents, signed)
		BeginEnergyAdjust,
		EndEnergyAdjust,
	};
	Kind kind = Kind::ToggleFullscreen;
	bool flag = false;
	float value = 0.0f;
	int steps = 0;
	std::string text;
	std::optional<float> durationSeconds;
};

struct Result {
	bool accepted = false;
	std::string code; // "ok", "invalid_duration", "invalid_energy", "not_adjusting"
};

class Controller {
public:
	// Load-time validation: every invalid field keeps its default; returns
	// the names of rejected fields (for logging).
	std::vector<std::string> load(const Settings& candidate) {
		std::vector<std::string> rejected;
		Settings s;
		s.sceneAutoCycle.enabled = candidate.sceneAutoCycle.enabled;
		if (validSceneDuration(candidate.sceneAutoCycle.durationSeconds)) s.sceneAutoCycle.durationSeconds = candidate.sceneAutoCycle.durationSeconds;
		else rejected.push_back("sceneAutoCycle.durationSeconds");
		s.mediaAutoCycle.enabled = candidate.mediaAutoCycle.enabled;
		if (validMediaDuration(candidate.mediaAutoCycle.durationSeconds)) s.mediaAutoCycle.durationSeconds = candidate.mediaAutoCycle.durationSeconds;
		else rejected.push_back("mediaAutoCycle.durationSeconds");
		s.launchFullscreen = candidate.launchFullscreen;
		s.clock24Hour = candidate.clock24Hour;
		if (validSystemModeId(candidate.systemMode)) s.systemMode = candidate.systemMode;
		else rejected.push_back("systemMode");
		if (validButtonLabels(candidate.buttonLabels)) s.buttonLabels = candidate.buttonLabels;
		else rejected.push_back("buttonLabels");
		if (validBackgroundIndex(candidate.minimalBackgroundLight)) s.minimalBackgroundLight = candidate.minimalBackgroundLight;
		else rejected.push_back("minimalBackground.light");
		if (validBackgroundIndex(candidate.minimalBackgroundDark)) s.minimalBackgroundDark = candidate.minimalBackgroundDark;
		else rejected.push_back("minimalBackground.dark");
		// Each invalid orbit field falls back to its documented default on its own.
		if (validOrbitSource(candidate.orbitA.source)) s.orbitA.source = candidate.orbitA.source;
		else rejected.push_back("orbitObservations.a.source");
		if (validOrbitMetric(candidate.orbitA.metric)) s.orbitA.metric = candidate.orbitA.metric;
		else rejected.push_back("orbitObservations.a.metric");
		if (validOrbitSource(candidate.orbitB.source)) s.orbitB.source = candidate.orbitB.source;
		else rejected.push_back("orbitObservations.b.source");
		if (validOrbitMetric(candidate.orbitB.metric)) s.orbitB.metric = candidate.orbitB.metric;
		else rejected.push_back("orbitObservations.b.metric");
		if (validEnergy(candidate.energyTarget)) s.energyTarget = candidate.energyTarget;
		else rejected.push_back("energy.target");
		settings_ = s;
		energySmoothed_ = settings_.energyTarget;
		sceneElapsed_ = 0.0f;
		return rejected;
	}

	// Runtime-thread request entry (keyboard, HUD, drained REST queue).
	Result submit(const Request& r) {
		using K = Request::Kind;
		switch (r.kind) {
			case K::SetSceneAutoCycle:
			case K::SetMediaAutoCycle: {
				const bool scene = r.kind == K::SetSceneAutoCycle;
				if (r.durationSeconds && !(scene ? validSceneDuration(*r.durationSeconds) : validMediaDuration(*r.durationSeconds))) {
					return {false, "invalid_duration"};
				}
				AutoCycle& c = scene ? settings_.sceneAutoCycle : settings_.mediaAutoCycle;
				c.enabled = r.flag;
				if (r.durationSeconds) c.durationSeconds = *r.durationSeconds;
				if (scene) sceneElapsed_ = 0.0f; // a new policy starts a fresh period
				settingsChanged_ = true;
				return {true, "ok"};
			}
			case K::SetLaunchFullscreen:
				settings_.launchFullscreen = r.flag;
				settingsChanged_ = true;
				return {true, "ok"};
			case K::SetClock24Hour:
				settings_.clock24Hour = r.flag;
				settingsChanged_ = true;
				return {true, "ok"};
			case K::SetSystemMode:
				if (!validSystemModeId(r.text)) return {false, "invalid_mode"};
				settings_.systemMode = r.text;
				settingsChanged_ = true;
				return {true, "ok"};
			case K::SetButtonLabels:
				if (!validButtonLabels(r.text)) return {false, "invalid_button_labels"};
				settings_.buttonLabels = r.text;
				settingsChanged_ = true;
				return {true, "ok"};
			case K::SetOrbitObservation: {
				Settings::OrbitSelection sel;
				if (!parseOrbitSelection(r.text, sel)) return {false, "invalid_orbit_selection"};
				Settings::OrbitSelection& cur = r.flag ? settings_.orbitB : settings_.orbitA; // the other observation is untouched
				if (!(cur == sel)) { // no change -> no write
					cur = sel;
					settingsChanged_ = true;
				}
				return {true, "ok"};
			}
			case K::SetMinimalBackground:
				if (!validBackgroundIndex(r.steps)) return {false, "invalid_background"};
				(r.flag ? settings_.minimalBackgroundDark : settings_.minimalBackgroundLight) = r.steps;
				settingsChanged_ = true;
				return {true, "ok"};
			case K::ToggleFullscreen:
				fullscreenToggleRequested_ = true;
				return {true, "ok"};
			case K::SetEnergyTarget:
				if (!validEnergy(r.value)) return {false, "invalid_energy"};
				settings_.energyTarget = r.value;
				settingsChanged_ = true;
				return {true, "ok"};
			case K::StepEnergy:
				if (!energyAdjusting_) return {false, "not_adjusting"};
				settings_.energyTarget = std::clamp(settings_.energyTarget + kEnergyStep * static_cast<float>(r.steps), 0.0f, 1.0f);
				settingsChanged_ = true;
				return {true, "ok"};
			case K::BeginEnergyAdjust:
				energyAdjusting_ = true;
				return {true, "ok"};
			case K::EndEnergyAdjust:
				if (!energyAdjusting_) return {false, "not_adjusting"};
				energyAdjusting_ = false;
				return {true, "ok"};
		}
		return {false, "unknown"};
	}

	// Thread-safe: queue a request for the runtime thread.
	void enqueue(const Request& r) {
		std::lock_guard<std::mutex> lock(queueMutex_);
		queue_.push_back(r);
	}

	// Runtime thread, once per frame: apply queued requests in order.
	std::vector<Result> drainQueue() {
		std::vector<Request> pending;
		{
			std::lock_guard<std::mutex> lock(queueMutex_);
			pending.swap(queue_);
		}
		std::vector<Result> results;
		results.reserve(pending.size());
		for (const auto& r : pending) results.push_back(submit(r));
		return results;
	}

	// Scene auto-cycle. Counts only while the scene owner is idle (no
	// transition/loading), caps a stalled frame's contribution, and fires at
	// most once — the period restarts at zero, never carrying a remainder.
	bool tickSceneCycle(float dt, bool sceneIdle) {
		if (automationSuspended_ || !settings_.sceneAutoCycle.enabled || !sceneIdle) return false;
		sceneElapsed_ += std::clamp(dt, 0.0f, kMaxScheduleStepSeconds);
		if (sceneElapsed_ < settings_.sceneAutoCycle.durationSeconds) return false;
		sceneElapsed_ = 0.0f;
		return true;
	}

	// Experience Settings page: while open, automatic scene cycling neither
	// counts nor fires; saved preferences are untouched. Resuming starts a
	// fresh period, so there is never a catch-up burst. (Shared Video's media
	// timer is gated by ExperienceRuntime::applyMediaCadence with the same flag.)
	void setAutomationSuspended(bool suspended) {
		if (automationSuspended_ && !suspended) sceneElapsed_ = 0.0f;
		automationSuspended_ = suspended;
	}
	bool automationSuspended() const { return automationSuspended_; }

	// Any scene change (manual navigation or the cycle's own) restarts the
	// period, so automation never cuts a freshly chosen scene short.
	void noteSceneChanged() { sceneElapsed_ = 0.0f; }

	// Frame-rate-independent exponential smoothing toward the target.
	float tickEnergy(float dt) {
		const float a = 1.0f - std::exp(-std::max(0.0f, dt) / kEnergySmoothingTau);
		energySmoothed_ += (settings_.energyTarget - energySmoothed_) * a;
		return energySmoothed_;
	}

	bool takeFullscreenToggle() {
		const bool t = fullscreenToggleRequested_;
		fullscreenToggleRequested_ = false;
		return t;
	}
	bool takeSettingsChanged() {
		const bool c = settingsChanged_;
		settingsChanged_ = false;
		return c;
	}

	const Settings& settings() const { return settings_; }
	float energySmoothed() const { return energySmoothed_; }
	bool energyAdjusting() const { return energyAdjusting_; }
	float sceneCycleElapsed() const { return sceneElapsed_; }

private:
	Settings settings_;
	float sceneElapsed_ = 0.0f;
	float energySmoothed_ = 0.5f;
	bool energyAdjusting_ = false;
	bool fullscreenToggleRequested_ = false;
	bool settingsChanged_ = false;
	bool automationSuspended_ = false;
	std::mutex queueMutex_;
	std::vector<Request> queue_;
};

} // namespace installation
