#include "BEFragment.h"
#include "BESettings.h"
#include "DefaultVideoEffectCatalog.h"
#include "VideoEffectRegistry.h"
#include "VideoEffectTypes.h"
#include "glm/glm.hpp"
#include "ofColor.h"
#include "ofGraphics.h"
#include "ofMath.h"
#include "ofUtils.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
	float sampleAverageBrightness(const ofPixels& px) {
		int W = px.getWidth(), H = px.getHeight();
		if (W <= 0 || H <= 0) return 0.5f;
		float sum = 0.f; int n = 0;
		const int STEP = 32;
		for (int y = 0; y < H; y += STEP)
			for (int x = 0; x < W; x += STEP) {
				auto c = px.getColor(x, y);
				sum += 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
				n++;
			}
		return n > 0 ? sum / (n * 255.f) : 0.5f;
	}

	// Excludes "desaturate" — BEFragment already has its own always-on
	// desaturate ramp via the fragmentEffects shader (Fragment::drawTexturedRect),
	// so this pool is the *additional* layered-effect vocabulary.
	const std::array<std::string, 18> kEffectPool = {
		"invert", "threshold", "recolor", "solarize", "dither", "scanlines",
		"channelshift", "hue_rotate", "ascii_solarpunk", "bioluminescence", "chromatic_aberration",
		"edge_glow", "ink_outlines", "pixel_drift", "pixel_sorting", "water_refraction",
		"ridgeline", "heatmap_recolor"
	};

	// Lazily-built, process-lifetime registry of the canonical Contract-A
	// single-pass catalog — see setEffectUniforms()'s fallback branch below.
	// Mirrors shared/src/VideoRegionEffectRenderer.cpp's identical helper.
	const videoeffects::VideoEffectRegistry & beCatalogRegistry() {
		static videoeffects::VideoEffectRegistry registry = [] {
			videoeffects::VideoEffectRegistry r;
			videoeffects::registerSinglePassEffects(r);
			return r;
		}();
		return registry;
	}
}

void BEFragment::setupBE(Fragment::Params params, GeometryType geometryType_, int canvasW, int canvasH) {
	geometryType = geometryType_;
	targetRadius = params.maskRadius;

	switch (geometryType) {
	case GeometryType::SLIVER:
		params.arrivalDuration = SLIDE_IN_DURATION;
		break;
	case GeometryType::CIRCLE:
		params.arrivalDuration = IRIS_OPEN_DURATION;
		params.circularMask = true;
		break;
	default:
		params.arrivalDuration = SCAN_REVEAL_DURATION + BORDER_DRAW_DURATION;
		break;
	}

	Fragment::setup(params);

	scaleMin = ofRandom(FRAG_SCALE_MIN_LO, FRAG_SCALE_MIN_HI);
	scaleMax = ofRandom(FRAG_SCALE_MAX_LO, FRAG_SCALE_MAX_HI);
	scaleSeed = ofRandom(0.0f, 1000.0f);
	currentScale = (scaleMin + scaleMax) * 0.5f;

	if (geometryType == GeometryType::SLIVER) {
		glm::vec2 center = glm::vec2(params.bounds.getCenter());
		float distLeft = center.x;
		float distRight = canvasW - center.x;
		float distTop = center.y;
		float distBottom = canvasH - center.y;
		float minDist = std::min({ distLeft, distRight, distTop, distBottom });

		glm::vec2 finalPos = glm::vec2(params.bounds.getPosition());
		if (minDist == distLeft) {
			slideStartPos = finalPos - glm::vec2(params.bounds.width, 0);
		} else if (minDist == distRight) {
			slideStartPos = finalPos + glm::vec2(params.bounds.width, 0);
		} else if (minDist == distTop) {
			slideStartPos = finalPos - glm::vec2(0, params.bounds.height);
		} else {
			slideStartPos = finalPos + glm::vec2(0, params.bounds.height);
		}
	}
}

