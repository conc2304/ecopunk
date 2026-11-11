#include "LabelWidget.h"

#include "HudWidgetDrawUtils.h"
#include "HudIconDraw.h"

#include <cmath>
#include <vector>
#include "HudLabelTypography.h"

namespace hudpresent {

namespace {
constexpr float kBitmapLineHeightLog = 13.0f; // native bitmap line, activity-log variant

// Faux bold: two passes 1 px apart. Their alphas compound where they overlap, which would make the
// text brighter than other primary text, so each pass uses a' = 1 - sqrt(1 - a): the overlap lands
// on the colour's own alpha (the mode name and the media file name share this exactly).
void drawBoldText(const std::string& text, float x, float y, ofColor color) {
	color.a = static_cast<unsigned char>(255.0f * (1.0f - std::sqrt(1.0f - color.a / 255.0f)) + 0.5f);
	ofSetColor(color);
	drawText(text, x, y);
	drawText(text, x + 1.0f, y);
}
}

void LabelWidget::draw(const ofRectangle& boundsIn, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	ofRectangle bounds = boundsIn; // icon + text mode lowers the text area below
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
		const float titleDrawBaseline = titleBaseline + 6.0f; // file title sits 6 px lower; the bar and sample keep titleBaseline
		const float barBaseline = twoLines ? bottomBaselineY(bounds, wu(bounds, 6.0f)) : titleBaseline;
		const float barMidY = barBaseline - kBitmapCapHeight * 0.5f;

		float barLeft = left;
		float barRight = right;
		const auto* suffixRole = input.findRole("suffix");
		if (suffixRole && suffixRole->shouldRender && !suffixRole->formattedText.empty()) {
			const auto& t = textCache_.truncateToWidth(suffixRole->formattedText, bounds.width * 0.5f);
			const float x = right - hudTextWidth(t.text, t.widthLocal);
			ofSetColor(scaledAlpha(colors.caption, opacity));
			drawText(t.text, x, barBaseline);
			barRight = x - 12.0f;
		}

		// "[Sample XX/YY]": upper right, above the clip time (two-line layout only),
		// 3 px smaller than the native 13 px line and 1 px lower than the title row.
		constexpr float kSampleScale = 10.0f / 13.0f;
		float sampleReserve = 0.0f;
		if (const auto* sampleRole = input.findRole("sample")) {
			if (twoLines && sampleRole->shouldRender && !sampleRole->formattedText.empty()) {
				const auto& t = textCache_.truncateToWidth(sampleRole->formattedText, (right - left) * 0.4f / kSampleScale);
				ofSetColor(scaledAlpha(colors.caption, opacity));
				const float sampleW = hudTextWidth(t.text, t.widthLocal, kSampleScale);
				drawTextScaled(t.text, right - sampleW, titleBaseline + 1.0f, kSampleScale);
				sampleReserve = sampleW + 12.0f;
			}
		}

		const bool hasTitle = textRole && textRole->shouldRender && !textRole->formattedText.empty();
		if (hasTitle) {
			// One line: the title gets at most ~45% of the line, the bar the rest.
			const float budget = twoLines ? std::max(0.0f, right - left - sampleReserve) : std::max(0.0f, std::min((right - left) * 0.45f, barRight - left - 12.0f));
			const auto& title = textCache_.truncateToWidth(textRole->formattedText, budget);
			// The media file label is left-aligned and bold, like the mode name above it.
			const float titleX = left;
			drawBoldText(title.text, titleX, titleDrawBaseline, scaledAlpha(colors.primary, opacity));
			if (!twoLines) barLeft = left + hudTextWidth(title.text, title.widthLocal) + 12.0f;
		}

		if (progressRole->shouldRender && !progressRole->useAmbientFallback && progressRole->value.present
			&& barRight > barLeft + 8.0f) {
			const float ratio = std::clamp(progressRole->value.numberValue, 0.0f, 1.0f);
			const ofRectangle track(barLeft, barMidY - 2.0f, barRight - barLeft, 4.0f);
			ofNoFill();
			ofSetColor(scaledAlpha(colors.muted, opacity));
			ofDrawRectangle(track);
			ofFill();
			ofSetColor(scaledAlpha(colors.primary, opacity));
			ofDrawRectangle(track.x, track.y, track.width * ratio, track.height);
		}

		// Automatic mode: minimal bar spanning the bottom of the widget, filling
		// toward the next scene/sample. The hold source is absent in manual mode.
		const auto* holdRole = input.findRole("hold");
		if (holdRole && holdRole->shouldRender && !holdRole->useAmbientFallback && holdRole->value.present) {
			const float ratio = std::clamp(holdRole->value.numberValue, 0.0f, 1.0f);
			constexpr float kHoldBarHeight = 2.0f;
			ofSetColor(scaledAlpha(colors.muted, opacity * 0.5f));
			ofDrawRectangle(bounds.x, bounds.getBottom() - kHoldBarHeight, bounds.width, kHoldBarHeight);
			ofSetColor(scaledAlpha(colors.primary, opacity));
			ofDrawRectangle(bounds.x, bounds.getBottom() - kHoldBarHeight, bounds.width * ratio, kHoldBarHeight);
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

	// "selector" variant (a compact one-line option row, drawn at native 8 px glyphs / one
	// scale so two of them fit a 43.2 px region):
	//   CAPTION  < VALUE                     > x/y
	// caption = the binding's static label (left, caption colour), VALUE = "text" role
	// (primary colour, truncated to the remaining budget), x/y = "suffix" role (right-aligned,
	// caption colour). The < > chevrons follow input.selectorState (0 none, 1 dim, 2 emphasised);
	// the columns are always reserved so the row never shifts when the group becomes active.
	// Scene-agnostic: it knows nothing about which scene or axis it shows.
	if (input.variantId == "selector") {
		const float padS = containerPad(bounds);
		const float gw = HudTextMetricsCache::kGlyphWidthLocal;
		const float left = bounds.x + padS;
		const float right = bounds.getRight() - padS;
		const float baselineS = bounds.getCenter().y + kBitmapCapHeight * 0.5f;
		constexpr float kCaptionCols = 8.0f; // "PALETTE " is the longest caption
		const auto* posRole = input.findRole("suffix");
		const std::string posText = (posRole && posRole->shouldRender) ? posRole->formattedText : std::string();
		const float posW = posText.empty() ? 0.0f : hudTextWidth(posText, textCache_.truncateToWidth(posText, bounds.width).widthLocal);
		if (!input.captionText.empty()) {
			const auto& cap = textCache_.truncateToWidth(input.captionText, gw * kCaptionCols);
			ofSetColor(scaledAlpha(colors.caption, opacity));
			drawText(cap.text, left, baselineS);
		}
		const float arrowW = gw * 2.0f;
		const float valueX = left + gw * kCaptionCols + arrowW;
		const float posX = right - posW;
		const float valueRight = posX - gw * 1.0f - arrowW; // " > " + gap before the position
		const auto& val = textCache_.truncateToWidth(text, std::max(0.0f, valueRight - valueX));
		ofSetColor(scaledAlpha(colors.primary, opacity));
		drawText(val.text, valueX, baselineS);
		if (input.selectorState != 0) {
			const bool strong = input.selectorState >= 2;
			ofSetColor(scaledAlpha(strong ? colors.accent : colors.muted, opacity));
			drawText("<", valueX - arrowW, baselineS);
			drawText(">", posX - gw * 1.0f - arrowW + gw, baselineS);
			if (strong) { // non-colour cue: emphasised chevrons are drawn twice, 1 px apart (heavier stroke)
				drawText("<", valueX - arrowW + 1.0f, baselineS);
				drawText(">", posX - gw * 1.0f - arrowW + gw + 1.0f, baselineS);
			}
		}
		if (!posText.empty()) {
			ofSetColor(scaledAlpha(colors.caption, opacity));
			drawText(posText, posX, baselineS);
		}
		ofPopStyle();
		return;
	}

	// "modeline" variant (universal.media_title): laid out like the scene title
	// row — "<Mode name>" left-aligned in primary, "[Mode x/y]" right-aligned in
	// the caption colour. Drawn through drawText so the dev typography playground
	// restyles it like any other info text.
	if (input.variantId == "modeline") {
		const float padM = containerPad(bounds);
		const float availM = std::max(0.0f, bounds.width - padM * 2.0f);
		const auto* posRole = input.findRole("suffix");
		const bool hasPos = posRole && posRole->shouldRender && !posRole->formattedText.empty();
		std::string word = input.captionText; // "Mode:" -> "Mode"
		while (!word.empty() && word.back() == ':') word.pop_back();
		const std::string posText = hasPos ? "[" + word + (word.empty() ? "" : " ") + posRole->formattedText + "]" : std::string();
		const float gap = HudTextMetricsCache::kGlyphWidthLocal * 3.0f;
		const float posW = hasPos ? hudTextWidth(posText, textCache_.truncateToWidth(posText, availM).widthLocal) : 0.0f;
		const auto& nameT = textCache_.truncateToWidth(text, std::max(0.0f, availM - posW - (hasPos ? gap : 0.0f)));
		// Centre the glyph box, but never let a larger restyled font rise above the region top.
		const float lineH = hudLineHeight(13.0f);
		const float baselineM = std::max(bounds.getCenter().y + kBitmapCapHeight * 0.5f, bounds.y + padM + lineH * 0.85f) + 6.0f; // lower in its panel, clear of the panel art's bottom edge
		drawBoldText(nameT.text, bounds.x + padM, baselineM, scaledAlpha(colors.primary, opacity));
		if (hasPos) {
			ofSetColor(scaledAlpha(colors.caption, opacity));
			drawText(posText, bounds.getRight() - padM - posW, baselineM);
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

	// Navigation-button icons (Settings "Button labels"): icon only, or the icon above the
	// text. A button without an icon keeps its text label whatever the mode.
	if (input.variantId == "button" && colors.buttonLabelMode != 0) {
		bool mirrored = false;
		if (const char* d = hudicon::buttonIconPath(input.regionId, mirrored)) {
			auto it = iconPaths_.find(input.regionId);
			if (it == iconPaths_.end()) {
				std::vector<hudicon::Contour> contours;
				ofPath path;
				if (hudicon::parse(d, contours)) {
					if (mirrored) hudicon::mirrorX(contours);
					buildIconPath(path, contours);
				}
				it = iconPaths_.emplace(input.regionId, std::move(path)).first;
			}
			const bool both = colors.buttonLabelMode == 2;
			const float size = both ? 22.0f : 34.0f;
			const float cy = both ? bounds.y + 8.0f + size * 0.5f : bounds.getCenter().y;
			const ofRectangle box(bounds.getCenter().x - size * 0.5f, cy - size * 0.5f, size, size);
			ofRectangle shadowBox = box;
			shadowBox.x += 1.0f;
			shadowBox.y += 1.0f;
			drawIconPath(it->second, shadowBox, scaledAlpha(colors.buttonShadow, opacity * colors.iconOpacity));
			drawIconPath(it->second, box, scaledAlpha(colors.buttonText, opacity * colors.iconOpacity));
			if (!both) { ofPopStyle(); return; }
			bounds.y += 18.0f; // the text block sits in the lower part of the face
			bounds.height -= 18.0f;
		}
	}

	// Development-only Live Typography Playground: offered first, declines
	// for anything it doesn't target (null in production -> no change).
	float pad = containerPad(bounds);
	// Scene title's "[EXP X/Y]" suffix: small, no glow, right-aligned in
	// the container (drawn by both the playground and the native paths).
	constexpr float kSuffixScale = 1.05f; // [EXP X/Y]: +~4 px over the previous 0.75x (13 px glyph: 9.75 -> 13.7 px)
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
