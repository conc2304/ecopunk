#pragma once

#include "HudFrameData.h"
#include "ofRectangle.h"

// HudCompositorStub — SUPERSEDED as of Engineering Session 2 by
// HudCompositorBridge (see HudCompositorBridge.h), which ExperienceRuntime
// now actually uses. Nothing in this repository constructs or calls this
// class anymore.
//
// Left on disk, unused, only because this environment's `rm` was
// consistently denied at the permission layer throughout both this and
// the prior engineering session (see this session's completion report,
// "stray files needing manual cleanup") — not because it is still needed.
// A human (or a future session with working `rm`) should delete this file
// and HudCompositorStub.cpp.
//
// Original comment, preserved for history:
//
// A temporary integration stub, NOT the finished HudCompositor. The real
// HudCompositor (canonical 1280x720 layout, MediaViewportMesh, HUD skins,
// universal/scene-specific widgets, vocabulary, presentation profiles) is
// owned by the HUD Runtime domain — see shared/src/hud-compositor/, now
// consumed for real via HudCompositorBridge. This class existed only to
// prove that a downstream consumer can be handed one assembled, read-only
// HudFrameData and draw from it, per the approved direction: "the
// compositor cannot poll scenes or services directly."
//
// It draws the scene texture as a plain, unclipped rectangle plus minimal
// debug text proving the scene/status/frame values actually arrived — no
// MediaViewportMesh, no rounded corners, no bevel, no skin, no widgets, no
// vocabulary, no scene-ID branching.
class HudCompositorStub {
public:
	// Accepts HudFrameData by const& — this stub cannot mutate scene
	// texture, scene status, or any other field, and it never calls back
	// into SceneManager, FakeScene, or RuntimeServices: everything it
	// draws comes from the one snapshot handed to it.
	void draw(const HudFrameData& frame, const ofRectangle& destRect) const;

	// Wired to the development-only RuntimeCommand::ToggleHud binding
	// (see InputRouter) purely to exercise that command's routing path;
	// this is not a real HUD visibility feature.
	void setVisible(bool visible) { visible_ = visible; }
	bool isVisible() const { return visible_; }

private:
	bool visible_ = true;
};
