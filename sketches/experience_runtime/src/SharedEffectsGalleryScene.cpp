#include "SharedEffectsGalleryScene.h"

#include "ofMain.h"

#include <algorithm>
#include <cmath>

using scenefx::Choice;
using scenefx::ControlDescriptor;
using scenefx::ModeDescriptor;
using videoeffects::VideoEffectParameterType;

namespace {
SceneMetric identifierMetric(const std::string& id, const std::string& value) {
	SceneMetric m;
	m.metricId = id;
	m.valueType = HudMetricValueType::Identifier;
	m.dataClass = HudDataClass::Literal;
	m.valueId = value;
	return m;
}
} // namespace

SharedEffectsGalleryScene::SharedEffectsGalleryScene(VideoPlaybackService& video) : video_(video) {}

// The curated roster: canonical effect id + 0..2 controls bound to canonical parameter ids. A control's
// default and hard bounds come from the CANONICAL definition (curation can only NARROW them).
void SharedEffectsGalleryScene::buildRoster() {
	const auto& registry = effects_.registry();
	(void)registry;
	std::vector<ModeDescriptor> modes;

	auto def = [this](const std::string& id) { return effects_.getDefinition(id); };
	// Numeric control on a canonical float/int parameter.
	auto N = [&](const std::string& effectId, const char* paramId, const char* label, double mn, double mx, double step, bool integral = false, const char* unit = "") -> std::optional<ControlDescriptor> {
		const auto* d = def(effectId);
		const auto* p = d ? d->findParam(paramId) : nullptr;
		if (!p || (p->type != VideoEffectParameterType::Float && p->type != VideoEffectParameterType::Int)) {
			excluded_.push_back(effectId + "/" + paramId + ": control dropped (not a canonical numeric parameter)");
			return std::nullopt;
		}
		mn = std::max(mn, double(videoeffects::asFloat(p->hardMin))); // curation may only narrow canonical bounds
		mx = std::min(mx, double(videoeffects::asFloat(p->hardMax)));
		auto c = scenefx::numeric(std::string(paramId), label, paramId, mn, mx, step, std::clamp(double(videoeffects::asFloat(p->defaultValue)), mn, mx), integral, unit);
		return c;
	};
	// Enum control: ordered human-readable choices; the default is the choice nearest the canonical default.
	auto E = [&](const std::string& effectId, const char* paramId, const char* label, std::vector<Choice> choices) -> std::optional<ControlDescriptor> {
		const auto* d = def(effectId);
		const auto* p = d ? d->findParam(paramId) : nullptr;
		if (!p) { excluded_.push_back(effectId + "/" + paramId + ": control dropped (parameter not canonical)"); return std::nullopt; }
		int best = 0; float bestDist = 1e9f;
		for (size_t i = 0; i < choices.size(); ++i) {
			float dist = 0.f;
			if (p->type == VideoEffectParameterType::Vec3) { const glm::vec3 dv = videoeffects::asVec3(p->defaultValue); dist = std::abs(choices[i].value[0] - dv.x) + std::abs(choices[i].value[1] - dv.y) + std::abs(choices[i].value[2] - dv.z); }
			else dist = std::abs(choices[i].value[0] - videoeffects::asFloat(p->defaultValue));
			if (dist < bestDist) { bestDist = dist; best = int(i); }
		}
		return scenefx::choice(paramId, label, paramId, std::move(choices), best);
	};
	auto add = [&](const std::string& effectId, std::vector<std::optional<ControlDescriptor>> controls) {
		const auto* d = def(effectId);
		if (!d) { excluded_.push_back(effectId + ": not in the canonical registry"); return; }
		if (d->kind != videoeffects::VideoEffectKind::SinglePassShader) { excluded_.push_back(effectId + ": stateful/processor effect (needs history/aux buffers)"); return; }
		if (!d->capabilities.safeForAutomaticSelection) { excluded_.push_back(effectId + ": canonical safeForAutomaticSelection=false (unvalidated)"); return; }
		if (const auto fx = effects_.knowledgeBase().loadEffectLevelKnowledge(effectId); fx && fx->piSafe.has_value() && !*fx->piSafe) {
			excluded_.push_back(effectId + ": canonical piSafe=false"); return;
		}
		ModeDescriptor m;
		m.effectId = effectId; m.displayName = d->displayName;
		for (auto& c : controls) if (c) m.controls.push_back(*c);
		if (m.controls.size() > size_t(scenefx::kMaxControls)) m.controls.resize(size_t(scenefx::kMaxControls));
		modes.push_back(std::move(m));
	};
	auto C3 = [](const char* l, float r, float g, float b) { return Choice{l, {r, g, b}}; };
	auto C1 = [](const char* l, float v) { return Choice{l, {v, 0.f, 0.f}}; };

	add("desaturate", {N("desaturate", "alpha", "AMOUNT", 0.0, 1.0, 0.1)});
	add("invert", {}); // zero controls: the parameter group hides and never traps focus
	add("solarize", {N("solarize", "alpha", "AMOUNT", 0.0, 1.0, 0.1)});
	add("scanlines", {N("scanlines", "alpha", "AMOUNT", 0.0, 1.0, 0.1)});
	add("recolor", {E("recolor", "tint", "TINT", {C3("AMBER", 0.85f, 0.55f, 0.20f), C3("GREEN", 0.30f, 0.90f, 0.40f), C3("CYAN", 0.30f, 0.80f, 0.90f), C3("MAGENTA", 0.90f, 0.30f, 0.80f), C3("WHITE", 1.f, 1.f, 1.f)}),
	                N("recolor", "alpha", "AMOUNT", 0.0, 1.0, 0.1)});
	add("threshold", {N("threshold", "threshold", "LEVEL", 0.2, 0.8, 0.05)});
	add("dither", {N("dither", "maxPixelation", "PIXELS", 2.0, 16.0, 1.0, true, "PX"), N("dither", "alpha", "ARC", 0.0, 1.0, 0.1)});
	add("channelshift", {N("channelshift", "shift", "SHIFT", 0.002, 0.03, 0.002)});
	add("heatmap_recolor", {E("heatmap_recolor", "palette", "PALETTE", {C1("THERMAL", 0), C1("BIOLUM", 1), C1("SOLARPUNK", 2), C1("INFRARED", 3)}),
	                        N("heatmap_recolor", "intensity", "INTENSE", 0.5, 1.5, 0.1)});
	add("hue_rotate", {N("hue_rotate", "hueOffset", "HUE", 0.0, 360.0, 15.0, false, "DEG"), N("hue_rotate", "hueSpeed", "SPEED", -60.0, 60.0, 10.0)});
	add("ascii_solarpunk", {N("ascii_solarpunk", "cellSize", "CELL", 6.0, 24.0, 2.0, true, "PX"),
	                        E("ascii_solarpunk", "asciiColorMode", "COLOR", {C1("SAMPLED", 0), C1("B&W", 1)})});
	add("bioluminescence", {N("bioluminescence", "threshold", "LEVEL", 0.1, 0.6, 0.05), N("bioluminescence", "intensity", "GLOW", 0.4, 2.4, 0.2)});
	add("chromatic_aberration", {N("chromatic_aberration", "amount", "AMOUNT", 0.0, 6.0, 0.5), N("chromatic_aberration", "radial", "RADIAL", 0.0, 1.0, 0.1)});
	add("edge_glow", {N("edge_glow", "edgeStrength", "EDGE", 0.5, 3.0, 0.25), N("edge_glow", "glowStrength", "GLOW", 0.4, 3.0, 0.2)});
	add("ink_outlines", {N("ink_outlines", "inkStrength", "INK", 0.2, 1.0, 0.1), N("ink_outlines", "posterizeLevels", "LEVELS", 2.0, 12.0, 1.0, true)});
	add("pixel_drift", {N("pixel_drift", "amount", "AMOUNT", 0.0, 16.0, 1.0, true, "PX"), N("pixel_drift", "speed", "SPEED", 0.0, 3.0, 0.25)});
	add("pixel_sorting", {N("pixel_sorting", "threshold", "LEVEL", 0.1, 0.9, 0.05), E("pixel_sorting", "direction", "DIR", {C1("HORIZ", 0), C1("VERT", 1)})});
	add("water_refraction", {N("water_refraction", "amplitude", "AMPL", 0.0, 12.0, 1.0, true, "PX"), N("water_refraction", "speed", "SPEED", 0.25, 3.0, 0.25)});
	add("caustics", {N("caustics", "scale", "SCALE", 0.01, 0.2, 0.01), N("caustics", "intensity", "GLOW", 0.2, 2.0, 0.2)}); // excluded below by canonical eligibility

	controller_ = std::make_unique<scenefx::ModeController>(std::move(modes));
	for (const auto& e : excluded_) ofLogNotice("SharedEffectsGallery") << "excluded: " << e;
	if (!controller_->ok()) ofLogError("SharedEffectsGallery") << "invalid roster: " << controller_->error();
	else ofLogNotice("SharedEffectsGallery") << "roster: " << controller_->modeCount() << " canonical effects";
}

