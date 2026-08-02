#pragma once

#include "ofMain.h"
#include "ofxGui.h"

class ContourDisplacementEffect;
namespace ContourPresets {
	void apply(ContourDisplacementEffect & fx, const std::string & name);
}

// Contour-line displacement portrait effect: a subject reconstructed from
// dense parallel scanlines that bend with source-image luminance/edges and
// fragment into a controllable dissolve toward one edge of the frame.
//
// Rendering approach (see README.md "Rendering approach" for the full
// rationale): a single small GPU preprocessing pass (grayscale, blur,
// contrast/gamma, threshold, edge magnitude, mask) is read back once per
// frame into a small ofPixels buffer, and the line mesh's vertex positions
// are displaced on the CPU by sampling that buffer. This is the CPU
// fallback the brief explicitly allows in place of vertex-texture-fetch:
// this repo's real deployment target for this monorepo (Raspberry Pi 3B,
// VideoCore IV / GLES2) does not reliably support sampling textures from a
// vertex shader, so the portable path is the primary path here rather than
// a desktop-only optimization. The one readback is small (order of
// 100-300px on a side) and happens once per frame regardless of mesh
// density, not per vertex.
//
// v2 additions (mask-clipped line existence + importance-weighted line
// spacing) follow the same CPU-first policy for the same reason: the v2
// brief's GPU path (row-span texture sampled in a vertex shader, GPU
// column-reduction for the density LUT) is described there as the
// preferred path with a CPU fallback "consistent with constrained
// hardware" -- that fallback is what's implemented here, for the same
// GLES2/Pi reason as the base effect, not as a separate decision.
//
// Mesh topology (vertex/index counts) is rebuilt only when lineCount,
// samplesPerLine or orientation change; per-frame work only ever rewrites
// vertex positions/colors on the existing buffers. Line *placement*
// (lineAcrossNorm) and mask spans (lineSpans) ARE recomputed every frame
// when their respective features are enabled -- that's content-driven
// motion, not topology, same category as displacement itself.
class ContourDisplacementEffect {
public:
	enum Orientation { ORIENT_HORIZONTAL = 0, ORIENT_VERTICAL = 1 };
	enum SourceFit { FIT_STRETCH = 0, FIT_CONTAIN = 1, FIT_COVER = 2 };
	enum DisplacementSource {
		SRC_LUMINANCE = 0,
		SRC_INVERTED_LUMINANCE = 1,
		SRC_EDGE = 2,
		SRC_THRESHOLD = 3,
		SRC_MASK = 4,
		SRC_HYBRID = 5
	};
	enum BreakupDirection {
		BREAKUP_LEFT_TO_RIGHT = 0,
		BREAKUP_RIGHT_TO_LEFT = 1,
		BREAKUP_TOP_TO_BOTTOM = 2,
		BREAKUP_BOTTOM_TO_TOP = 3,
		BREAKUP_RADIAL = 4
	};
	enum RenderMode {
		RENDER_LINE_ONLY = 0,
		RENDER_LINE_OVER_SOURCE = 1,
		RENDER_MASKED_OVERLAY = 2,
		RENDER_EFFECT_PLUS_DISSOLVE = 3
	};
	enum BlendMode { BLEND_ALPHA = 0, BLEND_ADDITIVE = 1 };
	enum QualityPreset { QUALITY_DESKTOP = 0, QUALITY_CONSTRAINED = 1, QUALITY_LOW_RES_PROJECTION = 2 };

	// v2
	enum MaskSource { MASK_SOURCE_NONE = 0, MASK_SOURCE_BACKGROUND_SUBTRACT = 1, MASK_SOURCE_EXTERNAL_TEXTURE = 2 };
	enum DensityMode { DENSITY_UNIFORM = 0, DENSITY_IMPORTANCE_WEIGHTED = 1 };
	enum DensitySource { DENSITY_SRC_LUMINANCE_GRADIENT = 0, DENSITY_SRC_EDGE_MAGNITUDE = 1, DENSITY_SRC_HYBRID = 2 };

	// v3: line color <- source color blending
	enum ColorBlendMode {
		COLOR_BLEND_MIX = 0,
		COLOR_BLEND_MULTIPLY = 1,
		COLOR_BLEND_SCREEN = 2,
		COLOR_BLEND_ADD = 3,
		COLOR_BLEND_OVERLAY = 4,
		COLOR_BLEND_DIFFERENCE = 5,
		COLOR_BLEND_SUBTRACT = 6,
		COLOR_BLEND_EXCLUSION = 7
	};

	void setup(int width, int height);
	void resize(int width, int height);
	void update(ofTexture & source, float deltaTime);
	void draw(const ofRectangle & bounds);
	void setMask(ofTexture * mask);
	void rebuildMesh();
	void reloadShaders();

