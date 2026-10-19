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

	// Progress Label (HUD & Scene Experience v0.3, media slot): when a
	// "progress" role is bound, draw the title ("text" role) plus a
	// horizontal bar and the right-aligned suffix ("0:42 / 1:30"). Two lines
	// when the region is tall enough (title above, bar + time below), else
	// one line (title, bar, time). A missing progress value (unknown duration
	// / nothing loaded) draws no bar at all, so the slot never shows a
	// misleading 0%; the title still shows.
	if (const auto* progressRole = input.findRole("progress")) {
		const float pad = wu(bounds, 4.0f);
		const float left = bounds.x + pad;
		const float right = bounds.getRight() - pad;
		const bool twoLines = bounds.height >= kBitmapTwoLineMinHeight;
		const float titleBaseline = twoLines ? captionBaselineY(bounds, wu(bounds, 12.0f)) : bounds.y + bounds.height * 0.65f;
		const float barBaseline = twoLines ? bottomBaselineY(bounds, wu(bounds, 6.0f)) : titleBaseline;
		const float barMidY = barBaseline - kBitmapCapHeight * 0.5f;

		float barLeft = left;
		float barRight = right;
		const auto* suffixRole = input.findRole("suffix");
		if (suffixRole && suffixRole->shouldRender && !suffixRole->formattedText.empty()) {
			const auto& t = textCache_.truncateToWidth(suffixRole->formattedText, bounds.width * 0.5f);
			const float x = right - t.widthLocal;
			ofSetColor(scaledAlpha(colors.caption, opacity));
			drawText(t.text, x, barBaseline);
			barRight = x - 12.0f;
		}

		const bool hasTitle = textRole && textRole->shouldRender && !textRole->formattedText.empty();
		if (hasTitle) {
			// One line: the title gets at most ~45% of the line, the bar the rest.
			const float budget = twoLines ? (right - left) : std::max(0.0f, std::min((right - left) * 0.45f, barRight - left - 12.0f));
			const auto& title = textCache_.truncateToWidth(textRole->formattedText, budget);
			ofSetColor(scaledAlpha(colors.primary, opacity));
			drawText(title.text, left, titleBaseline);
			if (!twoLines) barLeft = left + title.widthLocal + 12.0f;
		}

		if (progressRole->shouldRender && !progressRole->useAmbientFallback && progressRole->value.present
			&& barRight > barLeft + 8.0f) {
			const float ratio = std::clamp(progressRole->value.numberValue, 0.0f, 1.0f);
			const ofRectangle track(barLeft, barMidY - 2.0f, barRight - barLeft, 4.0f);
			ofNoFill();
			ofSetColor(scaledAlpha(colors.muted, opacity));
			ofDrawRectangle(track);
			ofFill();
			ofSetColor(scaledAlpha(colors.accent, opacity));
			ofDrawRectangle(track.x, track.y, track.width * ratio, track.height);
		}
		ofPopStyle();
		return;
	}

	// Icon-only control ("icon:<id>" vocabulary value): a square reserved
	// icon slot centred in the region, no text.
	if (text.rfind("icon:", 0) == 0) {
		const float side = std::max(8.0f, std::min(bounds.height - 12.0f, 28.0f));
		const ofRectangle square(bounds.getCenter().x - side * 0.5f, bounds.getCenter().y - side * 0.5f, side, side);
		drawReservedIcon(text.substr(5), square, scaledAlpha(colors.primary, opacity));
		ofPopStyle();
		return;
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
	const float baseline = bounds.y + bounds.height * 0.65f;

	// Optional dynamic suffix (scene title "  2/5"): reserved first so the
	// main text truncates around it and the suffix itself is never cut.
	const auto* suffixRole = input.findRole("suffix");
	const bool hasSuffix = suffixRole && suffixRole->shouldRender && !suffixRole->formattedText.empty();
	float suffixWidth = 0.0f;
	if (hasSuffix) {
		suffixWidth = textCache_.truncateToWidth(suffixRole->formattedText, localMaxWidth).widthLocal + kSuffixGap;
	}
	const auto& truncated = textCache_.truncateToWidth(text, std::max(0.0f, localMaxWidth - suffixWidth));

	ofSetColor(scaledAlpha(colors.primary, opacity));
	drawText(truncated.text, bounds.x + pad, baseline, scale);
	if (hasSuffix) {
		const auto& suffix = textCache_.truncateToWidth(suffixRole->formattedText, localMaxWidth);
		ofSetColor(scaledAlpha(colors.caption, opacity)); // navigation metadata = informational caption
		drawText(suffix.text, bounds.x + pad + truncated.widthLocal + kSuffixGap, baseline, scale);
	}

	ofPopStyle();
}

} // namespace hudpresent
