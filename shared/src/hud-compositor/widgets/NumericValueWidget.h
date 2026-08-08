#pragma once

#include "HudWidgetBase.h"

namespace hudpresent {

class NumericValueWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::NumericValue; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;
};

} // namespace hudpresent
