#include "TFAmbientTextureLayer.h"
#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated, no behavior change)
#include <set>
#include "ofFileUtils.h"
#include "ofGraphics.h"
#include "ofJson.h"
#include "ofLog.h"
#include "ofMath.h"
#include "ofGLUtils.h"
#include <cstdlib>
#include <fstream>
#if defined(__APPLE__)
#include <mach/mach.h>
#endif
#if defined(__linux__)
#include <unistd.h>
#endif

namespace {
// Process resident set size in MiB, or -1 if unavailable. Diagnostic only:
// RSS is not a GPU-memory measure (drivers may or may not mirror texture
// storage into the process).
double residentMiB() {
#if defined(__APPLE__)
	mach_task_basic_info_data_t info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
		return static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
	}
#elif defined(__linux__)
	std::ifstream statm("/proc/self/statm");
	long pages = 0, residentPages = 0;
	if (statm >> pages >> residentPages) {
		return static_cast<double>(residentPages) * static_cast<double>(sysconf(_SC_PAGESIZE)) / (1024.0 * 1024.0);
	}
#endif
	return -1.0;
}

size_t textureBytes(const ofImage& img, bool padRgbToRgba) {
	const ofTexture& t = img.getTexture();
	if (!t.isAllocated()) return 0;
	const ofTextureData& d = t.getTextureData();
	size_t channels = static_cast<size_t>(ofGetNumChannelsFromGLFormat(ofGetGLFormatFromInternal(d.glInternalFormat)));
	if (padRgbToRgba && channels == 3) channels = 4;
	return static_cast<size_t>(d.width) * static_cast<size_t>(d.height) * channels;
}
} // namespace

size_t TFAmbientTextureLayer::tintPixelBytes() const {
	size_t total = 0;
	for (const auto& t : textures) total += t.tint.getPixels().getTotalBytes();
	return total;
}

size_t TFAmbientTextureLayer::maskPixelBytes() const {
	size_t total = 0;
	for (const auto& t : textures) total += t.mask.getPixels().getTotalBytes();
	return total;
}

size_t TFAmbientTextureLayer::residentTintTextures() const {
	size_t n = 0;
	for (const auto& t : textures) n += t.tint.getTexture().isAllocated() ? 1 : 0;
	return n;
}

size_t TFAmbientTextureLayer::residentMaskTextures() const {
	size_t n = 0;
	for (const auto& t : textures) n += t.mask.getTexture().isAllocated() ? 1 : 0;
	return n;
}

size_t TFAmbientTextureLayer::gpuBytesEstimate(bool padRgbToRgba) const {
	size_t total = 0;
	for (const auto& t : textures) total += textureBytes(t.tint, padRgbToRgba) + textureBytes(t.mask, padRgbToRgba);
	return total;
}

bool TFAmbientTextureLayer::ensureMaskLoaded(size_t index) {
	if (index >= textures.size()) return false;
	TFBackgroundTexture& tex = textures[index];
	if (tex.mask.getTexture().isAllocated()) return true;
	RT004_SCOPE("TFAmbient.lazyLoad.mask");
	// Same result as ofImage::load() (decode into the image's pixels, then
	// update() uploads), split so decode and upload are timed separately.
	ofPixels px;
	bool ok;
	{ RT004_SCOPE("TFAmbient.lazyLoad.mask.decode"); ok = ofLoadImage(px, tex.maskPath); }
	if (!ok) {
		ofLogWarning("TFAmbientTextureLayer") << "lazy mask load failed: " << tex.maskPath;
		return false;
	}
	{
		RT004_SCOPE("TFAmbient.lazyLoad.mask.upload");
		tex.mask.getPixels() = std::move(px);
		tex.mask.update();
	}
	if (residencyMode == Residency::Bounded && tex.mask.getTexture().isAllocated()) tex.mask.getPixels().clear();
	return tex.mask.getTexture().isAllocated();
}

bool TFAmbientTextureLayer::releaseMask(size_t index) {
	if (index >= textures.size()) return false;
	TFBackgroundTexture& tex = textures[index];
	RT004_SCOPE("TFAmbient.releaseMask");
	tex.mask.clear(); // CPU pixels + GL texture; maskPath and the file on disk are kept
	return !tex.mask.getTexture().isAllocated();
}