void SharedEffectsGalleryScene::setup(const SceneServices& services) {
	(void)services;
	if (!effects_.setup()) ofLogError("SharedEffectsGallery") << "VideoEffectService setup failed";
	buildRoster();
	srcFbo_.allocate(kNativeWidth, kNativeHeight, GL_RGBA);
	srcFbo_.begin(); ofClear(0, 0, 0, 255); srcFbo_.end();
	effFbo_.allocate(kNativeWidth, kNativeHeight, GL_RGBA);
	effFbo_.begin(); ofClear(0, 0, 0, 255); effFbo_.end();
	didSetup_ = true;
}

bool SharedEffectsGalleryScene::instanceReadyForTesting() const {
	if (!controller_ || !controller_->ok()) return false;
	const auto it = instances_.find(controller_->mode().effectId);
	return it != instances_.end() && it->second && it->second->isReady();
}

// Lazily creates (and caches) the active mode's canonical instance; a failure is remembered so the
// scene stays operable and reports Degraded honestly while the user cycles to another effect.
void SharedEffectsGalleryScene::ensureInstance() {
	if (!controller_ || !controller_->ok()) return;
	const std::string& id = controller_->mode().effectId;
	if (instances_.count(id) || failed_.count(id)) return;
	auto inst = effects_.createInstance(id);
	if (inst && inst->isReady()) instances_[id] = std::move(inst);
	else { failed_.insert(id); ofLogError("SharedEffectsGallery") << "effect '" << id << "' could not be created; passing video through"; }
}

