#include "ofApp.h"
#include "BlobEffectCapacityBench.h"
#include "BlobRenderPathAbHarness.h"
#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated)

#include <cstdlib>

void ofApp::setup() {
	// RT-004 GL error attribution (diagnostic; no-op unless the probe and
	// EXPERIENCE_RUNTIME_GL_ERROR_TRACE are both set). The context exists here.
	rt004::Probe::get().setGlErrorHook([]() -> unsigned { return static_cast<unsigned>(glGetError()); });
	rt004::Probe::get().glCheckpoint("(before ofApp::setup: window/context creation)");
	if (rt004::Probe::get().glTraceActive()) {
		// Proves the tracer works: must appear once as glError,...,0x500.
		RT004_SCOPE("rt004.glTraceSelfTest(expect 0x500)");
		glEnable(0xFFFF);
	}
	// Blob First Complete Production Migration: the GL harness's existing
	// proofs (GlRestorationHarness) directly manipulate
	// runtime.sceneManagerForTesting().devScene() (FakeScene) and assume
	// it is the scene actually driving the pipeline — so Blob or Temporal
	// is installed as the resident production scene whenever the GL
	// harness is NOT requested (the normal run and every lifecycle harness
	// need one of them installed), and only before runtime.setup() runs
	// (see ExperienceRuntime::installBlobProductionScene()/
	// installTemporalProductionScene()'s own comments for why the ordering
	// matters).
	//
	// Blob First Production Acceptance narrow patch / Blob Post-Acceptance
	// Hardening / Temporal Production Scene #2 Migration: up to six
	// mutually exclusive paths now exist (normal-Blob / normal-Temporal /
	// GL harness / Blob lifecycle harness / Blob soak harness / Temporal
	// lifecycle harness). If more than one env var is somehow set at once,
	// priority is GL harness, then Blob lifecycle, then Blob soak, then
	// Temporal lifecycle (documented here, not silently ambiguous).
	//
	// EXPERIENCE_RUNTIME_TEMPORAL_SCENE selects which production scene a
	// NORMAL run (no harness at all) installs — unset/0 keeps today's
	// default (Blob), matching every existing test/usage path byte-for-
	// byte unaffected by this migration.
	bool glHarnessRequested = GlRestorationHarness::isRequested();
	bool blobLifecycleRequested = !glHarnessRequested && BlobLifecycleHarness::isRequested();
	bool blobSoakRequested = !glHarnessRequested && !blobLifecycleRequested && BlobLifecycleHarness::isSoakRequested();
	bool temporalLifecycleRequested =
		!glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested && TemporalLifecycleHarness::isRequested();
	// RT-003: lowest-priority harness; always starts in Blob (RT-002's
	// deterministic startup), so it never selects the Temporal startup.
	bool switchHarnessRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && SceneSwitchHarness::isRequested();
	// RT-002: lowest priority; always starts in Blob (deterministic startup).
	bool acceptanceRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && TwoSceneAcceptanceHarness::isRequested();
	// HUD-004 first shell loader proof: lowest priority; runs with the normal
	// installed production scene (Blob by default) so the live scene is
	// visible through the shell — see SkinLoaderHarness.h.
	bool skinHarnessRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& SkinLoaderHarness::isRequested();
	// HUD Art reference export (tooling): lowest priority, normal production scene.
	bool artExportRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && HudArtReferenceExporter::isRequested();
	// Playable HUD Menu v0.1 demonstration: lowest priority, normal production scene.
	bool focusHarnessRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && !artExportRequested && FocusMenuHarness::isRequested();
	// BLUE-001 roster verification: lowest priority; starts in Blueprint.
	bool blueprintHarnessRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && !artExportRequested && !focusHarnessRequested
		&& BlueprintRosterHarness::isRequested();
	// Blueprint HUD integration check: lowest priority; default roster, startup Blob.
	bool blueprintHudHarnessRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && !artExportRequested && !focusHarnessRequested && !blueprintHarnessRequested
		&& BlueprintHudHarness::isRequested();
	// Three-scene production-parity soak: lowest priority; default roster, startup Blob.
	bool paritySoakRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && !artExportRequested && !focusHarnessRequested && !blueprintHarnessRequested
		&& !blueprintHudHarnessRequested && ParitySoakHarness::isRequested();
	// Shared Video resume-window verification: lowest priority; default roster, startup Blob.
	bool resumeWindowRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && !artExportRequested && !focusHarnessRequested && !blueprintHarnessRequested
		&& !blueprintHudHarnessRequested && !paritySoakRequested && ResumeWindowHarness::isRequested();
	// Blueprint cycle-driven cadence verification: lowest priority; default roster, startup Blob.
	bool cadenceRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && !artExportRequested && !focusHarnessRequested && !blueprintHarnessRequested
		&& !blueprintHudHarnessRequested && !paritySoakRequested && !resumeWindowRequested && CadenceHarness::isRequested();
	// Temporal first-activation stall verification: lowest priority; default roster, startup Blob.
	bool temporalWarmRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && !artExportRequested && !focusHarnessRequested && !blueprintHarnessRequested
		&& !blueprintHudHarnessRequested && !paritySoakRequested && !resumeWindowRequested && !cadenceRequested
		&& TemporalWarmHarness::isRequested();
	// RT-005 three-scene circuit proof: lowest priority; default roster, startup Blob.
	bool threeSceneRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && !artExportRequested && !focusHarnessRequested && !blueprintHarnessRequested
		&& !blueprintHudHarnessRequested && !paritySoakRequested && !resumeWindowRequested && !cadenceRequested
		&& !temporalWarmRequested && ThreeSceneCircuitHarness::isRequested();
	// Scene & Pattern Navigation check: lowest priority; full five-scene
	// roster (deliberately not in anyThreeSceneHarness below), startup Blob.
	bool modeHarnessRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && !artExportRequested && !focusHarnessRequested && !blueprintHarnessRequested
		&& !blueprintHudHarnessRequested && !paritySoakRequested && !resumeWindowRequested && !cadenceRequested
		&& !temporalWarmRequested && !threeSceneRequested && ModeNavigationHarness::isRequested();
	// Installation Controls & ENERGY check: lowest priority; full roster.
	bool installationHarnessRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && !artExportRequested && !focusHarnessRequested && !blueprintHarnessRequested
		&& !blueprintHudHarnessRequested && !paritySoakRequested && !resumeWindowRequested && !cadenceRequested
		&& !temporalWarmRequested && !threeSceneRequested && !modeHarnessRequested && InstallationHarness::isRequested();
	// HUD & Scene Experience v0.3 check: lowest priority; full roster.
	bool v03HarnessRequested = !glHarnessRequested && !blobLifecycleRequested && !blobSoakRequested
		&& !temporalLifecycleRequested && !switchHarnessRequested && !acceptanceRequested
		&& !skinHarnessRequested && !artExportRequested && !focusHarnessRequested && !blueprintHarnessRequested
		&& !blueprintHudHarnessRequested && !paritySoakRequested && !resumeWindowRequested && !cadenceRequested
		&& !temporalWarmRequested && !threeSceneRequested && !modeHarnessRequested && !installationHarnessRequested
		&& ExperienceV03Harness::isRequested();
	bool wantTemporalScene = temporalLifecycleRequested
		|| (!switchHarnessRequested && !acceptanceRequested && std::getenv("EXPERIENCE_RUNTIME_TEMPORAL_SCENE") != nullptr);

	// BLUE-001: EXPERIENCE_RUNTIME_BLUEPRINT_SCENE starts a normal run in
	// Blueprint Emergence (same opt-in pattern as the Temporal variable).
	bool wantBlueprintScene = !wantTemporalScene && !switchHarnessRequested && !acceptanceRequested
		&& !blobLifecycleRequested && !blobSoakRequested
		&& (blueprintHarnessRequested || std::getenv("EXPERIENCE_RUNTIME_BLUEPRINT_SCENE") != nullptr);

	// RT-006: EXPERIENCE_RUNTIME_FRAGMENT_SCENE / EXPERIENCE_RUNTIME_QUADRANT_SCENE
	// start a normal run in Fragment Trail / Quadrant Crosshair (same opt-in
	// pattern).
	bool wantFragmentScene = !wantTemporalScene && !wantBlueprintScene && !switchHarnessRequested
		&& !acceptanceRequested && !blobLifecycleRequested && !blobSoakRequested
		&& std::getenv("EXPERIENCE_RUNTIME_FRAGMENT_SCENE") != nullptr;
	bool wantQuadrantScene = !wantTemporalScene && !wantBlueprintScene && !wantFragmentScene && !switchHarnessRequested
		&& !acceptanceRequested && !blobLifecycleRequested && !blobSoakRequested
		&& std::getenv("EXPERIENCE_RUNTIME_QUADRANT_SCENE") != nullptr;

	// Roster: every harness keeps the ring it was accepted against — RT-002/
	// RT-003 the two-scene ring, every other existing harness (and the RT-004
	// stall driver's scripted key sequences) the three-scene ring. The normal
	// run, and the full-ring circuit proof, get the full five-scene roster.
	using Roster = ExperienceRuntime::ProductionRoster;
	const bool anyThreeSceneHarness = blobLifecycleRequested || blobSoakRequested || temporalLifecycleRequested
		|| skinHarnessRequested || artExportRequested || focusHarnessRequested || blueprintHarnessRequested
		|| blueprintHudHarnessRequested || paritySoakRequested || resumeWindowRequested || cadenceRequested
		|| temporalWarmRequested || (threeSceneRequested && !ThreeSceneCircuitHarness::isFullRingRequested())
		|| Rt004StallDriver::isRequested();
	const Roster roster = (switchHarnessRequested || acceptanceRequested) ? Roster::BlobTemporal
		: anyThreeSceneHarness ? Roster::ThreeScene : Roster::Full;

	if (!glHarnessRequested) {
		if (wantTemporalScene) {
			runtime.installTemporalProductionScene(roster);
		} else if (wantBlueprintScene) {
			runtime.installBlueprintProductionScene(roster);
		} else if (wantFragmentScene && roster == Roster::Full) {
			runtime.installFragmentProductionScene();
		} else if (wantQuadrantScene && roster == Roster::Full) {
			runtime.installQuadrantProductionScene();
		} else {
			runtime.installBlobProductionScene(roster);
		}
	}
	if (Rt004FramePhaseProbe::isRequested()) framePhaseProbe = std::make_unique<Rt004FramePhaseProbe>(runtime);
	// Installation settings persist only for normal runs: harnesses never
	// read or overwrite the installation's file. An explicit
	// EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS path always wins.
	{
		const bool anyHarness = glHarnessRequested || blobLifecycleRequested || blobSoakRequested || temporalLifecycleRequested
			|| switchHarnessRequested || acceptanceRequested || skinHarnessRequested || artExportRequested || focusHarnessRequested
			|| blueprintHarnessRequested || blueprintHudHarnessRequested || paritySoakRequested || resumeWindowRequested
			|| cadenceRequested || temporalWarmRequested || threeSceneRequested || modeHarnessRequested || installationHarnessRequested || v03HarnessRequested
			|| Rt004StallDriver::isRequested() || BlobRenderPathAbHarness::isRequested() || BlobEffectCapacityBench::isRequested();
		const char* settingsPath = std::getenv("EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS");
		runtime.setInstallationSettingsPath(settingsPath ? std::string(settingsPath)
			: anyHarness ? std::string() : ofToDataPath("installation-settings.json", true));
	}
	runtime.setup();
	if (framePhaseProbe) framePhaseProbe->afterRuntimeSetup(); // limiter/vsync controls apply after the runtime's own baseline

	// Blob render-path A/B pixel check: self-contained, runs once here
	// (needs only a live GL context) and exits — see BlobRenderPathAbHarness.h.
	if (BlobRenderPathAbHarness::isRequested()) {
		BlobRenderPathAbHarness::run();
		ofExit();
		return;
	}

	if (glHarnessRequested) {
		harness = std::make_unique<GlRestorationHarness>(runtime);
	} else if (blobLifecycleRequested) {
		blobHarness = std::make_unique<BlobLifecycleHarness>(runtime, BlobLifecycleHarness::Mode::Lifecycle);
	} else if (blobSoakRequested) {
		blobHarness = std::make_unique<BlobLifecycleHarness>(runtime, BlobLifecycleHarness::Mode::Soak);
	} else if (temporalLifecycleRequested) {
		temporalHarness = std::make_unique<TemporalLifecycleHarness>(runtime);
	} else if (switchHarnessRequested) {
		switchHarness = std::make_unique<SceneSwitchHarness>(runtime);
	} else if (acceptanceRequested) {
		acceptanceHarness = std::make_unique<TwoSceneAcceptanceHarness>(runtime);
	} else if (skinHarnessRequested) {
		skinHarness = std::make_unique<SkinLoaderHarness>(runtime);
	} else if (artExportRequested) {
		artExporter = std::make_unique<HudArtReferenceExporter>(runtime);
	} else if (focusHarnessRequested) {
		focusHarness = std::make_unique<FocusMenuHarness>(runtime);
	} else if (blueprintHarnessRequested) {
		blueprintHarness = std::make_unique<BlueprintRosterHarness>(runtime);
	} else if (blueprintHudHarnessRequested) {
		blueprintHudHarness = std::make_unique<BlueprintHudHarness>(runtime);
	} else if (paritySoakRequested) {
		paritySoakHarness = std::make_unique<ParitySoakHarness>(runtime);
	} else if (resumeWindowRequested) {
		resumeWindowHarness = std::make_unique<ResumeWindowHarness>(runtime);
	} else if (cadenceRequested) {
		cadenceHarness = std::make_unique<CadenceHarness>(runtime);
	} else if (temporalWarmRequested) {
		temporalWarmHarness = std::make_unique<TemporalWarmHarness>(runtime);
	} else if (threeSceneRequested) {
		threeSceneHarness = std::make_unique<ThreeSceneCircuitHarness>(runtime);
	} else if (modeHarnessRequested) {
		modeHarness = std::make_unique<ModeNavigationHarness>(runtime);
	} else if (installationHarnessRequested) {
		installationHarness = std::make_unique<InstallationHarness>(runtime);
	} else if (v03HarnessRequested) {
		v03Harness = std::make_unique<ExperienceV03Harness>(runtime);
	} else if (Rt004StallDriver::isRequested()) {
		stallDriver = std::make_unique<Rt004StallDriver>(runtime);
	}
	if (rt004::Probe::get().enabled()) {
		rt004::Probe::get().note("probe.scopeOverheadNs", std::to_string(rt004::Probe::get().measureScopeOverheadNs()));
	}
}

