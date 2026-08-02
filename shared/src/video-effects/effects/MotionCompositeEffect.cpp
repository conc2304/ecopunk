#include "MotionCompositeEffect.h"
#include "DefaultVideoEffectCatalog.h"
#include "VideoEffectAssetRegistry.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include <memory>

namespace videoeffects {

	bool MotionCompositeEffect::setup() {
		const VideoEffectDefinition * def = getDefinition();
		if (def == nullptr || def->assetPaths.size() < 2) {
			ofLogError("MotionCompositeEffect") << "missing definition or asset paths";
			return false;
		}
		VideoEffectAssetRegistry assets;
		bool loaded = shader.load(assets.resolve(def->assetPaths[0]), assets.resolve(def->assetPaths[1]));
		if (!loaded) {
			ofLogError("MotionCompositeEffect") << "failed to load motion_effect.glsl";
		}
		ready = loaded;
		return loaded;
	}

	void MotionCompositeEffect::render(const VideoEffectContext & context, const VideoEffectParameters & params) {
		if (!ready || context.sourceTexture == nullptr || !context.sourceTexture->isAllocated()) return;

		auto motionIt = context.auxiliaryTextures.find("motionTex");
		auto delayedIt = context.auxiliaryTextures.find("motionDelayedTex");
		if (motionIt == context.auxiliaryTextures.end() || delayedIt == context.auxiliaryTextures.end()
			|| motionIt->second == nullptr || delayedIt->second == nullptr) {
			ofLogError("MotionCompositeEffect")
				<< "render() called without motionTex/motionDelayedTex — attach a motion_extraction instance's "
				<< "output via context.auxiliaryTextures first";
			return;
		}

		bool usingFbo = context.destinationFbo != nullptr;
		if (usingFbo) context.destinationFbo->begin();

		ofRectangle dest = context.destinationRect;
		if (dest.width <= 0 || dest.height <= 0) {
			dest = ofRectangle(0, 0, context.sourceTexture->getWidth(), context.sourceTexture->getHeight());
		}

		shader.begin();
		shader.setUniformTexture("tex", *context.sourceTexture, 0);
		shader.setUniformTexture("motionTex", *motionIt->second, 4);
		shader.setUniformTexture("motionDelayedTex", *delayedIt->second, 5);
		shader.setUniform1f("alpha", params.getFloat("alpha", 1.0f));
		shader.setUniform1f("motionGamma", params.getFloat("motionGamma", 1.0f));
		shader.setUniform1i("blendMode", params.getInt("blendMode", 0));
		shader.setUniform1i("motionSourceMode", params.getInt("motionSourceMode", 0));

		ofSetColor(255);
		context.sourceTexture->draw(dest.x, dest.y, dest.width, dest.height);
		shader.end();

		if (usingFbo) context.destinationFbo->end();
	}

	void registerMotionComposite(VideoEffectRegistry & registry) {
		VideoEffectDefinition def;
		def.id = "motion_composite";
		def.displayName = "Motion Composite";
		def.kind = VideoEffectKind::Composite;
		def.contract = ShaderContract::A;
		def.assetPaths = { "common/vert.glsl", "motion-extraction/motion_effect.glsl" };
		def.requiredUniforms = { "tex", "motionTex", "motionDelayedTex", "alpha", "motionGamma", "blendMode", "motionSourceMode" };

		VideoEffectParameterSchema alpha;
		alpha.id = "alpha";
		alpha.label = "Alpha";
		alpha.type = VideoEffectParameterType::Float;
		alpha.defaultValue = 1.0f;
		alpha.hardMin = 0.0f;
		alpha.hardMax = 1.0f;
		alpha.artisticMin = 0.0f;
		alpha.artisticMax = 1.0f;
		alpha.randomizable = false;

		VideoEffectParameterSchema motionGamma;
		motionGamma.id = "motionGamma";
		motionGamma.label = "Motion gamma";
		motionGamma.type = VideoEffectParameterType::Float;
		motionGamma.defaultValue = 1.0f;
		motionGamma.hardMin = 0.2f;
		motionGamma.hardMax = 4.0f;
		motionGamma.artisticMin = 0.5f;
		motionGamma.artisticMax = 2.0f;

		VideoEffectParameterSchema blendMode;
		blendMode.id = "blendMode";
		blendMode.label = "Blend mode (0=mix,1=add,2=screen)";
		blendMode.type = VideoEffectParameterType::Int;
		blendMode.defaultValue = 0;
		blendMode.hardMin = 0;
		blendMode.hardMax = 2;
		blendMode.artisticMin = 0;
		blendMode.artisticMax = 2;

		VideoEffectParameterSchema motionSourceMode;
		motionSourceMode.id = "motionSourceMode";
		motionSourceMode.label = "Motion source (0=accum,1=delayed,2=blend)";
		motionSourceMode.type = VideoEffectParameterType::Int;
		motionSourceMode.defaultValue = 0;
		motionSourceMode.hardMin = 0;
		motionSourceMode.hardMax = 2;
		motionSourceMode.artisticMin = 0;
		motionSourceMode.artisticMax = 2;

		def.params = { alpha, motionGamma, blendMode, motionSourceMode };
		def.capabilities.requiresSourceTexture = true;
		def.capabilities.requiresMotionTexture = true;
		def.capabilities.requiresDelayedMotionTexture = true;
		def.capabilities.safeForAutomaticSelection = true;

		registry.registerEffect(std::move(def), [] { return std::make_unique<MotionCompositeEffect>(); });
	}

} // namespace videoeffects
