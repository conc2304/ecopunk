#pragma once

#include "HudWidgetBase.h"
#include "HudTextMetricsCache.h"

namespace hudpresent {

// Renders `captionText` (a static, already-vocabulary-resolved caption)
// or, when the "text" role rendered this frame, that role's
// formattedText instead — see HudWidgetContract.h's comment on why Label
// supports both a static-caption and dynamic-text usage. When an
// "enabled" role is bound and resolves to false, the whole label draws at
// reduced opacity — this is the "nonfunctional visual state" this task's
// §13 asks for; it never disables input handling, because nothing in this
// widget dispatches commands at all.
class LabelWidget : public IHudWidget {
public:
	HudWidgetType widgetType() const override { return HudWidgetType::Label; }
	void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const override;

private:
	// Owned per-widget-type instance, not per-binding — HudWidgetRegistry
	// holds exactly one LabelWidget for the whole app lifetime (see that
	// class's header comment), so this cache persists across frames
	// exactly as this task's §11 "cache font/text resources" expects,
	// with no extra plumbing through HudWidgetInput needed. `mutable`
	// because draw() is const (widgets are stateless with respect to
	// RESOLVED DATA — see HudWidgetBase.h — but a measurement cache is a
	// pure memoization detail, not resolved-data state).
	mutable HudTextMetricsCache textCache_;
};

} // namespace hudpresent
