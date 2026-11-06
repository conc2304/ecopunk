#pragma once

// ============================================================================
// MinimalTheme.h — System Mode themes for the "minimal" HUD (the plain
// wireframe look: every widget and button, none of the skin artwork).
//
// OF-free and header-only (covered by test/minimal_theme_tests.cpp). A
// minimal theme is ONE primary colour (red .. violet) on a selectable
// background, in a LIGHT or a DARK group. Every other colour the HUD draws —
// text, captions, accents, outlines, panel fill, button labels, the media
// screen border — is a lighter or darker version of that primary, so a theme
// is one hex value (plus a background) and nothing else.
//
// System Mode ids (persisted in installation-settings.json "systemMode"):
//   "auto"                    day/night skin schedule (the existing behaviour)
//   "<skin id>"               a skin package, e.g. "plywood-marble"
//   "minimal.light.<hue>"     minimal HUD, light group
//   "minimal.dark.<hue>"      minimal HUD, dark group
// The default hue and background tables below can be overridden from
// assets/hud/themes/minimal-theme.json (ExperienceRuntime loads it; an absent
// or invalid file leaves these defaults). The web editor that follows writes
// that same file.
// ============================================================================

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace minimaltheme {

struct Rgb {
	uint8_t r = 0, g = 0, b = 0;
	bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
};
struct Rgba {
	uint8_t r = 0, g = 0, b = 0, a = 255;
};

inline Rgb mix(Rgb a, Rgb b, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	auto m = [&](uint8_t x, uint8_t y) { return static_cast<uint8_t>(x + (y - x) * t + 0.5f); };
	return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}
inline Rgba withAlpha(Rgb c, int a) { return {c.r, c.g, c.b, static_cast<uint8_t>(std::clamp(a, 0, 255))}; }

// "#RRGGBB" (the '#' is optional). nullopt for anything else.
inline std::optional<Rgb> parseHex(const std::string& text) {
	std::string s = text;
	if (!s.empty() && s[0] == '#') s.erase(0, 1);
	if (s.size() != 6) return std::nullopt;
	for (char c : s) if (!std::isxdigit(static_cast<unsigned char>(c))) return std::nullopt;
	const unsigned long v = std::strtoul(s.c_str(), nullptr, 16);
	return Rgb{static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v)};
}
inline std::string toHex(Rgb c) {
	char b[8];
	std::snprintf(b, sizeof(b), "#%02X%02X%02X", c.r, c.g, c.b);
	return b;
}

enum class Group { Light, Dark };
inline const char* groupKey(Group g) { return g == Group::Light ? "light" : "dark"; }

struct Hue {
	std::string id;    // "red" ... "violet"
	std::string label; // "Red" ... "Violet/Pink"
	Rgb primary;
};
struct Background {
	std::string name;
	Rgb color;
};
struct GroupTheme {
	std::vector<Hue> hues;
	std::vector<Background> backgrounds;
};

// The colours the HUD draws, all derived from (primary, background, group).
struct Derived {
	Rgba primary, secondary, accent, muted, caption, panel, warning, shadow, buttonText, buttonShadow;
	Rgb background;      // the canvas behind the HUD
	Rgb viewportBacking; // behind the scene video inside the media screen
	Rgba border;         // 1 px line around the media screen
};

inline Derived derive(Rgb primary, Rgb background, Group group) {
	const Rgb white{255, 255, 255}, black{0, 0, 0};
	Derived d;
	d.background = background;
	if (group == Group::Dark) {
		d.primary = withAlpha(mix(primary, white, 0.30f), 235);
		d.secondary = withAlpha(primary, 215);
		d.accent = withAlpha(mix(primary, white, 0.55f), 240);
		d.muted = withAlpha(primary, 95);
		d.caption = withAlpha(mix(primary, background, 0.30f), 200);
		d.panel = withAlpha(mix(primary, black, 0.85f), 40);
		d.warning = {255, 120, 90, 230};
		d.shadow = {0, 0, 0, 150};
		d.buttonText = withAlpha(mix(primary, white, 0.30f), 235);
		d.buttonShadow = {0, 0, 0, 150};
		d.viewportBacking = mix(background, black, 0.45f);
	} else {
		d.primary = withAlpha(mix(primary, black, 0.60f), 240);
		d.secondary = withAlpha(mix(primary, black, 0.40f), 235);
		d.accent = withAlpha(mix(primary, black, 0.25f), 240);
		d.muted = withAlpha(mix(primary, black, 0.35f), 125);
		d.caption = withAlpha(mix(primary, black, 0.50f), 220);
		d.panel = withAlpha(mix(primary, white, 0.80f), 50);
		d.warning = {170, 48, 28, 240};
		d.shadow = {255, 255, 255, 150};
		d.buttonText = withAlpha(mix(primary, black, 0.60f), 240);
		d.buttonShadow = {255, 255, 255, 150};
		d.viewportBacking = mix(background, black, 0.12f);
	}
	d.border = {d.accent.r, d.accent.g, d.accent.b, 255};
	return d;
}

