#include "SceneManager.h"

#include "ofLog.h"

namespace {

SceneTransitionPhase toContractPhase(sceneswitch::Phase phase) {
	switch (phase) {
		case sceneswitch::Phase::Idle:      return SceneTransitionPhase::Idle;
		case sceneswitch::Phase::FadingOut: return SceneTransitionPhase::FadingOut;
		case sceneswitch::Phase::Loading:   return SceneTransitionPhase::Loading;
		case sceneswitch::Phase::FadingIn:  return SceneTransitionPhase::FadingIn;
		case sceneswitch::Phase::Failed:    return SceneTransitionPhase::Failed;
	}
	return SceneTransitionPhase::Failed;
}

const char* requestResultName(sceneswitch::RequestResult r) {
	switch (r) {
		case sceneswitch::RequestResult::Accepted:                 return "accepted";
		case sceneswitch::RequestResult::RejectedTransitionActive: return "rejected (transition in progress)";
		case sceneswitch::RequestResult::RejectedNoOtherScene:     return "rejected (no other registered scene)";
		case sceneswitch::RequestResult::RejectedNoOwner:          return "rejected (no active scene owner)";
	}
	return "rejected";
}

} // namespace

SceneHudStatus SceneManager::fallbackStatus() {
	SceneHudStatus s;
	s.schemaVersion = 1;
	s.sceneId = "";
	s.displayName = "(no active scene)";
	s.health = SceneHealth::Loading;
	s.message = "SceneManager: no scene has been activated yet";
	return s;
}

// Published on Loading/Failed frames: no outgoing and no incoming identity,
// no semantic payload — never a fabricated incoming steady state.
SceneHudStatus SceneManager::transitionStatus(const std::string& message) {
	SceneHudStatus s;
	s.schemaVersion = 1;
	s.sceneId = "";
	s.displayName = "";
	s.health = SceneHealth::Loading;
	s.message = message;
	return s;
}

// -- Registry ----------------------------------------------------------------

void SceneManager::registerProductionScene(IEcopunkScene* scene, EffectActivitySource effectSource) {
	if (scene == nullptr) {
		ofLogWarning("SceneManager") << "registerProductionScene(nullptr) ignored";
		return;
	}
	if (didSetup_) {
		ofLogWarning("SceneManager") << "registerProductionScene() after setup() ignored for \"" << scene->sceneId() << "\"";
		return;
	}
	for (const Entry& e : entries_) {
		if (e.scene == scene || e.scene->sceneId() == scene->sceneId()) {
			ofLogWarning("SceneManager") << "registerProductionScene(): \"" << scene->sceneId() << "\" already registered";
			return;
		}
	}
	Entry entry;
	entry.scene = scene;
	entry.effectSource = std::move(effectSource);
	entries_.push_back(std::move(entry));
}

bool SceneManager::setStartupScene(const std::string& sceneId) {
	for (size_t i = 0; i < entries_.size(); ++i) {
		if (entries_[i].scene->sceneId() == sceneId) {
			startupIndex_ = static_cast<int>(i);
			return true;
		}
	}
	ofLogWarning("SceneManager") << "setStartupScene(\"" << sceneId << "\"): not registered; startup unchanged";
	return false;
}

std::vector<std::string> SceneManager::registeredSceneIdsForTesting() const {
	std::vector<std::string> ids;
	for (const Entry& e : entries_) ids.push_back(e.scene->sceneId());
	return ids;
}

const SceneManager::EntryCounters* SceneManager::countersForTesting(const std::string& sceneId) const {
	for (const Entry& e : entries_) {
		if (e.scene->sceneId() == sceneId) return &e.counters;
	}
	return nullptr;
}

SceneManager::Entry* SceneManager::ownerEntry() {
	int owner = controller_.ownerIndex();
	return owner >= 0 ? &entries_[static_cast<size_t>(owner)] : nullptr;
}

