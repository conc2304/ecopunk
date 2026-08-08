#pragma once

#include "HudFrameData.h"
#include "HudWireframeRenderer.h"

// HudCompositorBridge — Engineering Session 2's replacement for
// HudCompositorStub (see that class's own header comment, now marked
// superseded — it remains on disk only because this environment's `rm` is
// consistently denied at the permission layer, not because it is still
// used; see this session's completion report, "stray files needing manual
// cleanup").
//
// This is a thin bridge, NOT a second HUD renderer: it owns exactly one
// hudpresent::HudWireframeRenderer — the SAME renderer class
// hud_validation_studio hosts against fake data — and does nothing this
// class's own logic couldn't be inlined into ExperienceRuntime, except
// keep that class's header from needing to include the HUD Runtime
// domain's ~15-header include chain directly. No HUD drawing logic lives
// in this file; draw() is a one-line forward.
//
// Ownership/consumption rules this class exists to preserve (Scene-HUD-
// Contract-v1.md §4, and this repo's root .claude/CLAUDE.md architecture
// diagram): it receives one assembled HudFrameData by const& per frame
// from ExperienceRuntime and never polls SceneManager/scenes/
// RuntimeServices/VideoPlaybackService/effect services itself — everything
// it draws comes from the HudFrameData snapshot handed to update().
//
// Canvas assumption: HudWireframeRenderer::draw() draws in a fixed
// 1280x720 canonical canvas coordinate space (HudRegionCatalog's
// normalized region bounds * canvasWidth_/canvasHeight_ — see that
// class's pixelBoundsFor()). ExperienceRuntime's window is already
// 1280x720 (sketches/experience_runtime/src/main.cpp) specifically so this
// bridge never needs a destRect/scale transform the way HudCompositorStub
// did for its own, unrelated (native-scene-size-based) rectangle — this
// bridge draws directly at (0,0)-(1280,720), matching the window 1:1.
class HudCompositorBridge {
public:
	void setup();

	// All per-frame resolution work — see
	// HudWireframeRenderer::update(float, const HudFrameData&)'s own
	// comment for exactly what this does (scene-epoch/frame-freshness/
	// generation handling, all from already-approved fields only).
	void update(float dt, const HudFrameData& frame);

	// Draws the last update()'d state at the canonical 1280x720 canvas
	// origin. No-op (but still leaves prior state intact) when !visible_,
	// matching HudCompositorStub's own toggle behavior.
	void draw() const;

	// Wired to the existing development-only RuntimeCommand::ToggleHud
	// binding (see InputRouter) — same role HudCompositorStub::setVisible()
	// played, preserved so ExperienceRuntime.cpp's existing call site needs
	// no behavioral change beyond the member's type.
	void setVisible(bool visible) { visible_ = visible; }
	bool isVisible() const { return visible_; }

	// Read-only inspection surface — tests/telemetry only, never used to
	// feed decisions back into update()/draw() (draw() is const and reads
	// only what update() already resolved, per HudWireframeRenderer's own
	// design).
	const hudpresent::HudWireframeRenderer& renderer() const { return renderer_; }

	// Architecture-Closure Session: ground-truth counter proving "exactly
	// one production HUD draw per runtime frame" — incremented in draw(),
	// even when !visible_ (a skipped draw is still one decision made per
	// frame, not a second draw), so GlRestorationHarness can assert
	// drawCallCount == frameCount after N frames rather than trusting
	// single-call-site code inspection alone.
	uint64_t drawCallCount() const { return drawCallCount_; }

private:
	hudpresent::HudWireframeRenderer renderer_;
	bool visible_ = true;
	mutable uint64_t drawCallCount_ = 0;
};
