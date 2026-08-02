#include "FTFragment.h"
#include <algorithm>

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
	float canvasW = ofGetWidth();
	float canvasH = ofGetHeight();

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

	glEnable(GL_SCISSOR_TEST);
	glScissor(static_cast<GLint>(destRect.x), static_cast<GLint>(canvasH - destRect.y - destRect.height),
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
		sh.setUniform2f("resolution", ofGetWidth(), ofGetHeight());
		if (effect == "dither") {
			float wScale = ofGetWidth() / 1280.f;
			sh.setUniform1f("alpha", params.ditherArc);
			sh.setUniform1f("opacity", fadeAlpha);
			sh.setUniform1f("maxPixelation", params.ditherPx * wScale);
		} else {
			sh.setUniform1f("alpha", fadeAlpha);
		}
		if (effect == "recolor") sh.setUniform3f("tint", params.tint);
		if (effect == "threshold") sh.setUniform1f("threshold", params.thresholdVal);
		if (effect == "channelshift") sh.setUniform1f("shift", params.shiftVal);
		if (effect == "heatmap_recolor") {
			sh.setUniform1f("intensity", 1.0f);
			sh.setUniform1f("gamma", params.heatmapGamma);
			sh.setUniform1f("minLuminance", params.heatmapMinLuminance);
			sh.setUniform1f("maxLuminance", params.heatmapMaxLuminance);
			sh.setUniform1i("palette", params.heatmapPalette);
			sh.setUniform1i("reverse", params.heatmapReverse);
		}

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
