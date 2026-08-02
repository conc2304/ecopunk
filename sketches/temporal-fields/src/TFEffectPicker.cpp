#include "TFEffectPicker.h"
#include "DefaultVideoEffectCatalog.h"
#include "TFRandom.h"
#include "TFTextureCropFill.h"
#include "VideoEffectRegistry.h"
#include "VideoEffectTypes.h"
#include "ofColor.h"
#include "ofGraphics.h"
#include "ofMath.h"
#include "ofUtils.h"
#include <utility>
#include <vector>

namespace {
	// Lazily-built, process-lifetime registry of the canonical Contract-A
	// single-pass catalog — see applyEffectUniforms()'s fallback branch
	// below. Mirrors shared/src/VideoRegionEffectRenderer.cpp's identical helper.
	const videoeffects::VideoEffectRegistry & tfCatalogRegistry() {
		static videoeffects::VideoEffectRegistry registry = [] {
			videoeffects::VideoEffectRegistry r;
			videoeffects::registerSinglePassEffects(r);
			return r;
		}();
		return registry;
	}
}

void TFEffectPicker::setup(ShaderLibrary* shaderLib_) {
	shaderLib = shaderLib_;
	pickNext(); // don't sit empty until the first cycleInterval elapses
}

void TFEffectPicker::update(float dt) {
	timer += dt;
	if (timer >= weights.cycleInterval) {
		timer = 0.0f;
		pickNext();
	}
}

void TFEffectPicker::pickNext() {
	std::vector<std::pair<std::string, float>> options;
	options.push_back({ "", weights.rawWeight });
	for (auto& kv : weights.effectWeights) {
		options.push_back({ kv.first, kv.second });
	}
	currentEffect = tfWeightedPick(options);
	randomizeEffectParams(currentEffect);
}

void TFEffectPicker::randomizeEffectParams(const std::string& name) {
	paramX = paramY = paramZ = paramW = 0.0f;

	if (name == "dither") {
		paramX = ofRandom(0.15f, 0.85f);
		paramY = ofRandom(2.0f, 10.0f);
	} else if (name == "threshold") {
		paramX = ofRandom(0.35f, 0.65f);
	} else if (name == "recolor") {
		ofColor c = ofColor::fromHsb(static_cast<int>(ofRandom(0, 255)), 200, 255);
		paramX = c.r / 255.0f;
		paramY = c.g / 255.0f;
		paramZ = c.b / 255.0f;
	} else if (name == "channelshift") {
		paramX = ofRandom(0.002f, 0.01f);
	} else if (name == "hue_rotate") {
		paramX = ofRandom(0.0f, 360.0f);
		paramY = ofRandom(15.0f, 60.0f) * (ofRandom(1.0f) < 0.5f ? -1.0f : 1.0f);
		paramZ = ofRandom(0.8f, 1.3f);
		paramW = ofRandom(0.9f, 1.1f);
	} else if (name == "pixel_sorting") {
		paramX = ofRandom(0.3f, 0.8f);
		paramY = ofRandom(0.0f, 1.0f) < 0.5f ? 0.0f : 1.0f;
	} else if (name == "heatmap_recolor") {
		// paramW packs palette index + reverse flag (paletteIndex + 0.5 if
		// reversed) — mirrors blueprint_emergence's BEFragment.cpp encoding,
		// since only 4 float slots exist here too.
		paramX = ofRandom(0.7f, 1.3f);           // gamma
		paramY = ofRandom(0.0f, 0.12f);          // minLuminance
		paramZ = ofRandom(0.88f, 1.0f);          // maxLuminance
		int paletteIdx = static_cast<int>(ofRandom(4.0f));
		bool reversePalette = ofRandom(1.0f) < 0.2f;
		paramW = paletteIdx + (reversePalette ? 0.5f : 0.0f);
	}
}

