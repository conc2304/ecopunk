#include "HudCompositorBridge.h"

void HudCompositorBridge::setup() {
	// The one and only canonical canvas size — see this class's header
	// comment for why no destRect/scale step is needed downstream.
	renderer_.setup(1280.0f, 720.0f);

	// Production behavior, not Validation Studio behavior: an invalid
	// binding is skipped + logged once, never rendered as a visible
	// BindingPlaceholder (HudWireframeRenderer::setStudioMode()'s own
	// doc comment) — studioMode_ defaults to false already, this call is
	// here only to make the choice explicit rather than implicit.
	renderer_.setStudioMode(false);
}

void HudCompositorBridge::update(float dt, const HudFrameData& frame) {
	// Scene epoch, frame freshness, and generation are all derived inside
	// this call from frame.sceneManager.activeSceneId /
	// frame.sceneFrame.frameNumber / frame.scene.semantic->timing.generation
	// respectively — see HudWireframeRenderer::update(float, const
	// HudFrameData&)'s own comment. This bridge adds no logic of its own.
	renderer_.update(dt, frame);
}

void HudCompositorBridge::draw() const {
	drawCallCount_++;
	if (!visible_) return;
	renderer_.draw();
}
