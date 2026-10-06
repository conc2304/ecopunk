#pragma once

#include "AllocationCounter.h"
#include "BlobProductionScene.h"
#include "HudCompositorBridge.h"
#include "HudFrameData.h"
#include "InputRouter.h"
#include "RuntimeServices.h"
#include "SceneContract.h"
#include "SceneManager.h"
#include "TemporalProductionScene.h"

#include "ofFbo.h"
#include "ofImage.h"

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
//   3. SceneManager advances any scene transition (RT-003) and, on live
//      frames only, updates the active scene.
//   4. SceneManager captures SceneHudStatus exactly once (live frames only).
//   5. Obtain cached SceneCapabilities.
//   6. Obtain SceneManagerStatus.
//   7. Update and capture RuntimeServices/RuntimeTelemetry.
//   8. Capture VideoPlaybackStatus (RuntimeServices, already updated step 7).
//   9. Capture the authoritative EffectActivityStatus exactly once
//      (Architecture-Closure Session, DEC-015) — a separate pull from
//      step 4, never derived from SceneHudStatus::activeEffects.
//  10. Bind and clear the runtime-owned scene FBO (live frames only —
//      transition-only frames retain the outgoing scene's last frame).
//  11. SceneManager calls drawToCurrentTarget().
//  12. SceneRenderGuard restores the approved GL baseline.
//  13. Unbind the scene FBO.
//  14. Construct SceneFrame.
//  15. Assemble one immutable HudFrameData.
//  16. Pass const HudFrameData& to HudCompositorBridge (one production draw).
//  17. Present.
class ExperienceRuntime {
public:
	// Blob First Complete Production Migration: blobScene_ below binds a
	// reference to runtimeServices_.video() at construction time (the one
	// canonical VideoPlaybackService instance — valid immediately, since
	// RuntimeServices default-constructs its VideoPlaybackService member
	// regardless of whether RuntimeServices::setup() has run yet), so an
	// explicit constructor is required. See member declaration order below
	// (runtimeServices_ before blobScene_/temporalScene_ — C++ initializes
	// members in declaration order, not initializer-list order).
	ExperienceRuntime();

	// RT-003: registers the real production scene pair — BlobProductionScene
	// then TemporalProductionScene (fixed, deterministic NextScene ring
	// order) — in SceneManager's registry, both simultaneously, and selects
	// the startup scene. Caller-controlled (not called automatically inside
	// setup() below) so GlRestorationHarness's existing FakeScene-only proofs
	// keep running unmodified: ofApp only calls one of these when that
	// harness is NOT requested, and it MUST be called before setup().
	//
	// Blob has no approved canonical effect producer: it is registered with
	// no effect source, so its frames publish HudFrameData.effects =
	// std::nullopt directly (no FakeScene involvement). Temporal's source is
	// its real TFEffectPicker-backed currentEffectActivitySnapshot().
	//
	// installBlobProductionScene() starts in Blob (the default normal run and
	// RT-002's deterministic startup); installTemporalProductionScene()
	// registers the same pair but starts in Temporal (the
	// EXPERIENCE_RUNTIME_TEMPORAL_SCENE run and TemporalLifecycleHarness).
	void installBlobProductionScene() { installProductionScenePair(BlobProductionScene::kSceneId); }
	void installTemporalProductionScene() { installProductionScenePair(TemporalProductionScene::kSceneId); }

	// Temporal Production Scene #2 Migration: development/test-only access
	// to the real TemporalProductionScene instance — same rationale as
	// blobSceneForTesting() below.
	TemporalProductionScene& temporalSceneForTesting() { return temporalScene_; }

	// Blob Post-Acceptance Hardening: development/test-only access to the
	// real BlobProductionScene instance — same class as
	// sceneManagerForTesting() above. SceneManager's own IEcopunkScene-level
	// public surface (hudStatus()/capabilities()/etc.) has no way to expose
	// Blob-local instrumentation (scratch-FBO dimensions, configured caps)
	// without widening the frozen IEcopunkScene contract itself, so harness
	// code that needs those reads blobScene_ directly through this getter
	// instead. Production code never calls this.
	BlobProductionScene& blobSceneForTesting() { return blobScene_; }

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
	// the old SceneFrame before any consumer accesses it" (§10). RT-003's
	// real switch path reallocates on its own (see
	// reallocateSceneFboForActiveSceneIfNeeded()); SceneSwitchHarness also
	// uses this hook to give the real switch path a genuine size difference
	// to correct, since Blob and Temporal share the same native size.
	void forceSceneFboReallocationForTesting(glm::ivec2 newSize) {
		sceneFbo_.allocate(newSize.x, newSize.y, GL_RGBA);
		presentationCounters_.sceneFboAllocations++;
	}

