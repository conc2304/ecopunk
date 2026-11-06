#include "FTFragment.h"
#include <algorithm>
#include "FTCanvas.h"

namespace fragmenttrail {

const std::vector<std::string>& ftEffectPool() {
	static const std::vector<std::string> pool = {
		"desaturate", "invert", "recolor", "threshold", "dither", "solarize", "scanlines", "channelshift",
		"heatmap_recolor"
	};
	return pool;
}

FTFragmentSpawnParams ftRandomEffectLook(const std::string& effectName) {
	FTFragmentSpawnParams p;
	p.effectName = effectName;
	if (effectName == "channelshift") {
		p.shiftVal = ofRandom(0.003f, 0.02f);
	} else if (effectName == "recolor") {
		ofColor c;
		c.setHsb(static_cast<unsigned char>(ofRandom(0, 255)), 200, 255);
		p.tint = glm::vec3(c.r / 255.f, c.g / 255.f, c.b / 255.f);
	} else if (effectName == "threshold") {
		p.thresholdVal = ofRandom(0.3f, 0.7f);
	} else if (effectName == "dither") {
		p.ditherPx = ofRandom(2.f, 8.f);
	} else if (effectName == "heatmap_recolor") {
		p.heatmapPalette = static_cast<int>(ofRandom(4.f));
	}
	return p;
}

void ftBindEffectUniforms(ofShader& sh, const std::string& effect, const FTFragmentSpawnParams& look, float fadeAlpha) {
	sh.setUniform2f("resolution", canvasWidth(), canvasHeight());
	// Every kept effect except "dither" shares one convention (confirmed
	// against the actual shader source, see implementation plan): a
	// single `alpha` uniform doubles as both effect-mix amount and
	// final output alpha, which happens to line up exactly with a
	// fragment's own age-based decay. "dither" is the exception — its
	// `alpha` is a fixed position along a multi-phase clean/dither/
	// pixelate arc, with fade carried by a separate `opacity` uniform
	// instead (see dither.glsl).
	if (effect == "dither") {
		float wScale = canvasWidth() / 1280.f;
		sh.setUniform1f("alpha", look.ditherArc);
		sh.setUniform1f("opacity", fadeAlpha);
		sh.setUniform1f("maxPixelation", look.ditherPx * wScale);
	} else {
		sh.setUniform1f("alpha", fadeAlpha);
	}
	if (effect == "recolor") sh.setUniform3f("tint", look.tint);
	if (effect == "threshold") sh.setUniform1f("threshold", look.thresholdVal);
	if (effect == "channelshift") sh.setUniform1f("shift", look.shiftVal);
	if (effect == "heatmap_recolor") {
		sh.setUniform1f("intensity", 1.0f);
		sh.setUniform1f("gamma", look.heatmapGamma);
		sh.setUniform1f("minLuminance", look.heatmapMinLuminance);
		sh.setUniform1f("maxLuminance", look.heatmapMaxLuminance);
		sh.setUniform1i("palette", look.heatmapPalette);
		sh.setUniform1i("reverse", look.heatmapReverse);
	}
}

void FTFragment::spawn(const FTFragmentSpawnParams& p, float sustainSeconds, float decaySeconds, const hud::HudTheme& theme) {
	params = p;
	age = 0.f;
	sustain = sustainSeconds;
	decay = std::max(0.05f, decaySeconds); // guard against div-by-zero in getAlpha()

	chrome.setTheme(theme);
	hud::WindowChromeOptions opts;
	opts.label = params.hexLabel;
	opts.showCloseBox = true;
	chrome.setOptions(opts);
}

void FTFragment::update(float dt) {
	age += dt;
	chrome.update(dt);
}

float FTFragment::getAlpha() const {
	if (age < sustain) return 1.0f;
	return ofClamp(1.0f - (age - sustain) / decay, 0.f, 1.f);
}

void FTFragment::drawContent(const ofTexture& tex, const ofRectangle& destRect, ShaderLibrary& shaderLib) {
	if (!tex.isAllocated()) return;
	// Guard: zero-width/height scissor rects cause GL errors on VideoCore
	// IV — same guard quadrant-crosshair's Quadrant::draw() uses.
	if (destRect.width < 2.f || destRect.height < 2.f) return;

	// Crop via glScissor + an oversized/offset full-texture draw, NOT
	// ofTexture::drawSubsection(). This mirrors quadrant-crosshair's
	// Quadrant::draw()/drawWithEffect() exactly (see Quadrant.cpp lines
	// ~170-218) — drawSubsection's source-rect selection does not combine
	// correctly with these shaders' gl_MultiTexCoord0-based vert.glsl in
	// this project (verified: full-screen and quadrant-crosshair's own
	// full-texture .draw() calls both render correctly; drawSubsection
	// produced a flat, undetailed fill instead of the video content).
	float canvasW = canvasWidth();
	float canvasH = canvasHeight();

	// Zoom > 1 draws the video far larger than the canvas so only a small,
	// magnified region lands inside this fragment's scissor window — the
	// "peephole" look. speedResponse's crop-zoom modulation (Section 3,
	// flagged nice-to-have, not required for v1) would vary this per
	// fragment instead of using one constant.
	constexpr float kZoom = 1.8f;
	float drawW = canvasW * kZoom;
	float drawH = canvasH * kZoom;
	float drawX = params.position.x * (1.0f - kZoom);
	float drawY = params.position.y * (1.0f - kZoom);

	// glScissor takes framebuffer rows. The window framebuffer's rows run
	// bottom-up relative to the canvas, so the rect is flipped there. An
	// ofFbo bound with its default matrix flip (ExperienceRuntime's scene
	// FBO) stores canvas row y at FBO row y — flipping there clipped each
	// fragment's content into the vertically mirrored rect.
	GLint boundFramebuffer = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFramebuffer);
	const float scissorY = boundFramebuffer != 0 ? destRect.y : canvasH - destRect.y - destRect.height;
	glEnable(GL_SCISSOR_TEST);
	glScissor(static_cast<GLint>(destRect.x), static_cast<GLint>(scissorY),
		static_cast<GLint>(destRect.width), static_cast<GLint>(destRect.height));