const SceneManager::Entry* SceneManager::ownerEntry() const {
	int owner = controller_.ownerIndex();
	return owner >= 0 ? &entries_[static_cast<size_t>(owner)] : nullptr;
}

std::string SceneManager::sceneIdAt(int index) const {
	if (index < 0 || index >= static_cast<int>(entries_.size())) return std::string();
	return entries_[static_cast<size_t>(index)].scene->sceneId();
}

// -- Real IEcopunkScene lifecycle calls (sequenced by controller_) ------------

void SceneManager::setupEntry(int index) {
	Entry& e = entries_[static_cast<size_t>(index)];
	// Scene-HUD-Contract-v1.md §12: sceneAssetRoot = assets/scenes/<sceneId>/ —
	// derived per entry from the scene's own stable ID, not a scene branch.
	SceneServices services = baseServices_;
	services.sceneAssetRoot = "assets/scenes/" + e.scene->sceneId() + "/";
	e.counters.setupCalls++;
	e.scene->setup(services);
}

void SceneManager::activateEntry(int index) {
	Entry& e = entries_[static_cast<size_t>(index)];
	e.counters.activateCalls++;
	e.scene->activate(); // an exception here leaves the entry inactive (controller handles failure)
	e.lifecycleActive = true;
	e.counters.successfulActivations++;
}

void SceneManager::deactivateEntry(int index) {
	Entry& e = entries_[static_cast<size_t>(index)];
	if (!e.lifecycleActive) return; // never double-deactivate
	e.lifecycleActive = false;
	e.counters.deactivateCalls++;
	e.scene->deactivate();
}

void SceneManager::queryAndCacheCapabilities(int index) {
	Entry& e = entries_[static_cast<size_t>(index)];
	cachedCapabilities_ = e.scene->capabilities();
	e.counters.capabilityQueries++;
}

// -- Startup -----------------------------------------------------------------

void SceneManager::setup(const SceneServices& services) {
	baseServices_ = services;
	if (entries_.empty()) {
		SceneServices fakeServices = services;
		fakeServices.sceneAssetRoot = "assets/scenes/" + fakeScene_.sceneId() + "/";
		fakeScene_.setup(fakeServices);
		didSetup_ = true;
		return;
	}
	controller_.configure(static_cast<int>(entries_.size()), sceneswitch::Config{kFadeOutFrames, kFadeInFrames});
	controller_.setOwner(startupIndex_);
	setupEntry(startupIndex_);
	controller_.markSetupDone(startupIndex_);
	didSetup_ = true;
}

void SceneManager::activateScene() {
	if (entries_.empty()) {
		fakeScene_.activate();
		cachedCapabilities_ = fakeScene_.capabilities();
		hasBeenActivated_ = true;
		return;
	}
	if (controller_.phase() != sceneswitch::Phase::Idle) {
		ofLogWarning("SceneManager") << "activateScene() ignored: scene transition in progress";
		return;
	}
	int owner = controller_.ownerIndex();
	if (owner < 0) return;
	activateEntry(owner);
	queryAndCacheCapabilities(owner); // exactly once per activation
	hasBeenActivated_ = true;
}

// -- Per-frame ---------------------------------------------------------------

