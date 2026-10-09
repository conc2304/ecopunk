#pragma once

#include "HudWidgetBase.h"
#include "HudTextMetricsCache.h"

namespace hudpresent {

// Architecture-Closure Session: individual chip labels (canonical effect
// IDs) truncate to the region's own width as a backstop — the existing
// row-wrapping already handles a chip that doesn't fit on the current
// row, but a single effect ID longer than the region itself still needs
// its own truncation, same as every other widget's dynamic text.
class EffectChipsWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::EffectChips; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;

private:
	static constexpr float kMinChipHeight = 17.0f; // one 13 px native line + 2 px inset each side

	mutable HudTextMetricsCache textCache_;
};

} // namespace hudpresent
