#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"
#include <deque>
#include <string>

namespace hud {

struct LogScrollOptions {
	int maxLines = 40;         // retained history, oldest dropped past this
	float scrollSpeed = 10.0f; // px/sec continuous upward drift
	bool showFrame = true;
};

// Continuous vertical scroll of monospace-style text, terminal-log style.
// Distinct from DataCardWidget, which only ever shows one static value line.
class LogScrollWidget : public HudWidget {
public:
	void setup() override;
	void setOptions(const LogScrollOptions& next) { options = next; }
	// Appends one line, entering at the bottom like a real terminal log —
	// the sole write API, mirroring DataCardWidget::setValueText()'s
	// single-field-update pattern.
	void pushLine(const std::string& line);
	void update(float dt) override;
	void draw() override;
	ofVec2f getMinSize() const override { return { 120.0f, 60.0f }; }

private:
	LogScrollOptions options;
	std::deque<std::string> lines;
	HudFrameRenderer frame;
	float scrollOffset = 0.0f; // px; wraps by one line height as it scrolls
	float lineHeightPx = 12.0f;
};

} // namespace hud
