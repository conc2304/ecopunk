#pragma once

// ============================================================================
// SceneVisualActivitySnapshot.h - OC-004 runtime-to-HUD scene visual-activity snapshot.
//
// Contract baseline: docs/reviews/oc-004-scene-visual-activity-probe.md section 6, confirmed by
// Architecture for implementation (OC-004 Contract Confirmation and Resume Authorization). An additive,
// optional, read-only, immutable snapshot carried by HudFrameData::sceneVisualActivity as
// std::shared_ptr<const SceneVisualActivitySnapshot>. A null pointer means "no snapshot" (sampling disabled
// or not begun) and is NEVER a snapshot of zeros. Built ONLY by ExperienceRuntime; the HUD never samples.
// Source id: scene.visual.activity. This is NOT scene.activity.overall (scene-semantic, unrelated to pixels).
//
// Measurement definition (ExperienceRuntime-owned, documented here for consumers):
//   source      the runtime-owned SceneFBO after the scene pass, before HUD composition
//   grid        64x36 box mean of encoded Rec.709 luma (texels composited over opaque black)
//   raw         mean absolute per-cell luma difference between two successive valid grids, 0..1,
//               UNSMOOTHED and UN-NORMALISED; never differenced across a scene generation or a
//               transition static run
//   level       fixed log mapping of raw, UNSMOOTHED: clamp(log10(raw / 3e-4) / log10(0.30 / 3e-4), 0, 1)
//               (floor 3e-4, ceiling 0.30; provisional, versioned with this contract)
//   smoothing, segment count: HUD-owned (region #6). The runtime ships level + state only.
//
// State semantics (consumers must check state before reading raw/level):
//   Unavailable  no valid observation in this generation (also: unsupported GPU path). raw/level are
//                meaningless and are NOT zero activity.
//   Valid        newest observation is younger than stalePeriods * nominalPeriodSeconds. raw == 0 / level == 0
//                is a genuine "no visual change".
//   Stale        an observation exists but newer ones stopped arriving. raw/level/rawT describe that last
//                observation only; it is not current.
//   Transition   a transition static frame is in progress (SceneFBO holds a retained frame). raw/level are
//                meaningless; this is NOT zero activity.
//
// One snapshot never mixes generations. OF-free: standard library only.
// ============================================================================

#include <cstdint>
#include <string>

namespace scenevisual {

enum class State : uint8_t { Unavailable, Valid, Stale, Transition };

constexpr double kLogFloor = 3e-4;   // raw that maps to level 0 (provisional)
constexpr double kLogCeiling = 0.30; // raw that maps to level 1 (provisional)

struct SceneVisualActivitySnapshot {
	uint64_t revision = 0;          // monotonic; bumps on a new observation, a state change or a new generation
	uint64_t generation = 0;        // scene generation (same counter as SceneColorSnapshot::generation)
	std::string sceneId;            // scene identity of this generation ("" before the first activation)
	State state = State::Unavailable;
	float raw = 0.0f;               // last valid mean |delta luma|, 0..1 (meaningful for Valid/Stale only)
	double rawT = 0.0;              // monotonic seconds of the observation `raw` came from
	float level = 0.0f;             // fixed-log level of `raw`, 0..1, unsmoothed (meaningful for Valid/Stale only)
	double nominalPeriodSeconds = 0.1; // sampling period in effect (documented desktop diagnostic cadence)
	int stalePeriods = 3;           // Stale after this many missed periods
};

} // namespace scenevisual
