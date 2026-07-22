#pragma once

#include "ofMain.h"
#include "HudTypes.h"

namespace hud {

class HudFrameRenderer {
public:
	void draw(const HudBounds & b, const WidgetColors & colors, const FrameOptions & options, float t = 0.0f) const;

private:
	void drawCorners(const HudBounds & b, const ofColor & c, const FrameOptions & o) const;
	void drawBox(const HudBounds & b, const ofColor & c, const FrameOptions & o) const;
	void drawBrackets(const HudBounds & b, const ofColor & c, const FrameOptions & o) const;
	void drawOrganic(const HudBounds & b, const ofColor & c, const FrameOptions & o, float t) const;
	void drawRegistrationMarks(const HudBounds & b, const ofColor & c, const FrameOptions & o) const;
	void drawTicks(const HudBounds & b, const ofColor & c, const FrameOptions & o) const;
};

} // namespace hud
