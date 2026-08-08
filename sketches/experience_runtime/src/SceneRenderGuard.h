#pragma once

#include "ofGraphics.h"
#include "ofRectangle.h"

#include "glm/mat4x4.hpp"

// SceneRenderGuard — runtime-private implementation detail per
// Scene-HUD-Contract-v1.md §5: "a reusable RAII-style guard implementing
// [the known-good baseline] is runtime implementation, not part of the
// public IEcopunkScene interface — scenes never see it or interact with it
// directly." It is deliberately NOT added to SceneContract.h.
//
// Scope this around exactly one call to activeScene->drawToCurrentTarget(),
// while the runtime-owned scene FBO is still bound:
//
//     sceneFbo.begin();
//     sceneFbo.clear(0, 0, 0, 0);
//     {
//         SceneRenderGuard guard(sceneFbo);
//         activeScene->drawToCurrentTarget();
//     } // guard destructor restores the baseline here, still inside
//       // sceneFbo.begin()/end() — matches the contract's draw-order
//       // diagram: baseline restoration (step 6) happens before the
//       // runtime unbinds SceneFbo (step 7).
//     sceneFbo.end();
//
// Per §5's obligation split, this restores a KNOWN BASELINE defensively —
// it does not attempt to snapshot-and-restore "whatever the scene had
// before," because the contract explicitly rejects relying on scene
// discipline: "verifying every scene's internal GL discipline by
// inspection is not reliable enough to skip the defensive restore."
// Viewport/matrices/style are the exception: those are restored to the
// EXPLICIT VALUES this guard captured at construction (the FBO's own
// viewport, and the matrices/style active at that moment) rather than to a
// hardcoded default — captured by value, not via OF's ofPush*/ofPop*
// stacks, specifically so restoration is correct even if the scene itself
// left its own push/pop calls unbalanced (see FakeScene's deliberate
// contamination, which does exactly this).
//
// Engineering Session 2 §3.2 — documented boundary (exact statement, per
// that task's instruction):
//
//   SceneRenderGuard restores defensive raw GL state.
//   SceneRenderGuard cannot repair unbalanced openFrameworks-managed
//   begin/end or push/pop stacks.
//   Scenes must balance their own OF-level stacks.
//
// This is not a theoretical caveat — it was found empirically in this
// increment: an early version of FakeScene's deliberate contamination
// called a throwaway ofFbo's begin() without a matching end(). That also
// pushes onto openFrameworks' own process-global ofMatrixStack (view/
// projection/viewport/orientation) as a side effect of begin() — since it
// was never popped, it corrupted that global stack for the rest of the
// process (logged at exit: "ofMatrixStack: clearStacks(): found 1 extra
// ... matrices/viewports/orientations"), and no amount of raw
// glBindFramebuffer()-level rebinding in this guard's destructor could fix
// it, because the corruption was in OF's own C++-level push/pop
// bookkeeping, not in queryable raw GL state. FakeScene's contamination
// was changed to a raw glBindFramebuffer() call instead (see
// FakeScene.cpp's applyGLContamination()), which this guard DOES fully
// recover from — see GlRestorationHarness.cpp's "default framebuffer (0)
// bound after draw()" check. No public contract method is added for this
// distinction; it is guard/scene-author knowledge, not part of
// IEcopunkScene or SceneServices.
class SceneRenderGuard {
public:
	explicit SceneRenderGuard(class ofFbo& targetFbo);
	~SceneRenderGuard();

	SceneRenderGuard(const SceneRenderGuard&) = delete;
	SceneRenderGuard& operator=(const SceneRenderGuard&) = delete;

private:
	unsigned int expectedFboId_ = 0;
	ofRectangle expectedViewport_;
	glm::mat4 savedModelViewMatrix_{1.0f};
	glm::mat4 savedProjectionMatrix_{1.0f};
	ofStyle savedStyle_;
};
