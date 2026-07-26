#include "TFPresetTimeline.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace {

	template <typename T>
	T clampT(T v, T lo, T hi) {
		return v < lo ? lo : (v > hi ? hi : v);
	}

	bool tryParseDouble(const std::string& s, double& out) {
		if (s.empty()) return false;
		try {
			std::size_t consumed = 0;
			double v = std::stod(s, &consumed);
			out = v;
			return true;
		} catch (...) {
			return false;
		}
	}

	bool tryParseBoolLike(const nlohmann::json& node, bool defaultValue) {
		if (node.is_boolean()) return node.get<bool>();
		if (node.is_number()) return node.get<double>() >= 0.5;
		if (node.is_string()) {
			double v;
			if (tryParseDouble(node.get<std::string>(), v)) return v >= 0.5;
		}
		return defaultValue;
	}

	float jsonToFloat(const nlohmann::json& node, float defaultValue) {
		if (node.is_number()) return static_cast<float>(node.get<double>());
		if (node.is_string()) {
			double v;
			if (tryParseDouble(node.get<std::string>(), v)) return static_cast<float>(v);
		}
		return defaultValue;
	}

}

void TFPresetTimeline::flattenJsonToPaths(const nlohmann::json& node, const std::string& prefix,
	std::map<std::string, std::string>& out) {
	if (node.is_object()) {
		for (auto it = node.begin(); it != node.end(); ++it) {
			std::string path = prefix.empty() ? it.key() : prefix + "." + it.key();
			flattenJsonToPaths(it.value(), path, out);
		}
	} else if (node.is_string()) {
		out[prefix] = node.get<std::string>();
	} else if (node.is_number() || node.is_boolean()) {
		// Tolerate a native JSON number/bool leaf even though every other
		// leaf in this preset format is a string (ofSerialize's own
		// convention) -- store its string form so downstream parsing (which
		// expects strings) still works either way.
		out[prefix] = node.dump();
	}
	// null/array leaves aren't a value this format ever produces -- ignored.
}

