#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"
#include <string>

namespace hud {

struct WindowChromeOptions {
	std::string label = "0x0000"; // hex-address readout, drawn in the title bar
	bool showCloseBox = true;
};

// Border + title bar + hex-address label + close-box glyph framing an
// arbitrary piece of content. Deliberately draws none of that content
// itself — pure vector chrome only, consistent with this library's
// "immediate-mode primitives only, no shaders/FBOs/textures" rule (see
// README.md). Callers (e.g. Fragment Trail's FTFragment) draw their own
// video crop first, at the same bounds, then call this widget's draw() on
// top to frame it.
class WindowChrome : public HudWidget {
public:
	void setOptions(const WindowChromeOptions& next) { options = next; }
	// Updates just the label, leaving other options untouched — same
	// single-field-update pattern as StatusLightWidget::setState().
	void setLabel(const std::string& label) { options.label = label; }
	void draw() override;
	ofVec2f getMinSize() const override { return { 60.0f, 40.0f }; }

private:
	WindowChromeOptions options;
	HudFrameRenderer frame;

	void drawTitleBar() const;
	void drawCloseBox(float x, float y, float size) const;
};

} // namespace hud
