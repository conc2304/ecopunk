#include "SinglePassShaderEffect.h"
#include "VideoEffectAssetRegistry.h"
#include "VideoEffectDefinition.h"
#include "ofGraphics.h"
#include "ofLog.h"

namespace videoeffects {

	bool SinglePassShaderEffect::setup() {
		const VideoEffectDefinition * def = getDefinition();
		if (def == nullptr || def->assetPaths.size() < 2) {
			ofLogError("SinglePassShaderEffect") << "missing definition or asset paths (expected [vertPath, fragPath])";
			ready = false;
			return false;
		}

		VideoEffectAssetRegistry assets;
		bool loaded = shader.load(assets.resolve(def->assetPaths[0]), assets.resolve(def->assetPaths[1]));
		if (!loaded) {
			ofLogError("SinglePassShaderEffect") << "failed to load shader for effect: " << def->id;
		}
		ready = loaded;
		return loaded;
	}

	void SinglePassShaderEffect::render(const VideoEffectContext & context, const VideoEffectParameters & params) {
		if (!ready || context.sourceTexture == nullptr || !context.sourceTexture->isAllocated()) {
			return;
		}

		const VideoEffectDefinition * def = getDefinition();

		bool usingFbo = context.destinationFbo != nullptr;
		if (usingFbo) context.destinationFbo->begin();

		ofRectangle dest = context.destinationRect;
		if (dest.width <= 0 || dest.height <= 0) {
			dest = ofRectangle(0, 0, context.sourceTexture->getWidth(), context.sourceTexture->getHeight());
		}

		shader.begin();
		shader.setUniformTexture("tex", *context.sourceTexture, 0);
		shader.setUniformTexture("tex0", *context.sourceTexture, 0); // nature-pack shaders sample tex0
		// Context-derived, not user-tunable parameters — every effects-pool
		// shader that declares these expects them bound unconditionally
		// (matches every existing dispatch site: VideoRegionEffectRenderer,
		// BEFragment, TFEffectPicker, Quadrant/DebugMode all set both on
		// every draw regardless of whether the active shader reads them).
		shader.setUniform2f("resolution", dest.width, dest.height);
		shader.setUniform1f("time", context.time);

		if (def != nullptr) {
			for (const auto & paramSchema : def->params) {
				switch (paramSchema.type) {
					case VideoEffectParameterType::Float:
						shader.setUniform1f(paramSchema.id, params.getFloat(paramSchema.id, asFloat(paramSchema.defaultValue)));
						break;
					case VideoEffectParameterType::Int:
						shader.setUniform1i(paramSchema.id, params.getInt(paramSchema.id, asInt(paramSchema.defaultValue)));
						break;
					case VideoEffectParameterType::Bool:
						shader.setUniform1i(paramSchema.id, params.getBool(paramSchema.id, asBool(paramSchema.defaultValue)) ? 1 : 0);
						break;
					case VideoEffectParameterType::Vec2: {
						glm::vec2 v = params.has(paramSchema.id) ? params.getVec2(paramSchema.id) : asVec2(paramSchema.defaultValue);
						shader.setUniform2f(paramSchema.id, v.x, v.y);
						break;
					}
					case VideoEffectParameterType::Vec3: {
						glm::vec3 v = params.has(paramSchema.id) ? params.getVec3(paramSchema.id) : asVec3(paramSchema.defaultValue);
						shader.setUniform3f(paramSchema.id, v.x, v.y, v.z);
						break;
					}
					case VideoEffectParameterType::Vec4: {
						glm::vec4 v = params.has(paramSchema.id) ? params.getVec4(paramSchema.id) : asVec4(paramSchema.defaultValue);
						shader.setUniform4f(paramSchema.id, v.x, v.y, v.z, v.w);
						break;
					}
				}
			}
		}

		ofSetColor(255);
		if (context.sourceRect.width > 0 && context.sourceRect.height > 0) {
			context.sourceTexture->drawSubsection(
				dest.x, dest.y, dest.width, dest.height,
				context.sourceRect.x, context.sourceRect.y, context.sourceRect.width, context.sourceRect.height);
		} else {
			context.sourceTexture->draw(dest.x, dest.y, dest.width, dest.height);
		}

		shader.end();

		if (usingFbo) context.destinationFbo->end();
	}

} // namespace videoeffects