void TFAmbientTextureLayer::setup(int canvasW_, int canvasH_, const std::string& folderPath) {
	canvasW = canvasW_;
	canvasH = canvasH_;
	group.setName("Background textures");

	const char* residencyEnv = std::getenv("EXPERIENCE_RUNTIME_TEMPORAL_AMBIENT_RESIDENCY");
	residencyMode = (residencyEnv != nullptr && std::string(residencyEnv) == "legacy") ? Residency::Legacy : Residency::Bounded;
	const bool bounded = residencyMode == Residency::Bounded;
	const double rssBefore = residentMiB();
	size_t tintPixelsKept = 0; // bounded: tints whose texture did not allocate keep their pixels

	ofDirectory dir;
	dir.allowExt("png");
	dir.listDir(folderPath);

	std::set<std::string> allNames;
	for (std::size_t i = 0; i < dir.size(); i++) {
		allNames.insert(dir.getName(i));
	}

	std::string manifestPath = ofFilePath::join(folderPath, "textures_manifest.json");
	ofJson manifest;
	if (ofFile::doesFileExist(manifestPath)) {
		manifest = ofLoadJson(manifestPath);
	} else {
		ofLogWarning("TFAmbientTextureLayer") << "no textures_manifest.json found in " << folderPath
			<< " -- every texture will fall back to generic defaults";
	}

	const std::string kTintSuffix = "_tint.png";
	const std::string kMaskSuffix = "_mask.png";

	std::set<std::string> handledMasks;

	// Each pair is loaded in place inside `textures` (reserved up front) — a
	// local TFBackgroundTexture pushed by value made ofImage's copy
	// constructor duplicate every decoded image AND re-upload its GL texture,
	// once per push plus again for every earlier entry on each vector
	// reallocation. That copy churn, not the PNG decode itself, was most of
	// Temporal's ~2.2 s first-activation stall.
	textures.clear();
	textures.reserve(dir.size() / 2 + 1);

	for (std::size_t i = 0; i < dir.size(); i++) {
		std::string name = dir.getName(i);
		if (name.size() <= kTintSuffix.size()
			|| name.compare(name.size() - kTintSuffix.size(), kTintSuffix.size(), kTintSuffix) != 0) {
			continue;
		}

		std::string baseName = name.substr(0, name.size() - kTintSuffix.size());
		std::string maskName = baseName + kMaskSuffix;
		if (allNames.find(maskName) == allNames.end()) {
			ofLogWarning("TFAmbientTextureLayer") << "orphaned tint texture, no matching mask -- skipping: " << name;
			continue;
		}
		handledMasks.insert(maskName);

		textures.emplace_back();
		TFBackgroundTexture& tex = textures.back();
		tex.baseName = baseName;

		std::string tintPath = ofFilePath::join(folderPath, name);
		std::string maskPath = ofFilePath::join(folderPath, maskName);

		bool tintOk;
		{ RT004_SCOPE("TFAmbient.load.tint"); tintOk = tex.tint.load(tintPath); }
		if (!tintOk) {
			ofLogWarning("TFAmbientTextureLayer") << "failed to load tint image, skipping: " << tintPath;
			textures.pop_back();
			continue;
		}
		tex.maskPath = maskPath;
		if (!bounded) {
			bool maskOk;
			{ RT004_SCOPE("TFAmbient.load.mask"); maskOk = tex.mask.load(maskPath); }
			if (!maskOk) {
				ofLogWarning("TFAmbientTextureLayer") << "failed to load mask image, skipping: " << maskPath;
				textures.pop_back();
				continue;
			}
		}
		// Bounded: the pair still requires its mask file on disk (checked
		// above), but nothing samples the mask, so it is not decoded here.
		if (bounded) {
			// Drawing only binds tint.getTexture() (ofImage::draw -> renderer
			// -> texture); width/height are ofImage members, not read from the
			// pixels. Release the CPU copy only once the texture exists.
			if (tex.tint.getTexture().isAllocated()) {
				tex.tint.getPixels().clear();
			} else {
				tintPixelsKept++;
			}
		}

		std::string displayName = "unknown";
		std::string layerStr = "underlay";
		std::string blendModeStr = "alpha";
		float opacityDefault = 0.10f;
		bool spansBothZones = false;

		if (manifest.contains(baseName)) {
			const ofJson& entry = manifest[baseName];
			displayName = entry.value("display_name", displayName);
			layerStr = entry.value("layer", layerStr);
			blendModeStr = entry.value("blend_mode", blendModeStr);
			opacityDefault = entry.value("opacity_default", opacityDefault);
			spansBothZones = entry.value("spans_both_zones", spansBothZones);
		} else {
			ofLogWarning("TFAmbientTextureLayer") << "no manifest entry for " << baseName
				<< " -- add one to textures_manifest.json; using generic defaults";
		}

		tex.displayName = displayName;
		tex.layer = (layerStr == "overlay") ? TFBackgroundTexture::Layer::OVERLAY : TFBackgroundTexture::Layer::UNDERLAY;
		tex.spansBothZones = spansBothZones;

		if (blendModeStr == "screen") {
			tex.blendMode = OF_BLENDMODE_SCREEN;
		} else if (blendModeStr == "multiply") {
			tex.blendMode = OF_BLENDMODE_MULTIPLY;
		} else {
			if (blendModeStr != "alpha") {
				ofLogWarning("TFAmbientTextureLayer") << "unknown blend_mode '" << blendModeStr << "' for " << baseName
					<< " -- falling back to alpha";
			}
			tex.blendMode = OF_BLENDMODE_ALPHA;
		}

		opacityDefault = ofClamp(opacityDefault, 0.0f, kMaxOpacity);
		std::string paramName = (displayName.empty() || displayName == "unknown")
			? ("unknown-" + baseName.substr(0, 8))
			: displayName;
		tex.opacity.set(paramName, opacityDefault, 0.0f, kMaxOpacity);

		group.add(tex.opacity);
	}

	for (std::size_t i = 0; i < dir.size(); i++) {
		std::string name = dir.getName(i);
		if (name.size() <= kMaskSuffix.size()
			|| name.compare(name.size() - kMaskSuffix.size(), kMaskSuffix.size(), kMaskSuffix) != 0) {
			continue;
		}
		if (handledMasks.find(name) == handledMasks.end()) {
			ofLogWarning("TFAmbientTextureLayer") << "orphaned mask texture, no matching tint -- skipping: " << name;
		}
	}

	loaded = true;
	ofLogNotice("TFAmbientTextureLayer") << "loaded " << textures.size() << " background texture(s) from " << folderPath;

	const GLenum glErr = glGetError(); // one check after the whole load, not per-frame polling
	const double rssAfter = residentMiB();
	auto mib = [](size_t b) { return ofToString(b / (1024.0 * 1024.0), 1); };
	const std::string report = std::string("residency=") + (bounded ? "bounded" : "legacy")
		+ " pairs=" + ofToString(textures.size())
		+ " tintTextures=" + ofToString(residentTintTextures()) + " maskTextures=" + ofToString(residentMaskTextures())
		+ " cpuTintMiB=" + mib(tintPixelBytes()) + " cpuMaskMiB=" + mib(maskPixelBytes())
		+ " gpuEstMiB=" + mib(gpuBytesEstimate()) + " (rgb-padded " + mib(gpuBytesEstimate(true)) + ")"
		+ " tintPixelsKept=" + ofToString(tintPixelsKept)
		+ " rssBeforeMiB=" + ofToString(rssBefore, 0) + " rssAfterMiB=" + ofToString(rssAfter, 0)
		+ " glError=0x" + ofToHex(static_cast<int>(glErr));
	ofLogNotice("TFAmbientTextureLayer") << report;
	rt004::Probe::get().note("temporal.ambientResidency", report);
	if (glErr != GL_NO_ERROR) ofLogError("TFAmbientTextureLayer") << "GL error after ambient load: 0x" << ofToHex(static_cast<int>(glErr));
}

