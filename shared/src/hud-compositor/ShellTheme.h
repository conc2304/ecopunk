#pragma once

// ============================================================================
// ShellTheme.h -- palette derivation for the HUD SHELL (OpenFrameworks-free).
//
// Scope: the shell only (hud-compositor widgets, focus chrome, media-screen
// border). Widgets drawn inside sketch scenes (shared/src/hud/, hud_overlay)
// are deliberately not themed here yet.
//
// A shell theme is a handful of seed colours plus the colour of the panel the
// text sits on. Every token the widgets draw (HudWidgetColors) is derived from
// those seeds, then nudged until it meets a contrast target against the panel
// surface, so a new skin can't ship unreadable text or an invisible outline:
//
//   seeds:  mode (dark = light-on-dark text, light = dark-on-light text)
//           primary, secondary (optional), surface (the panel colour)
//   out:    primary secondary accent muted caption warning dataA dataB panel
//           shadow viewportBacking border
//
// Targets (WCAG): text-bearing tokens >= 4.5:1, outline/graphic tokens >= 3:1.
// `overrides` pins a token to an exact colour; pinned tokens are not adjusted.
//
// Same derivation idea as sketches/experience_runtime/src/MinimalTheme.h
// (one seed -> every colour); that file keeps its own copy for now.
// ============================================================================

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>

