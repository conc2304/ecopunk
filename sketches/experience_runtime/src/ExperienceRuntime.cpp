#include "ExperienceRuntime.h"

#include "SceneRenderGuard.h"

#include "ofAppRunner.h"
#include "ofFileUtils.h"
#include "ofGraphics.h"
#include "ofLog.h"

ExperienceRuntime::ExperienceRuntime()
	: blobScene_(runtimeServices_.video())
	, temporalScene_(runtimeServices_.video()) {
	// See ExperienceRuntime.h's constructor comment and blobScene_'s own
	// declaration comment: runtimeServices_.video() is a valid, stable
	// reference from the moment runtimeServices_ itself is constructed
	// (declaration order, above blobScene_/temporalScene_ in the header),
	// independent of whether RuntimeServices::setup() has run yet — that
	// happens later, in setup() below, before either scene is activated.
}

void ExperienceRuntime::establishGlobalRenderingBaseline() {
	// Moved out of scene setup() code per the placement/readiness probe's
	// discovery report §6 — every one of these was previously set
	// independently, inconsistently, or (for ofDisableArbTex) redundantly
	// by individual scenes.
	ofSetVerticalSync(true);
	ofSetFrameRate(kProvisionalDevelopmentFrameRateFps);

	// blob-region-prototype's own setup() comment explains why this is
	// required globally: every shared ShaderLibrary/video-effects shader
	// assumes normalized [0,1] sampler2D texcoords; without this call, OF
	// defaults to GL_TEXTURE_RECTANGLE textures and shaded output samples
	// as solid black.
	ofDisableArbTex();
}

void ExperienceRuntime::setup() {
	establishGlobalRenderingBaseline();

	// VideoPlaybackService setup, per Implement-Shared-Video-Playback-
	// System-Agent-Prompt.md §2/§9's approved ownership (RuntimeServices
	// owns VideoPlaybackService) — moved ahead of SceneManager setup/
	// activation (Blob First Complete Production Migration): blobScene_
	// consumes runtimeServices_.video() directly (the ONE canonical
	// instance — see BlobProductionScene's constructor comment for why
	// this must never be a second/local instance), so the video service
	// must be configured before the scene that reads it is set up and
	// activated below.
	VideoPlaybackService::Config videoConfig;
	videoConfig.mediaRoot = ofToDataPath("../../../../assets/shared/media", true);
	videoConfig.holdDurationSeconds = 30.0f;
	videoConfig.automaticAdvance = true;
	runtimeServices_.setup(videoConfig);

	// Blob First Complete Production Migration: whether blobScene_ (vs.
	// fakeScene_) is the scene that gets set up/activated below is decided
	// by the CALLER, before this method runs — see
	// installBlobProductionScene()'s own comment for why (GlRestorationHarness
	// must keep exercising fakeScene_ unmodified). Nothing here forces one
	// or the other.

	// SceneServices construction.
	//
	// Engineering Session 2 §3.1 — exact meaning, documented here per that
	// task's instruction, near the public contract and this runtime setup
	// path:
	//
	//   SceneServices::canvasSize        = canonical runtime composition canvas
	//   IEcopunkScene::nativeRenderSize() = active scene native render size
	//   ExperienceRuntime scene FBO size  = active scene native render size
	//
	// canvasSize is NOT a synonym for the scene FBO size — the FBO always
	// follows nativeRenderSize() (see below, "glm::ivec2 nativeSize =
	// sceneManager_.activeSceneNativeRenderSize()"), deliberately
	// independent of whatever canvasSize reports. For the current product
	// the canonical composition canvas is 1280x720 (FakeScene reports a
	// deliberately different 320x180 nativeRenderSize() to prove this
	// independence holds in code, not just in comments).
	SceneServices services;
	services.canvasSize = glm::ivec2(1280, 720);

	// Canonical-shaped paths per Scene-HUD-Contract-v1.md §12's asset
	// tree. sceneAssetRoot/sharedEffectAssetRoot still do not exist in
	// this repo and neither FakeScene nor BlobProductionScene reads from
	// them in this increment (Blob consumes runtimeServices_.video()
	// directly instead — see BlobProductionScene::setup()'s own comment).
	// sharedMediaRoot resolves to a REAL, populated directory
	// (Shared Video Playback Engineering Session 2, Task C established
	// assets/shared/media/ as the canonical physical media root) — the
	// value's *meaning* is unchanged from the draft placeholder this
	// replaces (still "the shared media root"), only whether it actually
	// resolves to real content changed. videoConfig.mediaRoot above is
	// populated independently, not derived from this field, since
	// VideoPlaybackService takes an already-OF-resolved path via its own
	// Config, not a raw SceneServices field.
	//
	// Blob First Complete Production Migration / Temporal Production Scene
	// #2 Migration: still an inert, unread value regardless of which scene
	// ends up installed (neither FakeScene nor BlobProductionScene nor
	// TemporalProductionScene reads it) — derived from whichever real
	// scene installTemporalProductionScene()/installBlobProductionScene()
	// most recently selected (see installedTemporalScene_'s own comment);
	// the harness path (fakeScene_ actually resident) is unaffected either
	// way.
	services.sceneAssetRoot =
		"assets/scenes/" + (installedTemporalScene_ ? temporalScene_.sceneId() : blobScene_.sceneId()) + "/";
	services.sharedMediaRoot = ofToDataPath("../../../../assets/shared/media", true);
	services.sharedEffectAssetRoot = "assets/shared/video-effects/";

	// PerfInstrumentation does not exist anywhere in this repo yet — left
	// null rather than invented here.
	services.perf = nullptr;

	sceneManager_.setup(services);
	sceneManager_.activateScene(); // caches SceneCapabilities exactly once here

	glm::ivec2 nativeSize = sceneManager_.activeSceneNativeRenderSize();
	sceneFbo_.allocate(nativeSize.x, nativeSize.y, GL_RGBA);

	// Engineering Session 2: real HudCompositor bridge setup — establishes
	// the canonical 1280x720 canvas (see HudCompositorBridge's own header
	// comment for why the window is already sized to match this exactly).
	hudCompositorBridge_.setup();

	didSetup_ = true;
}