void TFAmbientTextureLayer::drawLayer(TFBackgroundTexture::Layer layer) const {
	if (!blendFadeShaderLoaded) {
		blendFadeShaderLoaded = true;
		if (!blendFadeShader.load("shaders/textureBlendFade.vert", "shaders/textureBlendFade.frag")) {
			ofLogError("TFAmbientTextureLayer") << "failed to load textureBlendFade shader "
				<< "-- SCREEN/MULTIPLY textures will fall back to plain alpha blending";
		}
	}

	ofEnableAlphaBlending();
	for (const auto& tex : textures) {
		if (tex.layer != layer) {
			continue;
		}
		float alpha = ofClamp(tex.opacity.get(), 0.0f, kMaxOpacity);
		if (alpha <= 0.0f) {
			continue;
		}

		bool useShader = (tex.blendMode == OF_BLENDMODE_SCREEN || tex.blendMode == OF_BLENDMODE_MULTIPLY)
			&& blendFadeShader.isLoaded();

		ofEnableBlendMode(tex.blendMode);
		if (useShader) {
			// oF's built-in SCREEN/MULTIPLY glBlendFunc combos don't
			// reference source alpha the way ALPHA blending does (SCREEN
			// ignores it entirely; MULTIPLY saturates near 1.0 at low
			// alpha), so the opacity slider would have no visible effect
			// through ofSetColor's alpha channel alone. The shader instead
			// pre-tints the source color toward that blend mode's identity
			// color (black for screen, white for multiply) by `opacity`,
			// which does fade smoothly to "no effect" at opacity 0.
			ofFloatColor identity = (tex.blendMode == OF_BLENDMODE_MULTIPLY)
				? ofFloatColor(1.0f, 1.0f, 1.0f)
				: ofFloatColor(0.0f, 0.0f, 0.0f);
			blendFadeShader.begin();
			blendFadeShader.setUniformTexture("tex", tex.tint.getTexture(), 0);
			blendFadeShader.setUniform1f("opacity", alpha);
			blendFadeShader.setUniform3f("identityColor", identity);
			tex.tint.draw(0, 0, canvasW, canvasH);
			blendFadeShader.end();
		} else {
			// OF_BLENDMODE_ALPHA (or the shader failed to load) -- opacity
			// via vertex-color alpha works correctly for this blend func.
			ofSetColor(255, 255, 255, alpha * 255.0f);
			tex.tint.draw(0, 0, canvasW, canvasH);
		}
		// Reset before the next texture/draw call -- leaving a non-alpha
		// blend mode active would corrupt whatever draws after this (the
		// next texture, fragments, HUD).
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
	}
	ofSetColor(255);
}

void TFAmbientTextureLayer::drawUnderlay() const {
	drawLayer(TFBackgroundTexture::Layer::UNDERLAY);
}

void TFAmbientTextureLayer::drawOverlay() const {
	drawLayer(TFBackgroundTexture::Layer::OVERLAY);
}
