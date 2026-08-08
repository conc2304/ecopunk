#pragma once

// ============================================================================
// HudWidgetBase.h — the compositor-oriented widget interface.
//
// Deliberately NOT hud::HudWidget (shared/src/hud/shared/HudWidget.h) —
// per this task's §7: "Create a new compositor-oriented widget registry.
// Do not directly promote scene-local widgets from shared/src/hud/."
// shared/src/hud/'s widgets are reused only as algorithmic/visual
// precedent (their sx/sy/su responsive-scaling helpers, composition of a
// separate frame renderer, immediate-mode-only drawing) — see
// HudWidgetDrawUtils.h.
//
// The most load-bearing difference from hud::HudWidget: IHudWidget has NO
// update(dt) and holds NO mutable per-instance state at all. Every input
// a widget needs — including elapsedSeconds for ambient motion — arrives
// through HudWidgetInput on every draw() call; nothing here "remembers"
// anything between frames. This is what "widgets must not own independent
// data-sampling clocks" (this task's §7) means concretely: a widget
// cannot have its own `time += dt` accumulator the way every
// shared/src/hud/ widget does.
// ============================================================================

#include "../HudWidgetInput.h"

#include "ofColor.h"
#include "ofRectangle.h"

namespace hudpresent {

struct HudWidgetColors {
	ofColor primary = ofColor(124, 232, 230, 220);
	ofColor secondary = ofColor(103, 255, 142, 200);
	ofColor accent = ofColor(244, 255, 106, 220);
	ofColor muted = ofColor(124, 232, 230, 90);
	ofColor background = ofColor(0, 20, 16, 40);
	ofColor warning = ofColor(255, 120, 90, 230);
};

class IHudWidget {
public:
	virtual ~IHudWidget() = default;
	virtual HudWidgetType widgetType() const = 0;

	// `bounds` is pixel-space, already resolved by the orchestrator from
	// the region's normalized HudNormalizedBounds + current canvas size —
	// widgets never see or compute normalized-to-pixel conversion
	// themselves. No sceneId parameter anywhere in this signature — see
	// HudWidgetBase.h's header comment and
	// shared/src/hud-compositor-test/hud_presentation_tests.cpp's
	// test_no_scene_id_leaks_into_compiled_bindings().
	virtual void draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const = 0;
};

} // namespace hudpresent
