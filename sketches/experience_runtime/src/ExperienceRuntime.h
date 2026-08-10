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
	// Blob First Complete Production Migration: blobScene_ below binds a
	// reference to runtimeServices_.video() at construction time (the one
	// canonical VideoPlaybackService instance — valid immediately, since
	// RuntimeServices default-constructs its VideoPlaybackService member
	// regardless of whether RuntimeServices::setup() has run yet), so an
	// explicit constructor is required. See member declaration order below
	// (runtimeServices_ before blobScene_/temporalScene_ — C++ initializes
	// members in declaration order, not initializer-list order).
	ExperienceRuntime();

	// Installs blobScene_ as the scene SceneManager actually drives.
	// Caller-controlled (not called automatically inside setup() below) so
	// GlRestorationHarness's existing FakeScene-only proofs keep running
	// unmodified: ofApp only calls this when the harness is NOT requested
	// (see ofApp.cpp), and it MUST be called before setup() (matches
	// SceneManager::installProductionScene()'s own requirement) — setup()
	// activates and caches capabilities for whichever scene is installed
	// at the time it runs.
	void installBlobProductionScene() {
		installedTemporalScene_ = false;
		sceneManager_.installProductionScene(&blobScene_);
	}

	// Temporal Production Scene #2 Migration: same contract as
	// installBlobProductionScene() above, for temporalScene_. Mutually
	// exclusive with it — SceneManager has exactly one productionScene_
	// slot (see that class's own comment); this session hosts Temporal as
	// a complete, independently launchable production scene, not
	// simultaneously alongside Blob (real Blob<->Temporal switching is the
	// separate, later two-scene acceptance milestone — see this
	// migration's completion report). Also installs temporalScene_'s own
	// real canonical-effect-activity source (currentEffectActivitySnapshot())
	// through SceneManager's generic production seam — see
	// SceneManager::installProductionScene()'s EffectActivitySource
	// parameter comment.
	void installTemporalProductionScene() {
		installedTemporalScene_ = true;
		sceneManager_.installProductionScene(
			&temporalScene_, [this] { return temporalScene_.currentEffectActivitySnapshot(); });
	}

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
	// the old SceneFrame before any consumer accesses it" (§10). No real
	// scene-switch path triggers this in this increment (single resident
	// scene, no multi-scene switching) — see the implementation report's
	// "known limitations" for why this is a synthetic, harness-only proof
	// rather than one exercised through real scene-switch behavior.
	void forceSceneFboReallocationForTesting(glm::ivec2 newSize) {
		sceneFbo_.allocate(newSize.x, newSize.y, GL_RGBA);
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

	// The first real production scene — see class header comment and
	// setup() for installation into sceneManager_. Constructed here
	// (member declaration order, not the constructor body) bound to
	// runtimeServices_.video(); must stay declared after runtimeServices_.
	BlobProductionScene blobScene_;

	// Temporal Production Scene #2 Migration: the second real production
	// scene, same construction-order rule as blobScene_ above (must stay
	// declared after runtimeServices_). Only one of blobScene_/
	// temporalScene_ is ever actually installed into sceneManager_ at a
	// time in this session — see installBlobProductionScene()/
	// installTemporalProductionScene() above.
	TemporalProductionScene temporalScene_;

	// Set by whichever of installBlobProductionScene()/
	// installTemporalProductionScene() was called last — read by setup()
	// below to derive services.sceneAssetRoot from the actually-installed
	// scene's own sceneId() (neither scene currently reads that field —
	// see BlobProductionScene::setup()'s own comment — so this only
	// affects an inert value either way).
	bool installedTemporalScene_ = false;

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
