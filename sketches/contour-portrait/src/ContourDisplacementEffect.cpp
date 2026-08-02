#include "ContourDisplacementEffect.h"
#include <algorithm>
#include <limits>

namespace {
	// GLSL-style smoothstep, used identically for mask-span edge softening
	// and (implicitly, via the same shape) breakup falloff -- kept local
	// since this is the only place outside a shader it's needed.
	inline float smoothstepf(float edge0, float edge1, float x) {
		if (edge0 == edge1) return x < edge0 ? 0.0f : 1.0f;
		float t = ofClamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	// v3: standard compositing-blend formulas, applied per channel. Each
	// case computes the *full* effect of that mode; computeDisplacement()
	// then lerps between the base line color and this result by
	// sourceColorAmount, so "amount" reads consistently as "how much of
	// this blend mode's result to apply" across every mode, including Mix
	// (whose full effect is simply the source color itself).
	inline ofFloatColor blendLineColor(const ofFloatColor & base, const ofFloatColor & src, int mode) {
		using BM = ContourDisplacementEffect;
		switch (mode) {
			case BM::COLOR_BLEND_MULTIPLY:
				return ofFloatColor(base.r * src.r, base.g * src.g, base.b * src.b, 1.0f);
			case BM::COLOR_BLEND_SCREEN:
				return ofFloatColor(1.0f - (1.0f - base.r) * (1.0f - src.r),
					1.0f - (1.0f - base.g) * (1.0f - src.g),
					1.0f - (1.0f - base.b) * (1.0f - src.b), 1.0f);
			case BM::COLOR_BLEND_ADD:
				return ofFloatColor(ofClamp(base.r + src.r, 0.0f, 1.0f), ofClamp(base.g + src.g, 0.0f, 1.0f), ofClamp(base.b + src.b, 0.0f, 1.0f), 1.0f);
			case BM::COLOR_BLEND_OVERLAY: {
				auto overlayCh = [](float b, float s) { return b < 0.5f ? 2.0f * b * s : 1.0f - 2.0f * (1.0f - b) * (1.0f - s); };
				return ofFloatColor(overlayCh(base.r, src.r), overlayCh(base.g, src.g), overlayCh(base.b, src.b), 1.0f);
			}
			case BM::COLOR_BLEND_DIFFERENCE:
				return ofFloatColor(fabsf(base.r - src.r), fabsf(base.g - src.g), fabsf(base.b - src.b), 1.0f);
			case BM::COLOR_BLEND_SUBTRACT:
				return ofFloatColor(ofClamp(base.r - src.r, 0.0f, 1.0f), ofClamp(base.g - src.g, 0.0f, 1.0f), ofClamp(base.b - src.b, 0.0f, 1.0f), 1.0f);
			case BM::COLOR_BLEND_EXCLUSION:
				return ofFloatColor(base.r + src.r - 2.0f * base.r * src.r,
					base.g + src.g - 2.0f * base.g * src.g,
					base.b + src.b - 2.0f * base.b * src.b, 1.0f);
			case BM::COLOR_BLEND_MIX:
			default:
				return src;
		}
	}
}

void ContourDisplacementEffect::setup(int width, int height) {
	canvasW = width;
	canvasH = height;

	ofPixels px;
	px.allocate(2, 2, OF_PIXELS_RGBA);
	px.setColor(ofColor(0, 0));
	blankDebugTex.loadData(px);

	inputGroup.setName("Input");
	inputGroup.add(inputEnabled, inputMode, mirrorX, invertSource, sourceFitMode, sourceOpacity);

	preprocessGroup.setName("Preprocess");
	preprocessGroup.add(blurRadius, contrast, brightness, gamma, threshold, thresholdSoftness, edgeAmount, luminanceEdgeMix);

	geometryGroup.setName("Geometry");
	geometryGroup.add(orientation, lineCount, samplesPerLine, lineSpacing, lineThickness, lineLength, meshScale, positionX, positionY);

	displacementGroup.setName("Displacement");
	displacementGroup.add(displacementSource, displacementAmount, displacementBias, displacementExponent, secondaryDisplacement, spatialSmoothing, temporalSmoothing);
	displacementGroup.add(displacementClamp);
	displacementGroup.add(depthInvert);

	breakupGroup.setName("Breakup");
	breakupGroup.add(breakupEnabled, breakupAmount, breakupStart, breakupWidth, breakupDirection, dropout, noiseScale, noiseSpeed, noiseStrength, jitter, fragmentStretch, fragmentDensity, seed);

	appearanceGroup.setName("Appearance");
	appearanceGroup.add(lineColor, lineAlpha, backgroundColor, backgroundAlpha, renderMode, blendMode, showSource, sourceAlpha);
	appearanceGroup.add(sourceColorEnabled, sourceColorAmount, colorBlendMode);

	maskGroup.setName("Mask");
	maskGroup.add(maskEnabled, maskSoftness, maskSource, maskSpansPerRow, maskEdgeGlow, maskTemporalSmoothing, maskMatchThreshold);

	densityGroup.setName("Line Density");
	densityGroup.add(densityMode, densityFloor, densitySource, densityTemporalSmoothing, densityLUTResolution);

	debugGroup.setName("Debug");
	debugGroup.add(showGui, showSourceDebug, showProcessedDebug, showMaskDebug, showColorDebug, showMeshBounds, freezeFrame, reloadShadersTrigger, showFps, qualityPreset);

	params.setName("Contour Effect");
	params.add(inputGroup, preprocessGroup, geometryGroup, displacementGroup, breakupGroup, maskGroup, densityGroup, appearanceGroup, debugGroup);

	applyQualityPreset(qualityPreset.get());
	allocateFbos();
	reloadShaders();
	rebuildMesh();
}

void ContourDisplacementEffect::applyQualityPreset(int quality) {
	// Recommended defaults from the brief's Performance Constraints section.
	// Applied once when the preset changes (not every frame) so the user can
	// still hand-tune lineCount/samplesPerLine afterward without fighting
	// this function.
	switch (quality) {
		case QUALITY_CONSTRAINED:
			workingW = 96; workingH = 54;
			lineCount = 110; samplesPerLine = 220;
			blurRadius = 0.0f;
			breakupAmount = std::min(breakupAmount.get(), 0.5f);
			break;
		case QUALITY_LOW_RES_PROJECTION:
			workingW = 64; workingH = 36;
			lineCount = 80; samplesPerLine = 160;
			blurRadius = 0.0f;
			break;
		case QUALITY_DESKTOP:
		default:
			workingW = 192; workingH = 108;
			break;
	}
	lastQualityApplied = quality;
	if (processedFbo.isAllocated()) {
		processedFbo.allocate(workingW, workingH, GL_RGBA);
	}
	if (colorFbo.isAllocated()) {
		colorFbo.allocate(workingW, workingH, GL_RGBA);
	}
}

void ContourDisplacementEffect::allocateFbos() {
	processedFbo.allocate(workingW, workingH, GL_RGBA);
	processedFbo.begin();
	ofClear(0, 0, 0, 255);
	processedFbo.end();

	// v3: same working resolution as processedFbo -- both passes sample the
	// same source at the same size, so their pixel grids line up exactly
	// for sampleField()/sampleColor() to agree on what a given (x,y) means.
	colorFbo.allocate(workingW, workingH, GL_RGBA);
	colorFbo.begin();
	ofClear(255, 255, 255, 255);
	colorFbo.end();

	outputFbo.allocate(std::max(canvasW, 1), std::max(canvasH, 1), GL_RGBA);
	outputFbo.begin();
	ofClear(0, 0, 0, 0);
	outputFbo.end();
}

void ContourDisplacementEffect::reloadShaders() {
	shadersLoaded = preprocessShader.load("shaders/contour_preprocess.vert", "shaders/contour_preprocess.frag");
	if (!shadersLoaded) {
		ofLogError("ContourDisplacementEffect") << "failed to load contour_preprocess shader";
	}
	if (!colorShader.load("shaders/contour_color.vert", "shaders/contour_color.frag")) {
		ofLogError("ContourDisplacementEffect") << "failed to load contour_color shader";
	}
}

void ContourDisplacementEffect::resize(int width, int height) {
	if (width == canvasW && height == canvasH) return;
	canvasW = width;
	canvasH = height;
	outputFbo.allocate(std::max(canvasW, 1), std::max(canvasH, 1), GL_RGBA);
	outputFbo.begin();
	ofClear(0, 0, 0, 0);
	outputFbo.end();
}

void ContourDisplacementEffect::setMask(ofTexture * mask) {
	activeMask = mask;
}

void ContourDisplacementEffect::setMaskPolygon(const std::vector<glm::vec2> & polygon) {
	maskPolygon = polygon;
}

void ContourDisplacementEffect::rebuildMesh() {
	int lc = std::max(lineCount.get(), 1);
	int spl = std::max(samplesPerLine.get(), 2);

	lineMesh.clear();
	lineMesh.setMode(OF_PRIMITIVE_LINES);

	int count = lc * spl;
	baseAlong.resize(count);
	lineOf.resize(count);

	for (int L = 0; L < lc; L++) {
		for (int s = 0; s < spl; s++) {
			float u = (float)s / (float)(spl - 1);
			int idx = L * spl + s;
			baseAlong[idx] = u;
			lineOf[idx] = L;
			lineMesh.addVertex(glm::vec3(0, 0, 0));
			lineMesh.addColor(ofFloatColor(1, 1, 1, 1));
		}
	}

	for (int L = 0; L < lc; L++) {
		for (int s = 0; s < spl - 1; s++) {
			int a = L * spl + s;
			int b = a + 1;
			lineMesh.addIndex(a);
			lineMesh.addIndex(b);
		}
	}

	particleVerts.assign(count, glm::vec3(0));
	particleColors.assign(count, ofFloatColor(0, 0, 0, 0));

	prevValue.assign(count, 0.0f);
	havePrevFrame = false;

	// v2 per-line state resets on topology change -- a stale index into a
	// resized array would otherwise read garbage/mismatched spans.
	lineAcrossNorm.assign(lc, 0.5f);
	lineSpans.assign(lc, { Span{ 0.0f, 1.0f } });
	prevLineSpans.clear();

	builtLineCount = lc;
	builtSamplesPerLine = spl;
	builtOrientation = orientation.get();
}

void ContourDisplacementEffect::updateTopologyIfNeeded() {
	if (lineCount.get() != builtLineCount || samplesPerLine.get() != builtSamplesPerLine || orientation.get() != builtOrientation) {
		rebuildMesh();
	}
}

void ContourDisplacementEffect::computeSourceTransform(int texW, int texH) {
	uvScale = glm::vec2(1, 1);
	uvOffset = glm::vec2(0, 0);

	int fit = sourceFitMode.get();
	if (fit == FIT_STRETCH || texW <= 0 || texH <= 0 || canvasW <= 0 || canvasH <= 0) return;

	float canvasAspect = (float)canvasW / (float)canvasH;
	float texAspect = (float)texW / (float)texH;
	bool sourceRelativelyWider = (texAspect > canvasAspect);

	if (fit == FIT_COVER) {
		if (sourceRelativelyWider) {
			float s = canvasAspect / texAspect;
			uvScale.x = s;
			uvOffset.x = (1.0f - s) * 0.5f;
		} else {
			float s = texAspect / canvasAspect;
			uvScale.y = s;
			uvOffset.y = (1.0f - s) * 0.5f;
		}
	} else { // FIT_CONTAIN
		if (sourceRelativelyWider) {
			float s = texAspect / canvasAspect;
			uvScale.y = s;
			uvOffset.y = (1.0f - s) * 0.5f;
		} else {
			float s = canvasAspect / texAspect;
			uvScale.x = s;
			uvOffset.x = (1.0f - s) * 0.5f;
		}
	}
}

void ContourDisplacementEffect::preprocessSource(ofTexture & source) {
	if (!shadersLoaded || !source.isAllocated()) return;

	computeSourceTransform((int)source.getWidth(), (int)source.getHeight());

	bool maskOn = maskEnabled.get() && activeMask != nullptr && activeMask->isAllocated();

	processedFbo.begin();
	preprocessShader.begin();
	preprocessShader.setUniformTexture("tex0", source, 0);
	if (maskOn) {
		preprocessShader.setUniformTexture("maskTex", *activeMask, 1);
	}
	preprocessShader.setUniform1i("maskEnabled", maskOn ? 1 : 0);
	preprocessShader.setUniform2f("uvScale", uvScale.x, uvScale.y);
	preprocessShader.setUniform2f("uvOffset", uvOffset.x, uvOffset.y);
	preprocessShader.setUniform1i("mirrorX", mirrorX.get() ? 1 : 0);
	preprocessShader.setUniform1i("invertSource", invertSource.get() ? 1 : 0);
	preprocessShader.setUniform1f("brightness", brightness.get());
	preprocessShader.setUniform1f("contrast", contrast.get());
	preprocessShader.setUniform1f("gamma", gamma.get());
	preprocessShader.setUniform1f("blurRadius", blurRadius.get());
	preprocessShader.setUniform1f("threshold", threshold.get());
	preprocessShader.setUniform1f("thresholdSoftness", thresholdSoftness.get());
	preprocessShader.setUniform2f("texel", 1.0f / (float)workingW, 1.0f / (float)workingH);

	ofSetColor(255);
	source.draw(0, 0, workingW, workingH);

	preprocessShader.end();
	processedFbo.end();

	// One small, bounded readback per frame (see the class-header rationale
	// for why this is the accepted CPU-fallback tradeoff for this repo's
	// GLES2/Pi target rather than a per-vertex GPU sample).
	processedFbo.readToPixels(processedPixels);
}

void ContourDisplacementEffect::preprocessColor(ofTexture & source) {
	if (!colorShader.isLoaded() || !source.isAllocated()) return;

	// Reuses uvScale/uvOffset (set by preprocessSource()'s
	// computeSourceTransform() call earlier this frame) so both passes
	// sample the source through the same mirror/fit transform -- required
	// for sampleField()/sampleColor() to stay pixel-aligned at a given
	// (x,y).
	colorFbo.begin();
	colorShader.begin();
	colorShader.setUniformTexture("tex0", source, 0);
	colorShader.setUniform2f("uvScale", uvScale.x, uvScale.y);
	colorShader.setUniform2f("uvOffset", uvOffset.x, uvOffset.y);
	colorShader.setUniform1i("mirrorX", mirrorX.get() ? 1 : 0);
	ofSetColor(255);
	source.draw(0, 0, workingW, workingH);
	colorShader.end();
	colorFbo.end();

	colorFbo.readToPixels(colorPixels);
}

float ContourDisplacementEffect::sampleField(float x, float y, int channel) const {
	if (!processedPixels.isAllocated()) return 0.0f;
	int w = processedPixels.getWidth();
	int h = processedPixels.getHeight();
	if (w < 2 || h < 2) return 0.0f;

	float fx = ofClamp(x, 0.0f, 1.0f) * (float)(w - 1);
	float fy = ofClamp(y, 0.0f, 1.0f) * (float)(h - 1);
	int x0 = (int)fx, y0 = (int)fy;
	int x1 = std::min(x0 + 1, w - 1);
	int y1 = std::min(y0 + 1, h - 1);
	float tx = fx - x0, ty = fy - y0;
	int nc = processedPixels.getNumChannels();
	const unsigned char * data = processedPixels.getData();

	auto at = [&](int px, int py) -> float {
		return data[(py * w + px) * nc + channel] / 255.0f;
	};

	float a = ofLerp(at(x0, y0), at(x1, y0), tx);
	float b = ofLerp(at(x0, y1), at(x1, y1), tx);
	return ofLerp(a, b, ty);
}

ofFloatColor ContourDisplacementEffect::sampleColor(float x, float y) const {
	if (!colorPixels.isAllocated()) return ofFloatColor(1, 1, 1, 1);
	int w = colorPixels.getWidth();
	int h = colorPixels.getHeight();
	if (w < 2 || h < 2) return ofFloatColor(1, 1, 1, 1);

	float fx = ofClamp(x, 0.0f, 1.0f) * (float)(w - 1);
	float fy = ofClamp(y, 0.0f, 1.0f) * (float)(h - 1);
	int x0 = (int)fx, y0 = (int)fy;
	int x1 = std::min(x0 + 1, w - 1);
	int y1 = std::min(y0 + 1, h - 1);
	float tx = fx - x0, ty = fy - y0;
	int nc = colorPixels.getNumChannels();
	const unsigned char * data = colorPixels.getData();

	auto at = [&](int px, int py, int ch) -> float {
		return data[(py * w + px) * nc + ch] / 255.0f;
	};

	ofFloatColor c;
	c.r = ofLerp(ofLerp(at(x0, y0, 0), at(x1, y0, 0), tx), ofLerp(at(x0, y1, 0), at(x1, y1, 0), tx), ty);
	c.g = ofLerp(ofLerp(at(x0, y0, 1), at(x1, y0, 1), tx), ofLerp(at(x0, y1, 1), at(x1, y1, 1), tx), ty);
	c.b = ofLerp(ofLerp(at(x0, y0, 2), at(x1, y0, 2), tx), ofLerp(at(x0, y1, 2), at(x1, y1, 2), tx), ty);
	c.a = 1.0f;
	return c;
}

float ContourDisplacementEffect::computeBreakupStrength(float screenX, float screenY) const {
	float axisPos;
	switch (breakupDirection.get()) {
		case BREAKUP_RIGHT_TO_LEFT: axisPos = 1.0f - screenX; break;
		case BREAKUP_TOP_TO_BOTTOM: axisPos = screenY; break;
		case BREAKUP_BOTTOM_TO_TOP: axisPos = 1.0f - screenY; break;
		case BREAKUP_RADIAL: {
			float dx = screenX - 0.5f, dy = screenY - 0.5f;
			axisPos = sqrtf(dx * dx + dy * dy) * 1.4142135f;
			break;
		}
		case BREAKUP_LEFT_TO_RIGHT:
		default: axisPos = screenX; break;
	}

	float start = breakupStart.get();
	float width = std::max(breakupWidth.get(), 0.001f);
	float t = ofClamp((axisPos - start) / width, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t); // smoothstep: soft transition, not a hard split
}

// ---- v2 Capability 2: importance-weighted line spacing ----------------

void ContourDisplacementEffect::computeLinePlacement() {
	int lc = builtLineCount;
	if ((int)lineAcrossNorm.size() != lc) lineAcrossNorm.assign(lc, 0.5f);
	if (lc <= 0) return;

	if (densityMode.get() == DENSITY_UNIFORM || lc <= 1) {
		// Reproduces the original (pre-v2) uniform placement exactly, so
		// lineSpacing keeps its original meaning whenever density weighting
		// is off. Density weighting takes over line *placement* entirely
		// when enabled -- lineSpacing becomes a no-op in that mode (see
		// README v2 addendum).
		bool horiz = (orientation.get() == ORIENT_HORIZONTAL);
		float canvasDim = horiz ? (float)canvasH : (float)canvasW;
		float lineGap = (canvasDim / (float)std::max(lc, 1)) * lineSpacing.get();
		for (int L = 0; L < lc; L++) {
			float pixelOffset = (L - (lc - 1) * 0.5f) * lineGap;
			lineAcrossNorm[L] = 0.5f + (canvasDim > 0.0f ? pixelOffset / canvasDim : 0.0f);
		}
		return;
	}

	computeDensityLUT(lineAcrossNorm);
}

void ContourDisplacementEffect::computeDensityLUT(std::vector<float> & outAcrossNorm) {
	int lc = builtLineCount;
	int res = std::max(densityLUTResolution.get(), 2);
	bool horiz = (orientation.get() == ORIENT_HORIZONTAL);
	int src = densitySource.get();
	float floorVal = std::max(densityFloor.get(), 0.0001f);
	float smoothing = densityTemporalSmoothing.get();

	if ((int)prevImportance.size() != res) {
		prevImportance.assign(res, floorVal);
	}

	// Step 1: reduce the "along" axis at each of `res` sample positions
	// across the line-index axis, into an edge-average and a luminance-
	// average profile. Bounded (alongSamples is fixed), independent of
	// samplesPerLine/lineCount, so this stays cheap regardless of mesh density.
	const int alongSamples = 24;
	std::vector<float> edgeAvg(res, 0.0f);
	std::vector<float> lumAvg(res, 0.0f);
	for (int j = 0; j < res; j++) {
		float across = (res > 1) ? (float)j / (float)(res - 1) : 0.5f;
		float edgeSum = 0.0f, lumSum = 0.0f;
		for (int k = 0; k < alongSamples; k++) {
			float along = (alongSamples > 1) ? (float)k / (float)(alongSamples - 1) : 0.5f;
			float x = horiz ? along : across;
			float y = horiz ? across : along;
			edgeSum += sampleField(x, y, 1);
			lumSum += sampleField(x, y, 0);
		}
		edgeAvg[j] = edgeSum / (float)alongSamples;
		lumAvg[j] = lumSum / (float)alongSamples;
	}

	// Step 2: importance = floor + gradient magnitude (brief's formula),
	// gradient sourced from luminance derivative, edge-channel average, or
	// a 50/50 blend of both per `densitySource`.
	std::vector<float> importance(res, 0.0f);
	for (int j = 0; j < res; j++) {
		float lumGrad = fabsf(lumAvg[std::min(j + 1, res - 1)] - lumAvg[std::max(j - 1, 0)]) * 0.5f * (float)(res - 1);
		lumGrad = ofClamp(lumGrad, 0.0f, 1.0f);
		float raw;
		if (src == DENSITY_SRC_LUMINANCE_GRADIENT) raw = lumGrad;
		else if (src == DENSITY_SRC_EDGE_MAGNITUDE) raw = edgeAvg[j];
		else raw = 0.5f * lumGrad + 0.5f * edgeAvg[j];
		importance[j] = floorVal + raw;
	}

	// Temporal smoothing of the signal itself (not just the resulting
	// positions) -- per the brief, this is what keeps live-video spacing
	// from jittering frame to frame the way raw edge detection would.
	for (int j = 0; j < res; j++) {
		importance[j] = ofLerp(importance[j], prevImportance[j], smoothing);
	}
	prevImportance = importance;

	// Step 3: integrate -> CDF.
	std::vector<float> cdf(res, 0.0f);
	float running = 0.0f;
	for (int j = 0; j < res; j++) {
		running += importance[j];
		cdf[j] = running;
	}
	float total = (running > 0.0f) ? running : 1.0f;
	for (int j = 0; j < res; j++) cdf[j] /= total;

	// Step 4: invert -- for line i, find where the CDF crosses i/(lc-1).
	outAcrossNorm.assign(lc, 0.5f);
	for (int i = 0; i < lc; i++) {
		float target = (lc > 1) ? (float)i / (float)(lc - 1) : 0.5f;
		int j = 0;
		while (j < res - 1 && cdf[j] < target) j++;
		float acrossPos;
		if (j == 0) {
			acrossPos = 0.0f;
		} else {
			float c0 = cdf[j - 1], c1 = cdf[j];
			float t = (c1 > c0) ? ofClamp((target - c0) / (c1 - c0), 0.0f, 1.0f) : 0.0f;
			float a0 = (float)(j - 1) / (float)(res - 1);
			float a1 = (float)j / (float)(res - 1);
			acrossPos = ofLerp(a0, a1, t);
		}
		outAcrossNorm[i] = acrossPos;
	}

	// Safety clamp: guarantee a minimum separation between adjacent lines so
	// an extreme single-edge image can't collapse many lines onto one
	// degenerate point (validation case the brief calls out explicitly).
	float minSep = 0.15f / (float)std::max(lc, 1);
	for (int i = 1; i < lc; i++) {
		if (outAcrossNorm[i] < outAcrossNorm[i - 1] + minSep) {
			outAcrossNorm[i] = outAcrossNorm[i - 1] + minSep;
		}
	}
	float maxV = outAcrossNorm.back();
	if (maxV > 1.0f) {
		for (int i = 0; i < lc; i++) outAcrossNorm[i] /= maxV;
	}
}

// ---- v2 Capability 1: mask-clipped line existence ----------------------

std::vector<float> ContourDisplacementEffect::scanlineCrossings(const std::vector<glm::vec2> & polygon, float acrossValue, bool horizontalLines) {
	std::vector<float> xs;
	size_t n = polygon.size();
	if (n < 3) return xs;
	for (size_t i = 0; i < n; i++) {
		glm::vec2 a = polygon[i];
		glm::vec2 b = polygon[(i + 1) % n];
		// Horizontal lines: brief's pseudocode as given (across=y, along=x).
		// Vertical lines: the same test transposed (across=x, along=y) --
		// this is the generalization the v2 doc's algorithm needs to be
		// orientation-correct; unchanged for the horizontal/default case.
		float aAcross = horizontalLines ? a.y : a.x;
		float bAcross = horizontalLines ? b.y : b.x;
		float aAlong = horizontalLines ? a.x : a.y;
		float bAlong = horizontalLines ? b.x : b.y;
		if ((aAcross <= acrossValue && bAcross > acrossValue) || (bAcross <= acrossValue && aAcross > acrossValue)) {
			float t = (acrossValue - aAcross) / (bAcross - aAcross);
			xs.push_back(aAlong + t * (bAlong - aAlong));
		}
	}
	std::sort(xs.begin(), xs.end());
	return xs;
}

void ContourDisplacementEffect::computeRowSpans() {
	int lc = builtLineCount;
	if ((int)lineSpans.size() != lc) lineSpans.assign(lc, {});
	bool horiz = (orientation.get() == ORIENT_HORIZONTAL);
	int maxSpans = maskSpansPerRow.get();

	for (int L = 0; L < lc; L++) {
		std::vector<Span> spans;
		float across = lineAcrossNorm[L];
		std::vector<float> xs = scanlineCrossings(maskPolygon, across, horiz);
		for (size_t k = 0; k + 1 < xs.size() && (int)spans.size() < maxSpans; k += 2) {
			spans.push_back({ ofClamp(xs[k], 0.0f, 1.0f), ofClamp(xs[k + 1], 0.0f, 1.0f) });
		}
		lineSpans[L] = spans; // empty = fully clipped (line entirely outside the mask at this row)
	}
}

void ContourDisplacementEffect::matchAndSmoothSpans() {
	int lc = builtLineCount;
	if ((int)prevLineSpans.size() != lc) {
		prevLineSpans.assign(lc, {});
	}

	float smoothingAlpha = 1.0f - maskTemporalSmoothing.get(); // fraction of the *new* value that survives the lerp
	float matchThreshold = maskMatchThreshold.get();

	for (int L = 0; L < lc; L++) {
		auto & newSpans = lineSpans[L];
		auto & prevSpans = prevLineSpans[L];
		for (auto & ns : newSpans) {
			float newCenter = (ns.start + ns.end) * 0.5f;
			float bestDist = std::numeric_limits<float>::max();
			Span * match = nullptr;
			for (auto & ps : prevSpans) {
				float d = fabsf(newCenter - (ps.start + ps.end) * 0.5f);
				if (d < bestDist) { bestDist = d; match = &ps; }
			}
			if (match && bestDist < matchThreshold) {
				ns.start = ofLerp(match->start, ns.start, smoothingAlpha);
				ns.end = ofLerp(match->end, ns.end, smoothingAlpha);
			}
			// Unmatched spans (bestDist >= matchThreshold, or no previous
			// spans on this line) pass through unsmoothed -- a genuine
			// topology change (e.g. an arm separating from the torso)
			// shouldn't be dragged toward a stale position.
		}
		prevSpans = newSpans;
	}
}

void ContourDisplacementEffect::computeDisplacement(float dt) {
	noiseTime += dt * noiseSpeed.get();

	int count = (int)baseAlong.size();
	if (count == 0) return;
	if (!havePrevFrame || (int)prevValue.size() != count) {
		prevValue.assign(count, 0.0f);
		havePrevFrame = true;
	}

	bool horiz = (orientation.get() == ORIENT_HORIZONTAL);

	// v2: line placement (uniform or importance-weighted) is computed once
	// per frame, before anything that depends on where a line actually is.
	computeLinePlacement();

	bool maskClipActive = maskEnabled.get() && maskSource.get() != MASK_SOURCE_NONE && maskPolygon.size() >= 3;
	if (maskClipActive) {
		// Row spans are computed from lineAcrossNorm -- i.e. from each
		// line's *actual* (possibly density-remapped) position -- not a
		// uniform index. That's what keeps masking and density-weighted
		// spacing in agreement about where "row" and "column" correspond
		// to on screen when both are active at once (the brief's "Known
		// Interaction to Resolve").
		computeRowSpans();
		matchAndSmoothSpans();
	} else if (!lineSpans.empty()) {
		for (auto & spans : lineSpans) {
			spans.assign(1, Span{ 0.0f, 1.0f });
		}
		prevLineSpans.clear(); // don't smooth into stale spans if re-enabled later
	}
	float maskSoftAlong = maskSoftness.get() * 0.15f; // fraction of line length used as the span-edge transition zone
	float edgeGlowAmt = maskEdgeGlow.get();

	float amount = displacementAmount.get() * meshScale.get();
	float bias = displacementBias.get();
	float expo = displacementExponent.get();
	glm::vec2 clampRange = displacementClamp.get();
	float clampMin = std::min(clampRange.x, clampRange.y);
	float clampMax = std::max(clampRange.x, clampRange.y);
	float spatialSm = spatialSmoothing.get();
	float temporalSm = temporalSmoothing.get();
	bool invertDepth = depthInvert.get();
	int src = displacementSource.get();
	float edgeMix = luminanceEdgeMix.get();
	float edgeAmt = edgeAmount.get();
	float secondary = secondaryDisplacement.get();

	bool doBreakup = breakupEnabled.get();
	float bAmount = breakupAmount.get();
	float nScale = noiseScale.get();
	float nStrength = noiseStrength.get();
	float jit = jitter.get();
	float drop = dropout.get();
	float fragStretch = fragmentStretch.get();
	float fragDensity = fragmentDensity.get();
	float rndSeed = (float)seed.get();

	bool useMaskGate = maskEnabled.get() && activeMask != nullptr && activeMask->isAllocated();
	float maskSoft = std::max(maskSoftness.get(), 0.001f);

	ofFloatColor baseColor = lineColor.get();
	baseColor.a = lineAlpha.get();

	bool colorBlendActive = sourceColorEnabled.get() && colorPixels.isAllocated();
	float colorAmt = sourceColorAmount.get();
	int colorMode = colorBlendMode.get();

	// Per-vertex screen-space coordinates, correct for either orientation
	// (fixes a v1 bug: field sampling and breakup-direction used to read
	// (along,lineIndex) as if it were always (x,y), which was only right
	// for horizontal lines -- vertical lines sampled/broke up along the
	// wrong screen axis. screenX/screenY below are the actual (x,y) in
	// normalized canvas space regardless of orientation.)
	auto screenXY = [&](float along, int L) -> glm::vec2 {
		float across = lineAcrossNorm[L];
		return horiz ? glm::vec2(along, across) : glm::vec2(across, along);
	};

	// Pass 1: raw field value per vertex from the selected displacement source.
	std::vector<float> raw(count);
	for (int i = 0; i < count; i++) {
		glm::vec2 xy = screenXY(baseAlong[i], lineOf[i]);
		float value;
		switch (src) {
			case SRC_LUMINANCE: value = sampleField(xy.x, xy.y, 0); break;
			case SRC_INVERTED_LUMINANCE: value = 1.0f - sampleField(xy.x, xy.y, 0); break;
			case SRC_EDGE: value = sampleField(xy.x, xy.y, 1); break;
			case SRC_THRESHOLD: value = sampleField(xy.x, xy.y, 2); break;
			case SRC_MASK: value = sampleField(xy.x, xy.y, 3); break;
			case SRC_HYBRID:
			default: {
				float lum = sampleField(xy.x, xy.y, 0);
				float edge = sampleField(xy.x, xy.y, 1);
				value = ofLerp(lum, edge, edgeMix);
				break;
			}
		}
		if (src != SRC_EDGE) {
			float edge = sampleField(xy.x, xy.y, 1);
			value = ofClamp(value + edge * edgeAmt, 0.0f, 1.0f);
		}
		raw[i] = value;
	}

	// Pass 2: spatial smoothing between neighboring samples along each line.
	std::vector<float> smoothed(raw);
	if (spatialSm > 0.001f) {
		int spl = builtSamplesPerLine;
		for (int L = 0; L < builtLineCount; L++) {
			int base = L * spl;
			for (int s = 0; s < spl; s++) {
				int idx = base + s;
				int iPrev = base + std::max(s - 1, 0);
				int iNext = base + std::min(s + 1, spl - 1);
				float avg = (raw[iPrev] + raw[idx] + raw[iNext]) / 3.0f;
				smoothed[idx] = ofLerp(raw[idx], avg, spatialSm);
			}
		}
	}

	// Pass 3: temporal smoothing against last frame's settled values.
	for (int i = 0; i < count; i++) {
		float v = ofLerp(smoothed[i], prevValue[i], temporalSm);
		smoothed[i] = v;
		prevValue[i] = v;
	}

	float length = lineLength.get();
	float posX = positionX.get() * canvasW * 0.5f;
	float posY = positionY.get() * canvasH * 0.5f;
	float scale = meshScale.get();
	float axisLenPx = (horiz ? (float)canvasW : (float)canvasH) * length;
	float acrossDimPx = horiz ? (float)canvasH : (float)canvasW;

	float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;

	for (int i = 0; i < count; i++) {
		float along = baseAlong[i];
		int L = lineOf[i];
		glm::vec2 xy = screenXY(along, L);

		float value = ofClamp(smoothed[i], clampMin, clampMax);
		float range = std::max(clampMax - clampMin, 0.0001f);
		value = powf(ofClamp((value - clampMin) / range, 0.0f, 1.0f), expo);
		float disp = (value + bias) * amount;
		if (invertDepth) disp = -disp;

		float alongPos = (along - 0.5f) * axisLenPx;
		float linePos = (lineAcrossNorm[L] - 0.5f) * acrossDimPx;

		float px, py;
		float perpX = 0.0f, perpY = 0.0f;
		if (horiz) {
			px = alongPos * scale;
			py = linePos * scale;
			perpY = 1.0f;
		} else {
			px = linePos * scale;
			py = alongPos * scale;
			perpX = 1.0f;
		}

		if (secondary > 0.0001f) {
			float n = ofSignedNoise(xy.x * nScale * 0.5f, xy.y * nScale * 0.5f, noiseTime * 0.5f + rndSeed * 13.1f);
			float alongOffset = n * secondary * amount * 0.5f;
			if (horiz) px += alongOffset; else py += alongOffset;
		}

		px += perpX * disp;
		py += perpY * disp;

		float visibility = 1.0f;
		float breakupT = 0.0f;
		if (doBreakup && bAmount > 0.0001f) {
			breakupT = computeBreakupStrength(xy.x, xy.y) * bAmount;

			if (useMaskGate) {
				float m = sampleField(xy.x, xy.y, 3); // 1 = inside subject/mask (raster mask)
				float softM = ofClamp((m - 0.5f) / maskSoft + 0.5f, 0.0f, 1.0f);
				breakupT *= (1.0f - softM); // clean inside the mask, full breakup outside it
			}

			float nx = ofSignedNoise(xy.x * nScale, xy.y * nScale, noiseTime + rndSeed * 7.0f);
			float ny = ofSignedNoise(xy.x * nScale + 100.0f, xy.y * nScale + 100.0f, noiseTime + rndSeed * 7.0f);
			float driftAmount = breakupT * nStrength * amount * 1.5f;
			px += nx * driftAmount;
			py += ny * driftAmount * (1.0f + fragStretch);

			if (jit > 0.0001f) {
				float jx = (ofNoise(i * 0.123f, noiseTime * 2.0f) - 0.5f) * 2.0f;
				float jy = (ofNoise(i * 0.456f, noiseTime * 2.0f) - 0.5f) * 2.0f;
				px += jx * jit * breakupT * 12.0f;
				py += jy * jit * breakupT * 12.0f;
			}

			float h = ofNoise(i * 0.0173f + rndSeed * 3.3f, 0.5f);
			float dropoutChance = drop * breakupT;
			if (h < dropoutChance * fragDensity) {
				visibility = 0.0f;
			} else {
				visibility = ofClamp(1.0f - breakupT * (1.0f - fragDensity), 0.0f, 1.0f);
			}
		}

		// v2 Capability 1: existence clipping + edge glow. Fully
		// transparent outside the mask (not just dim/flat), per the brief --
		// multiplies straight into visibility rather than being a separate
		// blend, so it composites correctly with breakup/dropout above.
		float maskGlow = 0.0f;
		if (maskClipActive) {
			float spanVis = 0.0f;
			for (const auto & span : lineSpans[L]) {
				float rise = smoothstepf(span.start - maskSoftAlong, span.start + maskSoftAlong, along);
				float fall = 1.0f - smoothstepf(span.end - maskSoftAlong, span.end + maskSoftAlong, along);
				spanVis = std::max(spanVis, std::min(rise, fall));
			}
			maskGlow = 4.0f * spanVis * (1.0f - spanVis); // peaks exactly at the transition, 0 deep inside/outside
			visibility *= spanVis;
		}

		glm::vec3 pos(canvasW * 0.5f + posX + px, canvasH * 0.5f + posY + py, 0.0f);
		lineMesh.setVertex(i, pos);

		ofFloatColor col = baseColor;
		if (colorBlendActive) {
			ofFloatColor srcCol = sampleColor(xy.x, xy.y);
			ofFloatColor fullEffect = blendLineColor(baseColor, srcCol, colorMode);
			col.r = ofLerp(baseColor.r, fullEffect.r, colorAmt);
			col.g = ofLerp(baseColor.g, fullEffect.g, colorAmt);
			col.b = ofLerp(baseColor.b, fullEffect.b, colorAmt);
		}
		col.a = ofClamp(col.a * visibility + maskGlow * edgeGlowAmt * visibility, 0.0f, 1.0f);
		lineMesh.setColor(i, col);

		if (visibility < 0.5f && breakupT > 0.01f) {
			particleVerts[i] = pos;
			particleColors[i] = ofFloatColor(col.r, col.g, col.b, breakupT * lineAlpha.get() * 0.6f);
		} else {
			particleColors[i].a = 0.0f;
		}

		minX = std::min(minX, pos.x); maxX = std::max(maxX, pos.x);
		minY = std::min(minY, pos.y); maxY = std::max(maxY, pos.y);
	}

	if (minX < maxX && minY < maxY) {
		meshBoundsPx.set(minX, minY, maxX - minX, maxY - minY);
	}
}

void ContourDisplacementEffect::update(ofTexture & source, float deltaTime) {
	if (qualityPreset.get() != lastQualityApplied) {
		applyQualityPreset(qualityPreset.get());
	}
	if (reloadShadersTrigger.get()) {
		reloadShaders();
		reloadShadersTrigger.set(false);
	}

	updateTopologyIfNeeded();

	if (freezeFrame.get()) return;

	if (inputEnabled.get() && source.isAllocated()) {
		lastSourceTex = &source;
		preprocessSource(source);
		// v3: opt-in second pass/readback -- skipped entirely while
		// disabled, so sourceColorEnabled=false (the default) costs nothing
		// beyond the one branch here.
		if (sourceColorEnabled.get()) {
			preprocessColor(source);
		}
	} else {
		lastSourceTex = nullptr;
		if (processedPixels.isAllocated()) processedPixels.set(0);
	}

	computeDisplacement(deltaTime);
}

void ContourDisplacementEffect::draw(const ofRectangle & bounds) {
	ofPushMatrix();
	ofPushStyle();
	ofTranslate(bounds.x, bounds.y);
	if (canvasW > 0 && (bounds.width != canvasW || bounds.height != canvasH)) {
		ofScale(bounds.width / (float)canvasW, bounds.height / (float)canvasH);
	}

	ofColor bg = backgroundColor.get();
	if (backgroundAlpha.get() > 0.001f) {
		ofSetColor(bg.r, bg.g, bg.b, (int)(backgroundAlpha.get() * 255));
		ofDrawRectangle(0, 0, canvasW, canvasH);
	}

	bool wantsSourceBehind = showSource.get() || renderMode.get() == RENDER_LINE_OVER_SOURCE;
	if (wantsSourceBehind && lastSourceTex && lastSourceTex->isAllocated()) {
		ofSetColor(255, 255, 255, (int)(sourceAlpha.get() * 255));
		lastSourceTex->draw(0, 0, canvasW, canvasH);
	}

	if (blendMode.get() == BLEND_ADDITIVE) {
		ofEnableBlendMode(OF_BLENDMODE_ADD);
	} else {
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
	}

	ofSetLineWidth(lineThickness.get());
	ofSetColor(255);
	lineMesh.draw();

	if (renderMode.get() == RENDER_EFFECT_PLUS_DISSOLVE) {
		glPointSize(std::max(lineThickness.get() * 1.4f, 1.0f));
		ofVboMesh points;
		points.setMode(OF_PRIMITIVE_POINTS);
		for (size_t i = 0; i < particleVerts.size(); i++) {
			if (particleColors[i].a <= 0.001f) continue;
			points.addVertex(particleVerts[i]);
			points.addColor(particleColors[i]);
		}
		points.draw();
	}

	ofDisableBlendMode();

	if (showMeshBounds.get()) {
		ofNoFill();
		ofSetColor(255, 0, 0);
		ofDrawRectangle(meshBoundsPx);

		if (maskPolygon.size() >= 3) {
			ofSetColor(0, 255, 0);
			ofBeginShape();
			for (const auto & p : maskPolygon) {
				ofVertex(p.x * canvasW, p.y * canvasH);
			}
			ofEndShape(true);
		}
		ofFill();
	}

	ofPopStyle();
	ofPopMatrix();
}

ofTexture & ContourDisplacementEffect::drawToFbo(const ofRectangle & bounds) {
	(void)bounds;
	outputFbo.begin();
	ofClear(0, 0, 0, 0);
	draw(ofRectangle(0, 0, canvasW, canvasH));
	outputFbo.end();
	return outputFbo.getTexture();
}
