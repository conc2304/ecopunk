#pragma once

// HudSkinLoader — the HUD-004 first shell-only production loader core
// (HUD-004 Skin Package Specification v1, frozen 2026-10-08; First
// Shell-Only Production Loader Implementation Authorization).
//
// openFrameworks-independent by design: everything that needs a GL
// context (PNG decode into ofPixels, texture upload, release) is behind
// ISkinImageBackend, so the whole state machine — stage order, atomic
// activation, failed-replacement preservation, candidate release — is
// covered headlessly by test/hud_skin_loader_tests.cpp with a fake backend,
// and the real backend (HudSkinOfImageBackend) is exercised in a live GL
// context by SkinLoaderHarness.
//
// Candidate flow (spec §5), each stage gating the next:
//   Selection → PackageRoot → Manifest (the ACCEPTED tools/hud-skin-manifest
//   validator, unchanged — not a second parser) → Capability (first-loader
//   shell-only policy) → ImageRead → ImageStructure (HudSkinPng) →
//   ImageDecode / GpuUpload (backend) → Activation.
// Any failure releases the candidate's resources and leaves the active
// skin (if any) untouched; there is never a partially active skin.
//
// No skin-specific logic: nothing here branches on a skin ID beyond
// requiring that the package directory name equals the manifest skinId
// (spec §3, "assets/hud/skins/<skin-id>/").

#include "HudSkinManifest.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace hudskin {

enum class SkinLoadStage : uint8_t {
	Selection,
	PackageRoot,
	Manifest,
	Capability,
	ImageRead,
	ImageStructure,
	ImageDecode,
	GpuUpload,
	Activation
};

const char* stageName(SkinLoadStage stage);

namespace loadcodes {
	inline constexpr const char* NoSelection = "SKIN_LOAD_NO_SELECTION";
	inline constexpr const char* SelectionInvalid = "SKIN_LOAD_SELECTION_INVALID";
	inline constexpr const char* NotConfigured = "SKIN_LOAD_NOT_CONFIGURED";
	inline constexpr const char* PackageNotFound = "SKIN_LOAD_PACKAGE_NOT_FOUND";
	inline constexpr const char* SkinIdMismatch = "SKIN_LOAD_SKIN_ID_MISMATCH";
	inline constexpr const char* UnsupportedClass = "SKIN_LOAD_UNSUPPORTED_CLASS";
	inline constexpr const char* ImageUnreadable = "SKIN_LOAD_IMAGE_UNREADABLE";
	inline constexpr const char* ImageWrongSize = "SKIN_LOAD_IMAGE_WRONG_SIZE";
	inline constexpr const char* Activated = "SKIN_LOAD_ACTIVATED";
	// Backend-reported (ImageDecode / GpuUpload stages):
	inline constexpr const char* DecodeFailed = "SKIN_LOAD_DECODE_FAILED";
	inline constexpr const char* DecodedFormatInvalid = "SKIN_LOAD_DECODED_FORMAT_INVALID";
	inline constexpr const char* UploadFailed = "SKIN_LOAD_UPLOAD_FAILED";
} // namespace loadcodes

// Result of decoding + uploading one image. A backend must leave NOTHING
// allocated when ok == false, and must free its CPU decode buffer before
// returning either way (only the GPU texture survives a success).
struct SkinImageStageResult {
	bool ok = false;
	SkinLoadStage stage = SkinLoadStage::ImageDecode; // stage that failed, when !ok
	std::string code;
	std::string detail;
	uint64_t handle = 0;      // backend-owned texture handle, valid when ok
	size_t gpuBytes = 0;      // resident bytes of the uploaded texture
	size_t cpuPeakBytes = 0;  // transient CPU bytes held during decode/upload
};

class ISkinImageBackend {
public:
	virtual ~ISkinImageBackend() = default;
	// `pngBytes` has already passed checkPngStructure(); the backend must
	// still verify the DECODED pixels are RGBA8 at exactly width x height.
	virtual SkinImageStageResult stageImage(const std::vector<uint8_t>& pngBytes,
		uint32_t requiredWidth, uint32_t requiredHeight) = 0;
	virtual void release(uint64_t handle) = 0;
	virtual size_t liveTextureCount() const = 0;
};

struct SkinLoaderConfig {
	std::filesystem::path skinsRoot; // canonical: <repo>/assets/hud/skins
	// Caller-supplied expected Blueprint pair — passed straight to the
	// accepted validator, which fails closed if either is unset.
	SkinManifestValidationPolicy expectedBlueprint;
};

struct ActiveSkin {
	std::string skinId;
	std::string skinVersion;
	uint64_t handle = 0;
	size_t gpuBytes = 0;
};

struct SkinLoadOutcome {
	bool activated = false;
	SkinLoadStage stage = SkinLoadStage::Selection; // failing stage, or Activation on success
	std::string code;
	std::string detail;
	std::string asset; // manifest field path or package-relative file, when applicable
	std::string selection;
	bool hadActiveBefore = false;
	bool retainedPreviousActive = false; // failure with an earlier skin kept active
	bool releasedPrevious = false;       // success that retired an earlier skin
	std::optional<std::string> activeSkinIdAfter;
	size_t peakGpuBytesDuringRequest = 0; // active + staged candidate
	size_t cpuPeakBytes = 0;              // PNG file bytes + backend decode buffer
};

class HudSkinLoader {
public:
	explicit HudSkinLoader(ISkinImageBackend& backend) : backend_(backend) {}

	void configure(SkinLoaderConfig config) { config_ = std::move(config); configured_ = true; }
	const SkinLoaderConfig& config() const { return config_; }

	// Validate, decode and stage `selection` (a package directory name under
	// skinsRoot); activate atomically only if every stage passes.
	SkinLoadOutcome requestSkin(const std::string& selection);

	// Releases the active skin's resources (shutdown / explicit unload).
	void shutdown();

	const std::optional<ActiveSkin>& active() const { return active_; }
	size_t steadyGpuBytes() const { return active_ ? active_->gpuBytes : 0; }
	size_t lifetimePeakGpuBytes() const { return lifetimePeakGpuBytes_; }
	size_t lifetimePeakCpuBytes() const { return lifetimePeakCpuBytes_; }

private:
	SkinLoadOutcome finish(SkinLoadOutcome outcome);

	ISkinImageBackend& backend_;
	SkinLoaderConfig config_;
	bool configured_ = false;
	std::optional<ActiveSkin> active_;
	size_t lifetimePeakGpuBytes_ = 0;
	size_t lifetimePeakCpuBytes_ = 0;
};

} // namespace hudskin
