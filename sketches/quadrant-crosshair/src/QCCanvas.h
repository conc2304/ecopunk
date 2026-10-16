#pragma once

#include "ofAppRunner.h"

// The logical canvas Quadrant Crosshair lays out in (quadrant regions,
// crosshair path, trigger zones, erosion/motion FBO sizes, HUD-layer widget
// bounds). The standalone sketch leaves it unset, so it tracks the window
// exactly as the old ofGetWidth()/ofGetHeight() calls did. ExperienceRuntime's
// QuadrantProductionScene sets it to the scene's native size, because there
// the artwork draws into the runtime-owned scene FBO, whose size is
// independent of the window (fullscreen, resizes).
//
// Process-wide: one Quadrant Crosshair instance per process (the standalone
// sketch, or the one production scene).
namespace quadrantcrosshair {

namespace detail {
inline float& canvasWidthOverride() { static float w = 0.0f; return w; }
inline float& canvasHeightOverride() { static float h = 0.0f; return h; }
} // namespace detail

inline void setCanvasSize(float width, float height) {
	detail::canvasWidthOverride() = width;
	detail::canvasHeightOverride() = height;
}

inline float canvasWidth() {
	const float w = detail::canvasWidthOverride();
	return w > 0.0f ? w : static_cast<float>(ofGetWidth());
}

inline float canvasHeight() {
	const float h = detail::canvasHeightOverride();
	return h > 0.0f ? h : static_cast<float>(ofGetHeight());
}

} // namespace quadrantcrosshair
