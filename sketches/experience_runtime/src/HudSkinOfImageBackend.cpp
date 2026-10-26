#include "HudSkinOfImageBackend.h"

#include "FreeImage.h" // PNG_IGNOREGAMMA

#include "ofFileUtils.h"
#include "ofGLUtils.h"
#include "ofImage.h"
#include "ofPixels.h"

#include <string>

using hudskin::SkinImageStageResult;
using hudskin::SkinLoadStage;
namespace loadcodes = hudskin::loadcodes;

namespace {

SkinImageStageResult failure(SkinLoadStage stage, const char* code, std::string detail, size_t cpuPeak) {
	SkinImageStageResult r;
	r.ok = false;
	r.stage = stage;
	r.code = code;
	r.detail = std::move(detail);
	r.cpuPeakBytes = cpuPeak;
	return r;
}

} // namespace

SkinImageStageResult HudSkinOfImageBackend::stageImage(const std::vector<uint8_t>& pngBytes,
	uint32_t requiredWidth, uint32_t requiredHeight) {
	stageCalls_++;

	// -- Decode ------------------------------------------------------------------
	ofPixels pixels;
	{
		ofBuffer buffer(reinterpret_cast<const char*>(pngBytes.data()), pngBytes.size());
		ofImageLoadSettings settings;
		settings.freeImageFlags = PNG_IGNOREGAMMA;
		if (!ofLoadImage(pixels, buffer, settings) || !pixels.isAllocated()) {
			return failure(SkinLoadStage::ImageDecode, loadcodes::DecodeFailed, "decoder rejected the PNG stream", 0);
		}
	}
	const size_t cpuBytes = pixels.getTotalBytes();
	if (pixels.getPixelFormat() != OF_PIXELS_RGBA || pixels.getNumChannels() != 4 ||
		pixels.getWidth() != requiredWidth || pixels.getHeight() != requiredHeight) {
		std::string detail = "decoded " + std::to_string(pixels.getWidth()) + "x" + std::to_string(pixels.getHeight()) +
			" with " + std::to_string(pixels.getNumChannels()) + " channel(s); required RGBA8 " +
			std::to_string(requiredWidth) + "x" + std::to_string(requiredHeight);
		pixels.clear();
		return failure(SkinLoadStage::ImageDecode, loadcodes::DecodedFormatInvalid, detail, cpuBytes);
	}

	// -- GPU staging -----------------------------------------------------------------
	while (glGetError() != GL_NO_ERROR) {} // drain errors not caused by this upload

	auto texture = std::make_unique<ofTexture>();
	texture->allocate(pixels);   // GL_RGBA8 for OF_PIXELS_RGBA
	texture->loadData(pixels);
	const GLenum glError = glGetError();
	pixels.clear(); // CPU decode buffer freed before returning, success or not

	bool injected = failNextUpload_;
	failNextUpload_ = false;
	if (failUploadSkips_ == 0) {
		injected = true;
		failUploadSkips_ = -1;
	} else if (failUploadSkips_ > 0) {
		failUploadSkips_--;
	}
	if (injected || glError != GL_NO_ERROR || !texture->isAllocated() ||
		texture->getTextureData().textureID == 0) {
		texture->clear();
		std::string detail = injected ? "injected upload failure (test hook)"
			: "texture upload failed (glGetError=" + std::to_string(glError) + ")";
		return failure(SkinLoadStage::GpuUpload, loadcodes::UploadFailed, detail, cpuBytes);
	}

	SkinImageStageResult r;
	r.ok = true;
	r.handle = nextHandle_++;
	r.gpuBytes = size_t(requiredWidth) * requiredHeight * 4; // GL_RGBA8, no mipmaps
	r.cpuPeakBytes = cpuBytes;
	textures_.emplace(r.handle, std::move(texture));
	return r;
}

void HudSkinOfImageBackend::release(uint64_t handle) {
	auto it = textures_.find(handle);
	if (it == textures_.end()) return;
	it->second->clear(); // deletes the GL texture (last reference)
	textures_.erase(it);
}

const ofTexture* HudSkinOfImageBackend::texture(uint64_t handle) const {
	auto it = textures_.find(handle);
	return it == textures_.end() ? nullptr : it->second.get();
}

unsigned int HudSkinOfImageBackend::glTextureId(uint64_t handle) const {
	const ofTexture* t = texture(handle);
	return t ? t->getTextureData().textureID : 0;
}