// Applies the active mode's control values to the canonical parameter set (typed by the canonical schema).
void SharedEffectsGalleryScene::rebuildParams() {
	params_ = videoeffects::VideoEffectParameters{};
	if (!controller_ || !controller_->ok()) return;
	const auto* d = effects_.getDefinition(controller_->mode().effectId);
	for (int i = 0; d && i < controller_->controlCount(); ++i) {
		const auto& c = controller_->control(i);
		const auto* p = d->findParam(c.paramId);
		if (!p) continue;
		const auto v = controller_->paramValue(i);
		switch (p->type) {
			case VideoEffectParameterType::Int: params_.set(c.paramId, int(std::lround(v[0]))); break;
			case VideoEffectParameterType::Vec3: params_.set(c.paramId, glm::vec3(v[0], v[1], v[2])); break;
			default: params_.set(c.paramId, v[0]); break;
		}
	}
	paramsDirty_ = false;
}

void SharedEffectsGalleryScene::activate() {
	active_ = true;
	time_ = 0.f;
	++activations_;
	reveal_.reset(); // always start in a mask-free dwell: no raw flash at activation
	ensureInstance();
	paramsDirty_ = true;
}

void SharedEffectsGalleryScene::deactivate() { active_ = false; }

void SharedEffectsGalleryScene::update(float dt) {
	if (!active_) return;
	dt_ = dt;
	time_ += dt;
	reveal_.update(dt, revealTimeScale_);
}