	// v2: supplies the silhouette boundary that clips line existence
	// (Capability 1). Coordinate-space contract: normalized [0,1] canvas
	// space, (0,0)=top-left -- the same space as the preprocess field and
	// baseAlong/lineAcrossNorm below. Pass an empty vector to clear it.
	// Caller (ofApp/ContourMaskSource) owns polygon computation; this class
	// only consumes it.
	void setMaskPolygon(const std::vector<glm::vec2> & polygon);

	// Renders into an internally-owned FBO instead of directly to screen, so
	// the effect can be composited as a layer elsewhere. Returns that FBO's
	// texture; safe to call every frame, no reallocation once sized.
	ofTexture & drawToFbo(const ofRectangle & bounds);

	ofParameterGroup & parameters() { return params; }

	// ---- debug view accessors (ofApp draws these; all off by default) ----
	ofTexture & getSourceDebugTexture() { return lastSourceTex ? *lastSourceTex : blankDebugTex; }
	ofTexture & getProcessedDebugTexture() { return processedFbo.getTexture(); }
	ofTexture & getMaskDebugTexture() { return activeMask ? *activeMask : blankDebugTex; }
	ofTexture & getColorDebugTexture() { return colorFbo.isAllocated() ? colorFbo.getTexture() : blankDebugTex; }
	const ofRectangle & getMeshBoundsPx() const { return meshBoundsPx; }
	int getVertexCount() const { return (int)baseAlong.size(); }
	const std::vector<glm::vec2> & getMaskPolygon() const { return maskPolygon; }
	const std::vector<float> & getLineAcrossNorm() const { return lineAcrossNorm; }

private:
	friend void ContourPresets::apply(ContourDisplacementEffect & fx, const std::string & name);

	struct Span {
		float start, end;
	};

	void allocateFbos();
	void preprocessSource(ofTexture & source);
	void preprocessColor(ofTexture & source);
	void updateTopologyIfNeeded();
	void computeSourceTransform(int texW, int texH);
	void computeDisplacement(float dt);
	float sampleField(float x, float y, int channel) const;
	ofFloatColor sampleColor(float x, float y) const;
	float computeBreakupStrength(float screenX, float screenY) const;
	void applyQualityPreset(int quality);

	// v2
	void computeLinePlacement();                               // fills lineAcrossNorm (uniform or density-weighted)
	void computeDensityLUT(std::vector<float> & outAcrossNorm); // Capability 2
	void computeRowSpans();                                          // fills lineSpans from maskPolygon + lineAcrossNorm, Capability 1
	void matchAndSmoothSpans();                                      // temporal span matching
	static std::vector<float> scanlineCrossings(const std::vector<glm::vec2> & polygon, float acrossValue, bool horizontalLines);

	// ---- GPU resources ----
	ofShader preprocessShader;
	ofShader colorShader; // v3: companion pass, source color for line/source blending
	ofFbo processedFbo;
	ofFbo colorFbo;
	ofFbo outputFbo;
	ofPixels processedPixels;
	ofPixels colorPixels;

	ofVboMesh lineMesh;
	std::vector<glm::vec3> particleVerts; // dissolve-mode particle overlay
	std::vector<ofFloatColor> particleColors;

	ofTexture * activeMask = nullptr;
	ofTexture blankDebugTex;
	ofTexture * lastSourceTex = nullptr;

	int canvasW = 0, canvasH = 0;
	int workingW = 128, workingH = 72;
	int lastQualityApplied = -1;

	int builtLineCount = -1, builtSamplesPerLine = -1, builtOrientation = -1;
	std::vector<float> baseAlong;         // per-vertex position along its own line, normalized [0,1]
	std::vector<int> lineOf;              // which line each vertex belongs to
	std::vector<float> prevValue;         // temporal smoothing state (raw field value)
	bool havePrevFrame = false;

	// v2 per-line (not per-vertex) state, recomputed each frame in
	// computeDisplacement() before the per-vertex loop runs, so every
	// vertex on a line reads the same line-level answer.
	std::vector<float> lineAcrossNorm;             // per-line position across the stack, normalized [0,1]
	std::vector<float> prevImportance;             // temporal smoothing state for the density signal
	std::vector<std::vector<Span>> lineSpans;      // per-line visible spans, in "along" units [0,1]
	std::vector<std::vector<Span>> prevLineSpans;  // previous frame's spans, for temporal matching
	std::vector<glm::vec2> maskPolygon;            // normalized canvas space; empty = no clipping

	glm::vec2 uvScale{ 1, 1 }, uvOffset{ 0, 0 };
	ofRectangle meshBoundsPx;

	float noiseTime = 0.0f;
	bool shadersLoaded = false;

