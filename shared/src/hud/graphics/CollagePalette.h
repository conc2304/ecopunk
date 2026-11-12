#pragma once

#include "ofColor.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace hud {

// Scene-agnostic palette for editorial / print-plate style graphics.
// One composition = paper + primary accent + optional secondary accent +
// black + white. At most two chromatic accents are ever active.
// Nothing here knows about any scene, layout, or the HUD compositor.

enum class PaletteToken { Background,
	Primary,
	Secondary,
	Dark,
	Light };

struct HudCollagePalette {
	const char * name = "";
	ofColor background;
	ofColor primary;
	std::optional<ofColor> secondary;
	ofColor dark;
	ofColor light;

	// Secondary falls back to primary when the palette defines only one accent.
	ofColor resolve(PaletteToken token) const;
};

// Curated, fixed families (no per-element random RGB).
const std::vector<HudCollagePalette> & curatedCollagePalettes();

// Deterministic: same seed -> same palette.
const HudCollagePalette & pickCollagePalette(uint32_t seed);

}
