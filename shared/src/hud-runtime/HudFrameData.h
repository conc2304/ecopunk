#pragma once

// ============================================================================
// HudFrameData.h — the runtime-to-HUD boundary value.
//
// Per Development Stream 1's approved direction: ExperienceRuntime owns and
// assembles the current HudFrameData once per completed runtime frame;
// HudCompositor (and any stand-in for it) only ever receives this by
// const&. It is composed ENTIRELY of already-approved existing types —
// nothing here is a new invented concept:
//
//   - SceneFrame        (SceneContract.h §4 — the read-only render view)
//   - SceneHudStatus     (SceneContract.h §7 — now optionally carrying
//                         SceneSemanticData, see SceneSemanticTypes.h)
//   - SceneManagerStatus (SceneContract.h §10)
//   - SceneCapabilities  (SceneContract.h §8 — cached on scene activation,
//                         not queried per frame)
//   - RuntimeTelemetry   (SceneContract.h §13)
//
// UPDATED by the Shared Video Playback System increment
// (Implement-Shared-Video-Playback-System-Agent-Prompt.md §11): adds one
// additive `std::optional<VideoPlaybackStatus> video` field, matching the
// shape already reviewed and approved in HUD-Semantic-Slot-Model-v1.md §10
// ("HudFrameData ... std::optional<VideoPlaybackStatus> video;") and
// Shared-Video-Playback-HUD-Semantic-Slot-Review.md's own recommendation.
//
// UPDATED AGAIN by the ExperienceRuntime Architecture-Closure Session,
// per DEC-015 ("Canonical Shared Effect Activity Transport") and DEC-016
// (Shared Effect Knowledge v1 freeze): adds one additive
// `std::optional<videoeffects::EffectActivityStatus> effects` field, the
// real, frozen, shared type from shared/src/video-effects/knowledge/
// EffectActivityStatus.h — not a local copy or redeclaration. std::nullopt
// means "no authoritative Shared Effects snapshot available"; a present
// value with empty `slots` means "snapshot exists, zero active effects" —
// these two states are semantically distinct and must stay so at every
// layer that touches this field. `SceneHudStatus::activeEffects` remains
// unchanged, compatibility-only, and is never used to derive this field
// (see ExperienceRuntime's own assembly code and this session's report).
// Quality-profile status, presentation data, and resolved slot maps remain
// deliberately undefined/unadded here — still-unapproved concepts outside
// this increment's scope.
//
// Lives in shared/src/hud-runtime/ rather than shared/src/hud-compositor/
// (an existing, separate, in-progress HUD Runtime workstream directory —
// see this task's own implementation report, "Newly discovered risks")
// specifically so this neutral runtime-owned boundary type is not confused
// with, or accidentally coupled to, that domain's own internal widget/
// presentation-profile types.
//
// This type must never be placed inside IEcopunkScene, SceneManager, or
// RuntimeServices — ExperienceRuntime alone assembles and owns the current
// value. Any change to this shape is a shared-contract change under
// docs/shared-project-docs/01-architecture-governance.md.
// ============================================================================

#include "SceneContract.h"
#include "VideoPlaybackStatus.h"
#include "EffectActivityStatus.h"

#include <optional>

struct HudFrameData {
	uint32_t schemaVersion = 1;

	SceneFrame sceneFrame;
	SceneHudStatus scene;
	SceneManagerStatus sceneManager;
	SceneCapabilities capabilities;
	RuntimeTelemetry runtime;

	// Absent (nullopt) whenever RuntimeServices' VideoPlaybackService has
	// not completed setup() yet, or has no configured media root at all —
	// distinct from VideoPlaybackHealth::Unavailable, which is a value
	// this optional would actually carry once the service exists but has
	// no playable media. See shared/src/video-playback/README.md.
	std::optional<VideoPlaybackStatus> video;

	// Absent (nullopt) whenever no authoritative Shared Effects snapshot
	// is available this frame — distinct from a present value with empty
	// `slots` ("snapshot exists, zero active effects"). See
	// shared/src/video-effects/knowledge/EffectActivityStatus.h and this
	// file's header comment above (DEC-015/DEC-016).
	std::optional<videoeffects::EffectActivityStatus> effects;
};