	ofParameterGroup params;

	ofParameterGroup inputGroup;
	ofParameter<bool> inputEnabled{ "Input Enabled", true };
	ofParameter<int> inputMode{ "Input Mode (0=Img 1=Vid 2=Cam)", 1, 0, 2 };
	ofParameter<bool> mirrorX{ "Mirror X", false };
	ofParameter<bool> invertSource{ "Invert Source", false };
	ofParameter<int> sourceFitMode{ "Source Fit (0=Stretch 1=Contain 2=Cover)", 2, 0, 2 };
	ofParameter<float> sourceOpacity{ "Source Field Opacity", 1.0f, 0.0f, 1.0f };

	ofParameterGroup preprocessGroup;
	ofParameter<float> blurRadius{ "Blur Radius", 0.0f, 0.0f, 1.0f };
	ofParameter<float> contrast{ "Contrast", 1.0f, 0.0f, 2.0f };
	ofParameter<float> brightness{ "Brightness", 0.0f, -1.0f, 1.0f };
	ofParameter<float> gamma{ "Gamma", 1.0f, 0.2f, 3.0f };
	ofParameter<float> threshold{ "Threshold", 0.5f, 0.0f, 1.0f };
	ofParameter<float> thresholdSoftness{ "Threshold Softness", 0.15f, 0.0f, 1.0f };
	ofParameter<float> edgeAmount{ "Edge Amount", 0.3f, 0.0f, 1.0f };
	ofParameter<float> luminanceEdgeMix{ "Luminance/Edge Mix", 0.5f, 0.0f, 1.0f };

	ofParameterGroup geometryGroup;
	ofParameter<int> orientation{ "Orientation (0=Horiz 1=Vert)", ORIENT_HORIZONTAL, 0, 1 };
	ofParameter<int> lineCount{ "Line Count", 140, 4, 400 };
	ofParameter<int> samplesPerLine{ "Samples Per Line", 240, 8, 800 };
	ofParameter<float> lineSpacing{ "Line Spacing", 1.0f, 0.1f, 4.0f };
	ofParameter<float> lineThickness{ "Line Thickness", 1.5f, 0.5f, 6.0f };
	ofParameter<float> lineLength{ "Line Length", 1.0f, 0.1f, 1.0f };
	ofParameter<float> meshScale{ "Mesh Scale", 1.0f, 0.1f, 2.0f };
	ofParameter<float> positionX{ "Position X", 0.0f, -1.0f, 1.0f };
	ofParameter<float> positionY{ "Position Y", 0.0f, -1.0f, 1.0f };

