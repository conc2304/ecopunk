#pragma once

#include "HudWidgetBase.h"
#include "HudTextMetricsCache.h"

namespace hudpresent {

// The empty-flexible-region fallback (this task's §9) and the direct
// binding target for HudMissingPolicy::UseAmbientFallback. Motion is a
// deterministic function of input.elapsedSeconds (+ the optional
// "influence" role, when bound and present) — never a per-widget RNG or
// accumulator, per HudWidgetBase.h's "no independent data-sampling clock"
// rule.
//
// Final Narrow Closure Patch typography audit: production-visible dynamic
// text is limited to the caption label (the wobble lines carry no text of
// their own) — see LabelWidget.h's own comment for why a per-widget-type
// text cache is the established pattern.
class AmbientFieldWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::AmbientField; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;

private:
	mutable HudTextMetricsCache textCache_;
};

} // namespace hudpresent
