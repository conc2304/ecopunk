#pragma once

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>

// RT-007 Fullscreen Presentation & HUD Scaling — the ONE final transform
// from the canonical 1280x720 composition (scene viewport + Dark Moss shell
// + dynamic HUD, all authored in canvas pixels) to the physical framebuffer.
//
// Aspect-preserving CONTAIN: scale = min(fbW / 1280, fbH / 720), centred.
// 1280x720 -> identity (the direct, unchanged draw path); 1600x900 -> 1.25,
// no letterbox; 1920x1200 -> 1.5 with 60 px bars top/bottom.
//
// Framebuffer, not logical window: openFrameworks' ofGetWidth()/ofGetHeight()
// (GLFW backend) already return framebuffer pixels (window size x
// pixelScreenCoordScale), and mouse events arrive in that same space — so
// the framebuffer size is the single input here, and toCanvas() is the exact
// inverse used for pointer coordinates.
//
// Pure (no OF/GL), so it is unit-tested in test/presentation_transform_tests.
struct PresentationTransform {
	static constexpr float kCanvasWidth = 1280.0f;
	static constexpr float kCanvasHeight = 720.0f;

	int framebufferWidth = static_cast<int>(kCanvasWidth);
	int framebufferHeight = static_cast<int>(kCanvasHeight);
	float scale = 1.0f;
	float offsetX = 0.0f; // framebuffer pixels
	float offsetY = 0.0f;

	static PresentationTransform contain(int fbW, int fbH) {
		PresentationTransform t;
		t.framebufferWidth = fbW;
		t.framebufferHeight = fbH;
		if (fbW <= 0 || fbH <= 0) return t; // minimized/degenerate: keep identity, draw nothing new
		t.scale = std::min(static_cast<float>(fbW) / kCanvasWidth, static_cast<float>(fbH) / kCanvasHeight);
		t.offsetX = (static_cast<float>(fbW) - kCanvasWidth * t.scale) * 0.5f;
		t.offsetY = (static_cast<float>(fbH) - kCanvasHeight * t.scale) * 0.5f;
		return t;
	}

	// True exactly when the framebuffer IS the canonical canvas — the runtime
	// then draws directly, byte-for-byte as before RT-007.
	bool isIdentity() const {
		return framebufferWidth == static_cast<int>(kCanvasWidth) && framebufferHeight == static_cast<int>(kCanvasHeight);
	}

	float drawWidth() const { return kCanvasWidth * scale; }
	float drawHeight() const { return kCanvasHeight * scale; }

	// Framebuffer pixel -> canonical canvas pixel (pointer input). Points in
	// the letterbox bars map outside [0,1280)x[0,720); see insideCanvas().
	float toCanvasX(float fbX) const { return (fbX - offsetX) / scale; }
	float toCanvasY(float fbY) const { return (fbY - offsetY) / scale; }
	float toFramebufferX(float canvasX) const { return canvasX * scale + offsetX; }
	float toFramebufferY(float canvasY) const { return canvasY * scale + offsetY; }
	static bool insideCanvas(float canvasX, float canvasY) {
		return canvasX >= 0.0f && canvasY >= 0.0f && canvasX < kCanvasWidth && canvasY < kCanvasHeight;
	}

	bool operator==(const PresentationTransform& o) const {
		return framebufferWidth == o.framebufferWidth && framebufferHeight == o.framebufferHeight;
	}
	bool operator!=(const PresentationTransform& o) const { return !(*this == o); }
};

// EXPERIENCE_RUNTIME_FULLSCREEN — the explicit startup override for NATIVE
// host fullscreen (not IMMERSIVE, which is scene-only presentation).
// "1" -> fullscreen, "0" -> windowed; unset/empty/other -> no override, and
// the persisted Installation Controls launchFullscreen preference decides.
// Read by main.cpp (window creation) and ExperienceRuntime::setup()
// (precedence), from this one function so the two can't disagree.
inline std::optional<bool> fullscreenOverrideFromEnv() {
	const char* v = std::getenv("EXPERIENCE_RUNTIME_FULLSCREEN");
	if (v == nullptr) return std::nullopt;
	const std::string s(v);
	if (s == "1") return true;
	if (s == "0") return false;
	return std::nullopt;
}
