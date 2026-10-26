#pragma once

// Development-only seam for the HUD Runtime Live Typography Playground.
//
// When (and only when) an owner installs an implementation through
// HudWireframeRenderer::setLabelTypography(), LabelWidget offers each plain
// text label to it before drawing its own native bitmap text. The
// implementation draws the label itself (returning true) or declines
// (returning false -> unchanged bitmap path). With nothing installed — the
// production default — no label behaves differently in any way.
//
// It never decides geometry: it receives the region's canonical bounds,
// the already-resolved label text and the label's opacity (disabled
// controls are dimmed), exactly as LabelWidget would have used them.

#include "ofColor.h"
#include "ofRectangle.h"

#include <string>

namespace hudpresent {

class IHudLabelTypography {
public:
	virtual ~IHudLabelTypography() = default;
	virtual bool drawLabel(const std::string& regionId, const ofRectangle& bounds, const std::string& text, float opacity) const = 0;
	// Every other native HUD text draw (HudWidgetDrawUtils drawText /
	// drawTextScaled) in the region being drawn: x / baseline y as the bitmap
	// path would use them, the caller's scale and current colour. Return false
	// to keep the native bitmap text.
	virtual bool drawText(const std::string& /*regionId*/, const std::string& /*text*/, float /*x*/, float /*y*/,
		float /*nativeScale*/, const ofColor& /*color*/) const { return false; }
	// Width drawText() would produce for `text` at `nativeScale` in this
	// region, or < 0 when this hook leaves the text native (caller measures).
	virtual float measureText(const std::string& /*regionId*/, const std::string& /*text*/, float /*nativeScale*/) const { return -1.0f; }
	// Line height multi-line text should use in this region (native bitmap
	// line = `nativeLine`); the base class leaves it unchanged.
	virtual float lineHeight(const std::string& /*regionId*/, float nativeLine) const { return nativeLine; }
};

// Dev-only: set by HudWireframeRenderer::draw() for the duration of its draw
// (nullptr otherwise / in production), with the region currently drawing.
inline const IHudLabelTypography*& devTextHook() { static const IHudLabelTypography* h = nullptr; return h; }
inline std::string& devTextRegion() { static std::string r; return r; }

} // namespace hudpresent
