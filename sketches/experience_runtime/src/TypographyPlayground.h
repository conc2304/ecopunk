#pragma once

// TypographyPlayground — DEVELOPMENT-ONLY live typography preview for the
// HUD navigation-button labels (HUD Runtime Live Typography Playground).
//
// Disabled unless EXPERIENCE_RUNTIME_TYPOGRAPHY_PLAYGROUND=1. When disabled
// it is never constructed: no keys are intercepted, no files are read and
// LabelWidget draws its native bitmap labels exactly as before.
//
// Files (relative to EXPERIENCE_RUNTIME_TYPOGRAPHY_DIR, default
// <repo>/assets/hud/dev-typography):
//   fonts.json   font registry (display name + path relative to this
//                folder), colour presets, target regions — edit + 'r' to
//                pick up a new font without recompiling
//   preset.json  the saved style ('w' writes it, 'r' reloads it)
//   fonts/       drop .ttf/.otf files here
//
// Rendering: openFrameworks' ofTrueTypeFont, cached per (file, pixel size).
// Fonts load only on a key press / reload — never inside draw(). Colour,
// letter spacing and glow reuse the cached glyphs. Labels are centred on
// the canonical HUD-006 control bounds using the real glyph bounding box.

#include "HudLabelTypography.h"

#include "ofColor.h"
#include "ofTrueTypeFont.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

class TypographyPlayground : public hudpresent::IHudLabelTypography {
public:
	static bool isRequested();

	// Reads fonts.json + preset.json from `directory` and preloads the
	// preset's font. Safe to call again (that is what 'r' does).
	void load(const std::string& directory);

	// Playground shortcuts only (all keys unbound elsewhere in the runtime);
	// returns false for every other key so normal input is untouched.
	bool handleKey(int key);
	// Production use: apply the saved preset only. No key handling, no overlay.
	void setApplyOnly() { applyOnly_ = true; overlayVisible_ = false; }
	// Runtime -> playground: the active skin and its text palette ("day" / "night").
	// Colour edits (`k`) and the preview then apply to that skin; a skin with no
	// colour of its own falls back to its palette's colour.
	void setSkin(const std::string& skin, const std::string& palette);

	bool drawLabel(const std::string& regionId, const ofRectangle& bounds, const std::string& text, float opacity) const override;
	bool drawText(const std::string& regionId, const std::string& text, float x, float y, float nativeScale, const ofColor& color) const override;
	float measureText(const std::string& regionId, const std::string& text, float nativeScale) const override;
	float lineHeight(const std::string& regionId, float nativeLine) const override;
	void drawOverlay() const; // dev info panel, canonical canvas coordinates

	// -- Inspection (harness / logs) ----------------------------------------------
	struct FontEntry {
		std::string name;
		std::string file;      // as written in fonts.json (relative)
		std::string path;      // resolved absolute path; empty for the built-in bitmap
		bool builtinBitmap = false;
		bool present = false;  // file exists
		bool invalid = false;  // exists but failed to load
	};
	const std::vector<FontEntry>& fonts() const { return fonts_; }
	int fontIndex() const { return fontIndex_; }
	int sizePx() const { return sizePx_; }
	float letterSpacing() const { return letterSpacing_; }
	int colorIndex() const { return colorIndex_; }
	float glow() const { return glow_; }
	int fontLoads() const { return fontLoads_; }          // successful ofTrueTypeFont::load calls (cache misses)
	int fontLoadFailures() const { return fontLoadFailures_; }
	int labelDraws() const { return labelDraws_; }
	bool dayPaletteForTesting() const { return dayPalette_; }
	// One line per group: font / size / colours, as currently applied (harness + logs).
	std::string styleSummaryForTesting() const {
		static const char* names[3] = {"buttons", "title", "info"};
		std::string out;
		for (int g = 0; g < 3; ++g) {
			const Style& s = styles_[g];
			out += std::string(names[g]) + "=" + fonts_[std::clamp(s.font, 0, int(fonts_.size()) - 1)].name + " " + std::to_string(s.size)
				+ "px night:" + colors_[std::clamp(s.color, 0, int(colors_.size()) - 1)].name
				+ " day:" + colors_[std::clamp(s.colorDay, 0, int(colors_.size()) - 1)].name;
			for (const auto& [skin, idx] : s.skin) out += " " + skin + ":" + colors_[std::clamp(idx, 0, int(colors_.size()) - 1)].name;
			out += "; ";
		}
		return out;
	}
	// Last drawn glyph box per region (canvas px), for centring checks.
	const std::map<std::string, ofRectangle>& lastLabelBoxes() const { return lastBoxes_; }
	const std::string& presetPath() const { return presetPath_; }
	bool save();      // writes preset.json; false on I/O failure
	void reload() { load(directory_); }
	// Harness-only: put the style in a known state (no font I/O here).
	void setStyleForTesting(int sizePx, float spacing, int colorIndex, float glow) {
		sizePx_ = sizePx; letterSpacing_ = spacing; colorIndex_ = colorIndex; glow_ = glow; colorEdited_ = true;
	}

private:
	struct ColorPreset {
		std::string name;
		ofColor color;
		bool hudDefault = false; // the HUD's own label colour (comparison baseline)
	};

