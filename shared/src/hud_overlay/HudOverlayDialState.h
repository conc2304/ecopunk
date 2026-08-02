#pragma once

// HUD Glitch Overlay System — standalone module, isolated per the design
// doc's engineering handoff addendum. Not wired to Composition Manager,
// Fragment, or VideoSampler; see shared/src/hud_overlay/README.md.
namespace hudoverlay {

enum class Palette { Mono, Orange, Lime };

// The four master dials (design doc Section 05). Plain data — written only
// by HudOverlayDialPanel (the GUI layer), read by everything else
// (HudOverlayLayer, HudOverlayScheduler, every organism).
struct HudOverlayDialState {
	float intensity     = 0.5f; // 0-1: scales organism frequency + scanline/radar speed. ~0.7 is the practical "ambient" ceiling.
	float discipline    = 0.5f; // 0-1: 1 = grid-snapped organism placement, 0 = free/jittered
	float eventCoupling = 0.2f; // 0-1: reduces independent timing jitter between organism schedulers
	Palette palette      = Palette::Mono;
};

} // namespace hudoverlay
