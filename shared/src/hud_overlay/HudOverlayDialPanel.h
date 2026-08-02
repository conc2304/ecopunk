#pragma once
#include "HudOverlayDialState.h"
#include "ofxGui.h"

namespace hudoverlay {

// Owns the ofxPanel/ofParameter bindings for the four master dials and
// writes them into a HudOverlayDialState each frame — the only class in
// this module that knows about ofxGui, mirroring temporal-fields'
// TFParameterPanel split (GUI concerns stay out of HudOverlayLayer, which
// only ever reads a plain HudOverlayDialState).
class HudOverlayDialPanel {
public:
	// initial seeds the panel's starting slider values — defaults to
	// HudOverlayDialState's own defaults, preserving every existing call
	// site's behavior. A caller with a specific dial posture in mind (e.g.
	// fragment-trail's low-intensity/high-discipline/low-coupling brief)
	// passes it explicitly instead of starting at generic mid-values.
	void setup(const HudOverlayDialState& initial = HudOverlayDialState{});
	void update(HudOverlayDialState& outDials);
	void draw();
	void toggleVisible() { visible = !visible; }
	bool isVisible() const { return visible; }

private:
	ofParameterGroup rootGroup;
	ofParameter<float> intensityParam;
	ofParameter<float> disciplineParam;
	ofParameter<float> eventCouplingParam;
	ofParameter<int> paletteParam; // 0 Mono / 1 Orange / 2 Lime
	ofxPanel panel;
	bool visible = true;
};

} // namespace hudoverlay
