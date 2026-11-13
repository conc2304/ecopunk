#include "ShredReconstructionScene.h"

#include "ofMain.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

using shred::Engine;
using shred::Fragment;
using shred::ModeId;

namespace {
SceneMetric identifierMetric(const std::string& id, const std::string& value) {
	SceneMetric m;
	m.metricId = id;
	m.valueType = HudMetricValueType::Identifier;
	m.dataClass = HudDataClass::Literal;
	m.valueId = value;
	return m;
}
const char* env(const char* k) { return std::getenv(k); }
} // namespace

namespace {
constexpr int kDetents = 11;
int detentOf(float n) { return std::clamp(int(std::lround(n * float(kDetents - 1))), 0, kDetents - 1); }
std::string dialText(const shred::ModeSpec& sp, int d, float phys) {
	char b[48];
	const char* u = sp.dial[d].unit;
	if (phys >= 10.f) std::snprintf(b, sizeof(b), "%.0f %s", phys, u); else std::snprintf(b, sizeof(b), "%.2f %s", phys, u);
	return b;
}
} // namespace

ShredReconstructionScene::ShredReconstructionScene(VideoPlaybackService& video) : video_(video) {}

// Curated candidate list (canonical ids). Eligibility is POSITIVE: single-pass, safeForAutomaticSelection, and
// not piSafe==false; on the Pi profile piSafe must be explicitly true (unclassified != eligible).
void ShredReconstructionScene::buildCandidates() {
	static const char* kCurated[] = {"invert", "solarize", "recolor", "heatmap_recolor", "hue_rotate", "threshold", "dither", "channelshift",
		"bioluminescence", "chromatic_aberration", "edge_glow", "ink_outlines", "pixel_drift", "scanlines", "ascii_solarpunk", "pixel_sorting", "water_refraction"};
	candidates_.clear(); excluded_.clear();
	for (const char* id : kCurated) {
		const auto* d = effects_.getDefinition(id);
		if (!d) { excluded_.push_back(std::string(id) + ": not in the canonical registry"); continue; }
		const auto fx = effects_.knowledgeBase().loadEffectLevelKnowledge(id);
		int piState = !fx || !fx->piSafe.has_value() ? -1 : (*fx->piSafe ? 1 : 0);
		if (!shred::effectEligible(d->kind == videoeffects::VideoEffectKind::SinglePassShader, d->capabilities.safeForAutomaticSelection, piState, pi_)) {
			excluded_.push_back(std::string(id) + ": not positively classified eligible");
			continue;
		}
		candidates_.push_back(id);
	}
	for (const auto& e : excluded_) ofLogNotice("ShredReconstruction") << "effect excluded: " << e;
	ofLogNotice("ShredReconstruction") << candidates_.size() << " canonical effect candidates (" << (pi_ ? "Pi profile" : "desktop") << ")";
}

void ShredReconstructionScene::ensureEffect(const std::string& id) {
	if (id.empty() || instances_.count(id)) return;
	auto inst = effects_.createInstance(id);
	if (inst && inst->isReady()) instances_[id] = std::move(inst);
	else {
		instances_[id] = nullptr; // remembered failure
		ofLogError("ShredReconstruction") << "effect '" << id << "' unavailable";
	}
}

void ShredReconstructionScene::setup(const SceneServices& services) {
	(void)services;
	const char* q = env("EXPERIENCE_RUNTIME_SHRED_QUALITY");
	pi_ = q && std::strcmp(q, "pi") == 0;
	renderScale_ = pi_ ? 0.5f : 1.f;
	rw_ = int(kNativeWidth * renderScale_); rh_ = int(kNativeHeight * renderScale_);
	if (!effects_.setup()) ofLogError("ShredReconstruction") << "VideoEffectService setup failed";
	buildCandidates();
	if (const char* e = env("EXPERIENCE_RUNTIME_SHRED_EFFECTS")) if (std::strcmp(e, "0") == 0) candidates_.clear();
	ensureEffect("desaturate");
	desatOk_ = instances_["desaturate"] != nullptr;
	for (const auto& id : candidates_) ensureEffect(id); // compile once at setup, not during playback
	candidates_.erase(std::remove_if(candidates_.begin(), candidates_.end(), [&](const std::string& id) { return instances_[id] == nullptr; }), candidates_.end());

	for (ofFbo* f : {&srcFbo_, &desatFbo_, &effFbo_[0], &effFbo_[1]}) {
		f->allocate(rw_, rh_, GL_RGBA);
		f->begin(); ofClear(0, 0, 0, 255); f->end();
	}

	uint64_t seed = 20261113;
	if (const char* s = env("EXPERIENCE_RUNTIME_SHRED_SEED")) seed = std::strtoull(s, nullptr, 10);
	engine_.configure(kNativeWidth, kNativeHeight, pi_, seed);
	engine_.setEffectCandidates(candidates_);
	if (const char* a = env("EXPERIENCE_RUNTIME_SHRED_AUTOCYCLE")) engine_.setAutoCycle(std::strcmp(a, "0") != 0);
	if (const char* m = env("EXPERIENCE_RUNTIME_SHRED_MODE")) {
		for (int i = 0; i < shred::kModeCount; ++i) if (std::strcmp(shred::spec(ModeId(i)).key, m) == 0) { engine_.setMode(ModeId(i)); if (!env("EXPERIENCE_RUNTIME_SHRED_AUTOCYCLE")) engine_.setAutoCycle(false); }
	}
	didSetup_ = true; didShutdown_ = false;
}

