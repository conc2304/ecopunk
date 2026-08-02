#include "DefaultVideoEffectCatalog.h"
#include "SinglePassShaderEffect.h"
#include <memory>

// Literal defaults/ranges below are transcribed, not redesigned, from the
// existing dispatch code this catalog consolidates — see
// docs/shader-effect-system-probe.md §3A/§5/§10 for the four independent
// call sites (VideoRegionEffectRenderer::setEffectUniforms,
// BEFragment::setEffectUniforms/pickAndStartEffect,
// TFEffectPicker::applyEffectUniforms/randomizeEffectParams,
// Quadrant::drawWithEffect + DebugMode) every value here traces back to.
// Artistic ranges come from BEFragment.cpp/TFEffectPicker.cpp's
// ofRandom(lo, hi) call sites where randomization exists; effects that are
// never randomized anywhere today keep artisticMin == artisticMax == the
// fixed literal, and randomizable=false, so migrating a sketch onto this
// catalog does not introduce new randomized behavior it didn't already have.
namespace videoeffects {

	namespace {
		VideoEffectParameterSchema floatParam(
			std::string id, std::string label, float defaultValue, float hardMin, float hardMax, float artisticMin,
			float artisticMax, float step = 0.01f, bool randomizable = true, bool safeToAnimate = true) {
			VideoEffectParameterSchema p;
			p.id = std::move(id);
			p.label = std::move(label);
			p.type = VideoEffectParameterType::Float;
			p.defaultValue = defaultValue;
			p.hardMin = hardMin;
			p.hardMax = hardMax;
			p.artisticMin = artisticMin;
			p.artisticMax = artisticMax;
			p.step = step;
			p.randomizable = randomizable;
			p.safeToAnimate = safeToAnimate;
			return p;
		}

		VideoEffectParameterSchema intParam(
			std::string id, std::string label, int defaultValue, int hardMin, int hardMax, bool randomizable = true) {
			VideoEffectParameterSchema p;
			p.id = std::move(id);
			p.label = std::move(label);
			p.type = VideoEffectParameterType::Int;
			p.defaultValue = defaultValue;
			p.hardMin = hardMin;
			p.hardMax = hardMax;
			p.artisticMin = hardMin;
			p.artisticMax = hardMax;
			p.step = 1.0f;
			p.randomizable = randomizable;
			return p;
		}

		VideoEffectParameterSchema boolParam(std::string id, std::string label, bool defaultValue, bool randomizable = false) {
			VideoEffectParameterSchema p;
			p.id = std::move(id);
			p.label = std::move(label);
			p.type = VideoEffectParameterType::Bool;
			p.defaultValue = defaultValue;
			p.hardMin = false;
			p.hardMax = true;
			p.artisticMin = false;
			p.artisticMax = true;
			p.randomizable = randomizable;
			return p;
		}

		VideoEffectParameterSchema vec3Param(std::string id, std::string label, glm::vec3 defaultValue) {
			VideoEffectParameterSchema p;
			p.id = std::move(id);
			p.label = std::move(label);
			p.type = VideoEffectParameterType::Vec3;
			p.defaultValue = defaultValue;
			p.hardMin = glm::vec3(0.0f);
			p.hardMax = glm::vec3(1.0f);
			p.artisticMin = defaultValue;
			p.artisticMax = defaultValue;
			// Not currently randomized anywhere in the codebase this
			// consolidates — every existing call site uses a fixed literal
			// color. EffectRandomizer doesn't implement vector randomization
			// in v1 regardless (see EffectRandomizer.h), so this is belt-and-suspenders.
			p.randomizable = false;
			return p;
		}

		VideoEffectParameterSchema alphaParam(float defaultValue = 1.0f) {
			return floatParam("alpha", "Alpha", defaultValue, 0.0f, 1.0f, 0.0f, 1.0f, 0.02f, false, true);
		}

		std::vector<std::string> effectsPoolAssets(const std::string & fragFile) {
			return { "common/vert.glsl", "single-pass/" + fragFile };
		}

		void registerSimplePassthroughEffect(VideoEffectRegistry & registry, const std::string & id, const std::string & displayName, const std::string & fragFile) {
			VideoEffectDefinition def;
			def.id = id;
			def.displayName = displayName;
			def.kind = VideoEffectKind::SinglePassShader;
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets(fragFile);
			def.requiredUniforms = { "tex", "alpha" };
			def.params = { alphaParam() };
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}
	} // namespace