void ofApp::update() {
	rt004::Probe::get().beginFrame();
	RT004_SCOPE("ofApp.update");
	if (Rt004FramePhaseProbe::presentOnlyControl()) return; // diagnostic control: no runtime/harness work, clear + swap only
	if (!blobHarness && !temporalHarness && !switchHarness && !acceptanceHarness) {
		// Normal path and GL-harness path both want runtime.update()
		// called exactly once per real tick, unconditionally, here — see
		// GlRestorationHarness.h's own "call step() AFTER runtime.update()"
		// contract. BlobLifecycleHarness/TemporalLifecycleHarness are the
		// exception: each needs full control over exactly when update()
		// happens relative to its own activateScene()/deactivateScene()
		// calls (e.g. asserting a no-stale-carryover status immediately
		// after activateScene(), strictly before that cycle's first
		// update()) — see each class's own step()/runOneFrame() comments
		// for why it drives update() itself instead.
		runtime.update(ofGetLastFrameTime());
	}
	if (harness) {
		harness->step(ofGetLastFrameTime());
	}
	if (skinHarness) {
		skinHarness->step(ofGetLastFrameTime());
	}
	if (artExporter) {
		artExporter->step(ofGetLastFrameTime());
	}
	if (focusHarness) {
		focusHarness->step(ofGetLastFrameTime());
	}
	if (blueprintHarness) {
		blueprintHarness->step(ofGetLastFrameTime());
	}
	if (blueprintHudHarness) {
		blueprintHudHarness->step(ofGetLastFrameTime());
	}
	if (paritySoakHarness) {
		paritySoakHarness->step(ofGetLastFrameTime());
	}
	if (resumeWindowHarness) {
		resumeWindowHarness->step(ofGetLastFrameTime());
	}
	if (cadenceHarness) {
		cadenceHarness->step(ofGetLastFrameTime());
	}
	if (temporalWarmHarness) {
		temporalWarmHarness->step(ofGetLastFrameTime());
	}
	if (threeSceneHarness) {
		threeSceneHarness->step(ofGetLastFrameTime());
	}
	if (modeHarness) {
		modeHarness->step(ofGetLastFrameTime());
	}
	if (installationHarness) {
		installationHarness->step(ofGetLastFrameTime());
	}
	if (v03Harness) {
		v03Harness->step(ofGetLastFrameTime());
	}
	if (blobHarness) {
		blobHarness->step(ofGetLastFrameTime());
	}
	if (temporalHarness) {
		temporalHarness->step(ofGetLastFrameTime());
	}
	if (switchHarness) {
		switchHarness->step(ofGetLastFrameTime());
	}
	if (acceptanceHarness) {
		acceptanceHarness->step(ofGetLastFrameTime());
	}
	if (stallDriver) {
		stallDriver->step(ofGetLastFrameTime());
	}
}

