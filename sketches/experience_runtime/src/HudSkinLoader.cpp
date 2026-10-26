#include "HudSkinLoader.h"

#include "HudSkinPng.h"
#include "HudReactiveControlArt.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

namespace hudskin {

namespace fs = std::filesystem;

const char* stageName(SkinLoadStage stage) {
	switch (stage) {
		case SkinLoadStage::Selection: return "selection";
		case SkinLoadStage::PackageRoot: return "package-root";
		case SkinLoadStage::Manifest: return "manifest";
		case SkinLoadStage::Capability: return "capability";
		case SkinLoadStage::ImageRead: return "image-read";
		case SkinLoadStage::ImageStructure: return "image-structure";
		case SkinLoadStage::ImageDecode: return "image-decode";
		case SkinLoadStage::GpuUpload: return "gpu-upload";
		case SkinLoadStage::Activation: return "activation";
	}
	return "unknown";
}

namespace {

// A selection names one directory directly under skinsRoot. Restricting it
// to the skinId character set before touching the filesystem keeps it from
// ever naming a path ("..", "a/b", absolute). The full skinId grammar is then
// enforced by the accepted validator on the manifest, and the two must match.
bool isSafeSelection(const std::string& s) {
	if (s.empty()) return false;
	return std::all_of(s.begin(), s.end(), [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
	});
}

} // namespace

SkinLoadOutcome HudSkinLoader::requestSkin(const std::string& selection) {
	SkinLoadOutcome o;
	o.selection = selection;
	o.hadActiveBefore = active_.has_value();
	const size_t activeBytes = steadyGpuBytes();
	o.peakGpuBytesDuringRequest = activeBytes;

	auto reject = [&](SkinLoadStage stage, const char* code, std::string detail, std::string asset = {}) {
		o.activated = false;
		o.stage = stage;
		o.code = code;
		o.detail = std::move(detail);
		o.asset = std::move(asset);
		return finish(std::move(o));
	};

	// -- Selection -----------------------------------------------------------
	if (selection.empty()) {
		return reject(SkinLoadStage::Selection, loadcodes::NoSelection, "no skin selected");
	}
	if (!isSafeSelection(selection)) {
		return reject(SkinLoadStage::Selection, loadcodes::SelectionInvalid,
			"selection must contain only [a-z0-9-]: \"" + selection + "\"");
	}
	if (!configured_) {
		return reject(SkinLoadStage::Selection, loadcodes::NotConfigured, "skin loader not configured");
	}

	// -- Package root ----------------------------------------------------------
	const fs::path packageRoot = config_.skinsRoot / selection;
	std::error_code ec;
	if (!fs::is_directory(fs::symlink_status(packageRoot, ec)) || ec) {
		// symlink_status: a symlinked package directory is not followed.
		return reject(SkinLoadStage::PackageRoot, loadcodes::PackageNotFound,
			"no package directory (symlinks not followed): " + packageRoot.string());
	}

	// -- Manifest: the accepted validator, unchanged -----------------------------
	SkinManifestResult manifest = validateSkinPackage(packageRoot, config_.expectedBlueprint);
	if (!manifest.valid) {
		const SkinDiagnostic* first = nullptr;
		for (const auto& d : manifest.diagnostics) {
			if (d.severity == SkinDiagnosticSeverity::Error) { first = &d; break; }
		}
		std::string code = first ? first->code : "SKIN_MANIFEST_INVALID";
		std::string detail = first ? first->message : "manifest invalid";
		if (manifest.errorCount() > 1) detail += " (+" + std::to_string(manifest.errorCount() - 1) + " more)";
		o.activated = false;
		o.stage = SkinLoadStage::Manifest;
		o.code = std::move(code);
		o.detail = std::move(detail);
		o.asset = first ? first->fieldPath : std::string{};
		return finish(std::move(o));
	}
	const SkinManifest& m = *manifest.manifest;
	if (m.skinId != selection) {
		return reject(SkinLoadStage::Manifest, loadcodes::SkinIdMismatch,
			"package directory \"" + selection + "\" does not match manifest skinId \"" + m.skinId + "\"", "skinId");
	}

	// -- Capability: first loader is shell-only (spec §3) ------------------------
	// Decoration and emissive are schema-recognized but NOT supported by this
	// loader: a declaration rejects the whole candidate, never silently ignored.
	// (Atlas, masks and other classes are already rejected by the validator.)
	if (m.decoration) {
		return reject(SkinLoadStage::Capability, loadcodes::UnsupportedClass,
			"asset class 'decoration' is not supported by the first shell-only loader", "assets.decoration");
	}
	if (m.emissive) {
		return reject(SkinLoadStage::Capability, loadcodes::UnsupportedClass,
			"asset class 'emissive' is not supported by the first shell-only loader", "assets.emissive");
	}

	// -- Image read (path already proven safe by the validator) ------------------
	const fs::path shellFile = packageRoot / m.shell.path;
	std::vector<uint8_t> bytes;
	{
		std::ifstream in(shellFile, std::ios::binary);
		if (!in) {
			return reject(SkinLoadStage::ImageRead, loadcodes::ImageUnreadable, "cannot open shell image", m.shell.path);
		}
		bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		if (in.bad()) {
			return reject(SkinLoadStage::ImageRead, loadcodes::ImageUnreadable, "error reading shell image", m.shell.path);
		}
	}
	o.cpuPeakBytes = bytes.size();

	// -- Image structure -----------------------------------------------------------
	const PngStructureResult png = checkPngStructure(bytes.data(), bytes.size());
	if (!png.ok) {
		return reject(SkinLoadStage::ImageStructure, png.code.c_str(), png.detail, m.shell.path);
	}
	const uint32_t requiredW = *m.shell.expectedWidth;  // validator: mandatory, == 1280
	const uint32_t requiredH = *m.shell.expectedHeight; // validator: mandatory, == 720
	if (png.width != requiredW || png.height != requiredH) {
		return reject(SkinLoadStage::ImageStructure, loadcodes::ImageWrongSize,
			"PNG is " + std::to_string(png.width) + "x" + std::to_string(png.height) + ", required " +
			std::to_string(requiredW) + "x" + std::to_string(requiredH), m.shell.path);
	}

	// -- Decode + GPU staging (backend) ----------------------------------------------
	SkinImageStageResult staged = backend_.stageImage(bytes, requiredW, requiredH);
	o.cpuPeakBytes = bytes.size() + staged.cpuPeakBytes;
	bytes.clear();
	bytes.shrink_to_fit();
	if (!staged.ok) {
		return reject(staged.stage, staged.code.c_str(), staged.detail, m.shell.path);
	}
	o.peakGpuBytesDuringRequest = activeBytes + staged.gpuBytes;

	// -- Optional artwork (Reactive Control Artwork v0.1) ----------------------------
	// Only after the required shell is fully staged, so a rejected candidate
	// never stages it. Nonfatal by construction: no path out of here rejects.
	std::optional<OptionalSkinImage> pressed = stageOptionalImage(packageRoot, hudpresent::reactive::kSceneNextPressedPath,
		config_.pressedSpriteWidth, config_.pressedSpriteHeight,
		"controls.scene_next region + 4 px bleed, snapped outward — HUD-006 v2", o);
	if (pressed) o.peakGpuBytesDuringRequest += pressed->gpuBytes;
	const std::string faceHint = "navigation control region + 4 px bleed per side — HUD-006 v2";
	auto stageFace = [&](const char* path) {
		auto img = stageOptionalImage(packageRoot, path, config_.buttonFaceWidth, config_.buttonFaceHeight, faceHint, o);
		if (img) o.peakGpuBytesDuringRequest += img->gpuBytes;
		return img;
	};
	std::optional<OptionalSkinImage> idleFace = stageFace(hudpresent::reactive::kButtonIdlePath);
	std::optional<OptionalSkinImage> focusedFace = stageFace(hudpresent::reactive::kButtonFocusedPath);
	std::optional<OptionalSkinImage> activeFace = stageFace(hudpresent::reactive::kButtonActivePath);

	// -- Atomic activation -----------------------------------------------------------
	std::optional<ActiveSkin> previous = std::move(active_);
	active_ = ActiveSkin{m.skinId, m.skinVersion, staged.handle, staged.gpuBytes, pressed, idleFace, focusedFace, activeFace};
	o.sceneNextPressedLoaded = pressed.has_value();
	o.buttonFacesLoaded = int(idleFace.has_value()) + int(focusedFace.has_value()) + int(activeFace.has_value());
	if (previous) {
		releaseSkin(*previous); // shell + its optional artwork
		o.releasedPrevious = true;
	}
	o.activated = true;
	o.stage = SkinLoadStage::Activation;
	o.code = loadcodes::Activated;
	o.detail = "activated " + m.skinId + " " + m.skinVersion;
	return finish(std::move(o));
}

SkinLoadOutcome HudSkinLoader::finish(SkinLoadOutcome o) {
	o.retainedPreviousActive = !o.activated && o.hadActiveBefore && active_.has_value();
	o.activeSkinIdAfter = active_ ? std::optional<std::string>(active_->skinId) : std::nullopt;
	lifetimePeakGpuBytes_ = std::max(lifetimePeakGpuBytes_, o.peakGpuBytesDuringRequest);
	lifetimePeakCpuBytes_ = std::max(lifetimePeakCpuBytes_, o.cpuPeakBytes);
	return o;
}

void HudSkinLoader::releaseSkin(const ActiveSkin& skin) {
	backend_.release(skin.handle);
	for (const auto* img : {&skin.sceneNextPressed, &skin.buttonIdle, &skin.buttonFocused, &skin.buttonActive}) {
		if (*img) backend_.release((*img)->handle);
	}
}

std::optional<OptionalSkinImage> HudSkinLoader::stageOptionalImage(const fs::path& packageRoot, const std::string& relativePath,
	uint32_t requiredWidth, uint32_t requiredHeight, const std::string& sizeHint, SkinLoadOutcome& o) {
	const std::string asset = relativePath;
	auto diagnose = [&](SkinLoadStage stage, const std::string& code, std::string detail) {
		o.optionalDiagnostics.push_back(SkinOptionalDiagnostic{asset, stage, code, std::move(detail)});
		return std::optional<OptionalSkinImage>{};
	};

	// Walk the fixed path component by component without following symlinks,
	// so the asset can never resolve outside the package. Absent = no
	// diagnostic at all: the artwork is optional.
	std::error_code ec;
	fs::path cursor = packageRoot;
	std::vector<std::string> parts;
	for (size_t start = 0; start <= relativePath.size();) {
		const size_t slash = relativePath.find('/', start);
		parts.push_back(relativePath.substr(start, slash == std::string::npos ? std::string::npos : slash - start));
		if (slash == std::string::npos) break;
		start = slash + 1;
	}
	for (size_t index = 0; index < parts.size(); ++index) {
		const bool isFile = index + 1 == parts.size();
		cursor /= parts[index];
		const fs::file_status st = fs::symlink_status(cursor, ec);
		if (ec || st.type() == fs::file_type::not_found) return std::nullopt; // absent
		const bool ok = isFile ? fs::is_regular_file(st) : fs::is_directory(st);
		if (!ok) {
			return diagnose(SkinLoadStage::ImageRead, loadcodes::OptionalNotRegularFile,
				"'" + cursor.lexically_relative(packageRoot).generic_string() + "' is not a " +
				(isFile ? "regular file" : "directory") + " (symlinks are not followed)");
		}
	}

	std::vector<uint8_t> bytes;
	{
		std::ifstream in(cursor, std::ios::binary);
		if (!in) return diagnose(SkinLoadStage::ImageRead, loadcodes::OptionalUnreadable, "cannot open");
		bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		if (in.bad()) return diagnose(SkinLoadStage::ImageRead, loadcodes::OptionalUnreadable, "read error");
	}

	// Same structural rules as the shell: non-indexed RGBA8, intact chunks.
	const PngStructureResult png = checkPngStructure(bytes.data(), bytes.size());
	if (!png.ok) return diagnose(SkinLoadStage::ImageStructure, png.code, png.detail);
	if (png.width != requiredWidth || png.height != requiredHeight) {
		return diagnose(SkinLoadStage::ImageStructure, loadcodes::OptionalWrongSize,
			"PNG is " + std::to_string(png.width) + "x" + std::to_string(png.height) + ", required " +
			std::to_string(requiredWidth) + "x" + std::to_string(requiredHeight) + " (" + sizeHint + ")");
	}

	// Decode + upload once, by the same backend (straight alpha, RGBA8, no
	// gamma conversion — identical pipeline to the shell).
	SkinImageStageResult staged = backend_.stageImage(bytes, png.width, png.height);
	o.cpuPeakBytes = std::max(o.cpuPeakBytes, bytes.size() + staged.cpuPeakBytes);
	if (!staged.ok) return diagnose(staged.stage, staged.code, staged.detail);
	return OptionalSkinImage{staged.handle, staged.gpuBytes, png.width, png.height};
}

void HudSkinLoader::shutdown() {
	if (active_) {
		releaseSkin(*active_);
		active_.reset();
	}
}

} // namespace hudskin
