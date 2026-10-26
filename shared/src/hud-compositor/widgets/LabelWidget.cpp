#include "LabelWidget.h"

#include "HudWidgetDrawUtils.h"

#include <cmath>
#include <vector>
#include "HudLabelTypography.h"

namespace hudpresent {

namespace {
constexpr float kBitmapLineHeightLog = 13.0f; // native bitmap line, activity-log variant
}

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
		const float pad = containerPad(bounds);
		const float left = bounds.x + pad;
		const float right = bounds.getRight() - pad;
		const bool twoLines = bounds.height >= kBitmapTwoLineMinHeight;
		// Title 3 px lower than the caption line (and centred, below).
		const float titleBaseline = (twoLines ? captionBaselineY(bounds, wu(bounds, 12.0f)) : bounds.y + bounds.height * 0.65f) + 3.0f;
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
			// Two lines: the media title is centred in its container.
			const float titleX = twoLines ? std::max(left, bounds.getCenter().x - hudTextWidth(title.text, title.widthLocal) * 0.5f) : left;
			drawText(title.text, titleX, titleBaseline);
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

	// "centered" variant (overlay phase "IDLE"): the text centred horizontally
	// in its slot, on the standard label baseline.
	if (input.variantId == "centered") {
		const float padC = containerPad(bounds);
		const auto& t = textCache_.truncateToWidth(text, std::max(0.0f, bounds.width - padC * 2.0f));
		ofSetColor(scaledAlpha(colors.primary, opacity));
		drawText(t.text, bounds.getCenter().x - hudTextWidth(t.text, t.widthLocal) * 0.5f, bounds.y + bounds.height * 0.65f);
		ofPopStyle();
		return;
	}

	// "modeline" variant (universal.media_title): the scene mode on ONE line,
	// "Mode: <name>   x/y" — caption + name in primary, the position in the
	// caption colour after a three-glyph gap. Drawn through drawText so the
	// dev typography playground restyles it like any other info text.
	if (input.variantId == "modeline") {
		const float padM = containerPad(bounds);
		const float availM = std::max(0.0f, bounds.width - padM * 2.0f);
		const std::string line = input.captionText.empty() ? text : input.captionText + " " + text;
		const auto* posRole = input.findRole("suffix");
		const bool hasPos = posRole && posRole->shouldRender && !posRole->formattedText.empty();
		const float gap = HudTextMetricsCache::kGlyphWidthLocal * 3.0f;
		const float posW = hasPos ? hudTextWidth(posRole->formattedText, textCache_.truncateToWidth(posRole->formattedText, availM).widthLocal) : 0.0f;
		const auto& lineT = textCache_.truncateToWidth(line, std::max(0.0f, availM - posW - (hasPos ? gap : 0.0f)));
		const float baselineM = bounds.getCenter().y + kBitmapCapHeight * 0.5f;
		ofSetColor(scaledAlpha(colors.primary, opacity));
		drawText(lineT.text, bounds.x + padM, baselineM);
		if (hasPos) {
			ofSetColor(scaledAlpha(colors.caption, opacity));
			drawText(posRole->formattedText, bounds.x + padM + hudTextWidth(lineT.text, lineT.widthLocal) + gap, baselineM);
		}
		ofPopStyle();
		return;
	}

	// "log" variant (overlay.transition): the most recent activity-log lines
	// (newline-separated, oldest first), truncated per line, as many as fit
	// (1-4) at the current text size. Caption colour: informational.
	if (input.variantId == "log") {
		const float padL = containerPad(bounds);
		std::vector<std::string> all;
		for (size_t start = 0; start <= text.size();) {
			const size_t nl = text.find('\n', start);
			const std::string one = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
			if (!one.empty()) all.push_back(one);
			if (nl == std::string::npos) break;
			start = nl + 1;
		}
		const float lineH = std::max(8.0f, hudLineHeight(kBitmapLineHeightLog));
		const int fit = static_cast<int>(std::floor((bounds.height - padL * 2.0f) / lineH));
		const size_t n = std::min(all.size(), static_cast<size_t>(std::clamp(fit, 1, 4)));
		const float avail = std::max(0.0f, bounds.width - padL * 2.0f);
		ofSetColor(scaledAlpha(colors.caption, opacity));
		for (size_t i = 0; i < n; ++i) {
			std::string line = all[all.size() - n + i];
			// A long "Module: message" would be cut inside the module name and
			// hide the message: when it doesn't fit, show the message part.
			if (textCache_.truncateToWidth(line, 1e5f).widthLocal > avail) {
				const size_t colon = line.find(": ");
				if (colon != std::string::npos) line = line.substr(colon + 2);
			}
			const auto& t = textCache_.truncateToWidth(line, avail);
			drawText(t.text, bounds.x + padL, bounds.y + padL + lineH * float(i) + lineH * 0.8f);
		}
		ofPopStyle();
		return;
	}

