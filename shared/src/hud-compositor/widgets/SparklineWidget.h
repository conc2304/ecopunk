#pragma once

#include "HudWidgetBase.h"
#include "HudTextMetricsCache.h"

namespace hudpresent {

// Draws from input.historySamples/historyStats only — this widget never
// touches HudHistoryStore itself (the orchestrator owns the store and
// pre-fetches read-only samples every frame), satisfying "widgets must
// not own independent data-sampling clocks" for the one widget type in
// this increment that actually supports history
// (HudWidgetContract.h: Sparkline is the only supportsHistory=true entry).
//
// Final Narrow Closure Patch typography audit: production-visible dynamic
// text is limited to the caption label (the plotted line/event-pulse dot
// carry no text of their own) — see LabelWidget.h's own comment for why a
// per-widget-type text cache is the established pattern.
class SparklineWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::Sparkline; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;

private:
	mutable HudTextMetricsCache textCache_;
};

} // namespace hudpresent