void ofApp::draw() {
	RT004_SCOPE("ofApp.draw");
	if (Rt004FramePhaseProbe::presentOnlyControl()) { ofClear(0, 0, 0, 255); return; }
	if (!harness && !blobHarness && !temporalHarness && !switchHarness && !acceptanceHarness && !skinHarness && !artExporter && !focusHarness && !blueprintHarness && !blueprintHudHarness && !paritySoakHarness && !resumeWindowHarness && !cadenceHarness && !temporalWarmHarness && !threeSceneHarness && !modeHarness && !installationHarness && !v03Harness) {
		runtime.draw();

		// Blob Experiment 2: serve the bench's capture requests (scene-only
		// from the runtime-owned scene FBO, plus the full composited window).
		std::string label;
		if (runtime.blobSceneForTesting().takeCapacityCaptureRequest(label)) {
			std::string base = "captures/blob_capacity/" + label;
			runtime.saveSceneFrameCaptureForTesting(base + "_scene_only.png");
			ofImage img;
			img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
			img.save(base + "_hud.png");
			ofLogNotice("BlobCapacity") << "captured " << base << "_{scene_only,hud}.png";
		}
	}
	// When a harness is active, it drives runtime.draw() itself (see
	// GlRestorationHarness::step()/BlobLifecycleHarness::step()/
	// TemporalLifecycleHarness::step()) so it can inspect state and grab
	// screenshots between specific, scripted calls.
}

void ofApp::exit() {
	runtime.exit();
	rt004::Probe::get().writeSummary();
}

void ofApp::mousePressed(int, int, int) {
	runtime.pointerPressed(); // device input: exits IMMERSIVE (only effect today)
}

void ofApp::keyPressed(int key) {
	// Blob Experiment 2: capacity keys first, only when the bench is on.
	if (runtime.blobSceneForTesting().handleCapacityKey(key)) {
		return;
	}
	runtime.keyPressed(key);
}
