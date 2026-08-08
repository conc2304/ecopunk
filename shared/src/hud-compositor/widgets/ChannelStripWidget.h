#pragma once

#include "HudWidgetBase.h"
#include "HudTextMetricsCache.h"

namespace hudpresent {

// Architecture-Closure Session: each of the 4 per-channel cells truncates
// its own value text to that cell's own (narrow — 1/4 of the region)
// width — this is the widget the canonical "longest scene/status text"
// screenshot case first caught overflowing past its cells.
class ChannelStripWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::ChannelStrip; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;

private:
	mutable HudTextMetricsCache textCache_;
};

} // namespace hudpresent
