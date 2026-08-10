#pragma once

#include "HudWidgetBase.h"
#include "HudTextMetricsCache.h"

namespace hudpresent {

// Final Narrow Closure Patch typography audit: production-visible dynamic
// text (a bottom caption + a centered formatted value inside the ring) —
// see LabelWidget.h's own comment for why a per-widget-type text cache is
// the established pattern.
class ProgressRingWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::ProgressRing; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;

private:
	mutable HudTextMetricsCache textCache_;
};

} // namespace hudpresent
