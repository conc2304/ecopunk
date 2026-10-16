#pragma once

#include "ExperienceRuntime.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// ThreeSceneCircuitHarness — RT-005 three-scene switching proof. Default
// roster [Blob, Temporal, Blueprint], startup Blob, real InputRouter keys.
//
// Ten complete circuits, alternating direction (odd = ']' NextScene, even =
// '[' PreviousScene), 30 real switches in all:
//
//   forward:  Blob -> Temporal -> Blueprint -> Blob
//   backward: Blob -> Blueprint -> Temporal -> Blob
//
// Per switch: a repeat/opposite scene key and a scene command are injected
// mid-transition and must be rejected (landing scene = ring target, exactly
// one activation of it, none of anything else). On landing: exactly one
// capabilities() query for that activation, capabilities/effects/semantic
// state owned by the incoming scene, activation-relative timing, scene FBO
// differs from the frozen outgoing frame, no GL errors. Blueprint landings
// must start a fresh composition cycle. Even circuits change media through
// canonical NextMedia on every leg; Blueprint legs of circuits 1/4/7 RESEED.
// Each circuit end samples RSS, the scene FBO texture/allocation counters.
// Captures (window + scene-only) of each scene and of transitions into and
// out of Blueprint under bin/data/captures/rt005-three-scene/.
//
// Opt-in: EXPERIENCE_RUNTIME_THREE_SCENE_HARNESS=1. Exits 0 only if every check passed.
class ThreeSceneCircuitHarness {
public:
	// EXPERIENCE_RUNTIME_THREE_SCENE_HARNESS (three-scene ring, RT-005) or
	// EXPERIENCE_RUNTIME_FULL_RING_HARNESS (the full five-scene ring).
	static bool isRequested();
	static bool isFullRingRequested();
	explicit ThreeSceneCircuitHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	enum class State { Startup, Dwell, Transition, Done };

	struct Leg {
		int circuit = 0;   // 1-based
		int index = 0;     // 0..2 within the circuit
		int key = ']';
		std::string fromId;
		std::string toId;
	};

	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void captureWindow(const std::string& name);
	void captureScene(const std::string& name);
	bool readScene(ofPixels& out) const;
	static float meanAbsDiff(const ofPixels& a, const ofPixels& b);
	static float meanLuma(const ofPixels& px);
	int drainGlErrors();
	void checkIdleInvariants();
	void beginLeg();
	void onTransitionFrame();
	void onLanded();
	void onDwellFrame();
	void endOfCircuit();
	void finish();
	std::string legLabel() const;

	ExperienceRuntime& runtime_;
	bool fullRing_ = false;
	std::string captureDir_;
	int followerLagFrames_ = 0;
	int fragmentFollowerFramesChecked_ = 0;
	int quadrantMediaLagFrames_ = 0;
	int quadrantFramesChecked_ = 0;
	uint64_t lastQuadrantGeneration_ = 0;
	bool quadrantExpansionTriggered_ = false;
	bool quadrantExpansionChecked_ = false;
	bool quadrantReticlesShown_ = false;
	int glStateFramesChecked_ = 0;
	int glStateLeaks_ = 0;
	float expansionTriggerSeconds_ = 0.0f;
	std::vector<std::string> ring_;
	std::vector<Leg> legs_;
	size_t leg_ = 0;
	State state_ = State::Startup;
	int framesInState_ = 0;
	float secondsInState_ = 0.0f;
	float maxFrameSeconds_ = 0.0f;
	bool capturedThisFrame_ = false; // PNG capture cost lands in the next frame's dt
	bool excludeNextDt_ = false;
	int captureFramesExcluded_ = 0;

	// Per-switch bookkeeping.
	std::map<std::string, SceneManager::EntryCounters> countersAtRequest_;
	ofPixels outgoingFrame_;
	bool rejectAttempted_ = false;
	bool fadeInRejectAttempted_ = false;
	SceneTransitionPhase lastPhase_ = SceneTransitionPhase::Idle;
	int framesInPhase_ = 0;
	std::string mediaBeforeNext_;
	bool nextMediaPressed_ = false;
	bool nextMediaChecked_ = false;
	bool reseedPressed_ = false;
	bool reseedChecked_ = false;
	int seedBeforeReseed_ = 0;
	int blueprintExitSeed_ = 0;
	bool haveBlueprintExitSeed_ = false;
	bool capturedThisLeg_ = false;

	// Circuit-level resource observations.
	std::vector<double> rssMiBPerCircuit_;
	std::vector<uint64_t> fboAllocsPerCircuit_;
	std::vector<unsigned int> fboTexPerCircuit_;
	std::vector<size_t> legFrameMaxMs_;

	int invariantFailures_ = 0;
	int idleFramesChecked_ = 0;
	int glErrors_ = 0;
	int rejectedSwitchInputs_ = 0;
	int rejectedSceneCommands_ = 0;
	int mediaChanges_ = 0;
	int reseeds_ = 0;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
