#include "LabelWidget.h"

#include "HudWidgetDrawUtils.h"

namespace hudpresent {

void LabelWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	// Engineering Session 2: the three-state control distinction
	// (unsupported / supported-but-disabled / supported-and-enabled) is
	// resolved upstream (HudRealFrameResolver.cpp's control.* handling)
	// and arrives here as: no "enabled" role bound at all (this binding
	// never asked for one) -> not a control, ignore; role bound but
	// shouldRender==false (the resolver returned missing -> Hide policy)
	// -> UNSUPPORTED, hide the whole label, not just dim it; role bound
	// and shouldRender==true with boolValue==false -> DISABLED, dim;
	// boolValue==true -> ENABLED, full opacity. This is the ONE place
	// that distinction is rendered — the resolver only ever hands back
	// typed present/missing values, never a widget-facing tri-state enum.
	const auto* enabledRole = input.findRole("enabled");
	if (enabledRole && !enabledRole->shouldRender) {
		return; // unsupported — no ofPushStyle() was opened yet, nothing to pop
	}

	ofPushStyle();
	ofEnableAlphaBlending();

	std::string text = input.captionText;
	const auto* textRole = input.findRole("text");
	if (textRole && textRole->shouldRender && !textRole->formattedText.empty()) {
		text = textRole->formattedText;
	}

	float opacity = 1.0f;
	if (enabledRole && enabledRole->shouldRender && !enabledRole->value.boolValue) {
		opacity = 0.35f; // supported but currently disabled
	}

	if (text.empty() && !input.regionAmbientFallback) {
		ofPopStyle();
		return;
	}

	float pad = wu(bounds, 4.0f);
	float scale = wu(bounds, 0.9f, 120.0f);
	// Deterministic truncation/ellipsis (this task's §11): measured
	// directly in real on-screen PIXELS, so a region narrower than the
	// resolved text degrades to "STATE NA..." instead of overflowing past
	// the region's own bounds (see the probe's LongestStrings finding
	// this closes) or being silently clipped by GL scissoring this widget
	// layer doesn't use.
	//
	// NOT divided by `scale`, even though drawText() below still passes
	// it: this session's own real_typography_overflow Validation Studio
	// screenshot caught text overflowing uncut, and direct inspection of
	// libs/openFrameworks/gl/ofGLProgrammableRenderer.cpp's drawString()
	// found why — OF's DEFAULT drawBitmapMode on desktop is
	// OF_BITMAPMODE_MODEL_BILLBOARD (ofGraphicsBaseTypes.h's ofStyle
	// constructor), which billboards only the TRANSLATION of the current
	// model transform to find where to anchor the string on screen, then
	// draws the glyphs at their native, UNSCALED pixel size regardless of
	// any ofScale() in effect — so drawText()'s `ofScale(scale, scale)`
	// has never actually changed rendered glyph size for ANY widget in
	// this codebase that computes a non-1.0 scale (a real, wireframe-
	// acceptance-relevant finding beyond just this widget — see this
	// session's review doc). Given text has therefore always rendered at
	// kGlyphWidthLocal's true native pixel width per character (already
	// screenshot-verified across every existing baseline), matching the
	// truncation budget to that SAME real behavior — not the intended-
	// but-inert `scale` factor — is the correct fix here without changing
	// how any text visually looks (out of scope: making `scale` actually
	// work is a wider, "final skin typography"-adjacent change this
	// wireframe-acceptance task is not scoped to make).
	float localMaxWidth = std::max(0.0f, bounds.width - pad * 2.0f);
	const auto& truncated = textCache_.truncateToWidth(text, localMaxWidth);

	ofSetColor(scaledAlpha(colors.primary, opacity));
	drawText(truncated.text, bounds.x + pad, bounds.y + bounds.height * 0.65f, scale);

	ofPopStyle();
}

} // namespace hudpresent