struct ThemeSet {
	GroupTheme light, dark;

	static ThemeSet defaults() {
		ThemeSet t;
		const std::vector<Hue> hues = {
			{"red", "Red", {0xE5, 0x48, 0x4D}},      {"orange", "Orange", {0xF7, 0x6B, 0x15}},
			{"yellow", "Yellow", {0xE5, 0xB8, 0x00}}, {"green", "Green", {0x30, 0xA4, 0x6C}},
			{"blue", "Blue", {0x3E, 0x63, 0xDD}},     {"indigo", "Indigo", {0x6E, 0x56, 0xCF}},
			{"violet", "Violet/Pink", {0xD6, 0x40, 0x9F}},
		};
		t.light.hues = hues;
		t.dark.hues = hues;
		// Light: slightly tinted, very light greys (not bright white).
		t.light.backgrounds = {
			{"Mist", {224, 228, 226}}, {"Paper", {232, 232, 228}}, {"Cloud", {222, 224, 230}}, {"Sage", {222, 228, 222}},
		};
		t.dark.backgrounds = {
			{"Ink", {12, 12, 12}}, {"Graphite", {24, 26, 28}}, {"Deep Blue", {10, 14, 22}}, {"Deep Green", {10, 18, 14}},
		};
		return t;
	}
	const GroupTheme& group(Group g) const { return g == Group::Light ? light : dark; }
	GroupTheme& group(Group g) { return g == Group::Light ? light : dark; }
};

// -- System Mode ids ----------------------------------------------------------
struct MinimalId {
	Group group = Group::Dark;
	std::string hue;
};
inline std::string minimalModeId(Group g, const std::string& hue) { return std::string("minimal.") + groupKey(g) + "." + hue; }
inline std::optional<MinimalId> parseMinimalId(const std::string& id) {
	const std::string light = "minimal.light.", dark = "minimal.dark.";
	if (id.rfind(light, 0) == 0 && id.size() > light.size()) return MinimalId{Group::Light, id.substr(light.size())};
	if (id.rfind(dark, 0) == 0 && id.size() > dark.size()) return MinimalId{Group::Dark, id.substr(dark.size())};
	return std::nullopt;
}
inline bool isMinimalId(const std::string& id) { return parseMinimalId(id).has_value(); }

// Only characters a mode id can legitimately contain (skin ids are [a-z0-9-]).
inline bool validModeId(const std::string& id) {
	if (id.empty() || id.size() > 64) return false;
	for (char c : id) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_')) return false;
	return true;
}

inline std::string skinLabel(const std::string& skinId) {
	if (skinId == "dark-moss-experimental") return "Moss";
	if (skinId == "abyssal-bioluminescence") return "Abyss";
	if (skinId == "plywood-marble") return "Wood";
	return skinId;
}

struct ModeOption {
	std::string id;
	std::string label;
	std::string group; // "AUTOMATIC" / "SKINS" / "MINIMAL - LIGHT" / "MINIMAL - DARK"
};

// The flat, grouped list the Settings page steps through: Auto, the skins,
// then every hue in the light group, then every hue in the dark group.
inline std::vector<ModeOption> modeOptions(const std::vector<std::string>& skinIds, const ThemeSet& themes) {
	std::vector<ModeOption> v;
	v.push_back({"auto", "Auto (day/night skins)", "AUTOMATIC"});
	for (const auto& s : skinIds) v.push_back({s, skinLabel(s), "SKINS"});
	for (const Group g : {Group::Light, Group::Dark}) {
		for (const auto& h : themes.group(g).hues) {
			v.push_back({minimalModeId(g, h.id), h.label, g == Group::Light ? "MINIMAL - LIGHT" : "MINIMAL - DARK"});
		}
	}
	return v;
}

// Resolve a minimal mode id + background choice to its colours; nullopt for an
// unknown hue (the caller keeps whatever it was showing).
inline std::optional<Derived> resolve(const ThemeSet& themes, const std::string& modeId, int backgroundIndex) {
	const auto m = parseMinimalId(modeId);
	if (!m) return std::nullopt;
	const GroupTheme& g = themes.group(m->group);
	if (g.backgrounds.empty()) return std::nullopt;
	for (const auto& h : g.hues) {
		if (h.id != m->hue) continue;
		const int bi = std::clamp(backgroundIndex, 0, static_cast<int>(g.backgrounds.size()) - 1);
		return derive(h.primary, g.backgrounds[static_cast<size_t>(bi)].color, m->group);
	}
	return std::nullopt;
}

} // namespace minimaltheme
