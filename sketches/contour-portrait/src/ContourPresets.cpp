#include "ContourPresets.h"

namespace ContourPresets {

	const std::vector<std::string> & names() {
		static const std::vector<std::string> n = {
			"Clean Portrait",
			"Topographic Figure",
			"Side Dissolve",
			"Analog Scan",
			"Ghost Contour",
			"Silhouette Emergence",
			"Converging Contours"
		};
		return n;
	}

	// Applies directly to ContourDisplacementEffect's private ofParameter
	// members (this function is a declared friend) rather than looking
	// values up by string name through the ofParameterGroup tree — a typo
	// in a get<T>("...") call fails silently at runtime, a typo in a member
	// name fails to compile. Only parameters a given preset actually cares
	// about are touched; everything else keeps its current/default value,
	// same as flipping a handful of sliders by hand.
	void apply(ContourDisplacementEffect & fx, const std::string & name) {
		using FX = ContourDisplacementEffect;

		// Shared readable-portrait baseline every preset starts from, then
		// overrides on top — keeps the five presets from silently diverging
		// on parameters nobody meant to touch.
		fx.inputEnabled = true;
		fx.mirrorX = false;
		fx.invertSource = false;
		fx.sourceFitMode = FX::FIT_COVER;
		fx.sourceOpacity = 1.0f;

		fx.blurRadius = 0.05f;
		fx.contrast = 1.15f;
		fx.brightness = 0.0f;
		fx.gamma = 1.0f;
		fx.threshold = 0.5f;
		fx.thresholdSoftness = 0.2f;
		fx.edgeAmount = 0.25f;
		fx.luminanceEdgeMix = 0.35f;

		fx.orientation = FX::ORIENT_HORIZONTAL;
		fx.lineSpacing = 1.0f;
		fx.lineThickness = 1.5f;
		fx.lineLength = 1.0f;
		fx.meshScale = 1.0f;
		fx.positionX = 0.0f;
		fx.positionY = 0.0f;

		fx.displacementSource = FX::SRC_HYBRID;
		fx.displacementBias = 0.0f;
		fx.displacementExponent = 1.0f;
		fx.secondaryDisplacement = 0.0f;
		fx.displacementClamp = glm::vec2(0.0f, 1.0f);
		fx.depthInvert = false;

		fx.breakupEnabled = false;
		fx.breakupAmount = 0.0f;
		fx.breakupStart = 0.6f;
		fx.breakupWidth = 0.3f;
		fx.breakupDirection = FX::BREAKUP_LEFT_TO_RIGHT;
		fx.dropout = 0.0f;
		fx.noiseScale = 4.0f;
		fx.noiseSpeed = 0.5f;
		fx.noiseStrength = 0.4f;
		fx.jitter = 0.0f;
		fx.fragmentStretch = 0.3f;
		fx.fragmentDensity = 1.0f;

		fx.lineColor = ofColor(255);
		fx.lineAlpha = 1.0f;
		fx.backgroundColor = ofColor(0);
		fx.backgroundAlpha = 1.0f;
		fx.renderMode = FX::RENDER_LINE_ONLY;
		fx.blendMode = FX::BLEND_ALPHA;
		fx.showSource = false;
		fx.sourceAlpha = 0.6f;
		fx.sourceColorEnabled = false;
		fx.sourceColorAmount = 0.7f;
		fx.colorBlendMode = FX::COLOR_BLEND_MIX;
		fx.maskEnabled = false;
		fx.maskSoftness = 0.2f;
		fx.maskSource = FX::MASK_SOURCE_NONE;
		fx.maskSpansPerRow = 4;
		fx.maskEdgeGlow = 0.4f;
		fx.maskTemporalSmoothing = 0.5f;
		fx.maskMatchThreshold = 0.08f;

		fx.densityMode = FX::DENSITY_UNIFORM;
		fx.densityFloor = 0.15f;
		fx.densitySource = FX::DENSITY_SRC_HYBRID;
		fx.densityTemporalSmoothing = 0.6f;
		fx.densityLUTResolution = 128;

		if (name == "Clean Portrait") {
			fx.lineCount = 180;
			fx.samplesPerLine = 320;
			fx.displacementAmount = 45.0f;
			fx.spatialSmoothing = 0.4f;
			fx.temporalSmoothing = 0.35f;
			fx.breakupEnabled = false;

		} else if (name == "Topographic Figure") {
			fx.lineCount = 90;
			fx.samplesPerLine = 220;
			fx.lineSpacing = 1.6f;
			fx.displacementAmount = 70.0f;
			fx.displacementSource = FX::SRC_HYBRID;
			fx.luminanceEdgeMix = 0.5f;
			fx.edgeAmount = 0.4f;
			fx.spatialSmoothing = 0.55f;
			fx.temporalSmoothing = 0.5f;
			fx.breakupEnabled = false;

		} else if (name == "Side Dissolve") {
			fx.lineCount = 150;
			fx.samplesPerLine = 280;
			fx.displacementAmount = 55.0f;
			fx.spatialSmoothing = 0.3f;
			fx.temporalSmoothing = 0.3f;
			fx.breakupEnabled = true;
			fx.breakupAmount = 0.8f;
			fx.breakupStart = 0.55f;
			fx.breakupWidth = 0.4f;
			fx.breakupDirection = FX::BREAKUP_LEFT_TO_RIGHT;
			fx.dropout = 0.35f;
			fx.noiseStrength = 0.5f;
			fx.fragmentDensity = 0.7f;
			fx.renderMode = FX::RENDER_EFFECT_PLUS_DISSOLVE;

		} else if (name == "Analog Scan") {
			fx.lineCount = 220;
			fx.samplesPerLine = 380;
			fx.lineThickness = 1.0f;
			fx.displacementAmount = 35.0f;
			fx.spatialSmoothing = 0.25f;
			fx.temporalSmoothing = 0.15f;
			fx.secondaryDisplacement = 0.08f;
			fx.breakupEnabled = true;
			fx.breakupAmount = 0.15f;
			fx.breakupStart = 0.85f;
			fx.breakupWidth = 0.3f;
			fx.jitter = 0.2f;
			fx.dropout = 0.05f;
			fx.noiseSpeed = 1.2f;
			fx.noiseStrength = 0.2f;

		} else if (name == "Ghost Contour") {
			fx.lineCount = 110;
			fx.samplesPerLine = 240;
			fx.displacementAmount = 50.0f;
			fx.blurRadius = 0.3f;
			fx.spatialSmoothing = 0.6f;
			fx.temporalSmoothing = 0.75f;
			fx.lineAlpha = 0.45f;
			fx.blendMode = FX::BLEND_ADDITIVE;
			fx.breakupEnabled = false;

		} else if (name == "Silhouette Emergence") {
			// v2 Capability 1 + 2 together, breakup off, so the two new
			// techniques are what's doing the work here -- nothing else is
			// competing for attention. Needs a mask polygon actually wired
			// up (ContourMaskSource -> setMaskPolygon()) to look like the
			// reference; with none supplied, maskClipActive is simply false
			// and this falls back to the full-frame look, which is the
			// documented no-mask-supplied stability behavior, not a bug.
			fx.lineCount = 160;
			fx.samplesPerLine = 300;
			fx.displacementAmount = 40.0f;
			fx.spatialSmoothing = 0.35f;
			fx.temporalSmoothing = 0.45f;
			fx.displacementSource = FX::SRC_HYBRID;
			fx.luminanceEdgeMix = 0.4f;
			fx.edgeAmount = 0.3f;
			fx.breakupEnabled = false;
			fx.renderMode = FX::RENDER_MASKED_OVERLAY;
			fx.maskEnabled = true;
			fx.maskSource = FX::MASK_SOURCE_BACKGROUND_SUBTRACT;
			fx.maskSoftness = 0.15f;
			fx.maskEdgeGlow = 1.2f;
			fx.maskTemporalSmoothing = 0.6f;
			fx.densityMode = FX::DENSITY_IMPORTANCE_WEIGHTED;
			fx.densitySource = FX::DENSITY_SRC_HYBRID;
			fx.densityFloor = 0.12f;
			fx.densityTemporalSmoothing = 0.65f;

		} else if (name == "Converging Contours") {
			// Density-weighted spacing alone, mask off, against the full
			// frame -- isolates Capability 2 to match the "2-3 lines in
			// open space, dense bands at the face" reference look.
			fx.lineCount = 60;
			fx.samplesPerLine = 260;
			fx.displacementAmount = 30.0f;
			fx.spatialSmoothing = 0.3f;
			fx.temporalSmoothing = 0.4f;
			fx.breakupEnabled = false;
			fx.maskEnabled = false;
			fx.maskSource = FX::MASK_SOURCE_NONE;
			fx.densityMode = FX::DENSITY_IMPORTANCE_WEIGHTED;
			fx.densitySource = FX::DENSITY_SRC_HYBRID;
			fx.densityFloor = 0.06f; // low floor -> strong convergence contrast
			fx.densityTemporalSmoothing = 0.6f;
		}
	}

} // namespace ContourPresets
