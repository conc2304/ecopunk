#pragma once

#include "HudWidgetBase.h"
#include "HudTextMetricsCache.h"

namespace hudpresent {

// Per this task's §7: "Timeline may be implemented as a minimal static
// phase strip if time permits. Otherwise, fully define and test its
// binding contract and mark rendering as deferred." The binding contract
// (HudWidgetContract.h: "phase" required Identifier, "progress" optional
// Ratio) is fully defined and compiler-validated regardless; this class
// implements the minimal static-strip rendering rather than deferring it.
//
// "Static" here means: a single fixed-position marker on a plain
// horizontal strip, driven directly by the "phase"/"progress" roles —
// there is no multi-state history/sequence animation, no per-instance
// memory of previously-visited phases, and no independent clock (see
// HudWidgetBase.h). A richer timeline (showing the actual path already
// traveled through a state sequence) would need history-like storage this
// increment's HudHistoryStore does not model for Identifier-typed values —
// flagged as a gap for a later increment, not solved here.
class TimelineWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::Timeline; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;

private:
	mutable HudTextMetricsCache textCache_;
};

} // namespace hudpresent
