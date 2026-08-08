#pragma once

// ============================================================================
// Scene/HUD Contract v1 — shared implementation boundary
//
// Transcribed — struct/interface shapes only — from
// docs/shared-project-docs/Scene-HUD-Contract-v1.md, the frozen contract
// between ExperienceRuntime/SceneManager and the HUD Runtime domain. Field
// names, defaults, and method signatures below match that document exactly.
//
// UPDATE (Development Stream 1 — ExperienceRuntime/SceneManager/HUD
// Semantic Slot Model): per that task's approved direction, SceneHudStatus
// gains exactly one additive field, `semantic` (SceneSemanticTypes.h) —
// the approved v1 shape is "optional SceneSemanticData inside
// SceneHudStatus," NOT a second `semanticSnapshot()` method on
// IEcopunkScene (explicitly rejected). No other field on SceneHudStatus,
// and no other type in this file, was added, removed, renamed, or
// reinterpreted for that change — see that field's own comment below.
//
// Do not add fields, methods, alternate enums, string command IDs, helper
// convenience methods, HUD types, or service interfaces here beyond the one
// additive field above. Any change to a public type in this file is a
// shared-contract change and requires Architecture review per
// docs/shared-project-docs/01-architecture-governance.md
// ("Frozen shared decisions": IEcopunkScene, SceneFrame, render/FBO
// ownership, scene lifecycle semantics, command ownership, "changing the
// approved semantic payload shape").
//
// NOTE on a contract-version constant: the roadmap's Phase 0 task list
// ("Add a contract version constant") does not correspond to any named
// symbol in Scene-HUD-Contract-v1.md itself — the only version field the
// contract actually defines is SceneHudStatus::schemaVersion (§7), which is
// per-status, not a whole-contract version. No unified contract-version
// constant name or location is specified anywhere in the source documents.
// Rather than invent a public symbol the contract never named, this is left
// out and reported as an open question (see the implementation report,
// "Contract changes requested" / "Deviations from prompt").
// ============================================================================

#include "ofTexture.h"
#include "glm/vec2.hpp"

#include "SceneSemanticTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Forward declaration only. The concrete PerfInstrumentation API is
// explicitly out of scope for this increment (Scene-HUD-Contract-v1.md §3's
// SceneServices revision note: "PerfInstrumentation* is added per §10's
// 'centralize now' call" — its own interior shape is not specified by the
// contract). SceneServices only needs to hold a pointer to it.
class PerfInstrumentation;

// ---------------------------------------------------------------------------
// §7 — Status Contract
// ---------------------------------------------------------------------------

enum class SceneHealth {
	Ready,
	Loading,
	Degraded,
	Failed
};

struct SceneHudStatus {
	uint32_t schemaVersion = 1;

	std::string sceneId;
	std::string displayName;

	SceneHealth health = SceneHealth::Ready;
	std::optional<std::string> message;

	std::optional<std::string> mediaName;
	std::optional<std::string> modeName;

	std::vector<std::string> activeEffects;
	std::vector<std::string> statusLines;

	std::optional<float> progress;
	std::optional<float> motionEnergy;
	std::optional<int>   activeItemCount;

	bool paused = false;

	// Additive, v1-approved (Development Stream 1): the single, optional
	// semantic payload for this same atomic status pull — there is no
	// second per-frame semantic pull and no semanticSnapshot() method.
	// Every existing field above is unchanged/unremoved/unrenamed; a
	// scene reporting std::nullopt here behaves exactly as scenes did
	// before this field existed.
	std::optional<SceneSemanticData> semantic;
};

// ---------------------------------------------------------------------------
// §9 — Command Contract (split into two enums; InputRouter dispatches each
// to a different owner — see Scene-HUD-Contract-v1.md §9)
// ---------------------------------------------------------------------------

enum class SceneCommand {
	NextMedia,
	PreviousMedia,
	Regenerate,
	Reset
};

enum class RuntimeCommand {
	NextScene,
	PreviousScene,
	ToggleHud,
	ToggleFullscreen,
	Exit
};

// ---------------------------------------------------------------------------
// §8 — Capability Contract
// ---------------------------------------------------------------------------

struct SceneCommandDescriptor {
	SceneCommand command;
	std::string label;
	std::optional<std::string> shortLabel;
	bool prominent = false;
};

struct SceneCapabilities {
	std::vector<SceneCommandDescriptor> commands;
};

// ---------------------------------------------------------------------------
// §3 — SceneServices
// ---------------------------------------------------------------------------

struct SceneServices {
	glm::ivec2 canvasSize;

	std::string sceneAssetRoot;         // e.g. assets/scenes/<sceneId>/
	std::string sharedMediaRoot;
	std::string sharedEffectAssetRoot;

	PerfInstrumentation* perf = nullptr;
};

// ---------------------------------------------------------------------------
// §3 — Proposed Scene Interface
// ---------------------------------------------------------------------------

class IEcopunkScene {
public:
	virtual ~IEcopunkScene() = default;

	virtual void setup(const SceneServices& services) = 0;
	virtual void activate() = 0;
	virtual void deactivate() = 0;

	virtual void update(float dt) = 0;
	virtual void drawToCurrentTarget() = 0;

	virtual glm::ivec2 nativeRenderSize() const = 0;

	virtual std::string sceneId() const = 0;
	virtual std::string displayName() const = 0;

	virtual SceneHudStatus hudStatus() const = 0;
	virtual SceneCapabilities capabilities() const = 0;
	virtual bool executeCommand(SceneCommand command) = 0;

	virtual void reset() = 0;
	virtual void shutdown() = 0;
};

// ---------------------------------------------------------------------------
// §4 — Render Ownership: SceneFrame
//
// ExperienceRuntime is the unambiguous owner of the scene output FBO.
// HudCompositor (and any stand-in for it) never touches that FBO directly —
// it only ever receives this read-only view.
// ---------------------------------------------------------------------------

struct SceneFrame {
	const ofTexture* texture = nullptr;
	glm::ivec2 nativeSize{0, 0};
	uint64_t frameNumber = 0;
};

// ---------------------------------------------------------------------------
// §10 — Transition Model
// ---------------------------------------------------------------------------

enum class SceneTransitionPhase {
	Idle,
	FadingOut,
	Loading,
	FadingIn,
	Failed
};

struct SceneManagerStatus {
	std::string activeSceneId;
	std::optional<std::string> pendingSceneId;
	SceneTransitionPhase transitionPhase = SceneTransitionPhase::Idle;
	float transitionProgress = 0.0f;
	std::optional<std::string> message;
};

// ---------------------------------------------------------------------------
// §13 — Runtime Telemetry (replaces fps in SceneHudStatus; owned by
// RuntimeServices, never by an individual scene)
// ---------------------------------------------------------------------------

struct RuntimeTelemetry {
	float fps = 0.0f;
	float frameTimeMs = 0.0f;

	std::optional<float> cpuTemperatureC;
	std::optional<uint64_t> residentMemoryBytes;
	std::optional<bool> throttled;
};