void SharedEffectsGalleryScene::drawToCurrentTarget() {
	if (!didSetup_ || didShutdown_ || !controller_ || !controller_->ok()) return;
	ensureInstance();
	if (paramsDirty_) rebuildParams();

	const ofTexture* tex = video_.currentTexture();
	srcFbo_.begin();
	ofDisableAlphaBlending();
	ofSetColor(0);
	ofDrawRectangle(0, 0, kNativeWidth, kNativeHeight);
	const bool rawValid = tex && tex->isAllocated() && tex->getWidth() > 0 && tex->getHeight() > 0; // missing frame => no raw strip
	if (rawValid) {
		const float s = std::max(float(kNativeWidth) / tex->getWidth(), float(kNativeHeight) / tex->getHeight());
		const float dw = tex->getWidth() * s, dh = tex->getHeight() * s;
		ofSetColor(255);
		tex->draw((kNativeWidth - dw) * 0.5f, (kNativeHeight - dh) * 0.5f, dw, dh);
	}
	srcFbo_.end();

	ofSetColor(255);
	const auto it = instances_.find(controller_->mode().effectId);
	if (it != instances_.end() && it->second && it->second->isReady()) {
		// Effected path: the SAME srcFbo_ frame, rendered into the reused effFbo_ (no per-frame allocation).
		effFbo_.begin();
		ofClear(0, 0, 0, 255);
		videoeffects::VideoEffectContext ctx;
		ctx.sourceTexture = &srcFbo_.getTexture();
		ctx.destinationRect = ofRectangle(0, 0, kNativeWidth, kNativeHeight);
		ctx.time = time_;
		ctx.deltaTime = dt_;
		ctx.alpha = 1.0f;
		it->second->update(ctx, params_);
		it->second->render(ctx, params_);
		effFbo_.end();
		ofSetColor(255);
		effFbo_.draw(0, 0, kNativeWidth, kNativeHeight);

		// Autonomous raw reveal: one full-height window of the UNPROCESSED srcFbo_ frame (same frame as the
		// effected path, same coordinates). Hidden dwell / sub-pixel width => empty span => nothing drawn.
		const scenefx::RevealSpan span = scenefx::revealSpanPx(reveal_.left(), reveal_.right(), kNativeWidth);
		if (rawValid && !span.empty()) {
			const ofTexture& raw = srcFbo_.getTexture();
			const int w = span.x1 - span.x0;
			ofSetColor(255);
			raw.drawSubsection(float(span.x0), 0.f, float(w), float(kNativeHeight), float(span.x0), 0.f, float(w), float(kNativeHeight));
			// near-crisp soft edges: re-blend the first/last few effected columns over the window edge
			const int soft = std::min(int(scenefx::RevealTuning{}.edgeSoftPx), w / 2);
			if (soft > 0) {
				ofEnableAlphaBlending();
				for (int i = 0; i < soft; ++i) {
					const float keepRaw = (float(i) + 0.5f) / float(soft);
					const int a = int(255.f * (1.f - keepRaw) + 0.5f); // effected re-drawn with falling alpha toward the inside
					ofSetColor(255, 255, 255, a);
					const auto& eff = effFbo_.getTexture();
					eff.drawSubsection(float(span.x0 + i), 0.f, 1.f, float(kNativeHeight), float(span.x0 + i), 0.f, 1.f, float(kNativeHeight));
					eff.drawSubsection(float(span.x1 - 1 - i), 0.f, 1.f, float(kNativeHeight), float(span.x1 - 1 - i), 0.f, 1.f, float(kNativeHeight));
				}
				ofDisableAlphaBlending();
				ofSetColor(255);
			}
		}
	} else {
		srcFbo_.draw(0, 0, kNativeWidth, kNativeHeight); // degraded: untreated video, still operable (no reveal: nothing to compare)
	}
}

SceneHudStatus SharedEffectsGalleryScene::hudStatus() const {
	SceneHudStatus status;
	status.schemaVersion = 1;
	status.sceneId = kSceneId;
	status.displayName = displayName();
	const bool rosterOk = controller_ && controller_->ok();
	const bool degraded = rosterOk && failed_.count(controller_->mode().effectId) > 0;
	if (!didSetup_ || !active_) status.health = SceneHealth::Loading;
	else if (!rosterOk) { status.health = SceneHealth::Failed; status.message = std::string("effect roster invalid"); }
	else if (degraded) { status.health = SceneHealth::Degraded; status.message = std::string("effect unavailable; untreated video"); }
	else status.health = SceneHealth::Ready;
	{
		VideoPlaybackStatus vs = video_.status();
		if (vs.fallbackDisplayTitle) status.mediaName = vs.fallbackDisplayTitle;
		else if (vs.titleId) status.mediaName = vs.titleId;
	}
	if (!rosterOk) return status;

	// Mode = the active effect. Everything below reads the SAME controller state, so it cannot disagree.
	SceneModeStatus mode;
	mode.modeId = controller_->mode().effectId;
	mode.displayName = controller_->mode().displayName;
	mode.index = controller_->modeIndex();
	mode.count = controller_->modeCount();
	status.modeName = mode.displayName;
	status.mode = mode;
	status.activeEffects = {controller_->mode().displayName};

	if (didSetup_ && active_) {
		SceneSemanticData data;
		data.state.primaryStateId = "scene.fx.state.online";
		data.timing.activeSeconds = time_;
		for (int i = 0; i < controller_->controlCount(); ++i) {
			const std::string k = "scene.fx.metric.c" + std::to_string(i) + "_";
			data.metrics.push_back(identifierMetric(k + "label", controller_->control(i).label));
			data.metrics.push_back(identifierMetric(k + "value", controller_->formattedValue(i)));
			const std::string pos = controller_->positionText(i);
			if (!pos.empty()) data.metrics.push_back(identifierMetric(k + "pos", pos));
		}
		status.semantic = data;
	}
	status.paused = false;
	return status;
}

