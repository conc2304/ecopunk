#include "TemporalSemanticMapping.h"

#include <algorithm>

namespace temporalsemantics {

namespace {
	float clamp01(float v) {
		return std::max(0.0f, std::min(1.0f, v));
	}
} // namespace

// Field-by-field source/formula/class/missing-behavior (reproduced in this
// session's completion report):
//
//   state.primaryStateId   | TFComposition::getPhase()                  | RUNNING -> "scene.temporal.state.running",
//                           |                                            | PATTERN_TRANSITION -> "scene.temporal.state.transitioning"
//                           | Literal    | always present
//   state.secondaryStateId | (none)                                     | always absent — "evolving"/"regenerating" require
//                           |                                            | TFPresetTimeline/evolution state, not driven in
//                           |                                            | production this session (see TemporalSceneCore's
//                           |                                            | own header comment, "Scope decision")
//   timing.activeSeconds   | TemporalProductionScene's own timer         | Literal    | always present (0.0 at fresh activation)
//   timing.stateElapsedSeconds | TFComposition::getPhaseElapsed()        | Literal    | always present
//   timing.stateProgress   | (none — no exposed cycle/transition duration getter) | always absent
//   timing.generation      | TemporalProductionScene's own activation counter | Literal | always present
//
//   scene.temporal.metric.pattern         | TFComposition::getActivePatternType(), lowered | Identifier | Literal | always present
//   scene.temporal.metric.temporal_depth  | (none — no settled formula; see migration prompt §10.5) | always absent
//   scene.temporal.metric.history_fill    | TimeOffsetVideoBuffer::getHistoryFrameCount()/getHistoryCapacityFrames() | Ratio | Normalized |
//                                          | absent iff capacity <= 0, otherwise clamp01(count/capacity)
//   scene.temporal.metric.field_activity  | (none — no honest cheap signal; see migration prompt §10.6) | always absent
//   scene.temporal.metric.evolution_state | (none — no explicit source driven in production; see §10.7) | always absent
//   scene.temporal.metric.playhead_count  | TimeOffsetVideoBuffer::getNumPlayheads(), the real current/configured count | Count | Literal | always present
SceneSemanticData compute(const Inputs& in) {
	SceneSemanticData out;
	out.schemaVersion = 1;

	out.state.primaryStateId = in.isTransitioning ? "scene.temporal.state.transitioning" : "scene.temporal.state.running";
	// secondaryStateId left absent — see table above.

	// activity: every field left absent — no honest, non-fabricated cheap
	// signal exists for overall/motion/density/variation/transition
	// distinct from state.primaryStateId itself (migration prompt §10.6:
	// "do not normalize arbitrary particle/node/fragment counts just to
	// produce activity").

	out.timing.activeSeconds = in.activeSeconds;
	out.timing.stateElapsedSeconds = in.phaseElapsedSeconds;
	// stateProgress left absent — see table above.
	out.timing.generation = in.generation;

	SceneMetric pattern;
	pattern.metricId = "scene.temporal.metric.pattern";
	pattern.valueType = HudMetricValueType::Identifier;
	pattern.dataClass = HudDataClass::Literal;
	pattern.valueId = in.activePatternId;
	out.metrics.push_back(pattern);

	// temporal_depth: absent — see table above.

	if (in.historyCapacityFrames > 0) {
		SceneMetric historyFill;
		historyFill.metricId = "scene.temporal.metric.history_fill";
		historyFill.valueType = HudMetricValueType::Ratio;
		historyFill.dataClass = HudDataClass::Normalized;
		historyFill.value = clamp01(static_cast<float>(in.historyFrameCount) / static_cast<float>(in.historyCapacityFrames));
		historyFill.normalizedValue = historyFill.value;
		out.metrics.push_back(historyFill);
	}

	// field_activity: absent — see table above.
	// evolution_state: absent — see table above.

	SceneMetric playheadCount;
	playheadCount.metricId = "scene.temporal.metric.playhead_count";
	playheadCount.valueType = HudMetricValueType::Count;
	playheadCount.dataClass = HudDataClass::Literal;
	playheadCount.value = static_cast<float>(in.playheadCount);
	out.metrics.push_back(playheadCount);

	return out;
}

} // namespace temporalsemantics