bool SceneManager::beginFrame() {
	lastActivationSucceeded_ = false;
	if (entries_.empty()) {
		lastPlanLive_ = true;
		lastPublication_ = sceneswitch::Publication::Live;
		return true;
	}
	if (didShutdown_) {
		lastPlanLive_ = false;
		return false;
	}

	sceneswitch::FramePlan plan = controller_.beginFrame(target_);
	lastPublication_ = plan.publication;
	lastPlanLive_ = plan.runActiveScene;

	if (plan.publication == sceneswitch::Publication::Neutral) {
		// Ownership released (Loading) or failed: no outgoing or incoming
		// identity, capabilities, or effects may be published.
		std::string message;
		if (controller_.phase() == sceneswitch::Phase::Failed) {
			message = "SceneManager: transition to \"" + sceneIdAt(controller_.pendingIndex()) + "\" failed — "
				+ controller_.failureMessage();
			if (plan.failedThisFrame) {
				ofLogError("SceneManager") << message;
			}
		} else {
			message = "SceneManager: loading \"" + sceneIdAt(controller_.pendingIndex()) + "\"";
		}
		managerMessage_ = message;
		cachedStatus_ = transitionStatus(message);
		cachedCapabilities_ = SceneCapabilities{};
		cachedEffectActivityStatus_.reset();
		neutralStatusPublications_++;
	}

	if (plan.activationSucceededThisFrame) {
		int owner = controller_.ownerIndex();
		queryAndCacheCapabilities(owner); // exactly once for this activation
		hasBeenActivated_ = true;
		lastActivationSucceeded_ = true;
		managerMessage_ = "SceneManager: activated \"" + sceneIdAt(owner) + "\"";
		ofLogNotice("SceneManager") << "scene \"" << sceneIdAt(owner) << "\" is now the active owner";
	}

	return lastPlanLive_;
}

void SceneManager::updateActiveScene(float dt) {
	if (entries_.empty()) {
		fakeScene_.update(dt);
		return;
	}
	if (!lastPlanLive_) return;
	if (Entry* e = ownerEntry()) {
		e->counters.updateCalls++;
		e->scene->update(dt);
	}
}

void SceneManager::captureSceneStatus() {
	if (entries_.empty()) {
		cachedStatus_ = fakeScene_.hudStatus();
		return;
	}
	if (!lastPlanLive_) return; // transition-only frame: frozen/neutral status stays published
	if (Entry* e = ownerEntry()) {
		e->counters.statusPulls++;
		cachedStatus_ = e->scene->hudStatus();
	}
}

void SceneManager::captureEffectActivityStatus() {
	if (effectActivitySourceOverride_) {
		cachedEffectActivityStatus_ = effectActivitySourceOverride_();
		return;
	}
	if (entries_.empty()) {
		// Tooling-only path (GlRestorationHarness): no production registry.
		cachedEffectActivityStatus_ = fakeScene_.currentEffectActivityStatus();
		return;
	}
	switch (lastPublication_) {
		case sceneswitch::Publication::Live: {
			Entry* e = ownerEntry();
			if (e != nullptr && e->effectSource) {
				e->counters.effectPulls++;
				cachedEffectActivityStatus_ = e->effectSource();
			} else {
				// No approved canonical producer for this scene (Blob): the
				// honest value is "no snapshot" — never FakeScene's default.
				cachedEffectActivityStatus_.reset();
			}
			break;
		}
		case sceneswitch::Publication::FrozenOutgoing:
			break; // keep the outgoing owner's last pulled snapshot
		case sceneswitch::Publication::Neutral:
			cachedEffectActivityStatus_.reset();
			break;
	}
}

void SceneManager::drawActiveScene() {
	if (entries_.empty()) {
		fakeScene_.drawToCurrentTarget();
		return;
	}
	if (!lastPlanLive_) return;
	if (Entry* e = ownerEntry()) {
		e->counters.drawCalls++;
		e->scene->drawToCurrentTarget();
	}
}

void SceneManager::deactivateScene() {
	if (entries_.empty()) {
		fakeScene_.deactivate();
		return;
	}
	int owner = controller_.ownerIndex();
	if (owner >= 0) deactivateEntry(owner);
}

void SceneManager::shutdown() {
	if (entries_.empty()) {
		fakeScene_.shutdown();
		return;
	}
	for (size_t i = 0; i < entries_.size(); ++i) {
		Entry& e = entries_[i];
		if (!controller_.didSetup(static_cast<int>(i)) || e.didShutdown) continue;
		if (e.lifecycleActive) deactivateEntry(static_cast<int>(i));
		e.counters.shutdownCalls++;
		e.scene->shutdown();
		e.didShutdown = true;
	}
	didShutdown_ = true;
	cachedCapabilities_ = SceneCapabilities{};
}