SceneCapabilities SharedEffectsGalleryScene::capabilities() const {
	SceneCapabilities caps;
	auto cmd = [&caps](SceneCommand c, const char* label, const char* shortLabel) {
		SceneCommandDescriptor d; d.command = c; d.label = label; d.shortLabel = shortLabel; caps.commands.push_back(d);
	};
	cmd(SceneCommand::NextMedia, "Next", "NEXT");
	cmd(SceneCommand::PreviousMedia, "Previous", "PREV");
	cmd(SceneCommand::NextMode, "Next effect", "NEXT MODE");
	// Two option axes = the two possible control rows; a mode with fewer controls refuses the extra axis.
	for (int i = 0; i < scenefx::kMaxControls; ++i) {
		SceneOptionAxis a; a.id = "control" + std::to_string(i); a.label = a.id; a.count = 0; // count varies per mode
		caps.optionAxes.push_back(a);
	}
	return caps;
}

bool SharedEffectsGalleryScene::executeCommand(SceneCommand command) {
	if (!didSetup_ || didShutdown_ || !controller_ || !controller_->ok()) return false;
	switch (command) {
		case SceneCommand::NextMedia: return video_.next();
		case SceneCommand::PreviousMedia: return video_.previous();
		case SceneCommand::NextMode:
			controller_->stepMode(+1); // atomic: effect, labels, controls and activity change together
			reveal_.reset();           // a new effect is shown cleanly first (hidden dwell); playback is untouched
			paramsDirty_ = true;
			ensureInstance();
			return true;
		case SceneCommand::Reset: reset(); return true;
		case SceneCommand::Regenerate:
		default: return false;
	}
}

bool SharedEffectsGalleryScene::stepOption(int axisIndex, int delta) {
	if (!didSetup_ || didShutdown_ || !controller_ || !controller_->ok()) return false;
	if (axisIndex < 0 || axisIndex >= controller_->controlCount()) return false;
	controller_->stepControl(axisIndex, delta);
	paramsDirty_ = true;
	return true;
}

void SharedEffectsGalleryScene::reset() {
	if (controller_ && controller_->ok()) controller_->resetAll();
	reveal_.reset();
	paramsDirty_ = true;
	time_ = 0.f;
	ensureInstance();
}

void SharedEffectsGalleryScene::shutdown() {
	active_ = false;
	didShutdown_ = true;
	instances_.clear();
}

std::optional<videoeffects::EffectActivityStatus> SharedEffectsGalleryScene::currentEffectActivitySnapshot() const {
	if (!didSetup_ || !controller_ || !controller_->ok()) return std::nullopt;
	videoeffects::EffectActivityStatus status;
	const std::string& id = controller_->mode().effectId;
	if (failed_.count(id)) { status.health = videoeffects::EffectHealth::Degraded; status.messageId = "effect.shader_unavailable"; }
	videoeffects::EffectActivitySlot slot;
	slot.slotId = "primary"; slot.effectId = id; slot.displayName = controller_->mode().displayName;
	slot.phase = videoeffects::EvolutionPhase::Holding; slot.transitionProgress01 = 1.0f; slot.prominence = 1.0f;
	status.slots.push_back(slot);
	return status;
}