	// Blob First Production Acceptance narrow patch: saves the
	// runtime-owned scene FBO's current contents to a PNG — i.e. the
	// active scene's rendered output BEFORE HUD compositing, for visual-
	// parity evidence (comparing Blob's runtime-hosted output directly
	// against the standalone sketch, without HUD framing in the way).
	// Test/tooling-only, same class as sceneManagerForTesting()/
	// forceSceneFboReallocationForTesting() above — not a SceneContract.h
	// addition, not part of IEcopunkScene, and not used by production
	// draw/update code. Returns false (no-op) if the FBO isn't allocated
	// yet. Caller is responsible for the enclosing directory existing
	// (matches GlRestorationHarness's own captures/ convention).
	bool saveSceneFrameCaptureForTesting(const std::string& path) const {
		if (!sceneFbo_.isAllocated()) {
			return false;
		}
		ofPixels pixels;
		sceneFbo_.getTexture().readToPixels(pixels);
		ofImage img;
		img.setFromPixels(pixels);
		return img.save(path);
	}

	// RT-003 evidence counters — test/report instrumentation only, never read
	// by production logic.
	struct PresentationCounters {
		uint64_t hudFrameDataAssemblies = 0;
		uint64_t hudDraws = 0;
		uint64_t liveSceneFrames = 0;   // the active scene drew into sceneFbo_ this frame
		uint64_t staticSceneFrames = 0; // transition-only: sceneFbo_ retained the last outgoing frame
		uint64_t sceneFboAllocations = 0; // every allocate(), including setup()'s
		uint64_t sceneFboSwitchReallocations = 0; // incoming native size differed on activation
	};
	const PresentationCounters& presentationCountersForTesting() const { return presentationCounters_; }

	// Development/test-only: the runtime-owned scene FBO's GL texture id, so a
	// harness can prove a reallocation replaced the texture SceneFrame points at.
	unsigned int sceneFboTextureIdForTesting() const {
		return sceneFbo_.isAllocated() ? sceneFbo_.getTexture().getTextureData().textureID : 0;
	}

	// Development/test-only: reads back the runtime-owned scene FBO, so a
	// harness can prove the outgoing frame is retained (unchanged) across
	// transition-only frames.
	bool readSceneFboPixelsForTesting(ofPixels& out) const {
		if (!sceneFbo_.isAllocated()) return false;
		sceneFbo_.getTexture().readToPixels(out);
		return true;
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
	void installProductionScenePair(const std::string& startupSceneId);

	// RT-003: on the frame an incoming scene's activation succeeds, reallocate
	// the runtime-owned scene FBO iff its native size differs. Clears the
	// published SceneFrame texture reference first so no consumer can read a
	// texture that is being replaced. Never called on any other frame — a
	// per-frame auto-resize would undo GlRestorationHarness's deliberate
	// forced-size proof.
	void reallocateSceneFboForActiveSceneIfNeeded();

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

	// The first real production scene — see class header comment and
	// setup() for installation into sceneManager_. Constructed here
	// (member declaration order, not the constructor body) bound to
	// runtimeServices_.video(); must stay declared after runtimeServices_.
	BlobProductionScene blobScene_;

	// Temporal Production Scene #2 Migration: the second real production
	// scene, same construction-order rule as blobScene_ above (must stay
	// declared after runtimeServices_). RT-003: registered alongside
	// blobScene_ (see installProductionScenePair()); set up lazily, at most
	// once, the first time it becomes active.
	TemporalProductionScene temporalScene_;

	bool productionScenesInstalled_ = false;

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

	PresentationCounters presentationCounters_;

	bool didSetup_ = false;
};