void ShredReconstructionScene::activate() {
	if (!didSetup_ || didShutdown_) return;
	active_ = true; time_ = 0;
	++generation_;
	engine_.hardReset(engine_.mode()); // fresh, deterministic layout on every (re)activation; dials persist
	engine_.setEffectCandidates(candidates_);
	lastMediaId_.clear();
}

void ShredReconstructionScene::deactivate() { active_ = false; }

void ShredReconstructionScene::update(float dt) {
	if (!active_) return;
	dt_ = dt; time_ += dt;
	const VideoPlaybackStatus st = video_.status();
	const std::string mid = st.mediaId ? *st.mediaId : std::string();
	if (mid != lastMediaId_) { // media change: retarget the composition to the new source
		if (!lastMediaId_.empty()) engine_.onMediaChanged();
		lastMediaId_ = mid;
	}
	engine_.update(dt);
}

void ShredReconstructionScene::renderSource(const ofTexture* tex) {
	srcFbo_.begin();
	ofDisableAlphaBlending();
	ofClear(0, 0, 0, 255);
	haveFrame_ = tex && tex->isAllocated() && tex->getWidth() > 0 && tex->getHeight() > 0;
	if (haveFrame_) {
		const float s = std::max(float(rw_) / tex->getWidth(), float(rh_) / tex->getHeight());
		const float dw = tex->getWidth() * s, dh = tex->getHeight() * s;
		ofSetColor(255);
		tex->draw((rw_ - dw) * 0.5f, (rh_ - dh) * 0.5f, dw, dh); // cover-fit: any aspect (portrait included)
	}
	srcFbo_.end();
}

void ShredReconstructionScene::drawFragments() {
	const float W = float(kNativeWidth), H = float(kNativeHeight);
	std::vector<const Fragment*> all;
	for (const auto& f : engine_.ghostFragments()) if (f.visible) all.push_back(&f);
	for (const auto& f : engine_.fragments()) if (f.visible) all.push_back(&f);
	(void)W; (void)H;
	ofEnableAlphaBlending();
	mesh_.setMode(OF_PRIMITIVE_TRIANGLES);
	auto quad = [&](const ofTexture& tex, const Fragment& f, float a) {
		const glm::vec2 t00 = tex.getCoordFromPercent(f.u0, f.v0), t10 = tex.getCoordFromPercent(f.u1, f.v0);
		const glm::vec2 t11 = tex.getCoordFromPercent(f.u1, f.v1), t01 = tex.getCoordFromPercent(f.u0, f.v1);
		const float x0 = f.dx, y0 = f.dy, x1 = f.dx + f.dw, y1 = f.dy + f.dh;
		const ofFloatColor c(1.f, 1.f, 1.f, a);
		auto v = [&](float x, float y, const glm::vec2& t) { mesh_.addVertex({x, y, 0}); mesh_.addTexCoord(t); mesh_.addColor(c); };
		v(x0, y0, t00); v(x1, y0, t10); v(x1, y1, t11);
		v(x0, y0, t00); v(x1, y1, t11); v(x0, y1, t01);
	};
	auto flush = [&](const ofTexture& tex) {
		if (mesh_.getNumVertices() == 0) return;
		tex.bind(); mesh_.draw(); tex.unbind(); mesh_.clear();
	};
	for (int z = 0; z <= 1; ++z) {
		// solid (vacant island) patches
		for (const Fragment* f : all) if (f->z == z && f->solid) { ofSetColor(0, 0, 0, int(255.f * f->alpha)); ofDrawRectangle(f->dx, f->dy, f->dw, f->dh); }
		ofSetColor(255);
		// desaturated baseline: one batch
		std::vector<const ofTexture*> srcs;
		auto srcOf = [&](const Fragment&) -> const ofTexture* { return &desatFbo_.getTexture(); };
		for (const Fragment* f : all) if (f->z == z && !f->solid) { const ofTexture* t = srcOf(*f); if (std::find(srcs.begin(), srcs.end(), t) == srcs.end()) srcs.push_back(t); }
		for (const ofTexture* t : srcs) {
			for (const Fragment* f : all) if (f->z == z && !f->solid && srcOf(*f) == t) quad(*t, *f, f->alpha);
			flush(*t);
		}
		// non-default effect fragments: revealed through the SAME geometry from the slot's effect FBO
		for (int s = 0; s < 2; ++s) {
			if (effFboId_[s].empty()) continue;
			const ofTexture& et = effFbo_[s].getTexture();
			for (const Fragment* f : all) if (f->z == z && f->effectSlot == s && f->effAlpha > 0.f) quad(et, *f, f->alpha * f->effAlpha);
			flush(et);
		}
	}
	ofDisableAlphaBlending();
	ofSetColor(255);
}

