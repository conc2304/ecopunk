#pragma once

#include "ExperienceRuntime.h"

#include <cstdint>
#include <string>
#include <vector>

// SceneSwitchHarness — RT-003 mechanism verification (NOT the RT-002 20-cycle
// acceptance). Drives the real ExperienceRuntime with BOTH real production
// scenes registered (BlobProductionScene + TemporalProductionScene, startup
// Blob) through kCycleCount complete Blob -> Temporal -> Blob cycles, issuing
// real RuntimeCommand::NextScene / PreviousScene through the production
// InputRouter path (runtime.keyPressed(']') / ('[')).
//
// Asserts, every presented frame:
//   - exactly one HudFrameData assembly and one production HUD draw
//   - SceneHudStatus.sceneId == SceneManagerStatus.activeSceneId (never mixed)
//   - HudFrameData.effects present  <=>  Temporal is the owner of record
//   - capabilities published only while Idle/FadingIn
//   - any semantic payload belongs to the active scene (ID prefix)
// plus per-phase checks: outgoing never updated/drawn/pulled after
// deactivation, retained static frame unchanged (pixel hash + texture id),
// Loading publishes no identity, capability queries == successful
// activations, setup at most once per scene, switch suppression during
// FadingOut/Loading/FadingIn, runtime FBO reallocation on a genuine size
// difference, GL baseline after draw, clean shutdown of both scenes.
//
// Activated by EXPERIENCE_RUNTIME_SWITCH_HARNESS; exits the process when done.
class SceneSwitchHarness {
public:
	static bool isRequested(); // EXPERIENCE_RUNTIME_SWITCH_HARNESS

	explicit SceneSwitchHarness(ExperienceRuntime& runtime);

	void step(float dt);

private:
	enum class Phase {
		Warmup,     // live Blob frames from the startup activation
		Switch,     // issue NextScene/PreviousScene for the current leg
		Transition, // frames until the manager returns to Idle
		Dwell,      // live frames on the incoming scene
		Shutdown,
		Done
	};

	void logResult(const std::string& checkName, bool passed, const std::string& detail = "");
	void runOneFrame(float dt);
	void checkFrameInvariants();
	void checkGlBaseline(const std::string& context);
	void issueSwitch();
	void onTransitionFrame();
	void onFirstIncomingFrame();
	void finishLeg();
	void captureScreenshot(const std::string& label);
	uint64_t sceneFboHash();
	static std::string phaseName(SceneTransitionPhase phase);
	std::string legLabel() const;
	void logCounters(const std::string& label);
	long long netLiveAllocations() const;
	uint64_t residentBytes() const;

	ExperienceRuntime& runtime_;
	Phase phase_ = Phase::Warmup;
	int framesInPhase_ = 0;
	int leg_ = 0; // even: Blob -> Temporal (NextScene); odd: Temporal -> Blob (PreviousScene)

	std::string outgoingId_;
	std::string incomingId_;
	SceneManager::EntryCounters outgoingBefore_;
	SceneManager::EntryCounters incomingBefore_;
	ExperienceRuntime::PresentationCounters presentationBefore_;
	uint64_t frozenHash_ = 0;
	unsigned int frozenTextureId_ = 0;
	unsigned int forcedTextureId_ = 0;
	bool forcedDifferentSize_ = false;

	int fadeOutFrames_ = 0;
	int loadingFrames_ = 0;
	int fadeInFrames_ = 0;
	bool sawFirstIncoming_ = false;
	std::vector<std::string> phaseTrace_;

	uint64_t lastFrameNumber_ = 0;
	uint64_t lastHudDraws_ = 0;
	uint64_t lastAssemblies_ = 0;
	SceneManager::EntryCounters lastActiveCounters_;
	std::string lastActiveId_;

	std::vector<long long> netAllocationsAtBlobReturn_;
	std::vector<uint64_t> residentAtBlobReturn_;

	int failureCount_ = 0;
	int totalChecks_ = 0;

	static constexpr int kCycleCount = 3;      // RT-003 minimum; the 20-cycle proof is RT-002
	static constexpr int kWarmupFrames = 60;
	static constexpr int kDwellFrames = 60;    // >= 60 active frames per activation
	static constexpr int kDifferentSizeLeg = 4; // cycle 3, Blob -> Temporal
};
