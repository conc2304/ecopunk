#pragma once

#include "ofMain.h"
#include "HudTypes.h"

namespace hud {

class HudWidget {
public:
	virtual ~HudWidget() = default;

	virtual void setup() {}
	virtual void update(float dt) { time += dt; }
	virtual void draw() = 0;
	virtual void randomize(int seed = -1) {
		if (seed >= 0) rng.seed(seed);
	}

	// Optional auto-layout hint. Not a rendering contract every widget must
	// honor meaningfully — most widgets render correctly at any bounds via
	// setBounds()/setSize(); this exists for consumers (e.g. blueprint_emergence's
	// snap-grid placement) that need to avoid placing a widget in a slot too
	// small for its content to read legibly.
	virtual ofVec2f getMinSize() const { return { 150.0f, 90.0f }; }

	void setBounds(float x, float y, float width, float height) {
		bounds = { x, y, width, height };
		onResize();
	}
	void setSize(float width, float height) {
		bounds.width = width;
		bounds.height = height;
		onResize();
	}
	void setPosition(float x, float y) {
		bounds.x = x;
		bounds.y = y;
	}
	void setTheme(const HudTheme & nextTheme) { theme = nextTheme; }
	void setColors(const WidgetColors & colors) { theme.colors = colors; }
	void setFrameOptions(const FrameOptions & options) { theme.frame = options; }
	void setMotion(const MotionSettings & settings) { motion = settings; }

	const HudBounds & getBounds() const { return bounds; }
	HudBounds & getBounds() { return bounds; }

protected:
	virtual void onResize() {}

	HudBounds bounds;
	HudTheme theme;
	MotionSettings motion;
	float time = 0.0f;
	// Shim: widgets call rng.seed(n) to re-seed OF's global random state
	struct {
		void seed(int s) { ofSetRandomSeed(s); }
	} rng;
};

} // namespace hud