	ofTrueTypeFont* cachedFont(int index) const;  // no I/O; nullptr if not loaded
	bool ensureFontLoaded(int index);            // I/O allowed (key/reload paths only)
	void selectFont(int direction);               // next/previous AVAILABLE font
	void logState(const std::string& why) const;

	std::string directory_;
	std::string presetPath_;
	std::vector<FontEntry> fonts_;
	std::vector<ColorPreset> colors_;
	std::set<std::string> targets_;
	std::vector<std::string> missing_;

	int fontIndex_ = 0;
	int sizePx_ = 15;
	float letterSpacing_ = 1.0f;
	int colorIndex_ = 0;
	float glow_ = 0.0f;
	bool overlayVisible_ = true; // never persisted
	bool applyOnly_ = false;
	// Three groups (Tab cycles): 0 buttons (controls.*), 1 scene title, 2 info/meta (other labels).
	// color = fallback colour under the night palette; colorDay = fallback under the
	// day palette (index 0 = "HUD default": the palette's own ink, i.e. dark on the
	// light skin); skin = per-skin colour overrides keyed by skin id. `k` edits the
	// active skin's override (see setSkin()).
	struct Style { int font = 0; int size = 15; float spacing = 1.0f; int color = 0; float glow = 0.0f; int colorDay = 0; std::map<std::string, int> skin; };
	bool dayPalette_ = false;
	std::string skin_;
	bool colorEdited_ = false; // colourIndex_ holds an edit not yet stored in the group
	int resolveColor(const Style& s) const {
		auto it = skin_.empty() ? s.skin.end() : s.skin.find(skin_);
		return it != s.skin.end() ? it->second : (dayPalette_ ? s.colorDay : s.color);
	}
	Style styles_[3];
	int group_ = 0;
	static int groupOf(const std::string& regionId);
	// Per-panel overrides inside the info/meta group (size / spacing only;
	// font, colour and glow stay group-wide). -1 = inherit the group value.
	struct PanelOverride { int size = -1; float spacing = -1.0f; };
	std::map<std::string, PanelOverride> overrides_;
	mutable std::set<std::string> infoPanels_; // info/meta regions seen while drawing (Shift+Tab order)
	std::string panel_;                        // selected info/meta panel; empty = whole group
	bool fitLayout_ = true; // 'l': never wider than the width the widget's layout reserved (native 8 px/char)
	Style effective(const std::string& regionId) const;
	bool ensureLoaded(int index, int size);
	void preloadAll(); // every group font at its size + every override size
	void storeGroup() {
		Style& s = styles_[group_];
		s.font = fontIndex_; s.size = sizePx_; s.spacing = letterSpacing_; s.glow = glow_;
		if (colorEdited_) {
			if (skin_.empty()) (dayPalette_ ? s.colorDay : s.color) = colorIndex_;
			else s.skin[skin_] = colorIndex_;
			colorEdited_ = false;
		}
	}
	void loadGroup() {
		const Style& s = styles_[group_];
		fontIndex_ = s.font; sizePx_ = s.size; letterSpacing_ = s.spacing; glow_ = s.glow;
		colorIndex_ = resolveColor(s);
	}

	// (absolute path, px) -> font; a failed load is cached as nullptr until 'r'.
	mutable std::map<std::pair<std::string, int>, std::unique_ptr<ofTrueTypeFont>> cache_;
	int fontLoads_ = 0;
	int fontLoadFailures_ = 0;
	mutable int labelDraws_ = 0;
	mutable std::map<std::string, ofRectangle> lastBoxes_;
	std::string status_; // last save/reload message for the overlay
};
