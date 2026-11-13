#include "PhosphorContoursScene.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace {
struct GlSnapshot {
	GLint viewport[4] = {0, 0, 0, 0}, fbo = 0, program = 0;
	GLboolean blend = 0, scissor = 0, depth = 0;
	bool operator==(const GlSnapshot& o) const {
		return viewport[0] == o.viewport[0] && viewport[1] == o.viewport[1] && viewport[2] == o.viewport[2] && viewport[3] == o.viewport[3]
			&& fbo == o.fbo && program == o.program && blend == o.blend && scissor == o.scissor && depth == o.depth;
	}
};
GlSnapshot snapGl() {
	GlSnapshot s;
	glGetIntegerv(GL_VIEWPORT, s.viewport);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s.fbo);
	glGetIntegerv(GL_CURRENT_PROGRAM, &s.program);
	s.blend = glIsEnabled(GL_BLEND); s.scissor = glIsEnabled(GL_SCISSOR_TEST); s.depth = glIsEnabled(GL_DEPTH_TEST);
	return s;
}
SceneMetric identifierMetric(const std::string& id, const std::string& value) {
	SceneMetric m;
	m.metricId = id;
	m.valueType = HudMetricValueType::Identifier;
	m.dataClass = HudDataClass::Literal;
	m.valueId = value;
	return m;
}
int wrapStep(int v, int delta) { return ((v + delta) % phc::kSelectorSteps + phc::kSelectorSteps) % phc::kSelectorSteps; }
int envIndex(const char* name, int fallback, int count) {
	const char* v = std::getenv(name);
	return v ? ((std::atoi(v) % count) + count) % count : fallback;
}
} // namespace

PhosphorContoursScene::PhosphorContoursScene(VideoPlaybackService& video)
	: video_(video) {
}

void PhosphorContoursScene::setup(const SceneServices& services) {
	(void)services; // consumes the canonical VideoPlaybackService directly, like the other scenes
	if (const char* q = std::getenv("EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_QUALITY"))
		quality_ = std::string(q) == "pisafe" ? phc::Quality::PiSafe : phc::Quality::Desktop;
	presetIdx_ = envIndex("EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_PRESET", presetIdx_, phc::kPresetCount); // MODE (0..3)
	density_ = envIndex("EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_DENSITY", density_, phc::kSelectorSteps);
	decay_ = envIndex("EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_DECAY", decay_, phc::kSelectorSteps);
	refreshLook();
	if (const char* s = std::getenv("EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_SEED")) seed_ = static_cast<uint32_t>(std::max(0, std::atoi(s)));
	pipe_.loadShaders(); // failure is reported via hudStatus(); the scene then passes video through untreated
	pipe_.allocate(width_, height_, quality_);
	didSetup_ = true;
	didShutdown_ = false;
}

void PhosphorContoursScene::clearHistory() {
	if (pipe_.allocated()) pipe_.clearHistory();
	historyClears_++;
}

void PhosphorContoursScene::resizeTargets(int w, int h) {
	if (!didSetup_ || didShutdown_ || w < 16 || h < 16) return;
	width_ = w; height_ = h;
	pipe_.allocate(width_, height_, quality_); // reallocates + clears history
	historyClears_++;
}

void PhosphorContoursScene::activate() {
	if (!didSetup_ || didShutdown_) return;
	if (!pipe_.allocated()) pipe_.allocate(width_, height_, quality_); // deactivate() released the targets
	active_ = true;
	time_ = 0.f;
	lastMediaId_ = video_.status().mediaId;
	lastActivationCount_ = video_.activationCount();
	clearHistory(); // nothing from a previous visit may persist
}

void PhosphorContoursScene::deactivate() {
	active_ = false;
	// Free the GPU targets while this scene is not on screen (it can be revisited: activate() re-allocates).
	if (didSetup_ && !didShutdown_) pipe_.release();
}

void PhosphorContoursScene::update(float dt) {
	if (!active_) return;
	dt_ = dt;
	time_ += dt;
	// Media identity is owned by the canonical service; we only observe it so persistence never smears one
	// clip into the next.
	const VideoPlaybackStatus st = video_.status();
	const uint64_t act = video_.activationCount();
	if (st.mediaId != lastMediaId_ || act != lastActivationCount_) {
		lastMediaId_ = st.mediaId;
		lastActivationCount_ = act;
		clearHistory();
	}
}

void PhosphorContoursScene::drawToCurrentTarget() {
	if (!didSetup_ || didShutdown_ || !pipe_.allocated()) return;
	GlSnapshot before;
	if (glProbe_) before = snapGl();
	const ofTexture* tex = video_.currentTexture();

	// Source into the pipeline's own target, cover-fit (the same framing every other scene uses).
	ofFbo& src = pipe_.sourceFbo();
	src.begin();
	ofDisableAlphaBlending();
	ofSetColor(0);
	ofDrawRectangle(0, 0, pipe_.width(), pipe_.height());
	haveFrame_ = tex != nullptr && tex->isAllocated() && tex->getWidth() > 0 && tex->getHeight() > 0;
	if (haveFrame_) {
		const float vw = tex->getWidth(), vh = tex->getHeight();
		const float s = std::max(pipe_.width() / vw, pipe_.height() / vh);
		const float dw = vw * s, dh = vh * s;
		ofSetColor(255);
		tex->draw((pipe_.width() - dw) * 0.5f, (pipe_.height() - dh) * 0.5f, dw, dh);
	}
	src.end();

	phc::Pipeline::Options opt;
	opt.seed = seed_;
	pipe_.process(look_, time_, dt_, opt);

	// Present into the runtime-owned scene target (already bound + cleared). Blank input -> the pipeline's
	// own dark background + grain: a stable safe fallback, never a stale frame.
	ofSetColor(255);
	(pipe_.usable() ? pipe_.output() : src).draw(0, 0, width_, height_);
	if (glProbe_) { glProbeSamples_++; if (!(snapGl() == before)) glMismatches_++; }
}

