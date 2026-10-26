#pragma once

// HUD Runtime — Reactive Control Artwork v0.1 (Architecture-approved HUD-004
// backward-compatible extension; see
// docs/hud-art/hud-004-optional-pressed-artwork-extension.md).
//
// Exactly ONE optional skin asset exists: a pressed-state face for the
// existing controls.scene_next control, at the FIXED package-relative path
// below. No skin.json field, no sidecar, no discovery — the path is the
// contract. Missing or invalid artwork never invalidates a skin.
//
// Plain C++17, no OF/GL: shared by the skin loader (path + accepted sizes),
// the renderer (when and how strongly to draw it) and the OF-free unit tests.

#include <cmath>
#include <cstdint>

namespace hudpresent {
namespace reactive {

// Package-relative, '/'-separated; every component must be a real
// directory/file inside the package (symlinks are not followed).
inline constexpr const char* kSceneNextPressedPath = "controls/scene_next/pressed.png";
inline constexpr const char* kSceneNextRegionId = "controls.scene_next";

// The pressed sprite's rectangle is DERIVED from the control's canonical
// region (HUD-006 v2 decision D3) — never a second hardcoded geometry:
// expand the region by 4 px on every side, then snap outward to whole
// canvas pixels. The sprite is drawn unscaled there and must be exactly that
// size. Interactive/focus bounds stay the region itself. v2 controls.scene_next
// (110.4, 633.6, 72x72) -> 81x81 at (106, 629).
inline constexpr float kPressedSpriteBleed = 4.0f;
struct PixelRect {
	int x = 0, y = 0, width = 0, height = 0;
};
inline PixelRect pressedSpriteRect(float regionX, float regionY, float regionW, float regionH) {
	const int x0 = static_cast<int>(std::floor(regionX - kPressedSpriteBleed));
	const int y0 = static_cast<int>(std::floor(regionY - kPressedSpriteBleed));
	const int x1 = static_cast<int>(std::ceil(regionX + regionW + kPressedSpriteBleed));
	const int y1 = static_cast<int>(std::ceil(regionY + regionH + kPressedSpriteBleed));
	return {x0, y0, x1 - x0, y1 - y0};
}

// -- Button faces (HUD Art "hud-state-buttons") --------------------------------
// One shared set of three optional faces per skin, used by ALL six navigation
// buttons, at fixed package-relative paths (no manifest field, no discovery):
//   controls/button/idle.png      the face at rest
//   controls/button/focused.png   the face while the control holds focus
//   controls/button/active.png    the momentary press (the accepted-activation
//                                 feedback interval, same fade as the flash)
// Each is exactly the control region + 4 px bleed per side (HUD-006 v2 control
// 72x72 -> 80x80), drawn UNSCALED centred on the region and snapped to whole
// canvas pixels. Interactive/focus bounds stay the region itself.
inline constexpr const char* kButtonIdlePath = "controls/button/idle.png";
inline constexpr const char* kButtonFocusedPath = "controls/button/focused.png";
inline constexpr const char* kButtonActivePath = "controls/button/active.png";
inline PixelRect buttonFaceRect(float regionX, float regionY, float regionW, float regionH) {
	const int w = static_cast<int>(std::lround(regionW + 2.0f * kPressedSpriteBleed));
	const int h = static_cast<int>(std::lround(regionH + 2.0f * kPressedSpriteBleed));
	const int x = static_cast<int>(std::lround(regionX + regionW * 0.5f - static_cast<float>(w) * 0.5f));
	const int y = static_cast<int>(std::lround(regionY + regionH * 0.5f - static_cast<float>(h) * 0.5f));
	return {x, y, w, h};
}

// Opacity of the pressed artwork this frame, reusing the renderer's EXISTING
// activation-feedback state and timing (HudWireframeRenderer::
// triggerActivationFeedback / kActivationFlashSeconds) — no new timer or
// event. Non-zero only while ALL hold: the artwork is loaded, the activation
// was ACCEPTED (refused/disabled never shows it), the activated control is
// this region and is visible, and the feedback interval is running. Same
// linear 1 -> 0 fade the accepted flash uses, so release is the flash's end.
inline float pressedArtworkOpacity(bool hasArtwork, bool focusedIsRegion, bool regionVisible, bool accepted,
	float activationStart, float elapsedSeconds, float feedbackSeconds) {
	if (!hasArtwork || !focusedIsRegion || !regionVisible || !accepted) return 0.0f;
	if (activationStart < 0.0f || feedbackSeconds <= 0.0f) return 0.0f;
	const float age = elapsedSeconds - activationStart;
	if (age < 0.0f || age >= feedbackSeconds) return 0.0f;
	return 1.0f - age / feedbackSeconds;
}

} // namespace reactive
} // namespace hudpresent
