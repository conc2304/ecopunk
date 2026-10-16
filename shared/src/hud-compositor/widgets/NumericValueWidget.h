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

	// Smallest caption baseline offset from the region top that keeps the
	// 8x13 bitmap font's cap height (~9 px above baseline) inside the region.
	static constexpr float kCaptionMinBaselineOffset = 11.0f;

private:
	mutable HudTextMetricsCache textCache_;
};

} // namespace hudpresent