void TFEffectPicker::drawCurrent(const ofTexture& sourceTex, const ofRectangle& destRect, float alpha) {
	if (!sourceTex.isAllocated() || destRect.width <= 0 || destRect.height <= 0) {
		return;
	}

	if (currentEffect.empty() || shaderLib == nullptr || !shaderLib->has(currentEffect)) {
		tfDrawTextureCroppedToFill(sourceTex, destRect, alpha);
		return;
	}

	int w = static_cast<int>(destRect.width);
	int h = static_cast<int>(destRect.height);

	if (!sourceFbo.isAllocated() || static_cast<int>(sourceFbo.getWidth()) != w
		|| static_cast<int>(sourceFbo.getHeight()) != h) {
		ofFbo::Settings s;
		s.width = w;
		s.height = h;
		s.internalformat = GL_RGBA;
		s.useDepth = false;
		sourceFbo.allocate(s);
		resultFbo.allocate(s);
	}

	// Stage 1: re-render the crop into a clean FBO with texcoords spanning
	// [0,1] over its own w x h — several effects (dither's Bayer grid,
	// ascii's cell grid, pixel snapping) assume that, which a direct
	// drawSubsection() of an arbitrary crop sub-rect doesn't give them. See
	// this class's header comment / BEFragment.cpp:330-347.
	ofRectangle srcCrop = tfComputeCropFillSrcRect(sourceTex.getWidth(), sourceTex.getHeight(), destRect);
	sourceFbo.begin();
	ofClear(0, 0, 0, 0);
	ofSetColor(255);
	sourceTex.drawSubsection(0, 0, static_cast<float>(w), static_cast<float>(h),
		srcCrop.x, srcCrop.y, srcCrop.width, srcCrop.height);
	sourceFbo.end();

	// Stage 2: run the selected shader against that clean crop.
	ofShader& sh = shaderLib->get(currentEffect);
	resultFbo.begin();
	ofClear(0, 0, 0, 0);
	sh.begin();
	sh.setUniformTexture("tex", sourceFbo.getTexture(), 0);
	sh.setUniformTexture("tex0", sourceFbo.getTexture(), 0);
	sh.setUniform2f("resolution", static_cast<float>(w), static_cast<float>(h));
	applyEffectUniforms(sh, currentEffect, static_cast<float>(w), static_cast<float>(h));
	ofSetColor(255);
	sourceFbo.getTexture().draw(0, 0, static_cast<float>(w), static_cast<float>(h));
	sh.end();
	resultFbo.end();

	// Stage 3: composite at the destination with a plain textured draw so
	// `alpha` blends correctly even for shaders with no alpha uniform of
	// their own (same reasoning as BEFragment.cpp:363-366).
	ofSetColor(255, 255, 255, static_cast<int>(ofClamp(alpha, 0.0f, 1.0f) * 255));
	resultFbo.getTexture().draw(destRect.x, destRect.y, destRect.width, destRect.height);
	ofSetColor(255);
}

void TFEffectPicker::applyEffectUniforms(ofShader& sh, const std::string& name, float w, float h) const {
	if (name == "dither") {
		sh.setUniform1f("alpha", paramX);
		sh.setUniform1f("opacity", 1.0f);
		sh.setUniform1f("maxPixelation", paramY);
	} else if (name == "threshold") {
		sh.setUniform1f("threshold", paramX);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "recolor") {
		sh.setUniform3f("tint", paramX, paramY, paramZ);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "channelshift") {
		sh.setUniform1f("shift", paramX);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "hue_rotate") {
		sh.setUniform1f("hueOffset", paramX);
		sh.setUniform1f("hueSpeed", paramY);
		sh.setUniform1f("time", ofGetElapsedTimef());
		sh.setUniform1f("saturationMult", paramZ);
		sh.setUniform1f("valueMult", paramW);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "pixel_sorting") {
		sh.setUniform1f("threshold", paramX);
		sh.setUniform1f("rangePx", 12.0f);
		sh.setUniform1f("direction", paramY);
		sh.setUniform1f("intensity", 1.0f);
	} else if (name == "heatmap_recolor") {
		sh.setUniform1f("alpha", 1.0f);
		sh.setUniform1f("intensity", 1.0f);
		sh.setUniform1f("gamma", paramX);
		sh.setUniform1f("minLuminance", paramY);
		sh.setUniform1f("maxLuminance", paramZ);
		int paletteIdx = static_cast<int>(paramW);
		bool reversePalette = (paramW - paletteIdx) > 0.25f;
		sh.setUniform1i("palette", paletteIdx);
		sh.setUniform1i("reverse", reversePalette ? 1 : 0);
	} else {
		// Every remaining effect (invert/solarize/scanlines, ascii_solarpunk,
		// bioluminescence, chromatic_aberration, edge_glow, ink_outlines,
		// pixel_drift, water_refraction) has no per-instance randomized
		// state — same fixed literals every time — so it binds from the
		// canonical catalog (shared/src/video-effects/catalog/DefaultVideoEffectCatalog.h)
		// instead of duplicating those literals a third time (see
		// docs/shader-effect-system-probe.md §10). Effects with real
		// per-instance randomization stay above as explicit branches reading
		// paramX..W.
		const videoeffects::VideoEffectDefinition * def = tfCatalogRegistry().getDefinition(name);
		if (def != nullptr) {
			for (const auto & param : def->params) {
				if (param.id == "alpha") {
					sh.setUniform1f("alpha", 1.0f);
					continue;
				}
				switch (param.type) {
					case videoeffects::VideoEffectParameterType::Float:
						sh.setUniform1f(param.id, videoeffects::asFloat(param.defaultValue));
						break;
					case videoeffects::VideoEffectParameterType::Int:
						sh.setUniform1i(param.id, videoeffects::asInt(param.defaultValue));
						break;
					case videoeffects::VideoEffectParameterType::Bool:
						sh.setUniform1i(param.id, videoeffects::asBool(param.defaultValue) ? 1 : 0);
						break;
					case videoeffects::VideoEffectParameterType::Vec3: {
						glm::vec3 v = videoeffects::asVec3(param.defaultValue);
						sh.setUniform3f(param.id, v.x, v.y, v.z);
						break;
					}
					default:
						break;
				}
			}
			sh.setUniform1f("time", ofGetElapsedTimef());
		}
	}
	(void)w;
	(void)h;
}
