#pragma once

#include <functional>

#include "AllocationCounter.h"
#include "BlobProductionScene.h"
#include "BlueprintProductionScene.h"
#include "FragmentProductionScene.h"
#include "QuadrantProductionScene.h"
#include "CrtPhosphorScene.h"
#include "SharedEffectsGalleryScene.h"
#include "HudCompositorBridge.h"
#include "HudFrameData.h"
#include "ExperienceSettingsPage.h"
#include "InstallationControl.h"
#include "SystemClockService.h"
#include "InputRouter.h"
#include "PresentationTransform.h"
#include "TypographyPlayground.h"
#include <map>
#include "HudSkinSchedule.h"
#include "MinimalTheme.h"
#include "ShellTheme.h"
#include "ClipActivity.h"
#include "HudLogCapture.h"
#include "RuntimeServices.h"
#include "SceneContract.h"
#include "SceneManager.h"
#include "SceneColorSampler.h"
#include "CpuRawAcquisition.h" // OC-003-F diagnostic (header-only, off by default)
#include "PaletteSink.h"
#include "SceneActivityProbe.h" // OC-004 diagnostic, opt-in
#include "TemporalProductionScene.h"
#include "WallClockSampler.h"

#include "ofFbo.h"
#include <filesystem>
#include <memory>
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
	// registers the same roster but starts in Temporal (the
	// EXPERIENCE_RUNTIME_TEMPORAL_SCENE run and TemporalLifecycleHarness);
	// installBlueprintProductionScene() starts in Blueprint.
	//
	// Roster (NextScene ring order), fixed registry order Blob, Temporal,
	// Blueprint, Fragment, Quadrant, CrtPhosphor:
	//   Full         — [Blob, Temporal, Blueprint, Fragment, Quadrant]: the
	//                  normal run and the full-ring circuit proof.
	//   ThreeScene   — [Blob, Temporal, Blueprint] (BLUE-001): the ring the
	//                  Blueprint roster/HUD, parity-soak and RT-005 harnesses
	//                  were accepted against (each asserts that registry).
	//   BlobTemporal — [Blob, Temporal]: the original two-scene ring RT-002's
	//                  TwoSceneAcceptanceHarness and RT-003's
	//                  SceneSwitchHarness were accepted against (both assert
	//                  exactly that registry and step '['/']' assuming it).
	//   Production   — [Blob, Temporal, Blueprint, Fragment, Quadrant, CrtPhosphor, SharedEffectsGallery]:
	//                  the NORMAL (non-harness) run only. Every accepted harness keeps
	//                  the ring it was accepted against (Full stays five scenes).
	enum class ProductionRoster { BlobTemporal, ThreeScene, Full, Production };

	void installBlobProductionScene(ProductionRoster roster = ProductionRoster::Full) {
		installProductionScenes(BlobProductionScene::kSceneId, roster);
	}
	void installTemporalProductionScene(ProductionRoster roster = ProductionRoster::Full) {
		installProductionScenes(TemporalProductionScene::kSceneId, roster);
	}
	void installBlueprintProductionScene(ProductionRoster roster = ProductionRoster::Full) {
		installProductionScenes(BlueprintProductionScene::kSceneId, roster);
	}
	void installFragmentProductionScene() {
		installProductionScenes(FragmentProductionScene::kSceneId, ProductionRoster::Full);
	}
	FragmentProductionScene& fragmentSceneForTesting() { return fragmentScene_; }
	void installQuadrantProductionScene() {
		installProductionScenes(QuadrantProductionScene::kSceneId, ProductionRoster::Full);
	}
	QuadrantProductionScene& quadrantSceneForTesting() { return quadrantScene_; }
	// CRT Phosphor, sixth scene of the normal-run (Production) roster, appended
	// after Quadrant. Starting here is opt-in (EXPERIENCE_RUNTIME_CRT_SCENE).
	void installCrtPhosphorProductionScene() {
		installProductionScenes(CrtPhosphorScene::kSceneId, ProductionRoster::Production);
	}
	CrtPhosphorScene& crtPhosphorSceneForTesting() { return crtPhosphorScene_; }
	// Shared Effects Gallery, seventh scene of the normal-run (Production) roster, appended after CRT
	// Phosphor. Starting here is opt-in (EXPERIENCE_RUNTIME_FX_SCENE).
	void installSharedEffectsGalleryProductionScene() {
		installProductionScenes(SharedEffectsGalleryScene::kSceneId, ProductionRoster::Production);
	}
	SharedEffectsGalleryScene& sharedEffectsGallerySceneForTesting() { return sharedEffectsGalleryScene_; }

	BlueprintProductionScene& blueprintSceneForTesting() { return blueprintScene_; }
	// Read-only: the one canonical VideoPlaybackService (resume-window verification).
	const VideoPlaybackService& videoServiceForTesting() const { return runtimeServices_.video(); }

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
	// Any pointer/device press (ofApp mouse events) — exits IMMERSIVE.
	// (x, y) are framebuffer pixels (OF's mouse space); RT-007 maps them
	// through the inverse final presentation transform to canvas pixels.
	void pointerPressed(float x = -1.0f, float y = -1.0f);
	bool immersiveForTesting() const { return immersive_; }

	// RT-007: the final presentation transform used for the current frame
	// (canonical 1280x720 composition -> physical framebuffer, contain).
	const PresentationTransform& presentationTransform() const { return presentation_; }
	// Last pointer press mapped to canonical canvas pixels (test/inspection).
	glm::vec2 lastPointerCanvasForTesting() const { return lastPointerCanvas_; }
	TypographyPlayground* typographyPlaygroundForTesting() { return typography_.get(); }
	// RT-007 test-only: the canonical composition target's last frame (false
	// while it has never been allocated, i.e. in a 1280x720 framebuffer).
	bool readPresentationFboPixelsForTesting(ofPixels& out) const {
		if (!presentationFbo_.isAllocated()) return false;
		presentationFbo_.readToPixels(out);
		return true;
	}

	const HudFrameData& currentHudFrameData() const { return currentHudFrameData_; }

	// -- Installation Controls & ENERGY v0.1 -----------------------------
	// The one validated request interface (keyboard/HUD today, a future
	// local REST server tomorrow). submit: runtime thread only, applied
	// immediately (fullscreen/scene work still happens at the next frame's
	// start). enqueue: thread-safe, drained by the runtime thread at the
	// start of update() — no other thread touches scenes, decoders or GL.
	installation::Result submitInstallationRequest(const installation::Request& r) { return installation_.submit(r); }
	void enqueueInstallationRequest(const installation::Request& r) { installation_.enqueue(r); }
	const installation::Controller& installationForTesting() const { return installation_; }
	const settingspage::Page& settingsPageForTesting() const { return settingsPage_; }
	// Settings file. Call before setup(); "" = no load/save (harness runs).
	void setInstallationSettingsPath(std::string path) { installationSettingsPath_ = std::move(path); }
	// Saved typography preset applies to normal runs only; harnesses keep the
	// stock HUD fonts their pixel measurements are baselined on (the dev
	// playground env still forces it on).
	void setTypographyPresetEnabled(bool enabled) { typographyPresetEnabled_ = enabled; }
	// Day/night HUD skin schedule (assets/hud/skins/skin-schedule.json):
	// normal runs only — harnesses keep their explicit skin selection.
	void setSkinScheduleEnabled(bool enabled) { skinScheduleEnabled_ = enabled; }
	const std::string& currentSkinForTesting() const { return currentSkin_; }
	bool skinAutoForTesting() const { return skinAuto_; }
	uint64_t frameNumber() const { return frameNumber_; }

	// Development/test-only access — see SceneManager::devScene().
	SceneManager& sceneManagerForTesting() { return sceneManager_; }

	// Development/test-only access — lets GlRestorationHarness ask the
	// real compositor bridge where it actually drew the media viewport
	// (HudRegionCatalog's "media_viewport" region, in pixel space),
	// instead of the harness hardcoding a rectangle that only matched
	// HudCompositorStub's old, unrelated placement logic.
	const HudCompositorBridge& hudCompositorBridgeForTesting() const { return hudCompositorBridge_; }
	HudCompositorBridge& hudCompositorBridgeMutableForTesting() { return hudCompositorBridge_; } // SkinLoaderHarness

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

	// Orbit Console Scene Sampling v0.1: test/measurement access to the
	// private sampler (same class as the other ForTesting accessors).
	const scenecolorsampling::SceneColorSampler& sceneColorForTesting() const { return sceneColor_; }
	scenecolorsampling::SceneColorSampler& sceneColorForTesting() { return sceneColor_; }
	const std::string& sceneColorSceneIdForTesting() const { return sceneColorSceneId_; }
	const scenecolorsampling::SnapshotPublisher& sceneColorPublisherForTesting() const { return sceneColorPublisher_; }
	// OC-003B RAW VIDEO sampler (EXPERIENCE_RUNTIME_VIDEO_COLOR=1; off by default): diagnostics/tests only.
	bool videoColorEnabledForTesting() const { return videoColorEnabled_; }
	const scenecolorsampling::SceneColorSampler& videoColorForTesting() const { return videoColor_; }
	const rawvideo::Gate& videoColorGateForTesting() const { return videoColorGate_; }
	// OC-003-F diagnostics (all off by default): CPU RAW acquisition, palette sink, per-frame acquisition accounting for the frame stream.
	bool cpuRawEnabledForTesting() const { return cpuRawEnabled_; }
	const cpuraw::Sampler& cpuRawForTesting() const { return cpuRaw_; }
	const rawvideo::Gate& cpuRawGateForTesting() const { return cpuRawGate_; }
	struct FrameAcquisition {   // what THIS frame's draw() did, read by Rt004FramePhaseProbe when the frame closes
		uint32_t cpuRaw = 0;     // CPU RAW samples taken (valid)
		uint32_t cpuRawGap = 0;  // CPU RAW gaps recorded
		double cpuRawUs = 0.0;   // wall microseconds: access + reduce + analyze (excludes the palette sink)
		double palUs = 0.0;      // wall microseconds spent in the palette sink (analysis + file I/O), any stream
		bool videoNew = false;   // the decoder delivered a new frame this frame
	};
	const FrameAcquisition& lastFrameAcquisition() const { return frameAcq_; }
	const scenecolorsampling::SnapshotPublisher& videoColorPublisherForTesting() const { return videoColorPublisher_; }

	// RT-003 evidence counters — test/report instrumentation only, never read
	// by production logic.
	struct PresentationCounters {
		uint64_t hudFrameDataAssemblies = 0;
		uint64_t hudDraws = 0;
		uint64_t liveSceneFrames = 0;   // the active scene drew into sceneFbo_ this frame
		uint64_t staticSceneFrames = 0; // transition-only: sceneFbo_ retained the last outgoing frame
		uint64_t sceneFboAllocations = 0; // every allocate(), including setup()'s
		uint64_t sceneFboSwitchReallocations = 0; // incoming native size differed on activation
		// RT-007: frames presented through the canonical composition FBO
		// (framebuffer != 1280x720) vs drawn directly; and its allocations
		// (expected: at most 1 per process — never on resize).
		uint64_t composedPresentationFrames = 0;
		uint64_t directPresentationFrames = 0;
		uint64_t presentationFboAllocations = 0;
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

	// -- Interactive HUD region: option port ---------------------------------------------------
	// The ONE place a Left/Right value step leaves the HUD. `row` = the selected row of the
	// scene's declared selector group, `delta` = +/-1 (the scene wraps). The constructor installs
	// the production port: SceneManager::dispatchSceneOption (the approved Tier C extension,
	// docs/reviews/crt-selectors-tier-c-proposal.md). The region is available only while the
	// active scene ADVERTISES enough option axes; clearing the port makes it unavailable. Never a
	// concrete-scene cast.
	void setRegionOptionSink(std::function<bool(int row, int delta)> sink) { regionOptionSink_ = std::move(sink); }
	const HudCompositorBridge& hudBridgeForTesting() const { return hudCompositorBridge_; }

private:
	void establishGlobalRenderingBaseline();
	void handleRuntimeCommand(RuntimeCommand command);
	void handleFocusInput(const HudFocusInput& input);

public:
	// Playable HUD Menu v0.1 — last focus activation, for logs/harness only.
	struct FocusActivationRecord {
		std::string region;      // focused control, "" if none focused
		std::string command;     // command dispatched, "" if none
		bool refused = false;    // focused control not enabled -> nothing dispatched
		bool accepted = false;   // result from the existing command path
		uint64_t serial = 0;     // increments on every Activate input
	};
	const FocusActivationRecord& lastFocusActivationForTesting() const { return lastFocusActivation_; }

private:
	FocusActivationRecord lastFocusActivation_;
	void installProductionScenes(const std::string& startupSceneId, ProductionRoster roster);

	// Per-scene Shared Video cadence, applied every frame from SceneManager's
	// live owner: a scene listed here drives media changes itself with
	// next() at its own boundaries, so timer-driven auto-advance is off while
	// it is live; every other state (other scenes, transitions, a failed
	// activation, shutdown) gets normal automatic advance back — with a fresh
	// dwell window (VideoPlaybackService::setAutomaticAdvanceEnabled()).
	void applyMediaCadence(bool liveThisFrame);
	void enterImmersive();
	bool exitImmersiveOnInput();
	void drawImmersiveScene() const;
	void loadInstallationSettings();
	void saveInstallationSettings();
	void applyOrbitSettingsToHud();                     // OC-005 Stage 2: committed selection -> HUD
	void commitOrbitObservation(hudpresent::OrbitObs which); // OC-005 Stage 2: Done -> installation settings
	// -- Experience Settings page v0.1 -----------------------------------
	void openSettingsPage();
	void closeSettingsPage();
	bool handleSettingsKey(int key); // true = consumed (every key is, while open)
	void performSettingsAction(const settingspage::Action& action);
	void refreshSettingsContext(bool rereadOsClock);
	void tickInstallation(float dt);
	InstallationStatus buildInstallationStatus() const;
	std::vector<std::string> boundaryDrivenMediaScenes_;

	// RT-003: on the frame an incoming scene's activation succeeds, reallocate
	// the runtime-owned scene FBO iff its native size differs. Clears the
	// published SceneFrame texture reference first so no consumer can read a
	// texture that is being replaced. Never called on any other frame — a
	// per-frame auto-resize would undo GlRestorationHarness's deliberate
	// forced-size proof.
	void reallocateSceneFboForActiveSceneIfNeeded();

	ofFbo sceneFbo_;
	// Orbit Console Scene Sampling v0.1: runtime-private SceneFBO color
	// sampler (pre-HUD). Not published through HudFrameData (pending the
	// additive contract proposal); read only via the ForTesting accessor.
	scenecolorsampling::SceneColorSampler sceneColor_;
	scenecolorsampling::SnapshotPublisher sceneColorPublisher_; // immutable snapshot -> HudFrameData::sceneColor
	uint64_t sceneColorGeneration_ = 0; // bumps on every scene activation
	std::string sceneColorSceneId_;     // scene identity of the current generation
	// OC-003B: independent RAW VIDEO sampler of VideoPlaybackService::currentTexture(). Disabled unless
	// EXPERIENCE_RUNTIME_VIDEO_COLOR=1. Runtime-private (not published to the HUD in this increment); its
	// generation follows media activations (activationCount) and scene activations, never the FX series.
	void sampleRawVideo();
	scenecolorsampling::SceneColorSampler videoColor_;
	rawvideo::Gate videoColorGate_;
	scenecolorsampling::SnapshotPublisher videoColorPublisher_; // immutable RAW snapshot -> HudFrameData::sceneColorRaw
	std::string videoColorMediaId_;     // mediaId measured in the current RAW generation
	bool videoColorEnabled_ = false;
	// OC-003-F: CPU RAW acquisition prototype (EXPERIENCE_RUNTIME_CPU_RAW=1) and palette sink (EXPERIENCE_RUNTIME_PALETTE_DIAG=<dir>).
	cpuraw::Sampler cpuRaw_;
	rawvideo::Gate cpuRawGate_;       // its OWN gate: eligibility / generations never depend on whether the GPU RAW sampler is on
	bool cpuRawEnabled_ = false;
	bool cpuRawStats_ = false;
	double cpuRawLastLogAt_ = 0.0;
	palette::Sink paletteSink_;
	bool paletteOn_ = false;
	FrameAcquisition frameAcq_;
	void sampleCpuRaw();
	bool videoColorStats_ = false;      // EXPERIENCE_RUNTIME_VIDEO_COLOR_STATS=1
	bool videoColorDiag_ = false;       // EXPERIENCE_RUNTIME_VIDEO_COLOR_DIAG=1 (texture facts + CPU reference per sample)
	double videoColorLastLogAt_ = 0.0;
	uint64_t videoColorLastDiagRev_ = 0;
	bool sceneColorEnabled_ = true;     // EXPERIENCE_RUNTIME_SCENE_COLOR=0 disables
	bool sceneColorLogStats_ = false;   // EXPERIENCE_RUNTIME_SCENE_COLOR_STATS=1
	double sceneColorLastLogAt_ = 0.0;
	// OC-004 visual-activity probe: diagnostic only, inert unless EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE is set.
	sceneactivity::SceneActivityProbe sceneActivityProbe_;
	sceneactivity::VisualActivityPublisher sceneVisualActivityPublisher_; // OC-004: immutable snapshot -> HudFrameData::sceneVisualActivity
	// OC-004 cost-effective path: per-clip activity curves measured offline (assets/shared/clip-activity/), looked up at
	// the shared video's playhead. Publishes scene.visual.activity with no GL work; the live probe above wins when enabled.
	void loadClipActivity(const std::string& path);
	clipactivity::ClipActivityIndex clipActivityIndex_;
	clipactivity::ClipActivityFeeder clipActivity_{clipActivityIndex_};
	bool clipActivityEnabled_ = false; // curves loaded and EXPERIENCE_RUNTIME_CLIP_ACTIVITY != 0
	// RT-007: canonical 1280x720 composition target (scene viewport + shell +
	// HUD), allocated lazily the first time the framebuffer is not exactly
	// 1280x720 and never reallocated on resize; then scaled once to the
	// physical framebuffer. Unused (not even allocated) in a 1280x720 window.
	// Development-only Live Typography Playground: constructed only when
	// EXPERIENCE_RUNTIME_TYPOGRAPHY_PLAYGROUND=1 (never in production).
	std::unique_ptr<TypographyPlayground> typography_;
	ofFbo presentationFbo_;
	PresentationTransform presentation_;
	glm::vec2 lastPointerCanvas_{-1.0f, -1.0f};
	void drawComposition();               // the canonical-canvas HUD composition, into the current target
	void presentComposedFrame();          // presentationFbo_ -> framebuffer (contain, one draw)
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

	// BLUE-001: the third real production scene, same construction-order
	// rule (declared after runtimeServices_); set up lazily on first switch.
	BlueprintProductionScene blueprintScene_;

	// RT-006: Fragment Trail, the fourth real production scene, same
	// construction-order rule (declared after runtimeServices_); set up lazily
	// on first switch.
	FragmentProductionScene fragmentScene_;

	// Quadrant Crosshair: the fifth real production scene, same
	// construction-order rule (declared after runtimeServices_); set up lazily
	// on first switch.
	QuadrantProductionScene quadrantScene_;

	// CRT Phosphor: sixth scene of the Production roster, same construction-order
	// rule (declared after runtimeServices_); set up lazily on first switch.
	CrtPhosphorScene crtPhosphorScene_;
	SharedEffectsGalleryScene sharedEffectsGalleryScene_;

	bool productionScenesInstalled_ = false;

	// The one current aggregate — replaced once per completed runtime
	// frame in draw(), valid for the whole compositor draw call, passed
	// only by const&. Contains value snapshots, not references to mutable
	// scene/service state; SceneFrame::texture is the sole intentional
	// non-owning reference (into sceneFbo_, which outlives every frame).
	HudFrameData currentHudFrameData_;

	// HUD Clock v0.1: sole producer of HudFrameData::wallClock.
	wallclock::WallClockSampler wallClockSampler_;

	// Installation Controls & ENERGY v0.1: sole owner of installation
	// settings, the scene auto-cycle schedule and ENERGY.
	installation::Controller installation_;
	std::string installationSettingsPath_;
	bool typographyPresetEnabled_ = true;
	// Experience Settings page: runtime-owned overlay (not a scene). The page
	// model holds no settings; it reads installation_ and SystemClockService.
	settingspage::Page settingsPage_;
	systemclock::SystemClockService clockService_;
	systemclock::Snapshot clockSnapshot_;
	double clockSnapshotAt_ = 0.0;        // monotonic seconds of the last OS read
	std::string saveStatus_;              // last settings-file outcome shown on the page
	bool saveFailed_ = false;
	std::string clockResult_;
	bool clockResultFailed_ = false;
	bool escapeQuitSuppressed_ = false;   // OF quits on ESC by itself; off while the page is open
	double settingsClosedAt_ = -1.0;      // monotonic; guards the closing Enter against re-activating MENU
	double lastOkInputAt_ = -1.0;         // monotonic; debounces OK auto-repeat
	double orbitEditorClosedAt_ = -1.0;   // monotonic; the closing Enter must not re-open the Orbit editor
	double interactiveClosedAt_ = -1.0;   // monotonic; the closing Enter must not re-enter the interactive region
	std::function<bool(int row, int delta)> regionOptionSink_; // see setRegionOptionSink()
	bool handleInteractiveKey(int key);
	void stepInteractiveOption(int delta);
	// Recent console activity for the HUD log (installed in setup()).
	std::shared_ptr<HudLogCapture> logCapture_;

	// Day/night skins (see setSkinScheduleEnabled()).
	bool skinScheduleEnabled_ = false;
	HudSkinSchedule skinSchedule_;
	bool skinAuto_ = false;              // following the schedule (vs a manual 'v' / env choice)
	std::string currentSkin_;            // active skin package id ("" = wireframe)
	std::optional<HudSkinSchedule::Period> skinPeriod_;
	float skinCheckSeconds_ = 0.0f;
	bool loadSkinSchedule(const std::string& path);
	bool applySkin(const std::string& skinId, const std::string& reason);
	// System Mode (Experience Settings page): "auto" | skin id | "minimal.<light|dark>.<hue>".
	// A minimal mode drops the skin artwork (plain wireframe HUD) and recolours it from one primary.
	bool applySystemMode(const std::string& modeId, const std::string& reason);
	void leaveMinimal();
	void applyButtonLabelMode();
	void loadMinimalThemes(const std::string& path);
	// Per-skin shell palettes (assets/hud/themes/shell-palettes.json), keyed by skin id. A skin
	// without an entry keeps the built-in day/night palette.
	void loadShellPalettes(const std::string& path);
	std::map<std::string, shelltheme::Seeds> shellPalettes_;
	minimaltheme::ThemeSet themes_ = minimaltheme::ThemeSet::defaults();
	bool minimalActive_ = false;
	// Live reload: the theme file is stat()ed about once a second; a newer mtime reloads it and,
	// while a minimal mode is showing, re-applies it so edits appear without a restart.
	std::string themePath_;
	std::filesystem::file_time_type themeMtime_{};
	float themeCheckSeconds_ = 0.0f;
	void tickThemeFile(float dt);
	ofColor canvasBackground_ = ofColor(12, 12, 12);
	void tickSkinSchedule(bool force);
	static int localMinuteNow();
	bool installationLoaded_ = false;
	// IMMERSIVE scene presentation (HUD hidden, scene cover-scaled to the
	// current window). Runtime presentation state only.
	bool immersive_ = false;

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
