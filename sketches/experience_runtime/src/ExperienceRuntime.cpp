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

void ExperienceRuntime::installProductionScenePair(const std::string& startupSceneId) {
	if (!productionScenesInstalled_) {
		// Fixed registry order = NextScene ring order: Blob, then Temporal.
		sceneManager_.registerProductionScene(&blobScene_); // no canonical effect producer -> effects = nullopt
		sceneManager_.registerProductionScene(
			&temporalScene_, [this] { return temporalScene_.currentEffectActivitySnapshot(); });
		productionScenesInstalled_ = true;
	}
	sceneManager_.setStartupScene(startupSceneId);
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

	// Whether the production scene pair (vs. the tooling-only FakeScene) is
	// what gets set up/activated below is decided by the CALLER, before this
	// method runs — see installBlobProductionScene()'s own comment for why
	// (GlRestorationHarness must keep exercising FakeScene unmodified).

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
	// tree. sharedMediaRoot resolves to the REAL canonical media root
	// (assets/shared/media/, Shared Video Playback Engineering Session 2,
	// Task C); videoConfig.mediaRoot above is populated independently, since
	// VideoPlaybackService takes an already-OF-resolved path via its own
	// Config. RT-003: sceneAssetRoot is filled in per scene by SceneManager
	// ("assets/scenes/<sceneId>/", derived from each registered scene's own
	// stable ID when that scene is set up) — still an inert, unread value
	// for every current scene.
	services.sceneAssetRoot = "";
	services.sharedMediaRoot = ofToDataPath("../../../../assets/shared/media", true);
	services.sharedEffectAssetRoot = "assets/shared/video-effects/";

	// PerfInstrumentation does not exist anywhere in this repo yet — left
	// null rather than invented here.
	services.perf = nullptr;

	sceneManager_.setup(services);        // sets up the startup scene only
	sceneManager_.activateScene();        // caches SceneCapabilities exactly once here

	glm::ivec2 nativeSize = sceneManager_.activeSceneNativeRenderSize();
	sceneFbo_.allocate(nativeSize.x, nativeSize.y, GL_RGBA);
	presentationCounters_.sceneFboAllocations++;

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

	// Step 3 (RT-003): advance any scene transition by one frame. Performs
	// that frame's lifecycle calls (incoming setup()/activate(), capability
	// caching) and reports whether the active scene runs live this frame.
	bool live = sceneManager_.beginFrame();

	if (live) {
		sceneManager_.updateActiveScene(dt); // step 3

		sceneManager_.captureSceneStatus();  // step 4 — hudStatus() pulled
		                                      // exactly once for this frame;
		                                      // nothing else may call it again
		                                      // until the next updateActiveScene().
	}
	// Transition-only static frames (FadingOut/Loading/Failed) neither update
	// any scene nor pull its status — SceneManager publishes the frozen
	// outgoing snapshot (FadingOut) or a neutral transition status instead.

	// step 5: SceneManager::activeCapabilities() is a cached getter, not
	// a poll — deliberately not called again here; it's read directly
	// during HudFrameData assembly in draw().

	lastSceneManagerStatus_ = sceneManager_.status(); // step 6

	runtimeServices_.update(dt); // step 7 — also drives step 8's VideoPlaybackStatus

	sceneManager_.captureEffectActivityStatus(); // step 9 — a separate pull from
	                                              // step 4; never derived from
	                                              // SceneHudStatus::activeEffects.
}

void ExperienceRuntime::reallocateSceneFboForActiveSceneIfNeeded() {
	glm::ivec2 size = sceneManager_.activeSceneNativeRenderSize();
	if (sceneFbo_.isAllocated() && static_cast<int>(sceneFbo_.getWidth()) == size.x
		&& static_cast<int>(sceneFbo_.getHeight()) == size.y) {
		return; // same native size: keep the existing allocation
	}
	// Drop the published reference before the texture behind it is
	// replaced; the new SceneFrame is built below only after the new FBO is
	// allocated and drawn.
	currentHudFrameData_.sceneFrame = SceneFrame{};
	sceneFbo_.allocate(size.x, size.y, GL_RGBA);
	presentationCounters_.sceneFboAllocations++;
	presentationCounters_.sceneFboSwitchReallocations++;
	ofLogNotice("ExperienceRuntime") << "scene FBO reallocated to " << size.x << "x" << size.y
									 << " for incoming scene \"" << lastSceneManagerStatus_.activeSceneId << "\"";
}

void ExperienceRuntime::draw() {
	// RT-003: the incoming scene's first active frame — make the runtime-owned
	// scene FBO match its native size before it draws.
	if (sceneManager_.activationSucceededThisFrame()) {
		reallocateSceneFboForActiveSceneIfNeeded();
	}

	if (sceneManager_.activeSceneLiveThisFrame()) {
		// Step 8: bind + clear the scene FBO, sized to the active scene's
		// nativeRenderSize().
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
		presentationCounters_.liveSceneFrames++;
	} else {
		// RT-003 static-frame transition: no scene is asked to render. The
		// runtime-owned scene FBO still holds the outgoing scene's last live
		// frame (nothing has drawn into it since), which is exactly the
		// approved static outgoing content — retained in runtime-owned
		// state, never a pointer into a scene-owned texture/FBO, and never
		// a dual-live-scene render.
		presentationCounters_.staticSceneFrames++;
	}
	frameNumber_++; // monotonically increasing, once per presented frame

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
	presentationCounters_.hudFrameDataAssemblies++;

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
	presentationCounters_.hudDraws++;
	lastHudOnlyNewCount_ = alloccounter::newCount() - hudAllocBefore;
	lastHudOnlyDeleteCount_ = alloccounter::deleteCount() - hudDeallocBefore;

	// Step 15: present — openFrameworks swaps buffers after draw() returns.
}

void ExperienceRuntime::exit() {
	sceneManager_.deactivateScene();
	sceneManager_.shutdown(); // every registered scene that was ever set up
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
			// RT-003: accepted only while Idle; the outgoing scene's last
			// rendered frame is already retained in sceneFbo_ (this runs
			// between frames, after that frame's draw), and SceneManager
			// deactivates the outgoing scene as part of acceptance.
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
