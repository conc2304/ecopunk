#include "CrtPhosphorScene.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace {
SceneMetric identifierMetric(const std::string& metricId, const std::string& valueId) {
	SceneMetric m;
	m.metricId = metricId;
	m.valueType = HudMetricValueType::Identifier;
	m.dataClass = HudDataClass::Literal;
	m.valueId = valueId;
	return m;
}
std::string positionText(const crt::Selection& s, crt::Axis a) {
	return std::to_string(s.position(a)) + "/" + std::to_string(s.count(a));
}
} // namespace

CrtPhosphorScene::CrtPhosphorScene(VideoPlaybackService& video)
	: video_(video) {
}

void CrtPhosphorScene::setup(const SceneServices& services) {
	(void)services; // consumes the canonical VideoPlaybackService directly, like the other scenes
	presets_ = crt::makePresets();
	// Startup selection by stable id (scene-owned state; dev/QA only, never a runtime input path).
	auto fromEnv = [this](const char* env, crt::Axis axis) {
		if (const char* v = std::getenv(env)) {
			if (!selection_.setById(axis, v)) ofLogWarning("CrtPhosphorScene") << env << "='" << v << "' is unknown or unavailable; keeping default";
		}
	};
	fromEnv("EXPERIENCE_RUNTIME_CRT_MODE", crt::Axis::Composition);
	fromEnv("EXPERIENCE_RUNTIME_CRT_STRUCTURE", crt::Axis::Structure);
	fromEnv("EXPERIENCE_RUNTIME_CRT_PALETTE", crt::Axis::Palette);
	pipe_.loadShaders(); // failure is reported via hudStatus(); the scene then passes video through untreated
	pipe_.allocate(kNativeWidth, kNativeHeight, 0);
	didSetup_ = true;
}

void CrtPhosphorScene::clearHistory() {
	if (pipe_.allocated()) pipe_.clearHistory();
	historyClears_++;
}

void CrtPhosphorScene::activate() {
	active_ = true;
	time_ = 0.f;
	lastMediaId_ = video_.status().mediaId;
	lastActivationCount_ = video_.activationCount();
	clearHistory(); // nothing from a previous visit may persist
}

void CrtPhosphorScene::deactivate() {
	active_ = false;
}

void CrtPhosphorScene::update(float dt) {
	if (!active_) return;
	dt_ = dt;
	time_ += dt;
	// Media identity is owned by the canonical service; we only observe it so
	// phosphor persistence never smears one clip into the next.
	const VideoPlaybackStatus st = video_.status();
	const uint64_t act = video_.activationCount();
	if (st.mediaId != lastMediaId_ || act != lastActivationCount_) {
		lastMediaId_ = st.mediaId;
		lastActivationCount_ = act;
		clearHistory();
	}
}

void CrtPhosphorScene::drawToCurrentTarget() {
	if (!didSetup_ || didShutdown_) return;
	const ofTexture* tex = video_.currentTexture();

	// Source into the pipeline's own target, cover-fit.
	ofFbo& src = pipe_.sourceFbo();
	src.begin();
	ofDisableAlphaBlending();
	ofSetColor(0);
	ofDrawRectangle(0, 0, pipe_.width(), pipe_.height());
	if (tex != nullptr && tex->isAllocated() && tex->getWidth() > 0 && tex->getHeight() > 0) {
		const float vw = tex->getWidth(), vh = tex->getHeight();
		const float s = std::max(pipe_.width() / vw, pipe_.height() / vh);
		const float dw = vw * s, dh = vh * s;
		ofSetColor(255);
		tex->draw((pipe_.width() - dw) * 0.5f, (pipe_.height() - dh) * 0.5f, dw, dh);
	}
	src.end();

	crt::Pipeline::Options opt; // bloom + persistence on
	opt.mode = selection_.mode();                    // the three axes drive the real pipeline
	opt.display.structure = selection_.structure();
	opt.display.cmap = selection_.colorMap();
	pipe_.process(presets_[presetIdx_], time_, dt_, opt);

	// Present into the runtime-owned scene target (already bound + cleared).
	ofSetColor(255);
	(pipe_.usable() ? pipe_.output() : src).draw(0, 0, kNativeWidth, kNativeHeight);
}

