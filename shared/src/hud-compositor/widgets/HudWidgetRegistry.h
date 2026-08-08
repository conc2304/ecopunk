#pragma once

// ============================================================================
// HudWidgetRegistry.h — the compositor-oriented widget registry this
// task's §7 asks for. Every IHudWidget implementation is stateless (see
// HudWidgetBase.h), so this registry owns exactly one shared instance per
// HudWidgetType rather than constructing a new widget per binding/frame —
// a direct "no per-frame widget allocation" measure (this task's §15).
// ============================================================================

#include "HudWidgetBase.h"

namespace hudpresent {

class HudWidgetRegistry {
public:
	HudWidgetRegistry();
	~HudWidgetRegistry();

	// Never nullptr for any HudWidgetType value this registry was built
	// with — every entry in the HudWidgetType enum has a real
	// implementation in this increment (Timeline included, per its
	// header's "minimal static phase strip" note).
	const IHudWidget& widgetFor(HudWidgetType type) const;

private:
	struct Impl;
	Impl* impl_;
};

} // namespace hudpresent