void ExperienceRuntime::update(float dt) {
	lastDt_ = dt; // reused by draw() (step 14) — see this member's own comment

	// Step 1 (resolve raw input) and step 2 (dispatch accepted
	// RuntimeCommand/SceneCommand values) already happened synchronously
	// in keyPressed(), which openFrameworks calls for any key event
	// before this frame's update() — adapting the task's single combined
	// per-frame pseudocode into OF's idiomatic separate event/update/draw
	// callbacks, same as the original ExperienceRuntime skeleton did.

	sceneManager_.updateActiveScene(dt); // step 3

	sceneManager_.captureSceneStatus();  // step 4 — hudStatus() pulled
	                                      // exactly once for this frame;
	                                      // nothing else may call it again
	                                      // until the next updateActiveScene().

	// step 5: SceneManager::activeCapabilities() is a cached getter, not
	// a poll — deliberately not called again here; it's read directly
	// during HudFrameData assembly in draw().

	lastSceneManagerStatus_ = sceneManager_.status(); // step 6

	runtimeServices_.update(dt); // step 7 — also drives step 8's VideoPlaybackStatus

	sceneManager_.captureEffectActivityStatus(); // step 9 — a separate pull from
	                                              // step 4; never derived from
	                                              // SceneHudStatus::activeEffects.
}

