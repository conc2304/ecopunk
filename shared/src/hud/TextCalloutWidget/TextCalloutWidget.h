#pragma once
#include "../shared/HudWidget.h"
#include <string>
#include <vector>

namespace hud {

enum class CalloutAnchor { Point, TopLeft, TopRight, BottomLeft, BottomRight };

struct TextCalloutOptions {
	float holdDuration    = 1.0f;
	float fadeOutDuration = 0.3f;
	float cornerPadding   = 10.0f;
};

// One-shot text label: fires at either an arbitrary point (e.g. a
// coordinate readout next to a reticle) or a fixed corner anchor (e.g. a
// status phrase), holds, then fades. Covers Lock Sequence's coordinate
// label + status phrase, Handshake's distance label, and Radar Ping's
// status phrase — nothing existing in this library fires a one-shot label.
class TextCalloutWidget : public HudWidget {
public:
	void setOptions(const TextCalloutOptions& next) { options = next; }
	void triggerAtPoint(float nx, float ny, const std::string& text);
	void triggerAtCorner(CalloutAnchor anchor, const std::string& text);
	void update(float dt) override;
	void draw() override;
	ofVec2f getMinSize() const override { return { 60.0f, 16.0f }; }

private:
	struct Callout {
		std::string text;
		CalloutAnchor anchor = CalloutAnchor::Point;
		float nx = 0.0f, ny = 0.0f;
		float age = 0.0f;
	};
	std::vector<Callout> callouts;
	TextCalloutOptions options;
};

} // namespace hud