SceneHudStatus PhosphorContoursScene::hudStatus() const {
	SceneHudStatus status;
	status.schemaVersion = 1;
	status.sceneId = kSceneId;
	status.displayName = displayName();
	if (didShutdown_) {
		status.health = SceneHealth::Failed;
		status.message = std::string("shut down");
	} else if (!didSetup_ || !active_) {
		status.health = SceneHealth::Loading;
	} else if (!pipe_.usable()) {
		status.health = SceneHealth::Degraded;
		status.message = std::string("Contour shaders failed; untreated video");
	} else if (!haveFrame_) {
		status.health = SceneHealth::Degraded;
		status.message = std::string("No video frame; safe dark fallback");
	} else {
		status.health = SceneHealth::Ready;
	}
	{
		VideoPlaybackStatus videoStatus = video_.status();
		if (videoStatus.fallbackDisplayTitle) status.mediaName = videoStatus.fallbackDisplayTitle;
		else if (videoStatus.titleId) status.mediaName = videoStatus.titleId;
	}
	// The look is this scene's mode (existing SceneHudStatus.mode).
	SceneModeStatus mode;
	mode.modeId = phc::modeId(presetIdx_);
	mode.displayName = phc::modeLabel(presetIdx_);
	mode.index = presetIdx_;
	mode.count = phc::kPresetCount;
	status.modeName = mode.displayName;
	status.mode = mode;
	// Two selector rows (DENSITY / DECAY), published only while running (missing data -> HUD rows hide).
	if (didSetup_ && active_ && !didShutdown_) {
		SceneSemanticData data;
		data.state.primaryStateId = "scene.contours.state.online";
		data.timing.activeSeconds = time_;
		data.metrics.push_back(identifierMetric("scene.contours.metric.c0_label", "DENSITY"));
		data.metrics.push_back(identifierMetric("scene.contours.metric.c0_value", phc::densityLabel(density_)));
		data.metrics.push_back(identifierMetric("scene.contours.metric.c0_pos", std::to_string(density_ + 1) + "/" + std::to_string(phc::kSelectorSteps)));
		data.metrics.push_back(identifierMetric("scene.contours.metric.c1_label", "DECAY"));
		data.metrics.push_back(identifierMetric("scene.contours.metric.c1_value", phc::decayLabel(decay_)));
		data.metrics.push_back(identifierMetric("scene.contours.metric.c1_pos", std::to_string(decay_ + 1) + "/" + std::to_string(phc::kSelectorSteps)));
		status.semantic = data;
	}
	status.paused = false;
	return status;
}

SceneCapabilities PhosphorContoursScene::capabilities() const {
	SceneCapabilities caps;
	SceneCommandDescriptor next;
	next.command = SceneCommand::NextMedia; next.label = "Next"; next.shortLabel = "NEXT";
	caps.commands.push_back(next);
	SceneCommandDescriptor prev;
	prev.command = SceneCommand::PreviousMedia; prev.label = "Previous"; prev.shortLabel = "PREV";
	caps.commands.push_back(prev);
	SceneCommandDescriptor mode;
	mode.command = SceneCommand::NextMode; mode.label = "Next mode"; mode.shortLabel = "NEXT MODE";
	caps.commands.push_back(mode);
	SceneCommandDescriptor regen;
	regen.command = SceneCommand::Regenerate; regen.label = "Reseed"; regen.shortLabel = "RESEED";
	caps.commands.push_back(regen);
	for (auto [id, label] : {std::pair<const char*, const char*>{"density", "DENSITY"}, {"decay", "DECAY"}}) {
		SceneOptionAxis a;
		a.id = id; a.label = label; a.count = phc::kSelectorSteps;
		caps.optionAxes.push_back(a);
	}
	return caps; // Reset honored but not advertised
}

bool PhosphorContoursScene::executeCommand(SceneCommand command) {
	if (!didSetup_ || didShutdown_) return false;
	switch (command) {
		case SceneCommand::NextMedia: return video_.next();
		case SceneCommand::PreviousMedia: return video_.previous();
		case SceneCommand::NextMode:
			presetIdx_ = (presetIdx_ + 1) % phc::kPresetCount;
			refreshLook();
			clearHistory();
			return true;
		case SceneCommand::Regenerate:
			seed_++;
			clearHistory();
			return true;
		case SceneCommand::Reset:
			reset();
			return true;
		default:
			return false;
	}
}

bool PhosphorContoursScene::stepOption(int axisIndex, int delta) {
	if (!didSetup_ || didShutdown_ || (delta != 1 && delta != -1)) return false;
	if (axisIndex == 0) density_ = wrapStep(density_, delta);
	else if (axisIndex == 1) decay_ = wrapStep(decay_, delta);
	else return false;
	refreshLook(); // history is kept: the change should read as a live retune, not a cut
	return true;
}

void PhosphorContoursScene::reset() {
	presetIdx_ = 0;
	density_ = 1; decay_ = 1;
	refreshLook();
	seed_ = 1;
	time_ = 0.f;
	clearHistory();
}

void PhosphorContoursScene::shutdown() {
	active_ = false;
	if (didSetup_ && !didShutdown_) pipe_.release(); // permanent release
	didShutdown_ = true;
}
