#pragma once

#include "HudWidgetBase.h"
#include "HudTextMetricsCache.h"

namespace hudpresent {

// Final Narrow Closure Patch typography audit: production-visible dynamic
// text (caption + formatted value, e.g. "ACTIVE TIME" / "3m 42s") — see
// LabelWidget.h's own comment for why a per-widget-type text cache is the
// established pattern.
class NumericValueWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::NumericValue; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;

private:
	mutable HudTextMetricsCache textCache_;
};

} // namespace hudpresent
