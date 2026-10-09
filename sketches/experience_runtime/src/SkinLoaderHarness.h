#pragma once

#include "ExperienceRuntime.h"

#include "ofImage.h"

#include <string>
#include <vector>

// SkinLoaderHarness — HUD-004 first shell-only loader proof, run inside the
// real ExperienceRuntime with the real installed production scene and the
// real HudCompositorBridge → HudSkinLoader → HudSkinOfImageBackend →
// HudWireframeRenderer path (no fake backend, no second compositor).
//
// Opt-in: EXPERIENCE_RUNTIME_SKIN_HARNESS=1. Drives runtime.draw() itself
// (same contract as GlRestorationHarness), saves captures under
// bin/data/captures/skin-harness/, writes results.txt there, and exits with
// status 0 only if every check passed.
//
// Inputs: the canonical package assets/hud/skins/fixture-shell-proof/ and the
// negative/replacement packages in test/skin_fixtures/skins/ (copied into
// bin/data/skin-harness/skins/ at startup, where the runtime-only
// symlink-escape case is added — the repository stores no symlinks).
class SkinLoaderHarness {
public:
	static bool isRequested();

	explicit SkinLoaderHarness(ExperienceRuntime& runtime);

	// Call once per frame from ofApp::update(), AFTER runtime.update(dt).
	void step(float dt);

private:
	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void renderFrame();
	ofPixels capture(const std::string& name);

	void runColdRejections();
	void runBlendChecks(const ofPixels& shellFrame);
	void runBackdropBlendCheck();
	void runFailedReplacements();
	void runSuccessfulReplacement();
	void runShutdown();
	void finish();

	ExperienceRuntime& runtime_;
	int frame_ = 0;
	std::string canonicalRoot_;
	std::string scratchRoot_;
	ofPixels baseline_;
	unsigned int textureIdA_ = 0;
	unsigned int textureIdB_ = 0;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
