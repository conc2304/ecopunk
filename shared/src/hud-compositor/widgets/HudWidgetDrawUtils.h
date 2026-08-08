#pragma once

// ============================================================================
// HudWidgetDrawUtils.h — small drawing helpers shared by every compositor
// widget. Deliberately mirrors shared/src/hud/shared/HudUtils.h's
// sx/sy/su/scaledAlpha/drawTextFallback shape closely — reused as visual/
// algorithmic PRECEDENT (per this task's §7 instruction), re-implemented
// here rather than #include-d, since this widget layer must not take a
// dependency on shared/src/hud/ (a scene-local library, per the frozen
// constraints this task's compositor widgets must stay independent of).
//
// Text: like shared/src/hud/, every widget here draws through
// ofDrawBitmapString (via drawText() below) — no ofTrueTypeFont loading.
//
// V1 TYPOGRAPHY STRATEGY (Architecture-Closure Session — see
// docs/reviews/hud-typography-metrics-v1.md for the full metrics this
// decision produces): EXPLICIT PIXEL-SPACE, fixed-native-size bitmap
// typography. Every glyph renders at OF's fixed bitmap font's own native
// size (8x13 px/glyph, unscaled — verified by direct inspection of
// libs/openFrameworks/graphics/ofBitmapFont.cpp's glyph byte arrays, same
// constant widgets/HudTextMetricsCache.h's kGlyphWidthLocal uses) —
// UNCONDITIONALLY, never adjusted by any matrix transform. This was
// already the de facto behavior of every widget below before this
// decision (see drawText()'s own comment for why), so choosing it
// formally as the v1 strategy changes zero rendered pixels in any
// existing baseline; it makes the ALREADY-true behavior the documented,
// intentional one instead of an accidental side effect of an inert
// ofScale() call. Truncation/ellipsis (HudTextMetricsCache) budgets
// against this same fixed native size, never a matrix-scale factor —
// see LabelWidget.cpp's own comment for the bug this closes.
// ============================================================================

#include "ofGraphics.h"
#include "ofMath.h"
#include "ofPolyline.h"
#include "ofRectangle.h"

#include <algorithm>
#include <cmath>

namespace hudpresent {

inline float wx(const ofRectangle& b, float valueAtBaseWidth, float baseWidth = 200.0f) {
	return valueAtBaseWidth * (b.width / baseWidth);
}

inline float wy(const ofRectangle& b, float valueAtBaseHeight, float baseHeight = 60.0f) {
	return valueAtBaseHeight * (b.height / baseHeight);
}

inline float wu(const ofRectangle& b, float valueAtBase, float base = 120.0f) {
	return valueAtBase * (std::min(b.width, b.height) / base);
}

inline ofColor withAlpha(ofColor c, float alpha01) {
	c.a = static_cast<unsigned char>(std::clamp(alpha01, 0.0f, 1.0f) * 255.0f);
	return c;
}

inline ofColor scaledAlpha(ofColor c, float scale) {
	c.a = static_cast<unsigned char>(std::clamp((c.a / 255.0f) * scale, 0.0f, 1.0f) * 255.0f);
	return c;
}

// `scale` is retained ONLY for call-site self-documentation (every widget
// below still passes a wu()-derived value expressing "how prominent is
// this text meant to be relative to its region") and API stability across
// this widget layer's ~25 call sites — it has NO effect on rendered glyph
// size in the v1 fixed-native-pixel strategy (see this file's header
// comment) and never has: openFrameworks' DEFAULT drawBitmapMode on
// desktop is OF_BITMAPMODE_MODEL_BILLBOARD (ofGraphicsBaseTypes.h's
// ofStyle constructor), which billboards the TRANSLATION of the current
// model transform to find an on-screen anchor point, then draws glyphs at
// native size — and since this function always draws at LOCAL origin
// (0,0) after translating to (x,y), scaling the matrix around that same
// local origin cannot move it either (scaling fixes the origin in place).
// This was previously implemented with a real `ofScale(scale, scale)`
// call left in — a no-op in this exact translate-then-draw-at-origin
// pattern, confirmed both by this reasoning and by a real Validation
// Studio screenshot (real_typography_overflow) that caught the resulting
// truncation-budget bug (see LabelWidget.cpp's own comment). Removed
// here, formally, as this domain's Architecture-Closure Session
// typography decision — not silently kept as dead/misleading code.
inline void drawText(const std::string& text, float x, float y, float scale = 1.0f) {
	(void)scale; // see this function's own comment — intentionally unused in v1
	ofPushMatrix();
	ofTranslate(x, y);
	ofDrawBitmapString(text, 0, 0);
	ofPopMatrix();
}

// A thin single-line frame — every widget here uses this instead of the
// full FrameStyle machinery shared/src/hud/'s HudFrameRenderer offers, per
// this increment's "minimal wireframe" scope.
inline void drawOutline(const ofRectangle& b, ofColor c) {
	ofPushStyle();
	ofNoFill();
	ofSetColor(c);
	ofSetLineWidth(1.0f);
	ofDrawRectangle(b);
	ofPopStyle();
}

} // namespace hudpresent
