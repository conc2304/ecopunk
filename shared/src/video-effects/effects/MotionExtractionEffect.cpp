#include "MotionExtractionEffect.h"
#include "DefaultVideoEffectCatalog.h"
#include "ofGraphics.h"
#include <memory>

namespace videoeffects {

	bool MotionExtractionEffect::setup() {
		motionExtraction.setup();
		ready = true;
		return true;
	}

	void MotionExtractionEffect::resize(int width, int height) {
		(void)width;
		(void)height;
		// MotionExtraction's extraction FBOs are sized from ofGetWidth()/Height()
		// at its own setup() time (capped on Pi) — re-running setup() reallocates
		// them at the current canvas size. Its low-res accumulation/history
		// buffers are resolution-independent (fixed ACCUM_W×ACCUM_H) and don't
		// need this at all.
		motionExtraction.setup();
	}

	void MotionExtractionEffect::update(const VideoEffectContext & context, const VideoEffectParameters & params) {
		if (context.sourceTexture == nullptr || !context.sourceTexture->isAllocated()) return;

		motionExtraction.extractNeutralGrey = params.getFloat("neutralGrey", 0.5f);
		motionExtraction.extractBoost = params.getFloat("boost", 1.0f);
		motionExtraction.extractGamma = params.getFloat("gamma", 1.0f);
		motionExtraction.setOutputMode(params.getInt("outputMode", 0));
		motionExtraction.setReferenceMode(params.getInt("referenceMode", 1));

		motionExtraction.update(
			*const_cast<ofTexture *>(context.sourceTexture), params.getFloat("decayWeight", 0.95f), params.getFloat("sensitivity", 4.0f));
	}

	void MotionExtractionEffect::render(const VideoEffectContext & context, const VideoEffectParameters & params) {
		(void)params;
		if (!ready) return;

		bool usingFbo = context.destinationFbo != nullptr;
		if (usingFbo) context.destinationFbo->begin();

		ofRectangle dest = context.destinationRect;
		if (dest.width <= 0 || dest.height <= 0) {
			dest = ofRectangle(0, 0, motionExtraction.getMotionTexture().getWidth(), motionExtraction.getMotionTexture().getHeight());
		}
		ofSetColor(255);
		motionExtraction.getMotionTexture().draw(dest.x, dest.y, dest.width, dest.height);

		if (usingFbo) context.destinationFbo->end();
	}

	void registerMotionExtraction(VideoEffectRegistry & registry) {
		VideoEffectDefinition def;
		def.id = "motion_extraction";
		def.displayName = "Motion Extraction";
		def.kind = VideoEffectKind::Processor;
		def.contract = ShaderContract::None; // owns its own shaders internally (motion_accum/motion_extract), not a single tex/vTexCoord pass
		def.assetPaths = {}; // MotionExtraction loads its own two shaders directly, not via the asset registry (see MotionExtraction.cpp)
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
			p.randomizable = false; // discrete mode selector, not an artistic-range value
			return p;
		};

		// Defaults transcribed from quadrant-crosshair/src/DebugMode.h's
		// pMotion* fields — the only place these were ever tunable.
		def.params = {
			floatP("decayWeight", "Decay weight", 0.95f, 0.85f, 0.999f, 0.90f, 0.98f),
			floatP("sensitivity", "Sensitivity", 4.0f, 0.5f, 16.0f, 2.0f, 8.0f),
			floatP("neutralGrey", "Neutral grey", 0.5f, 0.0f, 1.0f, 0.5f, 0.5f),
			floatP("boost", "Boost", 1.0f, 0.1f, 6.0f, 1.0f, 1.0f),
			floatP("gamma", "Gamma", 1.0f, 0.2f, 4.0f, 1.0f, 1.0f),
			intP("outputMode", "Output mode (0=luma,1=chroma,2=signed,3=raw,4=ref)", 0, 0, 4),
			intP("referenceMode", "Reference mode (0=delayed,1=accumulation)", 1, 0, 1),
		};

		def.capabilities.requiresSourceTexture = true;
		def.capabilities.producesAuxiliaryTexture = true;
		def.capabilities.requiresPersistentState = true;
		def.capabilities.supportsAlphaMix = false;
		def.capabilities.passCount = 3; // accum + accumulation-reference extract + delayed-reference extract
		def.capabilities.safeForAutomaticSelection = true; // real production use in quadrant-crosshair today

		registry.registerEffect(std::move(def), [] { return std::make_unique<MotionExtractionEffect>(); });
	}

} // namespace videoeffects