void ShredReconstructionScene::drawToCurrentTarget() {
	if (!didSetup_ || didShutdown_ || !srcFbo_.isAllocated()) return;
	renderSource(video_.currentTexture());

	// default look: canonical desaturate (not counted as a non-default effect)
	auto runEffect = [&](const std::string& id, ofFbo& dst, const ofTexture& src) -> bool {
		auto it = instances_.find(id);
		if (it == instances_.end() || !it->second) return false;
		dst.begin(); ofDisableAlphaBlending(); ofClear(0, 0, 0, 255);
		videoeffects::VideoEffectContext ctx;
		ctx.sourceTexture = &src; ctx.destinationRect = ofRectangle(0, 0, rw_, rh_);
		ctx.time = time_; ctx.deltaTime = dt_; ctx.alpha = 1.f;
		videoeffects::VideoEffectParameters params;
		if (id == "desaturate") params.set("alpha", 1.0f);
		it->second->update(ctx, params); it->second->render(ctx, params);
		dst.end();
		return true;
	};
	if (!desatOk_ || !runEffect("desaturate", desatFbo_, srcFbo_.getTexture())) {
		desatFbo_.begin(); ofSetColor(255); srcFbo_.draw(0, 0, rw_, rh_); desatFbo_.end(); // degraded: untreated video (reported by hudStatus)
	}
	// effects: render each ACTIVE slot's canonical effect once (full frame); fragments reveal it
	for (int s = 0; s < 2; ++s) {
		const auto info = engine_.slotInfo(s);
		if (info.effectId.empty() || info.assigned == 0) { effFboId_[s].clear(); continue; }
		if (!runEffect(info.effectId, effFbo_[s], srcFbo_.getTexture())) { effFboId_[s].clear(); continue; }
		effFboId_[s] = info.effectId;
	}
	drawFragments();
}

std::optional<videoeffects::EffectActivityStatus> ShredReconstructionScene::currentEffectActivitySnapshot() const {
	if (!didSetup_ || didShutdown_) return std::nullopt;
	videoeffects::EffectActivityStatus st;
	if (!desatOk_) { st.health = videoeffects::EffectHealth::Degraded; st.messageId = "effect.shader_unavailable"; }
	static const char* ids[2] = {"slot_a", "slot_b"};
	for (int s = 0; s < 2; ++s) {
		const auto info = engine_.slotInfo(s);
		if (info.effectId.empty() || info.assigned == 0 || effFboId_[s].empty()) continue;
		videoeffects::EffectActivitySlot slot;
		slot.slotId = ids[s]; slot.effectId = info.effectId;
		const auto* d = effects_.getDefinition(info.effectId); slot.displayName = d ? d->displayName : info.effectId;
		const bool trans = info.retiring || info.progress < 0.98f;
		slot.phase = trans ? videoeffects::EvolutionPhase::Transitioning : videoeffects::EvolutionPhase::Holding;
		slot.transitionProgress01 = trans ? (info.retiring ? 1.f - info.progress : info.progress) : 1.f;
		slot.prominence = 1.0f; // constant: NOT coverage/intensity (those stay in scene telemetry)
		st.slots.push_back(slot);
	}
	return st;
}

