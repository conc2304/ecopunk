#pragma once

// HudInteractionStyle — semantic interaction-state styling for interactive HUD regions.
//
// OF-free, header-only (unit-tested). Maps the ACTIVE SKIN'S already-resolved palette tokens
// (HudWidgetColors: accent / secondary / muted) to the per-state stroke colours of an
// interactive region. No literal colours except the conservative white/black used to LIFT or
// DEEPEN an accent when the skin gives no distinct secondary. Nothing here knows which scene
// or region it is drawing, and it never reads the scene's own (e.g. CRT phosphor) colours.
//
//   token                  source (priority: skin token -> derived from skin accent -> default)
//   idle     (faint)       accent at reduced alpha
//   focus    (current)     accent at high alpha
//   active   (editing)     secondary when it is visibly different from accent, else accent
//                          LIFTED toward white (dark accent) / DEEPENED toward black (light accent)
//   disabled (muted)       muted token
//   rowFill  (selected row) active colour at low alpha (a tint; never a flood fill)
//
// Colour alone never carries the state: see HudInteractionStyle::Geometry (stroke weight,
// corner ticks, full frame, selected-row bar, chevrons).

#include <algorithm>
#include <cstdint>

namespace hudpresent {

struct HudRgba8 {
	uint8_t r = 0, g = 0, b = 0, a = 255;
};

enum class HudInteractionPhase : uint8_t {
	Passive,  // not an interactive region (or none declared): no interaction overlay at all
	Idle,     // interactive, not focused
	Focused,  // HUD focus is on it; nothing is captured
	Active,   // directional input is captured by the region
	Disabled, // declared but currently unavailable: not in focus traversal
};

struct HudInteractionStyle {
	HudRgba8 idle, focus, active, disabled, rowFill;
	bool secondaryDistinct = false; // true when the skin's secondary was usable as the active colour

	// Non-colour cues (constants, shared by every skin).
	struct Geometry {
		float idleStroke = 1.0f;   float idleArm = 4.0f;    // corner ticks only
		float focusStroke = 1.0f;  float focusArm = 9.0f;   // thin full frame + longer, heavier corners
		float focusCornerStroke = 2.0f;
		float activeStroke = 2.0f;                          // heavier full frame
		float rowBarWidth = 3.0f;                           // selected-row marker
	};
	static constexpr Geometry geometry() { return Geometry{}; }
};

namespace interaction_detail {
inline float dist(const HudRgba8& a, const HudRgba8& b) {
	const float dr = float(a.r) - float(b.r), dg = float(a.g) - float(b.g), db = float(a.b) - float(b.b);
	return dr * dr + dg * dg + db * db; // squared
}
inline float luma(const HudRgba8& c) { return (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b) / 255.0f; }
inline HudRgba8 towards(const HudRgba8& c, uint8_t t, float k) {
	auto m = [&](uint8_t v) { return static_cast<uint8_t>(float(v) + (float(t) - float(v)) * k + 0.5f); };
	return {m(c.r), m(c.g), m(c.b), c.a};
}
inline HudRgba8 withA(HudRgba8 c, float a) {
	c.a = static_cast<uint8_t>(std::clamp(a, 0.0f, 255.0f));
	return c;
}
} // namespace interaction_detail

// Squared RGB distance below which secondary and accent are treated as "the same colour".
constexpr float kInteractionMinSeparationSq = 60.0f * 60.0f;

inline HudInteractionStyle resolveInteractionStyle(const HudRgba8& accent, const HudRgba8& secondary, const HudRgba8& muted) {
	using namespace interaction_detail;
	HudInteractionStyle s;
	s.focus = withA(accent, std::max<float>(accent.a, 220.0f));
	s.idle = withA(accent, std::max(135.0f, float(accent.a) * 0.55f)); // faint but legible on pale AND dark skins
	if (dist(accent, secondary) >= kInteractionMinSeparationSq && secondary.a >= 120) {
		s.secondaryDistinct = true;
		s.active = withA(secondary, std::max<float>(secondary.a, 230.0f));
	} else { // coherent fallback: same hue family as the focus colour, clearly stronger/different
		const bool lightAccent = luma(accent) > 0.6f;
		s.active = withA(lightAccent ? towards(accent, 0, 0.35f) : towards(accent, 255, 0.40f), 255.0f);
	}
	s.disabled = withA(muted, std::min<float>(muted.a, 90.0f));
	s.rowFill = withA(s.active, 46.0f);
	return s;
}

} // namespace hudpresent
