#pragma once

#include "AllocationCounter.h"
#include "HudCompositorBridge.h"
#include "HudFrameData.h"
#include "InputRouter.h"
#include "RuntimeServices.h"
#include "SceneContract.h"
#include "SceneManager.h"

#include "ofFbo.h"

// ExperienceRuntime — top-level owner, per Scene-HUD-Contract-v1.md §4's
// sibling-system diagram:
//
//     ExperienceRuntime
//     ├── SceneManager
//     ├── HudCompositor        (HudCompositorBridge — Engineering Session 2's
//     │                          thin bridge to the real, shared
//     │                          hudpresent::HudWireframeRenderer; see that
//     │                          class's own header comment)
//     ├── InputRouter
//     └── RuntimeServices
//
// Owns the scene output FBO unambiguously, assembles and owns the current
// HudFrameData (never SceneManager/IEcopunkScene/RuntimeServices — see
// HudFrameData.h), and owns window/frame-rate/fullscreen/exit.
//
// update()+draw() together implement this task's exact required logical
// order (adapted into openFrameworks' idiomatic separate update()/draw()
// callbacks — see the two methods' own comments for exactly where each
// numbered step lands):
//
//   1. Resolve raw input.
//   2. Dispatch accepted RuntimeCommand and SceneCommand values.
//   3. SceneManager updates the active FakeScene.
//   4. SceneManager captures SceneHudStatus exactly once.
//   5. Obtain cached SceneCapabilities.
//   6. Obtain SceneManagerStatus.
//   7. Update and capture RuntimeServices/RuntimeTelemetry.
//   8. Capture VideoPlaybackStatus (RuntimeServices, already updated step 7).
//   9. Capture the authoritative EffectActivityStatus exactly once
//      (Architecture-Closure Session, DEC-015) — a separate pull from
//      step 4, never derived from SceneHudStatus::activeEffects.
//  10. Bind and clear the runtime-owned scene FBO.
//  11. SceneManager calls drawToCurrentTarget().
//  12. SceneRenderGuard restores the approved GL baseline.
//  13. Unbind the scene FBO.
//  14. Construct SceneFrame.
//  15. Assemble one immutable HudFrameData.
//  16. Pass const HudFrameData& to HudCompositorBridge (one production draw).
//  17. Present.
class ExperienceRuntime {
public:
	void setup();
	void update(float dt);
	void draw();
	void exit();

	void keyPressed(int key);

	const HudFrameData& currentHudFrameData() const { return currentHudFrameData_; }
	uint64_t frameNumber() const { return frameNumber_; }

	// Development/test-only access — see SceneManager::devScene().
	SceneManager& sceneManagerForTesting() { return sceneManager_; }

	// Development/test-only access — lets GlRestorationHarness ask the
	// real compositor bridge where it actually drew the media viewport
	// (HudRegionCatalog's "media_viewport" region, in pixel space),
	// instead of the harness hardcoding a rectangle that only matched
	// HudCompositorStub's old, unrelated placement logic.
	const HudCompositorBridge& hudCompositorBridgeForTesting() const { return hudCompositorBridge_; }

	// Architecture-Closure Session: HUD-ONLY allocation delta, isolated
	// from the whole-process AllocationCounter measurement
	// GlRestorationHarness already reports (Development Stream 1 §14) —
	// this reads alloccounter's counters immediately before and after
	// ONLY the hudCompositorBridge_.update()+draw() calls in draw()
	// below, excluding SceneManager/RuntimeServices/video-decode work
	// that happens earlier in the same frame. Reuses the SAME global
	// counter (no second instrumentation subsystem) — isolation comes
	// from WHERE the before/after reads are taken, not a different
	// mechanism. Counting itself must still be enabled externally
	// (alloccounter::setEnabled(true), same as today) for these to be
	// meaningful; both stay 0 when disabled.
	long long lastHudOnlyNewCount() const { return lastHudOnlyNewCount_; }
	long long lastHudOnlyDeleteCount() const { return lastHudOnlyDeleteCount_; }

	// Development/test-only: forces the runtime-owned scene FBO to
	// reallocate at a different size, to prove "FBO reallocation replaces
	// the old SceneFrame before any consumer accesses it" (§10). No real
	// scene-switch path triggers this in this increment (single resident
	// scene, no multi-scene switching) — see the implementation report's
	// "known limitations" for why this is a synthetic, harness-only proof
	// rather than one exercised through real scene-switch behavior.
	void forceSceneFboReallocationForTesting(glm::ivec2 newSize) {
		sceneFbo_.allocate(newSize.x, newSize.y, GL_RGBA);
	}

	// Provisional development frame rate. NOT the Raspberry Pi 3B
	// production target — the six existing runtime scenes set 24/30/unset
	// inconsistently (see the placement/readiness probe §6). This value
	// exists only so the scaffold runs at a sane, centralized rate during
	// development; the real target is Roadmap Phase 9 (Raspberry Pi
	// Build, Deployment, and Baseline Profiling) work, not decided here.
	static constexpr float kProvisionalDevelopmentFrameRateFps = 30.0f;

private:
	void establishGlobalRenderingBaseline();
	void handleRuntimeCommand(RuntimeCommand command);

	ofFbo sceneFbo_;
	uint64_t frameNumber_ = 0;

	// Captured at the top of update(float dt) (step 3-7's own dt) and
	// reused in draw() (step 14) to call HudCompositorBridge::update() —
	// openFrameworks' draw() callback itself takes no dt parameter, and
	// this class deliberately does not call ofGetLastFrameTime() a second
	// time (which could observably differ from the dt this frame's
	// SceneManager/RuntimeServices update already advanced by).
	float lastDt_ = 0.0f;

	SceneManager sceneManager_;
	InputRouter inputRouter_;
	RuntimeServices runtimeServices_;
	HudCompositorBridge hudCompositorBridge_;

	// The one current aggregate — replaced once per completed runtime
	// frame in draw(), valid for the whole compositor draw call, passed
	// only by const&. Contains value snapshots, not references to mutable
	// scene/service state; SceneFrame::texture is the sole intentional
	// non-owning reference (into sceneFbo_, which outlives every frame).
	HudFrameData currentHudFrameData_;

	// Captured during update() (step 6) and reused, unchanged, when
	// assembling HudFrameData during draw() (step 13) — avoids a second,
	// draw-phase call into SceneManager::status() (which itself reads
	// the scene's sceneId()), keeping the "status/manager-state obtained
	// during the update phase" ordering literal, not just observably
	// equivalent.
	SceneManagerStatus lastSceneManagerStatus_;

	long long lastHudOnlyNewCount_ = 0;
	long long lastHudOnlyDeleteCount_ = 0;

	bool didSetup_ = false;
};