bool TFPresetTimeline::load(const nlohmann::json& timelineJson, const std::map<std::string, std::string>& baseValues_,
	const std::map<std::string, TFTimelineBinding>& bindings_) {
	clear();

	if (!timelineJson.is_object()) {
		warn("timeline is not a JSON object -- ignored");
		return false;
	}

	if (!timelineJson.contains("States") || !timelineJson["States"].is_array()) {
		warn("timeline has no States[] array -- ignored");
		return false;
	}

	loop = timelineJson.contains("Loop") ? tryParseBoolLike(timelineJson["Loop"], false) : false;
	startStateName = timelineJson.contains("Start_State") && timelineJson["Start_State"].is_string()
		? timelineJson["Start_State"].get<std::string>()
		: std::string();

	baseValues = baseValues_;
	bindings = bindings_;

	std::set<std::string> seenNames;
	for (const auto& stateJson : timelineJson["States"]) {
		if (!stateJson.is_object() || !stateJson.contains("Name") || !stateJson["Name"].is_string()
			|| stateJson["Name"].get<std::string>().empty()) {
			warn("timeline state missing a Name -- skipped");
			continue;
		}

		State s;
		s.name = stateJson["Name"].get<std::string>();
		if (seenNames.count(s.name)) {
			warn("duplicate timeline state name '" + s.name + "' -- skipped");
			continue;
		}

		float holdDuration = stateJson.contains("Hold_Duration") ? jsonToFloat(stateJson["Hold_Duration"], 0.0f) : 0.0f;
		if (holdDuration < 0.0f) {
			warn("state '" + s.name + "': negative Hold_Duration clamped to 0");
			holdDuration = 0.0f;
		}
		s.holdDuration = holdDuration;

		float transitionDuration
			= stateJson.contains("Transition_Duration") ? jsonToFloat(stateJson["Transition_Duration"], 0.0f) : 0.0f;
		if (transitionDuration < 0.0f) {
			warn("state '" + s.name + "': negative Transition_Duration clamped to 0");
			transitionDuration = 0.0f;
		}
		s.transitionDuration = transitionDuration;

		std::string easingName
			= stateJson.contains("Easing") && stateJson["Easing"].is_string() ? stateJson["Easing"].get<std::string>() : std::string();
		bool unknownEasing = false;
		s.easing = tftimeline::resolveEasingType(easingName, unknownEasing);
		if (unknownEasing) {
			warn("state '" + s.name + "': unknown easing '" + easingName + "' -- falling back to linear");
		}

		if (stateJson.contains("Overrides") && stateJson["Overrides"].is_object()) {
			for (auto it = stateJson["Overrides"].begin(); it != stateJson["Overrides"].end(); ++it) {
				if (it.key() == "pattern") {
					warn("state '" + s.name + "': pattern override is unsupported in this version -- ignored");
					continue;
				}
				flattenJsonToPaths(it.value(), it.key(), s.overrides);
			}
		}

		// Resolve this state's full target snapshot now (base values +
		// this state's own sparse overrides only -- never accumulated from
		// any other state), once, so update() never touches JSON or does
		// string parsing.
		std::set<std::string> unionKeys;
		for (const auto& kv : baseValues) unionKeys.insert(kv.first);
		for (const auto& kv : s.overrides) unionKeys.insert(kv.first);

		for (const auto& path : unionKeys) {
			auto bindIt = bindings.find(path);
			if (bindIt == bindings.end()) {
				warn("state '" + s.name + "': unknown parameter path '" + path + "' -- skipped");
				continue;
			}
			const TFTimelineBinding& binding = bindIt->second;
			if (binding.kind == TFTimelineBinding::Kind::Unsupported) {
				continue; // non-numeric parameter -- not supported for timeline animation in this version
			}

			auto overrideIt = s.overrides.find(path);
			std::string rawValue = overrideIt != s.overrides.end() ? overrideIt->second : baseValues.at(path);

			double numeric = 0.0;
			if (!tryParseDouble(rawValue, numeric)) {
				warn("state '" + s.name + "': non-numeric value for '" + path + "' -- skipped");
				continue;
			}

			if (binding.hasRange) {
				double clamped = clampT(numeric, binding.minValue, binding.maxValue);
				if (clamped != numeric) {
					warn("state '" + s.name + "': value for '" + path + "' out of range -- clamped");
				}
				numeric = clamped;
			}

			s.resolvedTargetNumeric[path] = numeric;
		}

		seenNames.insert(s.name);
		states.push_back(std::move(s));
	}

	if (states.empty()) {
		warn("no usable timeline states after validation -- timeline disabled for this preset");
		clear();
		return false;
	}

	return true;
}

void TFPresetTimeline::clear() {
	states.clear();
	bindings.clear();
	baseValues.clear();
	fromSnapshot.clear();
	loop = false;
	startStateName.clear();
	active = false;
	paused = false;
	finished = false;
	currentIndex = 0;
	phase = Phase::Inactive;
	phaseElapsed = 0.0f;
	totalElapsed = 0.0f;
}

void TFPresetTimeline::start() {
	if (states.empty()) return;

	active = true;
	paused = false;
	finished = false;
	totalElapsed = 0.0f;

	std::size_t startIdx = 0;
	if (!startStateName.empty()) {
		bool found = false;
		for (std::size_t i = 0; i < states.size(); i++) {
			if (states[i].name == startStateName) {
				startIdx = i;
				found = true;
				break;
			}
		}
		if (!found) {
			warn("Start_State '" + startStateName + "' not found -- starting at the first state");
		}
	}

	beginTransitionTo(startIdx);
}

void TFPresetTimeline::restart() { start(); }

void TFPresetTimeline::pause() { paused = true; }

void TFPresetTimeline::resume() { paused = false; }

void TFPresetTimeline::update(float dt) {
	if (!active || paused || states.empty()) return;

	totalElapsed += dt;
	phaseElapsed += dt;

	if (phase == Phase::Transitioning) {
		float dur = states[currentIndex].transitionDuration;
		float t = dur <= 0.0001f ? 1.0f : clampT(phaseElapsed / dur, 0.0f, 1.0f);
		float eased = tftimeline::applyEasing(states[currentIndex].easing, t);
		applyInterpolatedFrame(eased);

		if (t >= 1.0f) {
			phase = Phase::Holding;
			phaseElapsed = 0.0f;
		}
	} else if (phase == Phase::Holding) {
		if (!finished && phaseElapsed >= states[currentIndex].holdDuration) {
			advanceToNextState();
		}
	}
}

