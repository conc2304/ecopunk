#pragma once
#include "../shared/HudWidget.h"
#include <string>

namespace hud {

enum class StatusState { Idle,
	Active,
	Alert };

struct StatusLightOptions {
	std::string label = "STATUS";
	StatusState state = StatusState::Idle;
	bool blinkOnAlert = true;
	float blinkRate = 3.0f; // blink cycles per second while Alert
};

// Single on/off/state indicator — a small dot plus a label, distinct from
// every other widget in this library (all of which are dials, cards, or
// full-field effects rather than a simple discrete-state light).
class StatusLightWidget : public HudWidget {
public:
	void setOptions(const StatusLightOptions& next) { options = next; }
	// Updates just the state, leaving label/other options untouched — same
	// single-field-update pattern as DataCardWidget::setValueText().
	void setState(StatusState next) { options.state = next; }
	void update(float dt) override;
	void draw() override;
	ofVec2f getMinSize() const override { return { 70.0f, 24.0f }; }

private:
	StatusLightOptions options;
	float pulsePhase = 0.0f;
};

} // namespace hud
