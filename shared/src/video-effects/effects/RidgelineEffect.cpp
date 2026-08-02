#include "RidgelineEffect.h"
#include "DefaultVideoEffectCatalog.h"
#include <memory>

namespace videoeffects {

	bool RidgelineEffect::setup() {
		// RidgelineRenderer::setup(w,h) needs a real canvas size — deferred to
		// the first render() call (mirrors Quadrant.cpp:127-141's own
		// rebuild-on-first-use pattern) rather than guessing a size here.
		ready = true;
		return true;
	}

	void RidgelineEffect::resize(int width, int height) {
		renderer.setup(width, height);
		setupW = width;
		setupH = height;
	}

	void RidgelineEffect::render(const VideoEffectContext & context, const VideoEffectParameters & params) {
		if (!ready || context.sourcePixels == nullptr || !context.sourcePixels->isAllocated()) return;

		int w = static_cast<int>(context.destinationRect.width > 0 ? context.destinationRect.width : context.sourcePixels->getWidth());
		int h = static_cast<int>(context.destinationRect.height > 0 ? context.destinationRect.height : context.sourcePixels->getHeight());
		if (w != setupW || h != setupH) {
			resize(w, h);
		}

		RidgelineRenderer::Params p;
		p.numLines = params.getInt("numLines", 80);
		p.samplesPerLine = params.getInt("samplesPerLine", 128);
		p.amplitude = params.getFloat("amplitude", 150.0f);
		p.spacingPct = params.getFloat("spacingPct", 0.02f);
		p.centerYPct = params.getFloat("centerYPct", 0.47f);
		p.marginXPct = params.getFloat("marginXPct", -0.02f);
		p.overlayMode = params.getBool("overlayMode", false);
		p.flipX = params.getBool("flipX", false);
		p.flipY = params.getBool("flipY", true);
		renderer.setParams(p);

		renderer.update(*context.sourcePixels);
		// RidgelineRenderer::draw() predates this const-correct context type and
		// takes a non-const ofTexture*; it only reads from the texture.
		renderer.draw(p.overlayMode ? const_cast<ofTexture *>(context.sourceTexture) : nullptr, context.alpha);
	}

	void registerRidgeline(VideoEffectRegistry & registry) {
		VideoEffectDefinition def;
		def.id = "ridgeline";
		def.displayName = "Ridgeline";
		def.kind = VideoEffectKind::CpuRenderer;
		def.contract = ShaderContract::None;
		def.assetPaths = {}; // no shader — see RidgelineRenderer.h's own header comment (GLSL-ES-1.0-safe by construction)
		def.requiredUniforms = {};

		auto floatP = [](std::string id, std::string label, float def_, float lo, float hi, float aLo, float aHi) {
			VideoEffectParameterSchema p;
			p.id = std::move(id);
			p.label = std::move(label);
			p.type = VideoEffectParameterType::Float;
			p.defaultValue = def_;
			p.hardMin = lo;
			p.hardMax = hi;
			p.artisticMin = aLo;
			p.artisticMax = aHi;
			return p;
		};
		auto intP = [](std::string id, std::string label, int def_, int lo, int hi) {
			VideoEffectParameterSchema p;
			p.id = std::move(id);
			p.label = std::move(label);
			p.type = VideoEffectParameterType::Int;
			p.defaultValue = def_;
			p.hardMin = lo;
			p.hardMax = hi;
			p.artisticMin = lo;
			p.artisticMax = hi;
			return p;
		};
		auto boolP = [](std::string id, std::string label, bool def_) {
			VideoEffectParameterSchema p;
			p.id = std::move(id);
			p.label = std::move(label);
			p.type = VideoEffectParameterType::Bool;
			p.defaultValue = def_;
			p.hardMin = false;
			p.hardMax = true;
			p.randomizable = false;
			return p;
		};

		// Defaults match blueprint_emergence/BEFragment.cpp's and
		// quadrant-crosshair/DebugMode.h's literal values, which already agree
		// with each other on every field except amplitude (BE randomizes
		// 60-255; QC fixes it at 150 — used here as the point default).
		def.params = {
			intP("numLines", "Num lines", 80, 10, 128),
			intP("samplesPerLine", "Samples per line", 128, 16, 128),
			floatP("amplitude", "Amplitude", 150.0f, 0.0f, 300.0f, 60.0f, 255.0f),
			floatP("spacingPct", "Spacing %", 0.02f, 0.02f, 0.30f, 0.02f, 0.02f),
			floatP("centerYPct", "Center Y %", 0.47f, 0.1f, 0.9f, 0.47f, 0.47f),
			floatP("marginXPct", "Margin X %", -0.02f, -0.02f, 0.25f, -0.02f, -0.02f),
			boolP("overlayMode", "Overlay (draw under source)", false),
			boolP("flipX", "Flip X", false),
			boolP("flipY", "Flip Y", true),
		};

		def.capabilities.requiresSourceTexture = false;
		def.capabilities.requiresCpuPixels = true;
		def.capabilities.supportsAlphaMix = true;
		def.capabilities.safeForAutomaticSelection = true;

		registry.registerEffect(std::move(def), [] { return std::make_unique<RidgelineEffect>(); });
	}

} // namespace videoeffects