SceneHudStatus CrtPhosphorScene::hudStatus() const {
	SceneHudStatus status;
	status.schemaVersion = 1;
	status.sceneId = kSceneId;
	status.displayName = displayName();
	if (!didSetup_ || !active_) {
		status.health = SceneHealth::Loading;
	} else if (!pipe_.usable()) {
		status.health = SceneHealth::Degraded;
		status.message = std::string("CRT shaders failed; untreated video");
	} else {
		status.health = SceneHealth::Ready;
	}
	{
		VideoPlaybackStatus videoStatus = video_.status();
		if (videoStatus.fallbackDisplayTitle) status.mediaName = videoStatus.fallbackDisplayTitle;
		else if (videoStatus.titleId) status.mediaName = videoStatus.titleId;
	}
	// Composition axis as the scene's mode (existing SceneHudStatus.mode; index 0-based, count = implemented modes).
	SceneModeStatus mode;
	mode.modeId = selection_.id(crt::Axis::Composition);
	mode.displayName = selection_.label(crt::Axis::Composition);
	mode.index = selection_.index(crt::Axis::Composition);
	mode.count = selection_.count(crt::Axis::Composition);
	status.modeName = mode.displayName;
	status.mode = mode;
	// Semantic payload only once the scene is actually running (missing data -> HUD rows hide).
	if (didSetup_ && active_) {
		SceneSemanticData data;
		data.state.primaryStateId = "scene.crt.state.online";
		data.timing.activeSeconds = time_;
		data.metrics.push_back(identifierMetric("scene.crt.metric.structure", std::string("scene.crt.metric.structure.") + selection_.id(crt::Axis::Structure)));
		data.metrics.push_back(identifierMetric("scene.crt.metric.structure_pos", positionText(selection_, crt::Axis::Structure)));
		data.metrics.push_back(identifierMetric("scene.crt.metric.palette", std::string("scene.crt.metric.palette.") + selection_.id(crt::Axis::Palette)));
		data.metrics.push_back(identifierMetric("scene.crt.metric.palette_pos", positionText(selection_, crt::Axis::Palette)));
		status.semantic = data;
	}
	status.paused = false;
	return status;
}

SceneCapabilities CrtPhosphorScene::capabilities() const {
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
	for (auto [id, axis] : {std::pair<const char*, crt::Axis>{"pixels", crt::Axis::Structure}, {"palette", crt::Axis::Palette}}) {
		SceneOptionAxis a;
		a.id = id; a.label = id; a.count = selection_.count(axis);
		caps.optionAxes.push_back(a);
	}
	return caps; // Reset honored but not advertised; Regenerate unsupported
}

bool CrtPhosphorScene::executeCommand(SceneCommand command) {
	if (!didSetup_ || didShutdown_) return false;
	switch (command) {
		case SceneCommand::NextMedia: return video_.next();
		case SceneCommand::PreviousMedia: return video_.previous();
		case SceneCommand::NextMode:
			selection_.step(crt::Axis::Composition, +1); // composition axis only; wraps; forward-only (no PreviousMode command exists)
			return true;
		case SceneCommand::Reset:
			reset();
			return true;
		case SceneCommand::Regenerate:
		default:
			return false;
	}
}

bool CrtPhosphorScene::stepOption(int axisIndex, int delta) {
	if (!didSetup_ || didShutdown_ || (delta != 1 && delta != -1)) return false;
	if (axisIndex == 0) selection_.step(crt::Axis::Structure, delta);
	else if (axisIndex == 1) selection_.step(crt::Axis::Palette, delta);
	else return false;
	return true;
}

void CrtPhosphorScene::reset() {
	presetIdx_ = 0;
	selection_.reset(); // Reset returns all three axes to the reference defaults
	time_ = 0.f;
	clearHistory();
}

void CrtPhosphorScene::shutdown() {
	active_ = false;
	didShutdown_ = true;
}
