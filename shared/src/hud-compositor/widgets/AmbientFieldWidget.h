#pragma once

#include "HudWidgetBase.h"

namespace hudpresent {

// The empty-flexible-region fallback (this task's §9) and the direct
// binding target for HudMissingPolicy::UseAmbientFallback. Motion is a
// deterministic function of input.elapsedSeconds (+ the optional
// "influence" role, when bound and present) — never a per-widget RNG or
// accumulator, per HudWidgetBase.h's "no independent data-sampling clock"
// rule.
class AmbientFieldWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::AmbientField; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;
};

} // namespace hudpresent
