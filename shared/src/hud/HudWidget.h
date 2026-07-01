#pragma once

#include "ofMain.h"
#include "HudTypes.h"

// Ported from quadrant-crosshair/src/hud_elements/shared/HudWidget.h verbatim
// — already responsive to an arbitrary container via setBounds()/onResize(),
// which is exactly what's needed to place a widget inside a grid slot
// instead of quadrant-crosshair's original fullscreen/corner placement.
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
