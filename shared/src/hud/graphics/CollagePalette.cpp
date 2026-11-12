#include "CollagePalette.h"

namespace hud {

ofColor HudCollagePalette::resolve(PaletteToken token) const {
	switch (token) {
	case PaletteToken::Background: return background;
	case PaletteToken::Primary: return primary;
	case PaletteToken::Secondary: return secondary.value_or(primary);
	case PaletteToken::Dark: return dark;
	case PaletteToken::Light: return light;
	}
	return dark;
}

const std::vector<HudCollagePalette> & curatedCollagePalettes() {
	static const std::vector<HudCollagePalette> palettes = {
		// pale paper + signal orange + black
		{ "paper-orange", ofColor(232, 228, 217), ofColor(236, 84, 34), std::nullopt, ofColor(18, 18, 18), ofColor(250, 248, 242) },
		// warm cream + powder blue + black
		{ "cream-powder", ofColor(238, 228, 205), ofColor(150, 186, 212), std::nullopt, ofColor(20, 20, 22), ofColor(251, 249, 243) },
		// bone + coral + teal
		{ "bone-coral-teal", ofColor(229, 224, 210), ofColor(232, 106, 90), ofColor(24, 112, 112), ofColor(18, 20, 20), ofColor(249, 247, 241) },
		// pale grey + acid yellow + deep charcoal
		{ "grey-acid", ofColor(218, 218, 213), ofColor(212, 226, 38), std::nullopt, ofColor(32, 34, 36), ofColor(246, 246, 242) },
		// warm ivory + desaturated green + rust
		{ "ivory-green-rust", ofColor(236, 230, 214), ofColor(108, 140, 102), ofColor(158, 72, 40), ofColor(22, 22, 20), ofColor(250, 247, 238) },
	};
	return palettes;
}

const HudCollagePalette & pickCollagePalette(uint32_t seed) {
	const auto & p = curatedCollagePalettes();
	// Cheap integer mix so consecutive seeds don't walk the list in order.
	uint32_t h = seed * 2654435761u;
	h ^= h >> 16;
	h *= 0x85EBCA6Bu;
	h ^= h >> 13;
	h *= 0xC2B2AE35u;
	h ^= h >> 16;
	return p[h % p.size()];
}

}
