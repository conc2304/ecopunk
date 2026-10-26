#pragma once

// HudSkinOfImageBackend — the real ISkinImageBackend: decodes a structurally
// checked skin PNG with openFrameworks/FreeImage and uploads it as a GPU
// texture. Requires a live GL context (ExperienceRuntime / SkinLoaderHarness).
//
// Decode/upload facts this class relies on, verified by inspecting
// libs/openFrameworks at implementation time (HUD-004 spec §4 requires
// verification, not assumption):
//   * Decode: ofLoadImage(ofPixels&, ofBuffer, settings) → FreeImage 3.19.
//     FreeImage applies gamma correction to PNGs carrying a gAMA chunk
//     unless PNG_IGNOREGAMMA is passed; it is passed here, so sRGB-authored
//     bytes reach the texture unconverted. No premultiplication exists
//     anywhere in ofImage/ofPixels/ofTexture — decoded alpha stays straight.
//   * Upload: ofTexture::allocate(ofPixels RGBA) → internal format GL_RGBA8
//     (ofGLUtils.cpp ofGetGLInternalFormatFromPixelFormat) — not
//     GL_SRGB8_ALPHA8, and nothing in OF enables GL_FRAMEBUFFER_SRGB, so no
//     sRGB↔linear conversion happens on sample or write.
//   * Blend: the renderer draws the shell under OF_BLENDMODE_ALPHA, which
//     both OF renderers map to glBlendEquation(GL_FUNC_ADD) +
//     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) — source-over for
//     straight alpha, the spec §4 pairing.

#include "HudSkinLoader.h"

#include "ofTexture.h"

#include <cstdint>
#include <map>
#include <memory>

class HudSkinOfImageBackend : public hudskin::ISkinImageBackend {
public:
	hudskin::SkinImageStageResult stageImage(const std::vector<uint8_t>& pngBytes,
		uint32_t requiredWidth, uint32_t requiredHeight) override;
	void release(uint64_t handle) override;
	size_t liveTextureCount() const override { return textures_.size(); }

	// Non-owning; valid until release(handle). nullptr for unknown handles.
	const ofTexture* texture(uint64_t handle) const;
	unsigned int glTextureId(uint64_t handle) const;

	// Deterministic GPU-upload-failure injection for the runtime harness
	// (spec §7: "GPU upload failure (using a runtime-capable harness)").
	// When set, the NEXT stageImage() performs the real decode, then fails
	// at the upload stage exactly as a real upload failure would (texture
	// released, error reported). Self-clearing after one use.
	void failNextUploadForTesting() { failNextUpload_ = true; }
	// Reactive Control Artwork v0.1: fail the upload of the stageImage() call
	// AFTER `skipCalls` successful ones (1 = the optional artwork staged right
	// after a skin's shell). Self-clearing after one use.
	void failUploadAfterForTesting(int skipCalls) { failUploadSkips_ = skipCalls; }
	// Total stageImage() calls (decode + upload) — proves assets are staged at
	// skin load only, never per frame.
	uint64_t stageCallCount() const { return stageCalls_; }

private:
	std::map<uint64_t, std::unique_ptr<ofTexture>> textures_;
	uint64_t nextHandle_ = 1;
	bool failNextUpload_ = false;
	int failUploadSkips_ = -1;
	uint64_t stageCalls_ = 0;
};