SceneHudStatus ShredReconstructionScene::hudStatus() const {
	SceneHudStatus status;
	status.schemaVersion = 1;
	status.sceneId = kSceneId;
	status.displayName = displayName();
	if (didShutdown_) { status.health = SceneHealth::Failed; status.message = std::string("shut down"); }
	else if (!didSetup_ || !active_) status.health = SceneHealth::Loading;
	else if (!desatOk_) { status.health = SceneHealth::Degraded; status.message = std::string("Desaturate unavailable; untreated video"); }
	else if (!haveFrame_) { status.health = SceneHealth::Degraded; status.message = std::string("No video frame"); }
	else status.health = SceneHealth::Ready;
	{
		VideoPlaybackStatus vs = video_.status();
		if (vs.fallbackDisplayTitle) status.mediaName = vs.fallbackDisplayTitle;
		else if (vs.titleId) status.mediaName = vs.titleId;
	}
	if (didSetup_ && !didShutdown_) {
		const auto& sp = shred::spec(engine_.mode());
		SceneModeStatus mode; mode.modeId = sp.key; mode.displayName = sp.title; mode.index = int(engine_.mode()); mode.count = shred::kModeCount;
		status.mode = mode; status.modeName = mode.displayName;
	}
	if (didSetup_ && active_ && !didShutdown_) {
		const auto& t = engine_.telemetry();
		SceneSemanticData data;
		data.state.primaryStateId = "scene.shred.state.online";
		data.timing.activeSeconds = time_;
		data.metrics.push_back(identifierMetric("scene.shred.metric.mode", shred::spec(engine_.mode()).key));
		data.metrics.push_back(identifierMetric("scene.shred.metric.phase", t.phase));
		const ModeId mm = engine_.mode();
		for (int d = 0; d < 2; ++d) {
			const std::string k = "scene.shred.metric.c" + std::to_string(d) + "_";
			data.metrics.push_back(identifierMetric(k + "label", shred::spec(mm).dial[d].label));
			data.metrics.push_back(identifierMetric(k + "value", dialText(shred::spec(mm), d, engine_.dialPhys(mm, d))));
			data.metrics.push_back(identifierMetric(k + "pos", std::to_string(detentOf(engine_.dial(mm, d)) + 1) + "/" + std::to_string(kDetents)));
		}
		SceneMetric coh; coh.metricId = "scene.shred.metric.coherence"; coh.valueType = HudMetricValueType::Scalar; coh.dataClass = HudDataClass::Literal; coh.value = t.coherence; coh.normalizedValue = t.coherence;
		data.metrics.push_back(coh);
		SceneMetric frag; frag.metricId = "scene.shred.metric.fragments"; frag.valueType = HudMetricValueType::Scalar; frag.dataClass = HudDataClass::Literal; frag.value = float(t.activeFragments);
		data.metrics.push_back(frag);
		status.activeItemCount = t.activeFragments;
		status.semantic = data;
	}
	status.paused = false;
	return status;
}

SceneCapabilities ShredReconstructionScene::capabilities() const {
	SceneCapabilities caps;
	auto cmd = [&caps](SceneCommand c, const char* label, const char* shortLabel) { SceneCommandDescriptor d; d.command = c; d.label = label; d.shortLabel = shortLabel; caps.commands.push_back(d); };
	cmd(SceneCommand::NextMedia, "Next", "NEXT");
	cmd(SceneCommand::PreviousMedia, "Previous", "PREV");
	cmd(SceneCommand::Regenerate, "Reseed", "RESEED");
	cmd(SceneCommand::NextMode, "Next mode", "NEXT MODE");
	for (int d = 0; d < 2; ++d) { SceneOptionAxis a; a.id = "dial" + std::to_string(d); a.label = shred::spec(engine_.mode()).dial[d].label; a.count = kDetents; caps.optionAxes.push_back(a); }
	return caps; // Reset honoured, not advertised.
}

bool ShredReconstructionScene::executeCommand(SceneCommand command) {
	if (!didSetup_ || didShutdown_) return false;
	switch (command) {
		case SceneCommand::NextMedia: return video_.next();
		case SceneCommand::PreviousMedia: return video_.previous();
		case SceneCommand::Regenerate: ++generation_; engine_.reseed(engine_.currentSeed() * 6364136223846793005ULL + 1442695040888963407ULL + generation_); return true;
		case SceneCommand::NextMode: engine_.setMode(ModeId((int(engine_.mode()) + 1) % shred::kModeCount)); return true;
		case SceneCommand::Reset: reset(); return true;
		default: return false;
	}
}

void ShredReconstructionScene::reset() {
	engine_.resetDials();
	engine_.hardReset(engine_.mode());
	engine_.setEffectCandidates(candidates_);
	time_ = 0;
}

void ShredReconstructionScene::shutdown() {
	active_ = false; didShutdown_ = true;
	instances_.clear();
}


bool ShredReconstructionScene::stepOption(int axisIndex, int delta) {
	if (!didSetup_ || didShutdown_ || axisIndex < 0 || axisIndex > 1) return false;
	const ModeId m = engine_.mode();
	const int i = std::clamp(detentOf(engine_.dial(m, axisIndex)) + (delta > 0 ? 1 : -1), 0, kDetents - 1);
	engine_.setDial(m, axisIndex, float(i) / float(kDetents - 1));
	return true;
}
