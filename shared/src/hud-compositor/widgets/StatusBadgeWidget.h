#pragma once

#include "HudWidgetBase.h"
#include "HudTextMetricsCache.h"

namespace hudpresent {

// Bundles captionText (a static label, e.g. "HEALTH") with the "value"
// role's formatted/vocabulary-resolved text (e.g. "READY") in one pill.
//
// Architecture-Closure Session: this is one of the widgets the canonical
// screenshot matrix's "longest scene/status text" case directly exercises
// (universal.primary_state and overlay.health/effect_summary.health are
// all StatusBadge — a scene-supplied state ID has no length guarantee) —
// see LabelWidget.h's own comment for why a per-widget-type text cache is
// the established pattern here.
class StatusBadgeWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::StatusBadge; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;

private:
	mutable HudTextMetricsCache textCache_;
};

} // namespace hudpresent
