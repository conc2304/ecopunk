#pragma once

#include "HudWidgetBase.h"

namespace hudpresent {

class ProgressRingWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::ProgressRing; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;
};

} // namespace hudpresent
