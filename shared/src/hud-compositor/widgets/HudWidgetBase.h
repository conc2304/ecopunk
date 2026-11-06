#pragma once

#include <string>

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
	// Informational caption TEXT (widget captions, "MODE i/n", clock date and
	// zone, scene-position suffix). Alpha 165 measures >= 4.5:1 against the
	// real composited Dark Moss panels (lowest: ~4.6:1 over moss texture;
	// ~6:1 over plain fill) while staying clearly below `primary` values.
	// `muted` stays the token for outlines, tracks, recessed/ambient
	// placeholders and other non-caption marks.
	ofColor caption = ofColor(124, 232, 230, 165);
	ofColor background = ofColor(0, 20, 16, 40);
	ofColor warning = ofColor(255, 120, 90, 230);
	// Data-series colours for charts that compare two things (Orbit observations A
	// and B). Themes pull them to readable lightness on their panel surface.
	ofColor dataA = ofColor(110, 210, 120);
	ofColor dataB = ofColor(95, 150, 240);
	// Drop-shadow colour under every text draw (alpha is scaled by the text's
	// own alpha): dark under light "night" text, light under dark "day" ink.
	ofColor shadow = ofColor(0, 0, 0, 140);
	// Nav-button labels. Button faces can differ in tone from the panels
	// (Plywood + Marble: black faces on cream panels), so they get their own
	// token; night = primary.
	ofColor buttonText = ofColor(124, 232, 230, 220);
	// Shadow under nav-button labels (dark: the baked button faces are dark).
	// When the skin supplies its own button faces the renderer substitutes
	// primary / shadow so labels follow the palette.
	ofColor buttonShadow = ofColor(0, 0, 0, 140);
	// System state, NOT theme: true while the system is in its DAY period (the day/night
	// schedule's clock), false at night. The clock widget shows the sun icon by day and the
	// moon at night whatever theme or skin is selected. The renderer owns it (setDayPeriod),
	// so replacing the colours never changes it.
	bool daytime = true;
	// Navigation-button labels: 0 = text, 1 = icon only, 2 = icon above text. Renderer-owned
	// (setButtonLabelMode), like `daytime`, so replacing the colours never changes it.
	int buttonLabelMode = 0;
	// Nav-button icon opacity (idle 0.8, focused 0.9, active 1.0), set per button by the renderer.
	float iconOpacity = 0.8f;
};

// Day/night text palettes, chosen by the runtime's skin schedule
// (assets/hud/skins/skin-schedule.json "palette"). "night" = the defaults
// above (light text for dark skins); "day" = dark ink for light skins such
// as Plywood + Marble's cream panels. Unknown names -> night.
inline HudWidgetColors hudPaletteNamed(const std::string& name) {
	HudWidgetColors c;
	if (name == "day") {
		c.primary = ofColor(38, 34, 24, 240);
		c.secondary = ofColor(44, 92, 46, 235);
		c.accent = ofColor(150, 92, 16, 240);
		c.muted = ofColor(60, 52, 36, 120);
		c.caption = ofColor(72, 62, 44, 215);
		c.background = ofColor(255, 248, 232, 50);
		c.warning = ofColor(170, 48, 28, 240);
		c.shadow = ofColor(255, 250, 238, 150);
		c.buttonText = ofColor(238, 228, 200, 240); // light labels on the day skin's dark button faces
	}
	return c;
}

// Caption text colour: the informational `caption` token while the widget
// is showing live data (`live` present, rendered, not an ambient stand-in);
// the recessed `muted` treatment when its caption stands alone over no data
// (Loading, or a scene that does not publish this value).
inline const ofColor& captionColor(const HudWidgetColors& c, const HudWidgetRoleValue* live) {
	return (live && live->shouldRender && !live->useAmbientFallback) ? c.caption : c.muted;
}

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
