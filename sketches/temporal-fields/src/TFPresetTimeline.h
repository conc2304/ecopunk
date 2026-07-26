#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "TFTimelineBinding.h"
#include "TFTimelineEasing.h"

// Runtime engine for a preset's optional multi-state "timeline" — a
// sequence of named states (arrival/breathing/activity/rest, ...) that the
// live dials smoothly transition between and hold at, so one preset can
// behave like a small environmental composition instead of a fixed
// snapshot. Owned by TFParameterPanel, exactly the way it already owns the
// Phase 7 evolution/waypoint system (see registerEvolving()/pickNewWaypoint()
// in TFParameterPanel.cpp) — this class is that same idiom (typed get/set
// closures over live ofParameters, driven every frame) generalized into a
// validated, authored state machine instead of a random-hover one.
//
// Deliberately independent of ofParameter/ofJson's oF-specific helpers
// (ofSerialize/ofDeserialize/ofLoadJson) — it only touches plain
// nlohmann::json and the TFTimelineBinding get/set abstraction, so it can be
// unit-tested (see src/test/tf_timeline_tests.cpp) without linking any part
// of openFrameworks. TFParameterPanel is the thin adapter that builds real
// bindings from live ofParameters and wires this engine's warnings into
// ofLogWarning.
//
// First state's transition starts from the base preset's own values: the
// caller (TFParameterPanel::loadNextPreset()) always deserializes a
// preset's base ofParameter values *before* calling load()+start() here, so
// by the time start() snapshots "current runtime values" for the first
// transition, those values already equal the base preset's values — no
// separate bootstrapping path is needed for that behavior.
class TFPresetTimeline {
	public:
		enum class Phase { Inactive, Transitioning, Holding };

		struct State {
			std::string name;
			float holdDuration = 0.0f;
			float transitionDuration = 0.0f;
			tftimeline::EasingType easing = tftimeline::EasingType::Linear;
			std::map<std::string, std::string> overrides; // sparse, path -> raw string value
			std::map<std::string, double> resolvedTargetNumeric; // base+overrides, resolved & clamped once at load()
		};

		// Recursively flattens a JSON object into "prefix.key" (and deeper,
		// "prefix.key.leafKey" for nested objects like Background's Effects
		// sub-object) -> string-value pairs, matching the exact JSON-key
		// convention every other part of the preset format already uses
		// (spaces -> underscores happens upstream, at ofParameter::
		// getEscapedName() time — this function just walks whatever keys are
		// already there). Shared by TFParameterPanel (to build a preset's
		// flat base-value map) and internally by load() (to flatten each
		// state's sparse Overrides object) so the flattening logic exists in
		// exactly one place.
		static void flattenJsonToPaths(const nlohmann::json& node, const std::string& prefix,
			std::map<std::string, std::string>& out);

		// Parses timelineJson's States[] against `bindings` (every
		// animatable parameter path this preset's pattern + shared groups
		// expose) and `baseValues` (this specific preset's own flat values,
		// already assembled by the caller from the same JSON's non-timeline
		// sections). Invalid pieces (a bad state, an unknown path, an
		// out-of-range value, an unsupported state-level pattern override,
		// an unrecognized easing name, ...) are warned about and skipped
		// individually — this only returns false if no usable state
		// survives, in which case the caller should treat the preset as a
		// plain static snapshot. Safe to call repeatedly; each call clears
		// prior state first.
		bool load(const nlohmann::json& timelineJson, const std::map<std::string, std::string>& baseValues,
			const std::map<std::string, TFTimelineBinding>& bindings);

		// Drops all parsed state/binding/base-value data and returns to
		// Phase::Inactive. Called unconditionally by TFParameterPanel before
		// loading any new preset (static or timelined) so no stale timeline
		// ever survives a preset switch.
		void clear();

		// Begins running from Start_State (or the first state if
		// Start_State is empty/unmatched), snapshotting current runtime
		// values as the first transition's "from" side.
		void start();
		void restart(); // == start() again; state list/loop flag are unchanged, only playback position resets
		void pause();
		void resume();
		void update(float dt);

		bool isActive() const { return active; }
		bool isPaused() const { return paused; }
		bool isLooping() const { return loop; }
		std::string getCurrentStateName() const;
		Phase getPhase() const { return phase; }
		float getPhaseProgress() const; // 0..1 within the current transition or hold
		float getTotalElapsed() const { return totalElapsed; }

		// Smoothly transitions into the named state (reusing that state's
		// own configured Transition_Duration/Easing) rather than snapping —
		// returns false and warns if no state has that name.
		bool jumpToState(const std::string& name);
		void advanceToNextState();

		// Multi-line status block, e.g.:
		//   State: breathing (2/4)
		//   Phase: transition
		//   Progress: 63%
		//   Next: activity
		//   Loop: on
		//   Paused: no
		// Excludes a title line (TFParameterPanel prepends the preset's own
		// display name, which this class has no notion of).
		std::string getDebugStatusLine() const;

		// Wired once by the owner (TFParameterPanel) to forward into
		// ofLogWarning/ofLogNotice — left as plain callbacks (rather than
		// calling ofLog directly) so this engine stays free of any oF
		// dependency; safe to leave unset (calls are always null-checked).
		std::function<void(const std::string&)> onWarning;
		std::function<void(const std::string&)> onNotice;

	private:
		void warn(const std::string& msg) const { if (onWarning) onWarning(msg); }

		void beginTransitionTo(std::size_t stateIndex);
		void applyInterpolatedFrame(float easedProgress);

		std::vector<State> states;
		std::map<std::string, TFTimelineBinding> bindings;
		std::map<std::string, std::string> baseValues;
		std::map<std::string, double> fromSnapshot; // captured at the start of the current transition

		bool loop = false;
		std::string startStateName;

		bool active = false;
		bool paused = false;
		bool finished = false; // true once a non-looping timeline has settled on its final state
		std::size_t currentIndex = 0;
		Phase phase = Phase::Inactive;
		float phaseElapsed = 0.0f;
		float totalElapsed = 0.0f;
};
