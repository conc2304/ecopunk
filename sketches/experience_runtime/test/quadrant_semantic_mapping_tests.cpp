// quadrantsemantics::compute() — Quadrant Crosshair production scene.
// OF-free, same bare-compiler convention as blueprint_semantic_mapping_tests.cpp.

#include <cmath>
#include <iostream>
#include <set>
#include <string>

#include "../src/QuadrantSemanticMapping.h"

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

const SceneMetric* findMetric(const SceneSemanticData& d, const std::string& id) {
	for (const auto& m : d.metrics) {
		if (m.metricId == id) return &m;
	}
	return nullptr;
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

} // namespace

#define SEM_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

using namespace quadrantsemantics;

int main() {
	// HUD-Semantic-Slot-Model-v1 §18.5 — the only IDs this scene may emit.
	const std::set<std::string> kFrozenStates = {"scene.quadrant.state.online", "scene.quadrant.state.quiet",
		"scene.quadrant.state.standby", "scene.quadrant.state.expanding", "scene.quadrant.state.reconfiguring"};
	std::set<std::string> kFrozenMetrics = {"scene.quadrant.metric.crosshair_preset",
		"scene.quadrant.metric.expansion_state", "scene.quadrant.metric.expansion_progress"};
	for (int i = 0; i < 4; ++i) {
		kFrozenMetrics.insert("scene.quadrant.metric.channel_" + std::to_string(i) + "_state");
		kFrozenMetrics.insert("scene.quadrant.metric.channel_" + std::to_string(i) + "_effect");
	}

	// Telemetry phase text.
	SEM_CHECK(channelPhaseFromTelemetry("ONLINE") == ChannelPhase::Online);
	SEM_CHECK(channelPhaseFromTelemetry("QUIET PHASE") == ChannelPhase::Quiet);
	SEM_CHECK(channelPhaseFromTelemetry("STANDBY") == ChannelPhase::Standby);
	SEM_CHECK(channelPhaseFromTelemetry("") == ChannelPhase::Standby);

	// Primary state precedence.
	{
		Inputs in;
		SEM_CHECK(primaryStateId(in) == "scene.quadrant.state.standby");
		in.channelPhase[2] = ChannelPhase::Quiet;
		SEM_CHECK(primaryStateId(in) == "scene.quadrant.state.quiet");
		in.channelPhase[1] = ChannelPhase::Online;
		SEM_CHECK(primaryStateId(in) == "scene.quadrant.state.online");
		in.expansion = Expansion::TravelOut;
		SEM_CHECK(primaryStateId(in) == "scene.quadrant.state.expanding");
		in.expansion = Expansion::Hold;
		SEM_CHECK(primaryStateId(in) == "scene.quadrant.state.expanding");
		in.expansion = Expansion::TravelBack;
		SEM_CHECK(primaryStateId(in) == "scene.quadrant.state.reconfiguring");
		SEM_CHECK(compute(in).state.primaryStateId == primaryStateId(in));
	}

	// Channels, effects, density, and only frozen IDs.
	{
		Inputs in;
		in.channelPhase = {ChannelPhase::Online, ChannelPhase::Quiet, ChannelPhase::Online, ChannelPhase::Standby};
		in.channelEffect = {"edge_glow", "", "ridgeline", ""};
		in.motionEnergy = 0.15f;
		in.crosshairPreset = 0;
		const SceneSemanticData d = compute(in);
		SEM_CHECK(kFrozenStates.count(d.state.primaryStateId) == 1);
		for (const auto& m : d.metrics) SEM_CHECK(kFrozenMetrics.count(m.metricId) == 1);
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.channel_0_state")->valueId == std::string("scene.quadrant.state.online"));
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.channel_1_state")->valueId == std::string("scene.quadrant.state.quiet"));
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.channel_3_state")->valueId == std::string("scene.quadrant.state.standby"));
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.channel_0_effect")->valueId == std::string("edge_glow"));
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.channel_2_effect")->valueId == std::string("ridgeline"));
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.channel_1_effect") == nullptr); // idle -> absent
		SEM_CHECK(near(*d.activity.overall, 0.5f) && near(*d.activity.density, 0.5f));
		SEM_CHECK(near(*d.activity.motion, 0.5f));
		SEM_CHECK(!d.activity.transition && !d.activity.variation);
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.crosshair_preset")->valueId
			== std::string("scene.fragment.metric.movement_preset.drift"));
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.expansion_state")->valueId == std::string("idle"));
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.expansion_progress") == nullptr);
		SEM_CHECK(!d.timing.stateProgress);
		SEM_CHECK(d.schemaVersion == 1);
	}

	// Expansion progress / transition.
	{
		Inputs in;
		in.expansion = Expansion::TravelOut;
		in.expansionT = 0.4f;
		SceneSemanticData d = compute(in);
		SEM_CHECK(near(*findMetric(d, "scene.quadrant.metric.expansion_progress")->value, 0.4f));
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.expansion_progress")->valueType == HudMetricValueType::Ratio);
		SEM_CHECK(d.activity.transition && near(*d.activity.transition, 0.4f));
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.expansion_state")->valueId == std::string("travel_out"));

		in.expansion = Expansion::Hold;
		in.expansionT = 1.0f;
		d = compute(in);
		SEM_CHECK(near(*findMetric(d, "scene.quadrant.metric.expansion_progress")->value, 1.0f));
		SEM_CHECK(!d.activity.transition);
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.expansion_state")->valueId == std::string("hold"));

		in.expansion = Expansion::TravelBack;
		d = compute(in);
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.expansion_progress") == nullptr);
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.expansion_state")->valueId == std::string("travel_back"));
	}

	// Clamping and out-of-range preset.
	{
		Inputs in;
		in.motionEnergy = 5.0f;
		in.crosshairPreset = 9;
		in.expansion = Expansion::TravelOut;
		in.expansionT = 1.7f;
		const SceneSemanticData d = compute(in);
		SEM_CHECK(near(*d.activity.motion, 1.0f));
		SEM_CHECK(near(*findMetric(d, "scene.quadrant.metric.expansion_progress")->value, 1.0f));
		SEM_CHECK(findMetric(d, "scene.quadrant.metric.crosshair_preset") == nullptr);
		SEM_CHECK(!d.state.secondaryStateId.has_value()); // no preset, no expansion effect -> no detail
	}

	// v0.3 secondary state (HUD state badge detail): the strategy when idle,
	// the expanding quadrant's effect during an expansion, the strategy again
	// when that quadrant has no effect.
	{
		Inputs in;
		in.crosshairPreset = 3; // NERVOUS
		in.channelEffect = {"recolor", "", "dither", ""};
		SceneSemanticData d = compute(in);
		SEM_CHECK(d.state.secondaryStateId == std::string("scene.fragment.metric.movement_preset.nervous"));
		in.expansion = Expansion::Hold;
		in.expansionQuadrant = 2;
		d = compute(in);
		SEM_CHECK(d.state.secondaryStateId == std::string("dither"));
		in.expansion = Expansion::TravelBack;
		in.expansionQuadrant = 1;
		d = compute(in);
		SEM_CHECK(d.state.secondaryStateId == std::string("scene.fragment.metric.movement_preset.nervous"));
	}

	if (g_failures == 0) {
		std::cout << g_total << "/" << g_total << " checks passed\n";
		return 0;
	}
	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return 1;
}
