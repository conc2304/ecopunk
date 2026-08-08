#pragma once

// ============================================================================
// SceneSemanticTypes.h — approved v1 semantic payload shape.
//
// Per the Development Stream 1 prompt's "approved direction" (superseding
// HUD-Semantic-Slot-Model-v1.md's SceneSemanticSnapshot/semanticSnapshot()
// proposal where they differ): semantic data is carried as one OPTIONAL
// field embedded inside the single, existing, atomic SceneHudStatus pull —
// there is no second status API and no separate per-frame semantic pull.
// See SceneContract.h's SceneHudStatus::semantic field.
//
// Deliberately excludes (per the approved direction and the prompt's
// explicit "do not add" list): scene identity, display title, health,
// message, active-effect lists, texture/frame information, widgets,
// vocabulary, presentation profiles, history buffers. All of those already
// live elsewhere (SceneHudStatus's existing compatibility fields, or the
// HUD/vocabulary/widget layers, none of which this file may reference).
//
// Any change to a type in this file is a shared-contract change under
// docs/shared-project-docs/01-architecture-governance.md's "changing the
// approved semantic payload shape" review trigger.
// ============================================================================

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class HudDataClass : uint8_t {
	Literal,
	Normalized,
	Derived,
	Ambient
};

struct SceneActivity {
	std::optional<float> overall;
	std::optional<float> motion;
	std::optional<float> density;
	std::optional<float> variation;
	std::optional<float> transition;
};

struct SceneSemanticState {
	std::string primaryStateId;
	std::optional<std::string> secondaryStateId;
};

struct SceneTimingStatus {
	float activeSeconds = 0.0f;
	std::optional<float> stateElapsedSeconds;
	std::optional<float> stateProgress; // the ONLY progress field — no
	                                     // duplicate progress concept
	                                     // elsewhere in this payload.
	uint64_t generation = 0;
};

enum class HudMetricValueType : uint8_t {
	Scalar,
	Count,
	Ratio,
	DurationSeconds,
	Identifier
};

struct SceneMetric {
	std::string metricId;
	HudMetricValueType valueType = HudMetricValueType::Scalar;
	HudDataClass dataClass = HudDataClass::Literal;

	std::optional<float> value;
	std::optional<float> normalizedValue;
	std::optional<std::string> valueId;
	std::optional<std::string> unitId;
};

struct SceneSemanticData {
	uint32_t schemaVersion = 1;

	SceneSemanticState state;
	SceneActivity activity;
	SceneTimingStatus timing;
	std::vector<SceneMetric> metrics;
};
