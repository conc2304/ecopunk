#include "ErosionEffect.h"
#include "DefaultVideoEffectCatalog.h"
#include "VideoEffectAssetRegistry.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include <memory>

namespace videoeffects {

	namespace {
		void allocateFbo(ofFbo & fbo, int w, int h) {
			ofFbo::Settings s;
			s.width = w;
			s.height = h;
			s.internalformat = GL_RGBA;
			s.useDepth = false;
			fbo.allocate(s);
			fbo.begin();
			ofClear(0, 0, 0, 255);
			fbo.end();
		}

		VideoEffectParameterSchema floatP(std::string id, std::string label, float def_, float lo, float hi, float aLo, float aHi) {
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
		}
	} // namespace

	// ---------------------------------------------------------------- accumulation ----

	bool ErosionAccumulationEffect::setup() {
		const VideoEffectDefinition * def = getDefinition();
		if (def == nullptr || def->assetPaths.size() < 2) {
			ofLogError("ErosionAccumulationEffect") << "missing definition or asset paths";
			return false;
		}
		VideoEffectAssetRegistry assets;
		ready = shader.load(assets.resolve(def->assetPaths[0]), assets.resolve(def->assetPaths[1]));
		if (!ready) ofLogError("ErosionAccumulationEffect") << "failed to load erosion_accumulation.glsl";
		return ready;
	}

	void ErosionAccumulationEffect::reset() {
		if (width > 0 && height > 0) {
			allocateFbo(fboA, width, height);
			allocateFbo(fboB, width, height);
		}
		pingPong = false;
	}

	void ErosionAccumulationEffect::resize(int w, int h) {
		width = w;
		height = h;
		reset();
	}

	void ErosionAccumulationEffect::render(const VideoEffectContext & context, const VideoEffectParameters & params) {
		if (!ready || context.sourceTexture == nullptr || !context.sourceTexture->isAllocated()) return;

		int w = static_cast<int>(context.destinationRect.width > 0 ? context.destinationRect.width : context.sourceTexture->getWidth());
		int h = static_cast<int>(context.destinationRect.height > 0 ? context.destinationRect.height : context.sourceTexture->getHeight());
		if (w != width || h != height || !fboA.isAllocated()) resize(w, h);

		writeFbo().begin();
		shader.begin();
		shader.setUniformTexture("accumulated", readFbo().getTexture(), 0);
		shader.setUniformTexture("videoFrame", *context.sourceTexture, 1);
		shader.setUniform1f("decayRate", params.getFloat("decayRate", 0.92f));
		shader.setUniform1f("videoAlpha", params.getFloat("videoAlpha", 1.0f));
		ofSetColor(255);
		readFbo().getTexture().draw(0, 0, w, h);
		shader.end();
		writeFbo().end();
		pingPong = !pingPong;

		bool usingFbo = context.destinationFbo != nullptr;
		if (usingFbo) context.destinationFbo->begin();
		ofSetColor(255, 255, 255, static_cast<int>(context.alpha * 255));
		readFbo().getTexture().draw(context.destinationRect.x, context.destinationRect.y, w, h);
		ofSetColor(255);
		if (usingFbo) context.destinationFbo->end();
	}

	// ---------------------------------------------------------------- history blend ----

	bool ErosionHistoryBlendEffect::setup() {
		const VideoEffectDefinition * def = getDefinition();
		if (def == nullptr || def->assetPaths.size() < 2) {
			ofLogError("ErosionHistoryBlendEffect") << "missing definition or asset paths";
			return false;
		}
		VideoEffectAssetRegistry assets;
		ready = shader.load(assets.resolve(def->assetPaths[0]), assets.resolve(def->assetPaths[1]));
		if (!ready) ofLogError("ErosionHistoryBlendEffect") << "failed to load erosion_history_blend shader";
		return ready;
	}

	void ErosionHistoryBlendEffect::reset() {
		if (width > 0 && height > 0) {
			allocateFbo(fboA, width, height);
			allocateFbo(fboB, width, height);
		}
		pingPong = false;
		primed = false;
	}

	void ErosionHistoryBlendEffect::resize(int w, int h) {
		width = w;
		height = h;
		reset();
	}

