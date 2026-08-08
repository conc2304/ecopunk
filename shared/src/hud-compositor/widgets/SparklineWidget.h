#pragma once

#include "HudWidgetBase.h"

namespace hudpresent {

// Draws from input.historySamples/historyStats only — this widget never
// touches HudHistoryStore itself (the orchestrator owns the store and
// pre-fetches read-only samples every frame), satisfying "widgets must
// not own independent data-sampling clocks" for the one widget type in
// this increment that actually supports history
// (HudWidgetContract.h: Sparkline is the only supportsHistory=true entry).
class SparklineWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::Sparkline; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;
};

} // namespace hudpresent
