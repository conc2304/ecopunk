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
// UPDATED for System Telemetry v1: adds additive `systemVitals` (see below).
//
// UPDATED for Orbit Console v0.1: adds additive `sceneColor` (see below).
//
// UPDATED for OC-004 (Architecture-confirmed contract baseline): adds additive, optional `sceneVisualActivity`.
//
// UPDATED for HUD Clock v0.1 (Architecture-approved): adds one additive
// `std::optional<WallClockStatus> wallClock` field — see the member's own
// comment and WallClockStatus.h.
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
#include "WallClockStatus.h"
#include "InstallationStatus.h"
#include "SystemVitalsSnapshot.h"
#include "SceneColorSnapshot.h"
#include "SceneVisualActivitySnapshot.h"

#include <memory>
#include <optional>
#include <vector>
#include <string>

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

	// HUD Clock v0.1 (approved, docs/reviews/hud-clock-v0.1-contract-change-
	// proposal.md): the runtime-owned OS wall-clock snapshot, sampled by
	// ExperienceRuntime (WallClockSampler.h), never by a scene. Absent
	// (nullopt) when no valid OS sample/conversion exists this frame —
	// never a fabricated or fixed-offset time.
	std::optional<WallClockStatus> wallClock;

	// Installation Controls & ENERGY v0.1: runtime-owned installation settings
	// and observed state (auto-cycle cadence, fullscreen, ENERGY). Absent only
	// before ExperienceRuntime has loaded its settings.
	std::optional<InstallationStatus> installation;

	// System Telemetry v1 (Architecture-ratified Tier C): runtime-owned,
	// immutable, bounded system vitals — see SystemVitalsSnapshot.h. A shared
	// pointer so the per-frame HudFrameData copy is a refcount bump, never a
	// history copy. Null = no snapshot yet (never a snapshot of zeros).
	// Sources: runtime.vitals.*. Additive field.
	std::shared_ptr<const sysvitals::SystemVitalsSnapshot> systemVitals;

	// Orbit Console v0.1 (Architecture Tier C ratified): runtime-owned, immutable, bounded
	// scene-colour measurement of the SceneFBO (linear-light average RGB + Rec.709 relative
	// luminance, 60 s history, scene generation) - see SceneColorSnapshot.h. A shared pointer
	// so the per-frame HudFrameData copy is a refcount bump. Null = no snapshot yet (never a
	// snapshot of zeros). Source: runtime.scene_color. Additive field.
	std::shared_ptr<const scenecolor::SceneColorSnapshot> sceneColor;

	// OC-003B (Architecture: opt-in publication authorized): the RAW VIDEO colour series - the canonical
	// live decoded-video reference from VideoPlaybackService::currentTexture(), NOT necessarily the frame
	// a specialized scene shows, and never reconstructed from `sceneColor`. Same immutable type and
	// measurement definition as `sceneColor`; its own revision/generation/state:
	//   generation = the RAW generation (advances on a media activation, a scene activation, and on
	//                losing/regaining eligibility - it is NOT the scene generation),
	//   sceneId    = the mediaId measured in this generation ("" before the first activation),
	//   state/count/history/capturedAtSeconds = this series' own (independent of FX cadence and resets).
	// Null = RAW sampling disabled (EXPERIENCE_RUNTIME_VIDEO_COLOR != 1) or nothing begun: the HUD shows
	// NO RAW VIDEO. Source: runtime.scene_color.raw. Additive field.
	std::shared_ptr<const scenecolor::SceneColorSnapshot> sceneColorRaw;

	// OC-004 (Architecture-confirmed baseline: oc-004-scene-visual-activity-probe.md section 6): runtime-owned,
	// immutable 64x36 frame-difference visual-activity observation of the SceneFBO - see
	// SceneVisualActivitySnapshot.h. NOT scene.activity.overall. Null = sampling disabled (the production
	// default: EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY != 1); the HUD then shows an honest unavailable state,
	// never a fallback to scene-semantic activity. Smoothing and segment mapping are HUD-owned.
	// Source: scene.visual.activity. Additive field.
	std::shared_ptr<const scenevisual::SceneVisualActivitySnapshot> sceneVisualActivity;

	// Recent runtime activity log (the same lines the console prints, without
	// the level tag), oldest first, newest last — runtime-owned
	// (HudLogCapture), never produced by a scene. Empty = nothing logged yet.
	// Source: runtime.log.recent. Additive field; no contract change.
	std::vector<std::string> activityLog;
};