	void ErosionHistoryBlendEffect::render(const VideoEffectContext & context, const VideoEffectParameters & params) {
		if (!ready || context.sourceTexture == nullptr || !context.sourceTexture->isAllocated()) return;

		int w = static_cast<int>(context.destinationRect.width > 0 ? context.destinationRect.width : context.sourceTexture->getWidth());
		int h = static_cast<int>(context.destinationRect.height > 0 ? context.destinationRect.height : context.sourceTexture->getHeight());
		if (w != width || h != height || !fboA.isAllocated()) resize(w, h);

		if (!primed) {
			// First frame: copy straight through instead of blending against
			// opaque-black initial history — same guard shared/src/ErosionFBO
			// uses, to avoid an extra unwanted fade-in from black.
			readFbo().begin();
			ofSetColor(255);
			context.sourceTexture->draw(0, 0, w, h);
			readFbo().end();
			primed = true;
		}

		writeFbo().begin();
		shader.begin();
		shader.setUniformTexture("history", readFbo().getTexture(), 0);
		shader.setUniformTexture("current", *context.sourceTexture, 1);
		shader.setUniform1f("decayRate", params.getFloat("decayRate", 0.92f));
		shader.setUniform1f("desatAmount", params.getFloat("desatAmount", 0.0f));
		// The fix: shared/src/ErosionFBO.cpp never bound this uniform, so it
		// silently read as GLSL's zero-init default — see this class's header
		// comment and docs/video-effect-promotion-inventory.md §2.
		shader.setUniform1f("currentAlpha", params.getFloat("currentAlpha", 1.0f));
		ofSetColor(255);
		context.sourceTexture->draw(0, 0, w, h);
		shader.end();
		writeFbo().end();
		pingPong = !pingPong;

		bool usingFbo = context.destinationFbo != nullptr;
		if (usingFbo) context.destinationFbo->begin();
		ofSetColor(255, 255, 255, static_cast<int>(context.alpha * 255));
		readFbo().getTexture().draw(context.destinationRect.x, context.destinationRect.y, w, h);
		ofSetColor(255);
		if (usingFbo) context.destinationFbo->end();
	}

	// ---------------------------------------------------------------- registration ----

	void registerErosionEffects(VideoEffectRegistry & registry) {
		{
			VideoEffectDefinition def;
			def.id = "erosion_accumulation";
			def.displayName = "Erosion (Accumulation)";
			def.kind = VideoEffectKind::TemporalShader;
			def.contract = ShaderContract::A;
			def.assetPaths = { "common/vert.glsl", "erosion/erosion_accumulation.glsl" };
			def.requiredUniforms = { "accumulated", "videoFrame", "decayRate", "videoAlpha" };
			def.params = {
				floatP("decayRate", "Decay rate", 0.92f, 0.5f, 0.99f, 0.85f, 0.96f),
				floatP("videoAlpha", "Video alpha", 1.0f, 0.0f, 1.0f, 0.5f, 1.0f),
			};
			def.capabilities.requiresSourceTexture = true;
			def.capabilities.requiresHistoryBuffer = true;
			def.capabilities.requiresPersistentState = true;
			def.capabilities.safeForAutomaticSelection = true;
			registry.registerEffect(std::move(def), [] { return std::make_unique<ErosionAccumulationEffect>(); });
		}
		{
			VideoEffectDefinition def;
			def.id = "erosion_history_blend";
			def.displayName = "Erosion (History Blend)";
			def.kind = VideoEffectKind::TemporalShader;
			// Neither Contract A nor B: its vert.glsl uses Contract B's
			// texCoordVarying naming, but its fragment uniforms are
			// history/current (not tex, not videoTex/maskTex) — a bespoke pair
			// with a fully custom VideoEffectInstance (ErosionHistoryBlendEffect
			// above), never routed through SinglePassShaderEffect's generic
			// tex/tex0 binder. contract=None correctly signals "not a
			// generically-bindable single-pass shader" rather than falsely
			// implying Contract B (videoTex/maskTex) compatibility.
			def.contract = ShaderContract::None;
			def.assetPaths = { "erosion/erosion_history_blend.vert", "erosion/erosion_history_blend.frag" };
			def.requiredUniforms = { "history", "current", "decayRate", "currentAlpha", "desatAmount" };
			def.params = {
				floatP("decayRate", "Decay rate", 0.92f, 0.5f, 0.99f, 0.85f, 0.96f),
				floatP("desatAmount", "Desaturation amount", 0.0f, 0.0f, 1.0f, 0.0f, 0.5f),
				floatP("currentAlpha", "Current-frame alpha", 1.0f, 0.0f, 1.0f, 0.5f, 1.0f),
			};
			def.capabilities.requiresSourceTexture = true;
			def.capabilities.requiresHistoryBuffer = true;
			def.capabilities.requiresPersistentState = true;
			def.capabilities.safeForAutomaticSelection = true;
			registry.registerEffect(std::move(def), [] { return std::make_unique<ErosionHistoryBlendEffect>(); });
		}
	}

} // namespace videoeffects
