#pragma once
#include "../shared/HudWidget.h"
#include <vector>

namespace hud {

struct GlitchTearOptions {
	float triggerIntervalMin = 30.0f;  // seconds between ambient tears
	float triggerIntervalMax = 60.0f;
	float tearDurationMin = 0.04f;    // seconds a single slice stays visible
	float tearDurationMax = 0.14f;
	int   sliceCountMin = 2;          // slices spawned per tear event
	int   sliceCountMax = 5;
	float maxJitter = 0.12f;          // displacement along the tear's long axis, fraction of that axis
	float verticalChance = 0.2f;      // probability a given slice tears vertically instead of horizontally
	float thicknessMin = 0.004f;      // thinnest possible tear, fraction of the cross axis
	float thicknessMax = 0.06f;       // thickest possible tear, fraction of the cross axis
};

// Vector-only "signal glitch" decoration: a handful of thin horizontal or
// vertical bands briefly appear with an offset along their length, then
// vanish within a couple of frames. This does not displace the actual video
// pixels beneath it (this library draws only immediate-mode vector
// primitives, no shaders/FBOs/textures, per README.md) — it reads as glitch
// chrome layered on top, not a true pixel-tear of the composition.
class GlitchTearWidget : public HudWidget {
public:
	void setup() override;
	void setOptions(const GlitchTearOptions& next) { options = next; }
	// Fires a tear immediately regardless of the ambient timer — lets a
	// consumer trigger glitches off real events (e.g. a pattern switch)
	// instead of only ambient timing.
	void trigger();
	void update(float dt) override;
	void draw() override;
	ofVec2f getMinSize() const override { return { 60.0f, 20.0f }; }

private:
	struct Slice {
		bool  vertical = false;   // false = horizontal band, true = vertical band
		float posNorm = 0.0f;     // position along the cross axis (y for horizontal, x for vertical)
		float sizeNorm = 0.05f;   // thickness along the cross axis
		float jitterNorm = 0.0f;  // displacement along the tear's own long axis
		float life = 0.0f;
		float lifeMax = 0.08f;
	};
	GlitchTearOptions options;
	std::vector<Slice> slices;
	float nextTriggerT = 2.0f;
	void spawnTear();
};

} // namespace hud
