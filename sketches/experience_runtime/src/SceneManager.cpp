#include "SceneManager.h"

#include "ofLog.h"

SceneHudStatus SceneManager::fallbackStatus() {
	SceneHudStatus s;
	s.schemaVersion = 1;
	s.sceneId = "";
	s.displayName = "(no active scene)";
	s.health = SceneHealth::Loading;
	s.message = "SceneManager: no scene has been activated yet";
	return s;
}

void SceneManager::setup(const SceneServices& services) {
	fakeScene_.setup(services);
	didSetup_ = true;
}

void SceneManager::activateScene() {
	fakeScene_.activate();
	// Cache capabilities exactly once, here, on activation — never
	// re-queried per frame. See class header comment, discipline #1.
	cachedCapabilities_ = fakeScene_.capabilities();
	hasBeenActivated_ = true;
}

void SceneManager::updateActiveScene(float dt) {
	fakeScene_.update(dt);
}

void SceneManager::captureSceneStatus() {
	// The single, atomic, once-per-frame status pull. See class header
	// comment, discipline #2. Must be called exactly once per completed
	// runtime frame and never again before the next update().
	cachedStatus_ = fakeScene_.hudStatus();
}

void SceneManager::captureEffectActivityStatus() {
	// Separate pull from captureSceneStatus() above — the task's own
	// instruction: canonical effect activity must not be derived from
	// SceneHudStatus::activeEffects.
	cachedEffectActivityStatus_ = fakeScene_.currentEffectActivityStatus();
}

void SceneManager::drawActiveScene() {
	fakeScene_.drawToCurrentTarget();
}

void SceneManager::deactivateScene() {
	fakeScene_.deactivate();
}

void SceneManager::shutdown() {
	fakeScene_.shutdown();
}

bool SceneManager::dispatchSceneCommand(SceneCommand command) {
	return fakeScene_.executeCommand(command);
}

void SceneManager::handleSceneSwitchCommand(RuntimeCommand command) {
	if (command != RuntimeCommand::NextScene && command != RuntimeCommand::PreviousScene) {
		ofLogWarning("SceneManager") << "handleSceneSwitchCommand() called with a non-switch "
			"RuntimeCommand — see ExperienceRuntime::handleRuntimeCommand for the intended split";
		return;
	}
	// Increment 1 has exactly one resident scene — real scene switching
	// (SceneTransitionPhase progression, load/fade, capability discovery
	// on the incoming scene) is Stage B scope, explicitly not built here.
	ofLogNotice("SceneManager") << "scene-switch command received ("
		<< (command == RuntimeCommand::NextScene ? "NextScene" : "PreviousScene")
		<< ") — not implemented in this increment; only one scene (\""
		<< fakeScene_.sceneId() << "\") is resident";
}

SceneManagerStatus SceneManager::status() const {
	SceneManagerStatus s;
	s.activeSceneId = hasBeenActivated_ ? fakeScene_.sceneId() : std::string();
	s.pendingSceneId = std::nullopt;
	s.transitionPhase = SceneTransitionPhase::Idle;
	s.transitionProgress = 0.0f;
	s.message = std::nullopt;
	return s;
}

glm::ivec2 SceneManager::activeSceneNativeRenderSize() const {
	return fakeScene_.nativeRenderSize();
}
