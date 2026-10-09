#include "ofApp.h"
#include "BlobRenderPathAbHarness.h"
#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated)

#include <cstdlib>

void ofApp::setup() {
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
	bool wantTemporalScene = temporalLifecycleRequested
		|| (!switchHarnessRequested && !acceptanceRequested && std::getenv("EXPERIENCE_RUNTIME_TEMPORAL_SCENE") != nullptr);

	// BLUE-001: EXPERIENCE_RUNTIME_BLUEPRINT_SCENE starts a normal run in
	// Blueprint Emergence (same opt-in pattern as the Temporal variable).
	bool wantBlueprintScene = !wantTemporalScene && !switchHarnessRequested && !acceptanceRequested
		&& !blobLifecycleRequested && !blobSoakRequested
		&& (blueprintHarnessRequested || std::getenv("EXPERIENCE_RUNTIME_BLUEPRINT_SCENE") != nullptr);

	if (!glHarnessRequested) {
		if (wantTemporalScene) {
			runtime.installTemporalProductionScene();
		} else if (wantBlueprintScene) {
			runtime.installBlueprintProductionScene();
		} else {
			// RT-002/RT-003 acceptance harnesses keep the two-scene ring they
			// were accepted against; every other run gets the full roster.
			runtime.installBlobProductionScene(!(switchHarnessRequested || acceptanceRequested));
		}
	}
	runtime.setup();

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
	if (!harness && !blobHarness && !temporalHarness && !switchHarness && !acceptanceHarness && !skinHarness && !artExporter && !focusHarness && !blueprintHarness && !blueprintHudHarness && !paritySoakHarness && !resumeWindowHarness && !cadenceHarness) {
		runtime.draw();
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

void ofApp::keyPressed(int key) {
	runtime.keyPressed(key);
}
