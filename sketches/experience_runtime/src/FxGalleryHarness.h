#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <string>
#include <vector>

// FxGalleryHarness — real-runtime check of the Shared Effects Gallery scene and the shared interactive
// HUD behaviour (EXPERIENCE_RUNTIME_FX_GALLERY_HARNESS; also set EXPERIENCE_RUNTIME_FX_SCENE=1).
// Drives the real key path (ExperienceRuntime::keyPressed) and asserts from the real HudFrameData and
// HUD bridge: full-roster mode cycling + wrap, canonical effect activity, 0/1/2-control modes, numeric
// and enum control steps, Down-on-last exit, focus repair on mode change, nested-focus nav dimming,
// scene switch away/back. Captures 1280x720 proofs under bin/data/captures/fx-gallery/.
class FxGalleryHarness {
public:
	static bool isRequested();
	explicit FxGalleryHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Step {
		std::string name;
		std::function<void()> act;
		int waitFrames = 8;
		std::function<void()> verify;
		std::function<bool()> until;   // optional: wait (bounded) until true before counting waitFrames
	};
	void check(const std::string& name, bool passed, const std::string& detail = {});
	void capture(const std::string& name);
	void buildScript();
	bool idleOn(const std::string& sceneId) const;
	std::string metric(const std::string& id) const;
	int controlsShown() const;
	std::string phaseName() const;
	std::string focused() const;
	SharedEffectsGalleryScene& scene();
	void goToEffect(const std::string& id);
	void focusRegion();

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t at_ = 0;
	int frame_ = 0;
	bool acted_ = false, done_ = false, untilDone_ = false;
	int roster_ = 0;
	std::string tmp_;
	int passes_ = 0, failures_ = 0;
};
