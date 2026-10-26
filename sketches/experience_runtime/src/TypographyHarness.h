#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <string>
#include <vector>

// TypographyHarness — Live Typography Playground real-runtime check
// (EXPERIENCE_RUNTIME_TYPOGRAPHY_HARNESS; requires the playground itself,
// EXPERIENCE_RUNTIME_TYPOGRAPHY_PLAYGROUND=1, and a scratch
// EXPERIENCE_RUNTIME_TYPOGRAPHY_DIR so saving never touches the repo preset).
// Drives the playground through its own keys over the real Dark Moss HUD:
// font cycling with cache reuse, an invalid font skipped gracefully, size /
// spacing / colour / glow, centring on canonical bounds, focus brackets
// untouched, save + reload round trip, and no font I/O while rendering.
class TypographyHarness {
public:
	static bool isRequested();
	explicit TypographyHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Step {
		std::string label;
		std::function<bool()> ready;
		std::function<void()> act;
	};
	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	TypographyPlayground* pg() const { return runtime_.typographyPlaygroundForTesting(); }
	std::string fontName() const;
	bool cycleTo(const std::string& nameStart); // press '.' until the active font's name starts with it
	void checkCentred(const std::string& where);
	void buildScript();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t stepIndex_ = 0;
	int framesInStep_ = 0;
	int loadsMark_ = 0;
	int sizeBefore_ = 0;
	float spacingBefore_ = 0.0f;
	std::vector<int> galleryVisited_;
	int galleryLastIndex_ = -1;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