	ofParameterGroup displacementGroup;
	ofParameter<int> displacementSource{ "Displacement Source", SRC_HYBRID, 0, 5 };
	ofParameter<float> displacementAmount{ "Displacement Amount", 60.0f, 0.0f, 400.0f };
	ofParameter<float> displacementBias{ "Displacement Bias", 0.0f, -1.0f, 1.0f };
	ofParameter<float> displacementExponent{ "Displacement Exponent", 1.0f, 0.2f, 4.0f };
	ofParameter<float> secondaryDisplacement{ "Secondary Displacement", 0.0f, 0.0f, 1.0f };
	ofParameter<float> spatialSmoothing{ "Spatial Smoothing", 0.35f, 0.0f, 1.0f };
	ofParameter<float> temporalSmoothing{ "Temporal Smoothing", 0.4f, 0.0f, 0.98f };
	ofParameter<glm::vec2> displacementClamp{ "Displacement Clamp (min,max)", glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f) };
	ofParameter<bool> depthInvert{ "Depth Invert", false };

	ofParameterGroup breakupGroup;
	ofParameter<bool> breakupEnabled{ "Breakup Enabled", false };
	ofParameter<float> breakupAmount{ "Breakup Amount", 0.0f, 0.0f, 1.0f };
	ofParameter<float> breakupStart{ "Breakup Start", 0.6f, 0.0f, 1.0f };
	ofParameter<float> breakupWidth{ "Breakup Width", 0.3f, 0.01f, 1.0f };
	ofParameter<int> breakupDirection{ "Breakup Direction", BREAKUP_LEFT_TO_RIGHT, 0, 4 };
	ofParameter<float> dropout{ "Dropout", 0.0f, 0.0f, 1.0f };
	ofParameter<float> noiseScale{ "Noise Scale", 4.0f, 0.5f, 20.0f };
	ofParameter<float> noiseSpeed{ "Noise Speed", 0.5f, 0.0f, 5.0f };
	ofParameter<float> noiseStrength{ "Noise Strength", 0.4f, 0.0f, 1.0f };
	ofParameter<float> jitter{ "Jitter", 0.0f, 0.0f, 1.0f };
	ofParameter<float> fragmentStretch{ "Fragment Stretch", 0.3f, 0.0f, 1.0f };
	ofParameter<float> fragmentDensity{ "Fragment Density", 1.0f, 0.0f, 1.0f };
	ofParameter<int> seed{ "Seed", 0, 0, 9999 };

	ofParameterGroup appearanceGroup;
	ofParameter<ofColor> lineColor{ "Line Color", ofColor(255), ofColor(0), ofColor(255) };
	ofParameter<float> lineAlpha{ "Line Alpha", 1.0f, 0.0f, 1.0f };
	ofParameter<ofColor> backgroundColor{ "Background Color", ofColor(0), ofColor(0), ofColor(255) };
	ofParameter<float> backgroundAlpha{ "Background Alpha", 1.0f, 0.0f, 1.0f };
	ofParameter<int> renderMode{ "Render Mode (0-3)", RENDER_LINE_ONLY, 0, 3 };
	ofParameter<int> blendMode{ "Blend Mode (0=Alpha 1=Add)", BLEND_ALPHA, 0, 1 };
	ofParameter<bool> showSource{ "Show Source", false };
	ofParameter<float> sourceAlpha{ "Source Alpha", 0.6f, 0.0f, 1.0f };

	// v3: line color <- source color blending. Off by default (amount has
	// no effect until enabled, and the color pass/readback is skipped
	// entirely while disabled -- see preprocessColor()'s call site).
	ofParameter<bool> sourceColorEnabled{ "Source Color Enabled", false };
	ofParameter<float> sourceColorAmount{ "Source Color Amount", 0.7f, 0.0f, 1.0f };
	ofParameter<int> colorBlendMode{ "Color Blend Mode (0=Mix 1=Mul 2=Screen 3=Add 4=Overlay 5=Diff 6=Sub 7=Excl)", COLOR_BLEND_MIX, 0, 7 };

	// v2: dedicated Mask group -- maskEnabled/maskSoftness moved here from
	// Appearance (they were the only mask-related fields at v1) so all mask
	// controls live in one place, per the v2 brief's "Mask (extends
	// existing mask fields)" framing.
	ofParameterGroup maskGroup;
	ofParameter<bool> maskEnabled{ "Mask Enabled", false };
	ofParameter<float> maskSoftness{ "Mask Softness", 0.2f, 0.0f, 1.0f };
	ofParameter<int> maskSource{ "Mask Source (0=None 1=BgSubtract 2=ExtTexture)", MASK_SOURCE_NONE, 0, 2 };
	ofParameter<int> maskSpansPerRow{ "Mask Spans Per Row", 4, 1, 8 };
	ofParameter<float> maskEdgeGlow{ "Mask Edge Glow", 0.4f, 0.0f, 2.0f };
	ofParameter<float> maskTemporalSmoothing{ "Mask Temporal Smoothing", 0.5f, 0.0f, 0.98f };
	ofParameter<float> maskMatchThreshold{ "Mask Match Threshold", 0.08f, 0.0f, 0.5f };

	// v2: Line Density group -- Capability 2 (importance-weighted spacing)
	ofParameterGroup densityGroup;
	ofParameter<int> densityMode{ "Density Mode (0=Uniform 1=ImportanceWeighted)", DENSITY_UNIFORM, 0, 1 };
	ofParameter<float> densityFloor{ "Density Floor", 0.15f, 0.0f, 1.0f };
	ofParameter<int> densitySource{ "Density Source (0=LumGrad 1=Edge 2=Hybrid)", DENSITY_SRC_HYBRID, 0, 2 };
	ofParameter<float> densityTemporalSmoothing{ "Density Temporal Smoothing", 0.6f, 0.0f, 0.98f };
	ofParameter<int> densityLUTResolution{ "Density LUT Resolution", 128, 16, 512 };

	ofParameterGroup debugGroup;
	ofParameter<bool> showGui{ "Show Gui", true };
	ofParameter<bool> showSourceDebug{ "Show Source Debug", false };
	ofParameter<bool> showProcessedDebug{ "Show Processed Debug", false };
	ofParameter<bool> showMaskDebug{ "Show Mask Debug", false };
	ofParameter<bool> showColorDebug{ "Show Color Debug", false };
	ofParameter<bool> showMeshBounds{ "Show Mesh Bounds", false };
	ofParameter<bool> freezeFrame{ "Freeze Frame", false };
	ofParameter<bool> reloadShadersTrigger{ "Reload Shaders", false };
	ofParameter<bool> showFps{ "Show Fps", false };
	ofParameter<int> qualityPreset{ "Quality (0=Desktop 1=Constrained 2=LowRes)", QUALITY_DESKTOP, 0, 2 };
};