void BEFragment::update(float dt) {
	Fragment::update(dt);
	setExternalDesatNudge(desatNudge);
	updateScaleDrift(dt);

	bool eligible = geometryType != GeometryType::CIRCLE
		&& (getState() == Fragment::State::DRIFTING || getState() == Fragment::State::STABLE);
	if (shaderLib != nullptr && eligible) {
		updateEffectCycle(dt);
	}
}

void BEFragment::updateScaleDrift(float /*dt*/) {
	currentScale = ofMap(ofNoise(ofGetElapsedTimef() * FRAG_SCALE_NOISE_SPEED + scaleSeed), 0.0f, 1.0f, scaleMin, scaleMax, true);
}

void BEFragment::draw() const {
	glm::vec2 center = glm::vec2(bounds.getCenter());
	ofPushMatrix();
	ofTranslate(center.x, center.y);
	ofScale(currentScale);
	ofTranslate(-center.x, -center.y);
	Fragment::draw();
	ofPopMatrix();
}

void BEFragment::updateEffectCycle(float dt) {
	using SlotState = EffectSlot::State;

	switch (effectSlot.state) {
	case SlotState::IDLE:
		effectSilenceAcc += dt;
		if (effectSilenceAcc >= effectSilenceDur) {
			pickAndStartEffect();
		}
		break;

	case SlotState::FADE_IN:
		effectSlot.alpha += dt / effectSlot.fadeDur;
		if (effectSlot.alpha >= 1.0f) {
			effectSlot.alpha = 1.0f;
			effectSlot.state = SlotState::ACTIVE;
			effectSlot.dwellAcc = 0.0f;
		}
		break;

	case SlotState::ACTIVE:
		effectSlot.dwellAcc += dt;
		if (effectSlot.dwellAcc >= effectSlot.dwellDur) {
			effectSlot.state = SlotState::FADE_OUT;
		}
		break;

	case SlotState::FADE_OUT:
		effectSlot.alpha -= dt / effectSlot.fadeDur;
		if (effectSlot.alpha <= 0.0f) {
			effectSlot.alpha = 0.0f;
			effectSlot.state = SlotState::IDLE;
			effectSlot.name.clear();
			effectSilenceAcc = 0.0f;
			effectSilenceDur = ofRandom(FRAG_EFFECT_SILENCE_MIN, FRAG_EFFECT_SILENCE_MAX);
		}
		break;
	}
}