	// Development-only Live Typography Playground: offered first, declines
	// for anything it doesn't target (null in production -> no change).
	float pad = containerPad(bounds);
	// Scene title's "[Scene X/Y]" suffix: small, no glow, right-aligned in
	// the container (drawn by both the playground and the native paths).
	constexpr float kSuffixScale = 1.05f; // [Scene X/Y]: +~4 px over the previous 0.75x (13 px glyph: 9.75 -> 13.7 px)
	const auto* titleSuffix = input.variantId == "title" ? input.findRole("suffix") : nullptr;
	auto drawTitleSuffix = [&]() {
		if (!titleSuffix || !titleSuffix->shouldRender || titleSuffix->formattedText.empty()) return;
		const auto& suffix = textCache_.truncateToWidth(titleSuffix->formattedText, bounds.width * 0.4f / kSuffixScale);
		const float w = hudTextWidth(suffix.text, suffix.widthLocal, kSuffixScale);
		ofSetColor(scaledAlpha(colors.caption, opacity));
		drawTextScaled(suffix.text, bounds.getRight() - pad - w, bounds.getCenter().y + kBitmapCapHeight * kSuffixScale * 0.5f, kSuffixScale);
	};
	if (input.labelTypography != nullptr && !text.empty()
		&& input.labelTypography->drawLabel(input.regionId, bounds, text, opacity)) {
		drawTitleSuffix();
		ofPopStyle();
		return;
	}

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

	// "title" variant (scene title): 1.5x glyphs, 10 px left padding,
	// vertically centred. Widths are native widths * kTitleScale.
	if (input.variantId == "title") {
		constexpr float kTitleScale = 1.5f;
		constexpr float kTitlePadLeft = 10.0f;
		const float left = bounds.x + kTitlePadLeft;
		const float maxLocal = std::max(0.0f, bounds.getRight() - pad - left) / kTitleScale;
		const float titleBaseline = bounds.getCenter().y + kBitmapCapHeight * kTitleScale * 0.5f;
		const bool hasSuffix = titleSuffix && titleSuffix->shouldRender && !titleSuffix->formattedText.empty();
		const float suffixReserve = hasSuffix
			? (textCache_.truncateToWidth(titleSuffix->formattedText, maxLocal).widthLocal * kSuffixScale + kSuffixGap) / kTitleScale : 0.0f;
		const auto& title = textCache_.truncateToWidth(text, std::max(0.0f, maxLocal - suffixReserve));
		ofSetColor(scaledAlpha(colors.primary, opacity));
		drawTextScaled(title.text, left, titleBaseline, kTitleScale);
		drawTitleSuffix();
		ofPopStyle();
		return;
	}

	// Nav buttons ("button" variant, HUD-006 v2 72x72 squares): stacked label,
	// one word per line ("PREV" / "SCENE"), each line centred; the block is
	// centred vertically. One word -> one centred line.
	if (input.variantId == "button") {
		std::vector<std::string> lines; // one word per line
		{
			std::string word;
			for (char ch : text + " ") {
				if (ch == ' ') { if (!word.empty()) lines.push_back(word); word.clear(); }
				else word += ch;
			}
			if (lines.empty()) lines.push_back(text);
		}
		constexpr float kLineGap = 6.0f;
		const float blockH = kBitmapCapHeight * float(lines.size()) + kLineGap * float(lines.size() - 1);
		float baseline = bounds.getCenter().y - blockH * 0.5f + kBitmapCapHeight;
		ofSetColor(scaledAlpha(colors.buttonText, opacity));
		const ofColor panelShadow = hudTextShadowColor();
		hudTextShadowColor() = colors.buttonShadow; // dark over the baked faces; palette shadow over skin faces
		for (const auto& line : lines) {
			const auto& t = textCache_.truncateToWidth(line, localMaxWidth);
			const float w = hudTextWidth(t.text, t.widthLocal);
			drawText(t.text, bounds.getCenter().x - w * 0.5f, baseline);
			baseline += kBitmapCapHeight + kLineGap;
		}
		hudTextShadowColor() = panelShadow;
		ofPopStyle();
		return;
	}

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
