#include "ofApp.h"

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
	bool wantTemporalScene = temporalLifecycleRequested || std::getenv("EXPERIENCE_RUNTIME_TEMPORAL_SCENE") != nullptr;

	if (!glHarnessRequested) {
		if (wantTemporalScene) {
			runtime.installTemporalProductionScene();
		} else {
			runtime.installBlobProductionScene();
		}
	}
	runtime.setup();

	if (glHarnessRequested) {
		harness = std::make_unique<GlRestorationHarness>(runtime);
	} else if (blobLifecycleRequested) {
		blobHarness = std::make_unique<BlobLifecycleHarness>(runtime, BlobLifecycleHarness::Mode::Lifecycle);
	} else if (blobSoakRequested) {
		blobHarness = std::make_unique<BlobLifecycleHarness>(runtime, BlobLifecycleHarness::Mode::Soak);
	} else if (temporalLifecycleRequested) {
		temporalHarness = std::make_unique<TemporalLifecycleHarness>(runtime);
	}
}

void ofApp::update() {
	if (!blobHarness && !temporalHarness) {
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
	if (blobHarness) {
		blobHarness->step(ofGetLastFrameTime());
	}
	if (temporalHarness) {
		temporalHarness->step(ofGetLastFrameTime());
	}
}

void ofApp::draw() {
	if (!harness && !blobHarness && !temporalHarness) {
		runtime.draw();
	}
	// When a harness is active, it drives runtime.draw() itself (see
	// GlRestorationHarness::step()/BlobLifecycleHarness::step()/
	// TemporalLifecycleHarness::step()) so it can inspect state and grab
	// screenshots between specific, scripted calls.
}

void ofApp::exit() {
	runtime.exit();
}

void ofApp::keyPressed(int key) {
	runtime.keyPressed(key);
}
