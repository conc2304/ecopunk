#include "SceneRenderGuard.h"

#include "ofFbo.h"
#include "ofGLUtils.h"

namespace {
// Scene-HUD-Contract-v1.md §5's baseline: "Alpha blending enabled,
// runtime's standard blend mode."
constexpr ofBlendMode kStandardBlendMode = OF_BLENDMODE_ALPHA;
}

SceneRenderGuard::SceneRenderGuard(ofFbo& targetFbo) {
	expectedFboId_ = targetFbo.getId();
	expectedViewport_ = ofRectangle(0, 0, targetFbo.getWidth(), targetFbo.getHeight());

	savedModelViewMatrix_ = ofGetCurrentMatrix(OF_MATRIX_MODELVIEW);
	savedProjectionMatrix_ = ofGetCurrentMatrix(OF_MATRIX_PROJECTION);
	savedStyle_ = ofGetStyle();
}

SceneRenderGuard::~SceneRenderGuard() {
	// -- Scissor: restored directly (forced disabled) -------------------
	glDisable(GL_SCISSOR_TEST);

	// -- Stencil: restored directly (forced disabled) --------------------
	glDisable(GL_STENCIL_TEST);

	// -- Blend mode: restored directly (forced to the standard mode) -----
	ofEnableBlendMode(kStandardBlendMode);

	// -- Shader: restored directly (forced unbind) ------------------------
	glUseProgram(0);

	// -- Texture: restored directly (forced unbind on unit 0 only; a
	//    scene that left a *different* texture unit bound is NOT covered
	//    by this — see the implementation report's GL-restoration section
	//    for this as a named, deliberately-scoped-out follow-up) ---------
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	// -- Viewport: restored directly to the FBO's own explicit size,
	//    not to a value saved-then-blindly-reapplied ---------------------
	ofViewport(expectedViewport_);

	// -- Matrices: restored directly via explicit captured-value reload,
	//    not via ofPopMatrix()/ofPushMatrix() — this recovers correctly
	//    even if the scene left its own push/pop calls unbalanced, which
	//    a pure pop-based restore could not guarantee -------------------
	ofSetMatrixMode(OF_MATRIX_PROJECTION);
	ofLoadMatrix(savedProjectionMatrix_);
	ofSetMatrixMode(OF_MATRIX_MODELVIEW);
	ofLoadMatrix(savedModelViewMatrix_);

	// -- Style: restored directly via explicit captured-value reload,
	//    same reasoning as matrices (ofGetStyle()/ofSetStyle() are the
	//    same underlying data ofPushStyle()/ofPopStyle() wraps; using the
	//    accessor pair directly avoids relying on the scene's own
	//    push/pop discipline) -----------------------------------------
	ofSetStyle(savedStyle_);

	// -- Framebuffer: restored directly, rebinding only if the currently
	//    bound framebuffer isn't the one this guard expects ------------
	GLint currentFbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFbo);
	if (static_cast<GLuint>(currentFbo) != expectedFboId_) {
		glBindFramebuffer(GL_FRAMEBUFFER, expectedFboId_);
	}
}