	void registerSinglePassEffects(VideoEffectRegistry & registry) {
		// --- simple opacity-only effects: desaturate, invert, solarize, scanlines ---
		registerSimplePassthroughEffect(registry, "desaturate", "Desaturate", "desaturate.glsl");
		registerSimplePassthroughEffect(registry, "invert", "Invert", "invert.glsl");
		registerSimplePassthroughEffect(registry, "solarize", "Solarize", "solarize.glsl");
		registerSimplePassthroughEffect(registry, "scanlines", "Scanlines", "scanlines.glsl");

		// --- recolor ---
		{
			VideoEffectDefinition def;
			def.id = "recolor";
			def.displayName = "Recolor";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("recolor.glsl");
			def.requiredUniforms = { "tex", "tint", "alpha" };
			def.params = { vec3Param("tint", "Tint", glm::vec3(0.85f, 0.55f, 0.20f)), alphaParam() };
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}

		// --- threshold --- (randomized 0.35-0.65 in BEFragment.cpp/TFEffectPicker.cpp)
		{
			VideoEffectDefinition def;
			def.id = "threshold";
			def.displayName = "Threshold";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("threshold.glsl");
			def.requiredUniforms = { "tex", "threshold", "alpha" };
			def.params = { floatParam("threshold", "Threshold", 0.5f, 0.0f, 1.0f, 0.35f, 0.65f), alphaParam() };
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}

		// --- dither --- ("alpha" is arc position, not opacity — see dither.glsl's own comment)
		{
			VideoEffectDefinition def;
			def.id = "dither";
			def.displayName = "Dither";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("dither.glsl");
			def.requiredUniforms = { "tex", "resolution", "alpha", "opacity", "maxPixelation" };
			def.params = {
				floatParam("alpha", "Arc position", 0.5f, 0.0f, 1.0f, 0.15f, 0.85f),
				floatParam("opacity", "Opacity", 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.02f, false),
				floatParam("maxPixelation", "Max pixelation", 6.0f, 1.0f, 32.0f, 2.0f, 10.0f, 0.5f),
			};
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}

		// --- channelshift --- (randomized 0.002-0.01 in BEFragment.cpp/TFEffectPicker.cpp)
		{
			VideoEffectDefinition def;
			def.id = "channelshift";
			def.displayName = "Channel shift";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("channelshift.glsl");
			def.requiredUniforms = { "tex", "shift", "alpha" };
			def.params = { floatParam("shift", "Shift", 0.005f, 0.0f, 0.05f, 0.002f, 0.01f, 0.001f), alphaParam() };
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}

		// --- heatmap_recolor --- (see shared/assets/video-effects/single-pass/heatmap_recolor.glsl)
		{
			VideoEffectDefinition def;
			def.id = "heatmap_recolor";
			def.displayName = "Heatmap Recolor";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("heatmap_recolor.glsl");
			def.requiredUniforms = { "tex", "alpha", "intensity", "gamma", "minLuminance", "maxLuminance", "palette", "reverse" };
			def.params = {
				alphaParam(),
				floatParam("intensity", "Intensity", 1.0f, 0.0f, 2.0f, 0.5f, 1.5f),
				floatParam("gamma", "Gamma", 1.0f, 0.25f, 3.0f, 0.6f, 1.8f),
				floatParam("minLuminance", "Min luminance", 0.0f, 0.0f, 1.0f, 0.0f, 0.15f),
				floatParam("maxLuminance", "Max luminance", 1.0f, 0.0f, 1.0f, 0.85f, 1.0f),
				intParam("palette", "Palette", 0, 0, 3),
				boolParam("reverse", "Reverse", false, true),
			};
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}

		// --- hue_rotate ---
		{
			VideoEffectDefinition def;
			def.id = "hue_rotate";
			def.displayName = "Hue Rotate";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("hue_rotate.glsl");
			def.requiredUniforms = { "tex", "hueOffset", "hueSpeed", "time", "saturationMult", "valueMult", "alpha" };
			def.params = {
				floatParam("hueOffset", "Hue offset", 0.0f, 0.0f, 360.0f, 0.0f, 360.0f, 5.0f),
				floatParam("hueSpeed", "Hue speed", 0.0f, -120.0f, 120.0f, -60.0f, 60.0f, 2.0f),
				floatParam("saturationMult", "Saturation mult", 1.0f, 0.0f, 2.0f, 0.8f, 1.3f),
				floatParam("valueMult", "Value mult", 1.0f, 0.0f, 2.0f, 0.9f, 1.1f),
				alphaParam(),
			};
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}

		// --- ascii_solarpunk --- (filename ascii_threshold_solarpunk.glsl; registry id kept as
		// "ascii_solarpunk" to match every existing consumer, see probe §3A's aliases note)
		{
			VideoEffectDefinition def;
			def.id = "ascii_solarpunk";
			def.displayName = "ASCII Solarpunk";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("ascii_threshold_solarpunk.glsl");
			def.requiredUniforms = { "tex", "resolution", "alpha", "cellSize", "thresholdMin", "thresholdMax",
				"thresholdMode", "opacity", "contrast", "bias", "softness", "asciiColorMode", "asciiInvertMono",
				"asciiBackgroundMode" };
			def.params = {
				alphaParam(),
				floatParam("cellSize", "Cell size", 12.0f, 4.0f, 32.0f, 8.0f, 16.0f, 1.0f, false),
				floatParam("thresholdMin", "Threshold min", 0.55f, 0.0f, 1.0f, 0.55f, 0.55f, 0.02f, false),
				floatParam("thresholdMax", "Threshold max", 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.02f, false),
				intParam("thresholdMode", "Threshold mode", 1, 0, 3, false),
				floatParam("opacity", "Opacity", 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.02f, false),
				floatParam("contrast", "Contrast", 1.15f, 0.1f, 4.0f, 1.15f, 1.15f, 0.05f, false),
				floatParam("bias", "Bias", 0.0f, -0.5f, 0.5f, 0.0f, 0.0f, 0.02f, false),
				floatParam("softness", "Softness", 0.03f, 0.01f, 0.15f, 0.03f, 0.03f, 0.005f, false),
				intParam("asciiColorMode", "Color mode (0=sampled,1=B&W)", 0, 0, 1, false),
				intParam("asciiInvertMono", "Invert mono", 0, 0, 1, false),
				intParam("asciiBackgroundMode", "Background mode (0=image,1=transparent)", 0, 0, 1, false),
			};
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}

		// --- nature pack: bioluminescence, chromatic_aberration, edge_glow, ink_outlines,
		// pixel_drift, pixel_sorting, water_refraction, caustics ---
		// tex0 (not tex) is the sampler name throughout this family — SinglePassShaderEffect
		// binds both "tex" and "tex0" to the same source texture, so this is transparent.
		{
			VideoEffectDefinition def;
			def.id = "bioluminescence";
			def.displayName = "Bioluminescence";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("bioluminescence.glsl");
			def.requiredUniforms = { "tex0", "resolution", "time", "threshold", "intensity", "glowColor" };
			def.params = {
				floatParam("threshold", "Threshold", 0.3f, 0.05f, 0.8f, 0.3f, 0.3f, 0.05f, false),
				floatParam("intensity", "Intensity", 1.2f, 0.0f, 3.0f, 1.2f, 1.2f, 0.1f, false),
				vec3Param("glowColor", "Glow color", glm::vec3(0.1f, 1.0f, 0.75f)),
			};
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}
		{
			VideoEffectDefinition def;
			def.id = "chromatic_aberration";
			def.displayName = "Chromatic Aberration";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("chromatic_aberration.glsl");
			def.requiredUniforms = { "tex0", "amount", "radial" };
			def.params = {
				floatParam("amount", "Amount", 2.0f, 0.0f, 6.0f, 2.0f, 2.0f, 0.1f, false),
				floatParam("radial", "Radial", 0.5f, 0.0f, 1.0f, 0.5f, 0.5f, 0.05f, false),
			};
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}
		{
			VideoEffectDefinition def;
			def.id = "edge_glow";
			def.displayName = "Edge Glow";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("edge_glow.glsl");
			def.requiredUniforms = { "tex0", "edgeStrength", "glowStrength", "glowColor" };
			def.params = {
				floatParam("edgeStrength", "Edge strength", 1.5f, 0.0f, 5.0f, 1.5f, 1.5f, 0.1f, false),
				floatParam("glowStrength", "Glow strength", 1.2f, 0.0f, 4.0f, 1.2f, 1.2f, 0.1f, false),
				vec3Param("glowColor", "Glow color", glm::vec3(0.3f, 1.0f, 0.55f)),
			};
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}
		{
			VideoEffectDefinition def;
			def.id = "ink_outlines";
			def.displayName = "Ink Outlines";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("ink_outlines.glsl");
			def.requiredUniforms = { "tex0", "threshold", "inkStrength", "posterizeLevels" };
			def.params = {
				floatParam("threshold", "Threshold", 0.15f, 0.01f, 0.5f, 0.15f, 0.15f, 0.01f, false),
				floatParam("inkStrength", "Ink strength", 0.8f, 0.0f, 1.0f, 0.8f, 0.8f, 0.05f, false),
				floatParam("posterizeLevels", "Posterize levels", 6.0f, 2.0f, 16.0f, 6.0f, 6.0f, 1.0f, false),
			};
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}
		{
			VideoEffectDefinition def;
			def.id = "pixel_drift";
			def.displayName = "Pixel Drift";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("pixel_drift.glsl");
			def.requiredUniforms = { "tex0", "time", "amount", "scale", "speed" };
			def.params = {
				floatParam("amount", "Amount", 6.0f, 0.0f, 20.0f, 6.0f, 6.0f, 0.5f, false),
				floatParam("scale", "Scale", 0.03f, 0.005f, 0.1f, 0.03f, 0.03f, 0.005f, false),
				floatParam("speed", "Speed", 0.5f, 0.0f, 4.0f, 0.5f, 0.5f, 0.1f, false),
			};
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}
		{
			// randomized in BEFragment.cpp/TFEffectPicker.cpp: threshold via ofRandom(0.3,0.8),
			// direction via a 0/1 coin flip — the only nature-pack effect with any existing
			// randomization anywhere in the codebase.
			VideoEffectDefinition def;
			def.id = "pixel_sorting";
			def.displayName = "Pixel Sorting";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("pixel_sorting.glsl");
			def.requiredUniforms = { "tex0", "threshold", "rangePx", "direction", "intensity" };
			def.params = {
				floatParam("threshold", "Threshold", 0.5f, 0.0f, 1.0f, 0.3f, 0.8f),
				floatParam("rangePx", "Range (px)", 12.0f, 1.0f, 48.0f, 12.0f, 12.0f, 1.0f, false),
				intParam("direction", "Direction (0=H,1=V)", 0, 0, 1, true),
				floatParam("intensity", "Intensity", 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.05f, false),
			};
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}
		{
			VideoEffectDefinition def;
			def.id = "water_refraction";
			def.displayName = "Water Refraction";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("water_refraction.glsl");
			def.requiredUniforms = { "tex0", "time", "amplitude", "frequency", "speed" };
			def.params = {
				floatParam("amplitude", "Amplitude", 6.0f, 0.0f, 20.0f, 6.0f, 6.0f, 0.5f, false),
				floatParam("frequency", "Frequency", 0.02f, 0.001f, 0.08f, 0.02f, 0.02f, 0.002f, false),
				floatParam("speed", "Speed", 1.0f, 0.0f, 4.0f, 1.0f, 1.0f, 0.1f, false),
			};
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}
		{
			// Never wired into any ShaderLibrary anywhere prior to this catalog
			// (see docs/video-effect-promotion-inventory.md §1) — defaults here
			// are new, reasonable starting values, not a transcription of any
			// existing call site. Flagged unvalidated pending real use.
			VideoEffectDefinition def;
			def.id = "caustics";
			def.displayName = "Caustics";
			def.contract = ShaderContract::A;
			def.assetPaths = effectsPoolAssets("caustics.glsl");
			def.requiredUniforms = { "tex0", "resolution", "time", "scale", "intensity", "causticColor" };
			def.params = {
				floatParam("scale", "Scale", 0.05f, 0.005f, 0.3f, 0.03f, 0.08f, 0.005f),
				floatParam("intensity", "Intensity", 1.0f, 0.0f, 3.0f, 0.6f, 1.5f, 0.1f),
				vec3Param("causticColor", "Caustic color", glm::vec3(0.4f, 0.8f, 1.0f)),
			};
			def.capabilities.safeForAutomaticSelection = false; // unvalidated — see promotion inventory
			registry.registerEffect(std::move(def), [] { return std::make_unique<SinglePassShaderEffect>(); });
		}
	}

	void registerDefaultVideoEffects(VideoEffectRegistry & registry) {
		registerSinglePassEffects(registry);
		registerMotionExtraction(registry);
		registerMotionComposite(registry);
		registerErosionEffects(registry);
		registerRidgeline(registry);
		registerTemporalTrails(registry);
		registerReactionDiffusion(registry);
	}

} // namespace videoeffects