bool SceneManager::dispatchSceneCommand(SceneCommand command) {
	if (entries_.empty()) {
		return fakeScene_.executeCommand(command);
	}
	if (didShutdown_) return false;
	sceneswitch::Phase phase = controller_.phase();
	if (phase != sceneswitch::Phase::Idle && phase != sceneswitch::Phase::FadingIn) return false;
	Entry* e = ownerEntry();
	return e != nullptr && e->scene->executeCommand(command);
}

sceneswitch::RequestResult SceneManager::handleSceneSwitchCommand(RuntimeCommand command) {
	if (command != RuntimeCommand::NextScene && command != RuntimeCommand::PreviousScene) {
		ofLogWarning("SceneManager") << "handleSceneSwitchCommand() called with a non-switch "
			"RuntimeCommand — see ExperienceRuntime::handleRuntimeCommand for the intended split";
		return sceneswitch::RequestResult::RejectedNoOtherScene;
	}
	const char* commandName = command == RuntimeCommand::NextScene ? "NextScene" : "PreviousScene";
	if (entries_.empty()) {
		ofLogNotice("SceneManager") << commandName << " ignored: no production scene registry (tooling FakeScene only)";
		return sceneswitch::RequestResult::RejectedNoOtherScene;
	}
	if (didShutdown_) {
		return sceneswitch::RequestResult::RejectedNoOwner;
	}

	sceneswitch::Direction direction =
		command == RuntimeCommand::NextScene ? sceneswitch::Direction::Next : sceneswitch::Direction::Previous;
	std::string outgoingId = sceneIdAt(controller_.ownerIndex());
	std::string incomingId = sceneIdAt(controller_.targetIndexFor(direction));

	sceneswitch::RequestResult result = controller_.requestSwitch(direction, target_);
	if (result == sceneswitch::RequestResult::Accepted) {
		// Outgoing controls stop being advertised at handoff start; the
		// outgoing status/effects stay frozen (FadingOut publication).
		cachedCapabilities_ = SceneCapabilities{};
		managerMessage_ = "SceneManager: " + std::string(commandName) + " \"" + outgoingId + "\" -> \"" + incomingId + "\"";
	}
	ofLogNotice("SceneManager") << commandName << " " << requestResultName(result)
								<< (result == sceneswitch::RequestResult::Accepted ? (": \"" + outgoingId + "\" -> \"" + incomingId + "\"") : std::string());
	return result;
}

SceneManagerStatus SceneManager::status() const {
	SceneManagerStatus s;
	if (entries_.empty()) {
		s.activeSceneId = hasBeenActivated_ ? fakeScene_.sceneId() : std::string();
		s.pendingSceneId = std::nullopt;
		s.transitionPhase = SceneTransitionPhase::Idle;
		s.transitionProgress = 0.0f;
		s.message = std::nullopt;
		return s;
	}
	const Entry* owner = ownerEntry();
	s.activeSceneId = (hasBeenActivated_ && owner != nullptr) ? owner->scene->sceneId() : std::string();
	if (controller_.pendingIndex() >= 0) {
		s.pendingSceneId = sceneIdAt(controller_.pendingIndex());
	}
	s.transitionPhase = toContractPhase(controller_.phase());
	s.transitionProgress = controller_.progress();
	if (controller_.phase() != sceneswitch::Phase::Idle) {
		s.message = managerMessage_;
	}
	return s;
}

glm::ivec2 SceneManager::activeSceneNativeRenderSize() const {
	if (entries_.empty()) {
		return fakeScene_.nativeRenderSize();
	}
	if (const Entry* e = ownerEntry()) {
		return e->scene->nativeRenderSize();
	}
	int pending = controller_.pendingIndex();
	if (pending >= 0) {
		return entries_[static_cast<size_t>(pending)].scene->nativeRenderSize();
	}
	return entries_[static_cast<size_t>(startupIndex_)].scene->nativeRenderSize();
}
