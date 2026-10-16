#include "QuadrantSemanticMapping.h"

#include <algorithm>

// Field table (source -> slot):
//   state.primaryStateId     | expansion TRAVEL_OUT/HOLD -> expanding; TRAVEL_BACK -> reconfiguring
//                            |   (quadrants contracting back); otherwise the liveliest channel:
//                            |   any ONLINE -> online, else any QUIET PHASE -> quiet, else standby
//   activity.overall/density | ONLINE channels / 4 (§18.5: active quadrants)
//   activity.motion          | MotionExtraction energy / 0.3 — the same 0..0.3 range the sketch
//                            |   maps its extraction sensitivity over — clamped to [0,1]
//   activity.transition      | expansionT while travelling out; absent otherwise
//   activity.variation       | absent (no derivation defined)
//   timing.activeSeconds/generation | production scene's own timer / activation counter
//   timing.stateElapsedSeconds | time since primaryStateId last changed (production scene)
//   timing.stateProgress     | absent (no timer-bounded state is published as one)
//   scene.quadrant.metric.channel_N_state  | Identifier, scene.quadrant.state.{online,quiet,standby}
//   scene.quadrant.metric.channel_N_effect | Identifier, bare active shader name; absent when idle
//   scene.quadrant.metric.crosshair_preset | Identifier, scene.fragment.metric.movement_preset.<key>
//                                          |   (the shared preset vocabulary, as FakeQuadrantScenario)
//   scene.quadrant.metric.expansion_state  | Identifier, bare idle/travel_out/hold/travel_back
//   scene.quadrant.metric.expansion_progress | Ratio, expansionT during TRAVEL_OUT/HOLD; absent otherwise
//                                          |   (travel-back progress is internal to QuadrantManager)
namespace quadrantsemantics {

namespace {

const char* const kPresetKeys[] = {"drift", "scan", "hunt", "nervous", "orbit"};

const char* channelStateId(ChannelPhase p) {
	switch (p) {
		case ChannelPhase::Online: return "scene.quadrant.state.online";
		case ChannelPhase::Quiet: return "scene.quadrant.state.quiet";
		case ChannelPhase::Standby: return "scene.quadrant.state.standby";
	}
	return "scene.quadrant.state.standby";
}

const char* expansionId(Expansion e) {
	switch (e) {
		case Expansion::Idle: return "idle";
		case Expansion::TravelOut: return "travel_out";
		case Expansion::Hold: return "hold";
		case Expansion::TravelBack: return "travel_back";
	}
	return "idle";
}

SceneMetric identifierMetric(const std::string& id, const std::string& valueId) {
	SceneMetric m;
	m.metricId = id;
	m.valueType = HudMetricValueType::Identifier;
	m.dataClass = HudDataClass::Literal;
	m.valueId = valueId;
	return m;
}

bool expanding(Expansion e) { return e == Expansion::TravelOut || e == Expansion::Hold; }

} // namespace

ChannelPhase channelPhaseFromTelemetry(const std::string& phase) {
	if (phase == "ONLINE") return ChannelPhase::Online;
	if (phase == "QUIET PHASE") return ChannelPhase::Quiet;
	return ChannelPhase::Standby;
}

std::string primaryStateId(const Inputs& in) {
	if (expanding(in.expansion)) return "scene.quadrant.state.expanding";
	if (in.expansion == Expansion::TravelBack) return "scene.quadrant.state.reconfiguring";
	const auto& ph = in.channelPhase;
	if (std::find(ph.begin(), ph.end(), ChannelPhase::Online) != ph.end()) return "scene.quadrant.state.online";
	if (std::find(ph.begin(), ph.end(), ChannelPhase::Quiet) != ph.end()) return "scene.quadrant.state.quiet";
	return "scene.quadrant.state.standby";
}

SceneSemanticData compute(const Inputs& in) {
	SceneSemanticData data;
	data.schemaVersion = 1;
	data.state.primaryStateId = primaryStateId(in);

	const int online = static_cast<int>(std::count(in.channelPhase.begin(), in.channelPhase.end(), ChannelPhase::Online));
	const float density = static_cast<float>(online) / 4.0f;
	data.activity.overall = density;
	data.activity.density = density;
	data.activity.motion = std::clamp(in.motionEnergy / 0.3f, 0.0f, 1.0f);
	if (in.expansion == Expansion::TravelOut) {
		data.activity.transition = std::clamp(in.expansionT, 0.0f, 1.0f);
	}

	data.timing.activeSeconds = in.activeSeconds;
	data.timing.stateElapsedSeconds = in.stateElapsedSeconds;
	data.timing.generation = in.generation;

	for (int i = 0; i < 4; ++i) {
		const std::string prefix = "scene.quadrant.metric.channel_" + std::to_string(i);
		data.metrics.push_back(identifierMetric(prefix + "_state", channelStateId(in.channelPhase[static_cast<size_t>(i)])));
		const std::string& effect = in.channelEffect[static_cast<size_t>(i)];
		if (!effect.empty()) data.metrics.push_back(identifierMetric(prefix + "_effect", effect));
	}
	if (in.crosshairPreset >= 0 && in.crosshairPreset < 5) {
		data.metrics.push_back(identifierMetric("scene.quadrant.metric.crosshair_preset",
			std::string("scene.fragment.metric.movement_preset.") + kPresetKeys[in.crosshairPreset]));
	}
	data.metrics.push_back(identifierMetric("scene.quadrant.metric.expansion_state", expansionId(in.expansion)));
	if (expanding(in.expansion)) {
		SceneMetric progress;
		progress.metricId = "scene.quadrant.metric.expansion_progress";
		progress.valueType = HudMetricValueType::Ratio;
		progress.dataClass = HudDataClass::Normalized;
		progress.value = std::clamp(in.expansionT, 0.0f, 1.0f);
		data.metrics.push_back(progress);
	}
	return data;
}

} // namespace quadrantsemantics