namespace shelltheme {

struct Rgb {
	uint8_t r = 0, g = 0, b = 0;
};
struct Rgba {
	uint8_t r = 0, g = 0, b = 0, a = 255;
};

enum class Mode { Dark, Light };

struct Seeds {
	Mode mode = Mode::Dark;
	Rgb primary{124, 232, 230};
	std::optional<Rgb> secondary;  // defaults to primary
	Rgb surface{32, 30, 25};       // mean colour of the panels text is drawn on
	std::map<std::string, Rgb> overrides; // token name -> exact opaque colour
};

struct Tokens {
	Rgba primary, secondary, accent, muted, caption, warning, dataA, dataB;
	Rgba panel, shadow;
	Rgb viewportBacking;
	Rgba border;
};

constexpr float kTextContrast = 4.5f;
constexpr float kGraphicContrast = 3.0f;

inline Rgb mix(Rgb a, Rgb b, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	auto m = [&](uint8_t x, uint8_t y) { return static_cast<uint8_t>(x + (y - x) * t + 0.5f); };
	return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}

inline Rgb overSurface(Rgba c, Rgb surface) {
	return mix(surface, Rgb{c.r, c.g, c.b}, static_cast<float>(c.a) / 255.0f);
}

inline float relativeLuminance(Rgb c) {
	auto lin = [](uint8_t v) {
		const float s = static_cast<float>(v) / 255.0f;
		return s <= 0.03928f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
	};
	return 0.2126f * lin(c.r) + 0.7152f * lin(c.g) + 0.0722f * lin(c.b);
}

inline float contrastRatio(Rgb a, Rgb b) {
	float la = relativeLuminance(a), lb = relativeLuminance(b);
	if (la < lb) std::swap(la, lb);
	return (la + 0.05f) / (lb + 0.05f);
}

// Contrast of a (possibly translucent) colour once composited on the surface.
inline float contrastOn(Rgba c, Rgb surface) { return contrastRatio(overSurface(c, surface), surface); }

// Lightens (dark mode) or darkens (light mode) `c` until it reaches `target`
// against `surface`; if the colour alone cannot get there, raises alpha up to
// `alphaCap`. Returns the input untouched when it already passes.
inline Rgba fitContrast(Rgba c, Rgb surface, Mode mode, float target, uint8_t alphaCap = 255) {
	if (contrastOn(c, surface) >= target) return c;
	const Rgb extreme = mode == Mode::Dark ? Rgb{255, 255, 255} : Rgb{0, 0, 0};
	for (int i = 1; i <= 50; ++i) {
		const Rgb m = mix(Rgb{c.r, c.g, c.b}, extreme, static_cast<float>(i) / 50.0f);
		const Rgba t{m.r, m.g, m.b, c.a};
		if (contrastOn(t, surface) >= target) return t;
	}
	Rgba t{extreme.r, extreme.g, extreme.b, c.a};
	while (t.a < alphaCap && contrastOn(t, surface) < target) t.a = static_cast<uint8_t>(std::min<int>(alphaCap, t.a + 5));
	return t;
}

inline Rgba withAlpha(Rgb c, int a) { return {c.r, c.g, c.b, static_cast<uint8_t>(std::clamp(a, 0, 255))}; }

inline Tokens derive(const Seeds& seeds) {
	const Rgb white{255, 255, 255}, black{0, 0, 0};
	const Rgb P = seeds.primary;
	const Rgb S = seeds.secondary.value_or(seeds.primary);
	const Rgb surface = seeds.surface;
	const bool dark = seeds.mode == Mode::Dark;
	Tokens t;

	// Observation series for the Orbit console: fixed green / blue hues, pulled to
	// readable lightness on this surface below.
	const Rgb greenHue{110, 210, 120}, blueHue{95, 150, 240};

	if (dark) {
		t.primary = withAlpha(mix(P, white, 0.30f), 235);
		t.secondary = withAlpha(S, 215);
		t.accent = withAlpha(mix(P, white, 0.55f), 240);
		t.muted = withAlpha(P, 110);
		t.caption = withAlpha(mix(P, surface, 0.30f), 210);
		t.warning = {255, 120, 90, 230};
		t.panel = withAlpha(mix(P, black, 0.85f), 40);
		t.shadow = {0, 0, 0, 150};
		t.viewportBacking = mix(surface, black, 0.45f);
	} else {
		t.primary = withAlpha(mix(P, black, 0.60f), 240);
		t.secondary = withAlpha(mix(S, black, 0.40f), 235);
		t.accent = withAlpha(mix(P, black, 0.25f), 240);
		t.muted = withAlpha(mix(P, black, 0.35f), 135);
		t.caption = withAlpha(mix(P, black, 0.50f), 220);
		t.warning = {170, 48, 28, 240};
		t.panel = withAlpha(mix(P, white, 0.80f), 50);
		t.shadow = {255, 255, 255, 150};
		t.viewportBacking = mix(surface, black, 0.12f);
	}
	t.dataA = withAlpha(greenHue, 255);
	t.dataB = withAlpha(blueHue, 255);

	// A pinned token takes its override exactly (alpha stays derived) and is not fitted.
	auto fit = [&](const char* name, Rgba& slot, float target, uint8_t alphaCap = 255) {
		const auto it = seeds.overrides.find(name);
		if (it != seeds.overrides.end()) slot = {it->second.r, it->second.g, it->second.b, slot.a};
		else slot = fitContrast(slot, surface, seeds.mode, target, alphaCap);
	};
	fit("primary", t.primary, kTextContrast);
	fit("secondary", t.secondary, kTextContrast);
	fit("accent", t.accent, kTextContrast);
	fit("caption", t.caption, kTextContrast);
	fit("warning", t.warning, kTextContrast);
	fit("dataA", t.dataA, kGraphicContrast);
	fit("dataB", t.dataB, kGraphicContrast);
	fit("muted", t.muted, kGraphicContrast, 200); // an outline, not text: keep it translucent

	t.border = {t.accent.r, t.accent.g, t.accent.b, 255};
	return t;
}

// "#RRGGBB" (the '#' is optional). nullopt for anything else.
inline std::optional<Rgb> parseHex(const std::string& text) {
	std::string s = text;
	if (!s.empty() && s[0] == '#') s.erase(0, 1);
	if (s.size() != 6) return std::nullopt;
	for (char c : s) if (!std::isxdigit(static_cast<unsigned char>(c))) return std::nullopt;
	const unsigned long v = std::strtoul(s.c_str(), nullptr, 16);
	return Rgb{static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v)};
}

} // namespace shelltheme