void ExperienceRuntime::draw() {
	// Step 8: bind + clear the scene FBO, sized to the active scene's
	// nativeRenderSize() (allocated once in setup() for this increment).
	sceneFbo_.begin();
	ofClear(0, 0, 0, 0);

	// Steps 9-10: scene draws to whatever's currently bound (the
	// just-bound SceneFbo), wrapped by SceneRenderGuard so the approved
	// GL baseline (Scene-HUD-Contract-v1.md §5) is restored afterward
	// regardless of what the scene left behind. Drawing must not trigger
	// another status pull — drawActiveScene() only calls
	// drawToCurrentTarget(), never hudStatus().
	{
		SceneRenderGuard guard(sceneFbo_);
		sceneManager_.drawActiveScene();
	}

	// Step 11: unbind SceneFbo.
	sceneFbo_.end();
	frameNumber_++; // monotonically increasing, only after a completed draw

	// Step 12: construct this frame's read-only SceneFrame.
	SceneFrame frame;
	frame.texture = &sceneFbo_.getTexture();
	frame.nativeSize = glm::ivec2(sceneFbo_.getWidth(), sceneFbo_.getHeight());
	frame.frameNumber = frameNumber_;

	// Step 13: assemble HudFrameData — ExperienceRuntime alone does this;
	// it is never assembled inside IEcopunkScene, SceneManager, or
	// RuntimeServices. All fields are value snapshots except
	// sceneFrame.texture (the one intentional non-owning render
	// reference, into sceneFbo_, which outlives this frame).
	currentHudFrameData_.schemaVersion = 1;
	currentHudFrameData_.sceneFrame = frame;
	currentHudFrameData_.scene = sceneManager_.currentSceneStatus(); // the frame-4-captured status
	currentHudFrameData_.sceneManager = lastSceneManagerStatus_;      // captured in update(), step 6
	currentHudFrameData_.capabilities = sceneManager_.activeCapabilities(); // cached, not re-polled
	currentHudFrameData_.runtime = runtimeServices_.telemetry();
	// Captured once here, from RuntimeServices' already-updated (step 7)
	// VideoPlaybackService — never polled again during the HUD draw call
	// below, matching this same rule for every other HudFrameData field
	// (see this class's own header comment, step 13).
	currentHudFrameData_.video = runtimeServices_.videoPlaybackStatus();
	// Captured in update() (step 9) — the authoritative Shared Effects
	// snapshot, transported unchanged. std::nullopt here means "no
	// snapshot available," not "zero effects" — see HudFrameData.h.
	currentHudFrameData_.effects = sceneManager_.currentEffectActivityStatus();

	// Step 16: hand the ONE assembled HudFrameData to the compositor
	// bridge, by const& — it never receives the FBO, and never polls
	// SceneManager/scenes/RuntimeServices itself. Engineering Session 2:
	// this is now the real, shared hudpresent::HudWireframeRenderer (via
	// HudCompositorBridge, not HudCompositorStub's plain unclipped
	// rectangle + debug text) — canonical 1280x720 layout, MediaViewportMesh,
	// widgets, drawn at the canvas origin (no destRect: see
	// HudCompositorBridge's header comment for why none is needed).
	ofBackground(12, 12, 12);
	// Architecture-Closure Session: HUD-only allocation isolation — see
	// this class's own header comment (lastHudOnlyNewCount()/
	// lastHudOnlyDeleteCount()). Reads alloccounter's global counters
	// immediately around ONLY these two calls; zero added cost when
	// alloccounter::g_enabled is false (the atomic loads are cheap, and
	// nothing else changes).
	long long hudAllocBefore = alloccounter::newCount();
	long long hudDeallocBefore = alloccounter::deleteCount();
	hudCompositorBridge_.update(lastDt_, currentHudFrameData_);
	hudCompositorBridge_.draw();
	lastHudOnlyNewCount_ = alloccounter::newCount() - hudAllocBefore;
	lastHudOnlyDeleteCount_ = alloccounter::deleteCount() - hudDeallocBefore;

	// Step 15: present — openFrameworks swaps buffers after draw() returns.
}

void ExperienceRuntime::exit() {
	sceneManager_.deactivateScene();
	sceneManager_.shutdown();
	runtimeServices_.shutdown();
}