void BEFragment::pickAndStartEffect() {
	if (shaderLib == nullptr) {
		return;
	}

	int idx = static_cast<int>(ofRandom(static_cast<float>(kEffectPool.size())));
	idx = std::min(idx, static_cast<int>(kEffectPool.size()) - 1);
	const std::string & name = kEffectPool[idx];
	if (!shaderLib->has(name)) {
		// Shader failed to load (e.g. missing data file) — skip this turn,
		// retry on the next silence window rather than drawing nothing useful.
		effectSilenceAcc = 0.0f;
		effectSilenceDur = ofRandom(FRAG_EFFECT_SILENCE_MIN, FRAG_EFFECT_SILENCE_MAX);
		return;
	}

	effectSlot.name = name;
	effectSlot.fadeDur = ofRandom(FRAG_EFFECT_FADE_MIN, FRAG_EFFECT_FADE_MAX);
	effectSlot.dwellDur = ofRandom(FRAG_EFFECT_DWELL_MIN, FRAG_EFFECT_DWELL_MAX);
	effectSlot.alpha = 0.0f;
	effectSlot.dwellAcc = 0.0f;
	effectSlot.state = EffectSlot::State::FADE_IN;

	if (name == "dither") {
		effectSlot.params = { ofRandom(0.15f, 0.85f), ofRandom(2.0f, 10.0f), 0, 0 };
	} else if (name == "threshold") {
		effectSlot.params = { ofRandom(0.35f, 0.65f), 0, 0, 0 };
	} else if (name == "recolor") {
		ofColor c = ofColor::fromHsb(ofRandom(0, 255), 200, 255);
		effectSlot.params = { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 0 };
	} else if (name == "channelshift") {
		effectSlot.params = { ofRandom(0.002f, 0.01f), 0, 0, 0 };
	} else if (name == "hue_rotate") {
		float speed = ofRandom(15.0f, 60.0f) * (ofRandom(1.0f) < 0.5f ? -1.0f : 1.0f);
		effectSlot.params = { ofRandom(0.0f, 360.0f), speed, ofRandom(0.8f, 1.3f), ofRandom(0.9f, 1.1f) };
	} else if (name == "pixel_sorting") {
		effectSlot.params = { ofRandom(0.3f, 0.8f), ofRandom(0, 1) < 0.5f ? 0.0f : 1.0f, 0, 0 };
	} else if (name == "heatmap_recolor") {
		// Packs palette index + reverse flag into the 4th slot (paletteIndex
		// + 0.5 if reversed) since glm::vec4 only has 4 floats and gamma/
		// minLuminance/maxLuminance already claim the other three.
		float gamma = ofRandom(0.7f, 1.3f);
		float minLum = ofRandom(0.0f, 0.12f);
		float maxLum = ofRandom(0.88f, 1.0f);
		int paletteIdx = static_cast<int>(ofRandom(4.0f));
		bool reversePalette = ofRandom(1.0f) < 0.2f;
		effectSlot.params = { gamma, minLum, maxLum, paletteIdx + (reversePalette ? 0.5f : 0.0f) };
	} else if (name == "ridgeline") {
		RidgelineRenderer::Params rp;
		rp.numLines       = 80;
		rp.samplesPerLine = 128;
		rp.amplitude      = ofRandom(60.f, 255.f);
		rp.spacingPct     = 0.020f;
		rp.centerYPct     = 0.470f;
		rp.marginXPct     = -0.020f;
		rp.overlayMode    = false; // BEFragment draws over existing content; no extra video pass
		rp.flipX          = false;
		rp.flipY          = true;
		ridgelineRenderer.setParams(rp);
		ridgelineSetupW = -1;
		ridgelineSetupH = -1;
	}
}

