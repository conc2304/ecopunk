#pragma once

#include "HudWidgetBase.h"

namespace hudpresent {

// Tooling-only — the Validation Studio's "show visible binding-error
// placeholder" requirement (this task's §6/§12). Never instantiated by
// production rendering: production skips an invalid optional binding
// entirely (log once, continue), per this task's §6 — only the studio
// substitutes this widget for an invalid HudCompiledBinding.
class BindingPlaceholderWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::BindingPlaceholder; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;
};

} // namespace hudpresent