void ExperienceRuntime::keyPressed(int key) {
	// -- Contract-shaped command routing: two separate paths -----------
	if (auto sceneCommand = inputRouter_.translateToSceneCommand(key)) {
		bool accepted = sceneManager_.dispatchSceneCommand(*sceneCommand);
		ofLogNotice("ExperienceRuntime") << "SceneCommand dispatched, accepted=" << accepted;
		return;
	}
	if (auto runtimeCommand = inputRouter_.translateToRuntimeCommand(key)) {
		handleRuntimeCommand(*runtimeCommand);
		return;
	}

	// Blob First Production Acceptance narrow patch: manual scene-only
	// (pre-HUD-composite) capture, for visual-parity evidence gathering —
	// see saveSceneFrameCaptureForTesting()'s own comment. Deliberately
	// scene-agnostic (works for whichever scene is actually installed),
	// unlike the FakeScene-only dev hooks below.
	if (key == 's' || key == 'S') {
		bool saved = saveSceneFrameCaptureForTesting("captures/scene_only_manual.png");
		ofLogNotice("ExperienceRuntime") << "Scene-only capture "
			<< (saved ? "saved to captures/scene_only_manual.png" : "FAILED (FBO not allocated?)");
		return;
	}

	// -- Development-only test hooks, deliberately NOT part of the
	//    contract command routing above -------------------------------
	FakeScene& scene = sceneManager_.devScene();
	switch (key) {
		case '3':
			// Direct IEcopunkScene::reset() — kept separate from
			// SceneCommand::Reset ('1' above) on purpose.
			scene.reset();
			break;
		case 'c':
		case 'C':
			scene.setContaminateGLStateOnDraw(!scene.contaminateGLStateOnDraw());
			ofLogNotice("ExperienceRuntime") << "FakeScene GL contamination now "
				<< (scene.contaminateGLStateOnDraw() ? "ON" : "off");
			break;
		case '4': {
			// Cycles Ready -> Loading -> Degraded -> Failed -> Ready.
			SceneHealth next;
			switch (scene.testHealth()) {
				case SceneHealth::Ready:    next = SceneHealth::Loading; break;
				case SceneHealth::Loading:  next = SceneHealth::Degraded; break;
				case SceneHealth::Degraded: next = SceneHealth::Failed; break;
				case SceneHealth::Failed:   next = SceneHealth::Ready; break;
				default:                    next = SceneHealth::Ready; break;
			}
			scene.setTestHealth(next);
			break;
		}
		case '5': {
			// Cycles through every SemanticVariant, in declaration order.
			using SV = FakeScene::SemanticVariant;
			SV next;
			switch (scene.semanticVariant()) {
				case SV::Absent:         next = SV::Complete; break;
				case SV::Complete:       next = SV::MissingSignals; break;
				case SV::MissingSignals: next = SV::ZeroSignals; break;
				case SV::ZeroSignals:    next = SV::ZeroMetrics; break;
				case SV::ZeroMetrics:    next = SV::ManyMetrics; break;
				case SV::ManyMetrics:    next = SV::Absent; break;
				default:                 next = SV::Absent; break;
			}
			scene.setSemanticVariant(next);
			break;
		}
		case '6': {
			// Cycles NoSnapshot -> PresentEmpty -> PresentActive -> NoSnapshot.
			using ES = FakeScene::EffectActivityTestState;
			ES next;
			switch (scene.effectActivityTestState()) {
				case ES::NoSnapshot:    next = ES::PresentEmpty; break;
				case ES::PresentEmpty:  next = ES::PresentActive; break;
				case ES::PresentActive: next = ES::NoSnapshot; break;
				default:                next = ES::NoSnapshot; break;
			}
			scene.setEffectActivityTestState(next);
			break;
		}
		default:
			break;
	}
}

void ExperienceRuntime::handleRuntimeCommand(RuntimeCommand command) {
	switch (command) {
		case RuntimeCommand::NextScene:
		case RuntimeCommand::PreviousScene:
			// Scene-switch commands: per the contract's draw-order note,
			// SceneManager "handles any pending RuntimeCommands itself"
			// — read here as specifically the scene-selection subset.
			sceneManager_.handleSceneSwitchCommand(command);
			break;
		case RuntimeCommand::ToggleHud:
			hudCompositorBridge_.setVisible(!hudCompositorBridge_.isVisible());
			break;
		case RuntimeCommand::ToggleFullscreen:
			// ExperienceRuntime's own unambiguous host ownership.
			ofToggleFullscreen();
			break;
		case RuntimeCommand::Exit:
			// Development-only binding (ESC).
			ofExit();
			break;
	}
}