void TFPresetTimeline::advanceToNextState() {
	if (states.empty() || finished) return;

	std::size_t next = currentIndex + 1;
	if (next >= states.size()) {
		if (loop) {
			beginTransitionTo(0);
		} else {
			finished = true; // parks on the final state's held values
		}
		return;
	}

	beginTransitionTo(next);
}

bool TFPresetTimeline::jumpToState(const std::string& name) {
	if (!active || states.empty()) return false;

	for (std::size_t i = 0; i < states.size(); i++) {
		if (states[i].name == name) {
			finished = false;
			beginTransitionTo(i);
			return true;
		}
	}

	warn("jumpToState: unknown state '" + name + "'");
	return false;
}

void TFPresetTimeline::beginTransitionTo(std::size_t stateIndex) {
	currentIndex = stateIndex;
	phase = Phase::Transitioning;
	phaseElapsed = 0.0f;
	finished = false;

	// Always snapshot the *actual current runtime value*, not the previous
	// state's resolved target -- this is what keeps motion continuous
	// through a pause/resume, a jumpToState(), or a state that doesn't
	// override a path the previous state did (that path's "from" is
	// wherever it actually sits right now, not some remembered prior
	// target).
	fromSnapshot.clear();
	for (const auto& kv : states[stateIndex].resolvedTargetNumeric) {
		auto bindIt = bindings.find(kv.first);
		if (bindIt != bindings.end() && bindIt->second.get) {
			fromSnapshot[kv.first] = bindIt->second.get();
		}
	}

	if (onNotice) {
		onNotice("timeline -> state '" + states[stateIndex].name + "'");
	}
}

void TFPresetTimeline::applyInterpolatedFrame(float easedProgress) {
	const State& s = states[currentIndex];

	for (const auto& kv : s.resolvedTargetNumeric) {
		auto bindIt = bindings.find(kv.first);
		if (bindIt == bindings.end()) continue;
		const TFTimelineBinding& binding = bindIt->second;

		double toValue = kv.second;
		auto fromIt = fromSnapshot.find(kv.first);
		double fromValue = fromIt != fromSnapshot.end() ? fromIt->second : toValue;

		double value;
		switch (binding.kind) {
			case TFTimelineBinding::Kind::Float:
				value = fromValue + (toValue - fromValue) * static_cast<double>(easedProgress);
				break;
			case TFTimelineBinding::Kind::Int:
				value = std::round(fromValue + (toValue - fromValue) * static_cast<double>(easedProgress));
				break;
			case TFTimelineBinding::Kind::Bool:
			case TFTimelineBinding::Kind::EnumInt:
				value = easedProgress < 0.5f ? fromValue : toValue;
				break;
			default:
				continue;
		}

		if (binding.hasRange) {
			value = clampT(value, binding.minValue, binding.maxValue);
		}

		if (binding.set) binding.set(value);
	}
}

std::string TFPresetTimeline::getCurrentStateName() const {
	if (states.empty() || currentIndex >= states.size()) return "";
	return states[currentIndex].name;
}

float TFPresetTimeline::getPhaseProgress() const {
	if (states.empty() || currentIndex >= states.size()) return 0.0f;
	float dur = phase == Phase::Transitioning ? states[currentIndex].transitionDuration : states[currentIndex].holdDuration;
	if (dur <= 0.0001f) return 1.0f;
	return clampT(phaseElapsed / dur, 0.0f, 1.0f);
}

std::string TFPresetTimeline::getDebugStatusLine() const {
	if (!active || states.empty()) return "";

	std::ostringstream oss;
	oss << "State: " << states[currentIndex].name << " (" << (currentIndex + 1) << "/" << states.size() << ")\n";
	oss << "Phase: " << (phase == Phase::Transitioning ? "transition" : "hold") << "\n";
	oss << "Progress: " << static_cast<int>(std::round(getPhaseProgress() * 100.0f)) << "%\n";

	std::string nextName;
	if (finished) {
		nextName = "(stopped)";
	} else if (currentIndex + 1 >= states.size()) {
		nextName = loop ? states[0].name : "(stopped)";
	} else {
		nextName = states[currentIndex + 1].name;
	}
	oss << "Next: " << nextName << "\n";
	oss << "Loop: " << (loop ? "on" : "off") << "\n";
	oss << "Paused: " << (paused ? "yes" : "no");

	return oss.str();
}
