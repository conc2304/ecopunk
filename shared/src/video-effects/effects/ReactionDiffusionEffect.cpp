#include "ReactionDiffusionEffect.h"
#include "DefaultVideoEffectCatalog.h"
#include "VideoEffectAssetRegistry.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include <memory>

namespace videoeffects {

	void ReactionDiffusionEffect::seedInitialState() {
		fboA.begin();
		ofClear(255, 0, 0, 255); // R=U=1, G=V=0
		ofSetColor(0, 255, 0, 255);
		ofDrawRectangle(GRID_W / 2 - 8, GRID_H / 2 - 8, 16, 16);
		fboA.end();

		fboB.begin();
		ofClear(255, 0, 0, 255);
		fboB.end();
	}

	bool ReactionDiffusionEffect::setup() {
		ofFbo::Settings s;
		s.width = GRID_W;
		s.height = GRID_H;
		s.internalformat = GL_RGBA;
		s.useDepth = false;
		fboA.allocate(s);
		fboB.allocate(s);
		pingPong = false;
		seedInitialState();

		const VideoEffectDefinition * def = getDefinition();
		if (def == nullptr || def->assetPaths.size() < 2) {
			ofLogError("ReactionDiffusionEffect") << "missing definition or asset paths";
			return false;
		}
		VideoEffectAssetRegistry assets;
		ready = shader.load(assets.resolve(def->assetPaths[0]), assets.resolve(def->assetPaths[1]));
		if (!ready) ofLogError("ReactionDiffusionEffect") << "failed to load rd_step.glsl";
		return ready;
	}

	void ReactionDiffusionEffect::reset() {
		pingPong = false;
		seedInitialState();
	}

	void ReactionDiffusionEffect::render(const VideoEffectContext & context, const VideoEffectParameters & params) {
		if (!ready) return;

		ofFbo & src = pingPong ? fboB : fboA;
		ofFbo & dst = pingPong ? fboA : fboB;

		dst.begin();
		shader.begin();
		shader.setUniformTexture("rdState", src.getTexture(), 0);
		shader.setUniform2f("resolution", GRID_W, GRID_H);
		shader.setUniform1f("feedRate", params.getFloat("feedRate", 0.037f));
		shader.setUniform1f("killRate", params.getFloat("killRate", 0.06f));
		src.getTexture().draw(0, 0, GRID_W, GRID_H);
		shader.end();
		dst.end();
		pingPong = !pingPong;

		ofFbo & current = pingPong ? fboA : fboB;
		bool usingFbo = context.destinationFbo != nullptr;
		if (usingFbo) context.destinationFbo->begin();

		ofRectangle dest = context.destinationRect;
		if (dest.width <= 0 || dest.height <= 0) dest = ofRectangle(0, 0, GRID_W, GRID_H);
		ofSetColor(255, 255, 255, static_cast<int>(context.alpha * 255));
		current.getTexture().draw(dest.x, dest.y, dest.width, dest.height);
		ofSetColor(255);

		if (usingFbo) context.destinationFbo->end();
	}

	void registerReactionDiffusion(VideoEffectRegistry & registry) {
		VideoEffectDefinition def;
		def.id = "reaction_diffusion";
		def.displayName = "Reaction Diffusion";
		def.kind = VideoEffectKind::Simulation;
		def.contract = ShaderContract::A;
		def.assetPaths = { "common/vert.glsl", "reaction-diffusion/rd_step.glsl" };
		def.requiredUniforms = { "rdState", "resolution", "feedRate", "killRate" };

		VideoEffectParameterSchema feedRate;
		feedRate.id = "feedRate";
		feedRate.label = "Feed rate";
		feedRate.type = VideoEffectParameterType::Float;
		feedRate.defaultValue = 0.037f;
		feedRate.hardMin = 0.0f;
		feedRate.hardMax = 0.1f;
		feedRate.artisticMin = 0.02f;
		feedRate.artisticMax = 0.06f;
		feedRate.performanceSensitive = false;

		VideoEffectParameterSchema killRate;
		killRate.id = "killRate";
		killRate.label = "Kill rate";
		killRate.type = VideoEffectParameterType::Float;
		killRate.defaultValue = 0.06f;
		killRate.hardMin = 0.0f;
		killRate.hardMax = 0.1f;
		killRate.artisticMin = 0.045f;
		killRate.artisticMax = 0.07f;

		def.params = { feedRate, killRate };
		def.capabilities.requiresSourceTexture = false;
		def.capabilities.requiresPersistentState = true;
		def.capabilities.supportsAlphaMix = true;
		// Never instantiated anywhere in production prior to this promotion
		// (confirmed via repo-wide grep — see docs/video-effect-promotion-inventory.md
		// §2) — excluded from unattended/automatic selection until it gets
		// real usage and Raspberry Pi validation, per
		// docs/shared-video-effect-architecture.md §4.7.
		def.capabilities.safeForAutomaticSelection = false;

		registry.registerEffect(std::move(def), [] { return std::make_unique<ReactionDiffusionEffect>(); });
	}

} // namespace videoeffects