void BEFragment::setEffectUniforms(ofShader & sh, const std::string & name) const {
	const glm::vec4 & p = effectSlot.params;

	if (name == "dither") {
		sh.setUniform1f("alpha", p.x);
		sh.setUniform1f("opacity", 1.0f);
		sh.setUniform1f("maxPixelation", p.y);
	} else if (name == "threshold") {
		sh.setUniform1f("threshold", p.x);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "recolor") {
		sh.setUniform3f("tint", p.x, p.y, p.z);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "channelshift") {
		sh.setUniform1f("shift", p.x);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "hue_rotate") {
		sh.setUniform1f("hueOffset", p.x);
		sh.setUniform1f("hueSpeed", p.y);
		sh.setUniform1f("time", ofGetElapsedTimef());
		sh.setUniform1f("saturationMult", p.z);
		sh.setUniform1f("valueMult", p.w);
		sh.setUniform1f("alpha", 1.0f);
	} else if (name == "heatmap_recolor") {
		sh.setUniform1f("alpha", 1.0f);
		sh.setUniform1f("intensity", 1.0f);
		sh.setUniform1f("gamma", p.x);
		sh.setUniform1f("minLuminance", p.y);
		sh.setUniform1f("maxLuminance", p.z);
		int paletteIdx = static_cast<int>(p.w);
		bool reversePalette = (p.w - paletteIdx) > 0.25f;
		sh.setUniform1i("palette", paletteIdx);
		sh.setUniform1i("reverse", reversePalette ? 1 : 0);
	} else if (name == "pixel_sorting") {
		sh.setUniform1f("threshold", p.x);
		sh.setUniform1f("rangePx", 12.0f);
		sh.setUniform1f("direction", p.y);
		sh.setUniform1f("intensity", 1.0f);
	} else {
		// Every remaining effect in kEffectPool (invert/solarize/scanlines,
		// ascii_solarpunk, bioluminescence, chromatic_aberration, edge_glow,
		// ink_outlines, pixel_drift, water_refraction) has no per-instance
		// randomized state — it's the same fixed literal values every time,
		// so it binds directly from the canonical catalog
		// (shared/src/video-effects/catalog/DefaultVideoEffectCatalog.h)
		// instead of duplicating those literals a fourth time (see
		// docs/shader-effect-system-probe.md §10's duplication finding).
		// Effects with real per-instance randomization (dither, threshold,
		// recolor, channelshift, hue_rotate, heatmap_recolor, pixel_sorting)
		// stay above as explicit branches reading effectSlot.params — the
		// catalog doesn't yet have a per-instance-override mechanism, so
		// migrating those would mean designing that first rather than
		// silently changing their behavior here.
		const videoeffects::VideoEffectDefinition * def = beCatalogRegistry().getDefinition(name);
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
}

void BEFragment::drawRidgelineEffect() const {
	if (ridgelinePixels == nullptr || !ridgelinePixels->isAllocated()) return;

	int w = static_cast<int>(bounds.width);
	int h = static_cast<int>(bounds.height);
	if (w <= 0 || h <= 0) return;

	if (w != ridgelineSetupW || h != ridgelineSetupH) {
		ridgelineRenderer.setup(w, h);
		ridgelineSetupW = w;
		ridgelineSetupH = h;
	}
	ridgelineRenderer.update(*ridgelinePixels);

	glm::vec2 pos    = getDrawPosition();
	glm::vec2 center = glm::vec2(bounds.getCenter());
	float     alpha  = opacity * effectSlot.alpha;

	ofEnableAlphaBlending();
	ofPushMatrix();
	ofTranslate(center.x, center.y);
	ofScale(currentScale);
	ofTranslate(-center.x, -center.y);
	ofTranslate(pos.x, pos.y);
	ridgelineRenderer.draw(nullptr, alpha);
	ofPopMatrix();
}

void BEFragment::drawOverlay() const {
	if (geometryType == GeometryType::CIRCLE) {
		return;
	}
	if (getState() == Fragment::State::DISSOLVING) {
		return;
	}
	if (effectSlot.isIdle()) {
		return;
	}

	if (effectSlot.name == "ridgeline") {
		drawRidgelineEffect();
		return;
	}

	if (shaderLib == nullptr || !shaderLib->has(effectSlot.name)) {
		return;
	}

	int w = static_cast<int>(bounds.width);
	int h = static_cast<int>(bounds.height);
	if (w <= 0 || h <= 0) {
		return;
	}

	if (!effectFbosAllocated) {
		ofFbo::Settings s;
		s.width = w;
		s.height = h;
		s.internalformat = GL_RGBA;
		s.useDepth = false;
		effectSourceFbo.allocate(s);
		effectResultFbo.allocate(s);
		effectFbosAllocated = true;
	}

	// 1. Capture this fragment's own crop into a same-size FBO. The shared
	// video texture's crop sub-rect does not span 0..1 in texture space, so
	// drawing the effect shaders directly against videoTexture would feed
	// them texcoords outside the [0,1] range several of them assume for
	// screen-space math (Bayer dithering, ASCII cell grid, pixel snapping).
	// Re-rendering into a fragment-sized FBO first gives them a clean local
	// 0..1 UV space to work in, matching how quadrant-crosshair's Quadrant
	// always draws its (uncropped) full video texture.
	effectSourceFbo.begin();
	ofClear(0, 0, 0, 0);
	if (hasMedia()) {
		ofSetColor(255);
		videoTexture->drawSubsection(0, 0, w, h, videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height);
	} else {
		ofSetColor(placeholderColor);
		ofDrawRectangle(0, 0, w, h);
	}
	effectSourceFbo.end();

	// 2. Run the effect shader at full opacity into a second FBO.
	ofShader & sh = shaderLib->get(effectSlot.name);
	effectResultFbo.begin();
	ofClear(0, 0, 0, 0);
	sh.begin();
	sh.setUniformTexture("tex", effectSourceFbo.getTexture(), 0);
	sh.setUniformTexture("tex0", effectSourceFbo.getTexture(), 0);
	sh.setUniform2f("resolution", static_cast<float>(w), static_cast<float>(h));
	setEffectUniforms(sh, effectSlot.name);
	ofSetColor(255);
	effectSourceFbo.getTexture().draw(0, 0, w, h);
	sh.end();
	effectResultFbo.end();

	// 3. Composite back at the fragment's screen position with a plain
	// textured draw (no shader bound) so the slot's fade alpha blends
	// correctly even for shaders with no "alpha" uniform of their own.
	// Wrapped in the same scale transform as draw() so the effect overlay
	// tracks the base content's breathing instead of sitting static over it.
	glm::vec2 pos = getDrawPosition();
	glm::vec2 center = glm::vec2(bounds.getCenter());
	ofPushMatrix();
	ofTranslate(center.x, center.y);
	ofScale(currentScale);
	ofTranslate(-center.x, -center.y);
	ofSetColor(255, 255, 255, static_cast<int>(255 * opacity * effectSlot.alpha));
	effectResultFbo.getTexture().draw(pos.x, pos.y, bounds.width, bounds.height);
	ofPopMatrix();
}

void BEFragment::drawDeparture(float t) const {
	if (geometryType == GeometryType::SLIVER) {
		drawSlideOut(t);
	} else {
		Fragment::drawDeparture(t); // fade in place
	}
}

void BEFragment::drawSlideOut(float t) const {
	float tEased = t * t; // ease-in: slow start, accelerates out
	glm::vec2 finalPos = glm::vec2(bounds.getPosition());
	glm::vec2 pos = glm::mix(finalPos, slideStartPos, tEased);

	if (hasMedia()) {
		ofSetColor(255, static_cast<int>(255 * opacity));
		videoTexture->drawSubsection(pos.x, pos.y, bounds.width, bounds.height,
			videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height);
	} else {
		ofSetColor(placeholderColor, static_cast<int>(255 * opacity));
		ofDrawRectangle(pos.x, pos.y, bounds.width, bounds.height);
	}
}

void BEFragment::drawArrival(float /*t*/) const {
	float elapsed = getStateElapsedSeconds();
	switch (geometryType) {
	case GeometryType::SLIVER:
		drawSlideIn(elapsed);
		break;
	case GeometryType::CIRCLE:
		drawIrisOpen(elapsed);
		break;
	default:
		drawScanReveal(elapsed);
		break;
	}
}

void BEFragment::drawStable() const {
	if (geometryType == GeometryType::CIRCLE) {
		glm::vec2 pos = getDrawPosition();
		glm::vec2 center = pos + glm::vec2(bounds.width * 0.5f, bounds.height * 0.5f);

		ofSetCircleResolution(96);

		// Draw the live video/image fill first.
		drawMaskedFill(targetRadius);

		// Ghost ring persists from STABLE onward and fades with the fragment.
		ofSetColor(255, 255, 255, static_cast<int>(80 * opacity));
		ofNoFill();
		ofDrawCircle(center.x, center.y, targetRadius * GHOST_RING_SCALE);
		ofFill();
		return;
	}

	if (geometryType == GeometryType::SLIVER) {
		float sinceArrival = getStateElapsedSeconds();
		if (sinceArrival <= AFTERIMAGE_FADE_DURATION) {
			float alpha = 1.0f - (sinceArrival / AFTERIMAGE_FADE_DURATION);
			ofSetColor(255, 255, 255, static_cast<int>(255 * alpha));
			ofNoFill();
			ofDrawRectangle(slideStartPos.x, slideStartPos.y, bounds.width, bounds.height);
			ofFill();
		}
	}

	Fragment::drawStable();
}

void BEFragment::drawScanReveal(float elapsed) const {
	glm::vec2 pos = getDrawPosition();

	if (elapsed <= SCAN_REVEAL_DURATION) {
		float t = ofClamp(elapsed / SCAN_REVEAL_DURATION, 0.0f, 1.0f);
		float revealedH = bounds.height * t;
		float revealFrac = revealedH / bounds.height;

		if (hasMedia()) {
			ofSetColor(255, static_cast<int>(255 * opacity));
			videoTexture->drawSubsection(pos.x, pos.y, bounds.width, revealedH,
				videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height * revealFrac);
		} else {
			ofSetColor(placeholderColor, static_cast<int>(255 * opacity));
			ofDrawRectangle(pos.x, pos.y, bounds.width, revealedH);
		}

		ofSetColor(255, 255, 255, 200);
		ofDrawLine(pos.x, pos.y + revealedH, pos.x + bounds.width, pos.y + revealedH);
	} else {
		if (hasMedia()) {
			ofSetColor(255, static_cast<int>(255 * opacity));
			videoTexture->drawSubsection(pos.x, pos.y, bounds.width, bounds.height,
				videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height);
		} else {
			ofSetColor(placeholderColor, static_cast<int>(255 * opacity));
			ofDrawRectangle(pos.x, pos.y, bounds.width, bounds.height);
		}

		float borderT = ofClamp((elapsed - SCAN_REVEAL_DURATION) / BORDER_DRAW_DURATION, 0.0f, 1.0f);
		drawClockwiseBorder(pos, bounds.width, bounds.height, borderT);
	}
}

void BEFragment::drawClockwiseBorder(glm::vec2 pos, float w, float h, float t) const {
	glm::vec2 corners[5] = {
		pos,
		pos + glm::vec2(w, 0),
		pos + glm::vec2(w, h),
		pos + glm::vec2(0, h),
		pos
	};

	float perimeter = 2.0f * (w + h);
	float target = perimeter * ofClamp(t, 0.0f, 1.0f);

	ofSetColor(255, 255, 255, 200);
	float drawn = 0.0f;
	for (int i = 0; i < 4 && drawn < target; i++) {
		glm::vec2 a = corners[i];
		glm::vec2 b = corners[i + 1];
		float segLen = glm::distance(a, b);
		float remain = target - drawn;
		if (remain >= segLen) {
			ofDrawLine(a.x, a.y, b.x, b.y);
			drawn += segLen;
		} else {
			glm::vec2 p = a + (b - a) * (remain / segLen);
			ofDrawLine(a.x, a.y, p.x, p.y);
			drawn = target;
		}
	}
}

void BEFragment::drawSlideIn(float elapsed) const {
	float t = ofClamp(elapsed / SLIDE_IN_DURATION, 0.0f, 1.0f);
	float tEased = 1.0f - (1.0f - t) * (1.0f - t);

	glm::vec2 finalPos = glm::vec2(bounds.getPosition());
	glm::vec2 pos = glm::mix(slideStartPos, finalPos, tEased);

	if (hasMedia()) {
		ofSetColor(255, static_cast<int>(255 * opacity));
		videoTexture->drawSubsection(pos.x, pos.y, bounds.width, bounds.height,
			videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height);
	} else {
		ofSetColor(placeholderColor, static_cast<int>(255 * opacity));
		ofDrawRectangle(pos.x, pos.y, bounds.width, bounds.height);
	}
}

void BEFragment::drawIrisOpen(float elapsed) const {
	float t = ofClamp(elapsed / IRIS_OPEN_DURATION, 0.0f, 1.0f);
	float tEased = 1.0f - std::pow(1.0f - t, 3.0f);

	float fillRadius = targetRadius * tEased;
	float outlineRadius = targetRadius * std::min(1.0f, tEased + IRIS_OPEN_LEAD_FRACTION);

	glm::vec2 pos = getDrawPosition();
	glm::vec2 center = pos + glm::vec2(bounds.width * 0.5f, bounds.height * 0.5f);

	ofSetCircleResolution(96);

	// This now works for live video because Fragment::drawMaskedFill() uses a
	// textured circle mesh for circularMask fragments instead of shader discard.
	drawMaskedFill(fillRadius);

	// Lead outline drawn slightly ahead of the fill.
	ofSetColor(255, 255, 255, 200);
	ofNoFill();
	ofDrawCircle(center.x, center.y, outlineRadius);
	ofFill();
}
