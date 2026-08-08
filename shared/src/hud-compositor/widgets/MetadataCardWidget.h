#pragma once

#include "HudWidgetBase.h"
#include "HudTextMetricsCache.h"

namespace hudpresent {

// Architecture-Closure Session: caption/title/value/meta each truncate to
// the card's own live-content width (bounds minus padding) — see
// LabelWidget.h's own comment for why a per-widget-type text cache is the
// established pattern.
class MetadataCardWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::MetadataCard; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;

private:
	mutable HudTextMetricsCache textCache_;
};

} // namespace hudpresent