	float fadeAlpha = getAlpha();
	const std::string& effect = params.effectName;
	bool useShader = !effect.empty() && effect != "passthrough" && shaderLib.has(effect);

	if (!useShader) {
		ofSetColor(255, 255, 255, static_cast<int>(fadeAlpha * 255));
		tex.draw(drawX, drawY, drawW, drawH);
	} else {
		// Every kept effect except "dither" shares one convention (confirmed
		// against the actual shader source, see implementation plan): a
		// single `alpha` uniform doubles as both effect-mix amount and
		// final output alpha, which happens to line up exactly with this
		// fragment's own age-based decay. "dither" is the exception — its
		// `alpha` is a fixed position along a multi-phase clean/dither/
		// pixelate arc, with fade carried by a separate `opacity` uniform
		// instead (see dither.glsl).
		ofShader& sh = shaderLib.get(effect);
		sh.begin();
		sh.setUniformTexture("tex", tex, 0);
		ftBindEffectUniforms(sh, effect, params, fadeAlpha);

		ofSetColor(255);
		tex.draw(drawX, drawY, drawW, drawH);
		sh.end();
	}

	glDisable(GL_SCISSOR_TEST);
}

void FTFragment::draw(TimeOffsetVideoBuffer& videoBuffer, ShaderLibrary& shaderLib) {
	ofRectangle destRect = getBounds();

	const ofTexture* tex = nullptr;
	if (params.mode == FTContentMode::TIME_SLICE && params.playheadIndex >= 0) {
		tex = &videoBuffer.getPlayheadTexture(params.playheadIndex);
	} else if (videoBuffer.hasMedia()) {
		tex = &videoBuffer.getRawVideoTexture();
	}
	if (tex) drawContent(*tex, destRect, shaderLib);

	chrome.setBounds(destRect.x, destRect.y, destRect.width, destRect.height);
	hud::MotionSettings motion;
	motion.opacity = getAlpha();
	chrome.setMotion(motion);
	chrome.draw();
}

} // namespace fragmenttrail
