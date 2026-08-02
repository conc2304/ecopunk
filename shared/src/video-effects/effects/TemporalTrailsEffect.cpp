#include "TemporalTrailsEffect.h"
#include "DefaultVideoEffectCatalog.h"
#include "VideoEffectAssetRegistry.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include <memory>

namespace videoeffects {

	bool TemporalTrailsEffect::setup() {
		const VideoEffectDefinition * def = getDefinition();
		if (def == nullptr || def->assetPaths.size() < 2) {
			ofLogError("TemporalTrailsEffect") << "missing definition or asset paths";
			return false;
		}
		VideoEffectAssetRegistry assets;
		ready = shader.load(assets.resolve(def->assetPaths[0]), assets.resolve(def->assetPaths[1]));
		if (!ready) ofLogError("TemporalTrailsEffect") << "failed to load temporal_trails.glsl";
		return ready;
	}

	void TemporalTrailsEffect::reset() {
		if (width <= 0 || height <= 0) return;
		prevFbo.allocate(width, height, GL_RGB);
		outFbo.allocate(width, height, GL_RGB);
		prevFbo.begin();
		ofClear(0);
		prevFbo.end();
	}

	void TemporalTrailsEffect::resize(int w, int h) {
		width = w;
		height = h;
		reset();
	}

	void TemporalTrailsEffect::render(const VideoEffectContext & context, const VideoEffectParameters & params) {
		if (!ready || context.sourceTexture == nullptr || !context.sourceTexture->isAllocated()) return;

		int w = static_cast<int>(context.destinationRect.width > 0 ? context.destinationRect.width : context.sourceTexture->getWidth());
		int h = static_cast<int>(context.destinationRect.height > 0 ? context.destinationRect.height : context.sourceTexture->getHeight());
		if (w != width || h != height || !prevFbo.isAllocated()) resize(w, h);

		shader.begin();
		shader.setUniformTexture("currentTex", *context.sourceTexture, 0);
		shader.setUniformTexture("previousTex", prevFbo.getTexture(), 1);
		shader.setUniform1f("decay", params.getFloat("decay", 0.92f));
		shader.setUniform1f("currentWeight", params.getFloat("currentWeight", 0.25f));
		shader.setUniform1f("brighten", params.getFloat("brighten", 1.05f));

		outFbo.begin();
		ofClear(0);
		ofSetColor(255);
		context.sourceTexture->draw(0, 0, w, h);
		outFbo.end();
		shader.end();

		bool usingFbo = context.destinationFbo != nullptr;
		if (usingFbo) context.destinationFbo->begin();
		ofSetColor(255, 255, 255, static_cast<int>(context.alpha * 255));
		outFbo.draw(context.destinationRect.x, context.destinationRect.y, w, h);
		ofSetColor(255);
		if (usingFbo) context.destinationFbo->end();

		prevFbo.begin();
		ofClear(0);
		outFbo.draw(0, 0, w, h);
		prevFbo.end();
	}

	void registerTemporalTrails(VideoEffectRegistry & registry) {
		VideoEffectDefinition def;
		def.id = "temporal_trails";
		def.displayName = "Temporal Trails";
		def.kind = VideoEffectKind::TemporalShader;
		def.contract = ShaderContract::A;
		def.assetPaths = { "common/vert.glsl", "temporal-trails/temporal_trails.glsl" };
		def.requiredUniforms = { "currentTex", "previousTex", "decay", "currentWeight", "brighten" };

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

		// Defaults transcribed from quadrant-crosshair/src/DebugMode.h's
		// pTrail* fields — the only place these were ever tunable.
		def.params = {
			floatP("decay", "Decay", 0.92f, 0.5f, 0.99f, 0.85f, 0.96f),
			floatP("currentWeight", "Current weight", 0.25f, 0.05f, 0.9f, 0.15f, 0.4f),
			floatP("brighten", "Brighten", 1.05f, 0.9f, 1.5f, 1.0f, 1.15f),
		};

		def.capabilities.requiresSourceTexture = true;
		def.capabilities.requiresPreviousFrame = true;
		def.capabilities.requiresPersistentState = true;
		def.capabilities.safeForAutomaticSelection = false; // zero production callers today — see promotion inventory §2

		registry.registerEffect(std::move(def), [] { return std::make_unique<TemporalTrailsEffect>(); });
	}

} // namespace videoeffects
