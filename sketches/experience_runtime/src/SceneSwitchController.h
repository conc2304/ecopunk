#pragma once

// SceneSwitchController — RT-003 (Blob + Temporal in-process SceneManager
// switching). The pure, openFrameworks-free transition/ownership state
// machine SceneManager delegates every scene-switch decision to — same
// split as FakeSceneLifecycleState.h / FakeScene.cpp: this header owns the
// logic, SceneManager owns the real IEcopunkScene calls (through
// LifecycleTarget below), and test/scene_switch_controller_tests.cpp
// exercises this exact production code with a bare compiler (no OF, no GL,
// no SceneContract.h — that header pulls in ofTexture.h).
//
// Model (Scene-HUD-Contract-v1.md §6/§10; Architecture RT-003 authorization
// for the Blob + Temporal pair only):
//
//   Idle ──request──▶ FadingOut ──N frames──▶ Loading ──1 frame──▶ FadingIn ──M frames──▶ Idle
//                         │                      │ (setup/activation throws)
//                         └──────────────────────┴──────────────▶ Failed
//
//   request accepted (between frames, Idle only):
//       outgoing.deactivate() immediately — the outgoing scene is never
//       updated/drawn live again for the transition. The owner of record
//       stays = outgoing, so FadingOut publishes the frozen outgoing
//       aggregate (HUD acceptance matrix: "Blob remains current").
//   FadingOut frames:  no scene update/draw/status pull; FrozenOutgoing publication
//   Loading frame:     ownership released (no owner); incoming setup() iff never set up;
//                      Neutral publication (neither outgoing nor incoming identity)
//   next frame:        incoming.activate(); on success incoming becomes owner, the
//                      caller queries + caches capabilities exactly once, and FadingIn
//                      starts with this frame (live incoming update/status/draw)
//   FadingIn frames:   live incoming; returns to Idle after M frames
//
// The frozen IEcopunkScene::setup()/activate() return void, so the ONLY
// observable setup/activation failure without a contract change is an
// exception escaping the call. Failed then means: no owner, pending = the
// scene that failed, Neutral publication, every further switch request
// rejected (phase is non-Idle). No recovery policy is defined here — see
// the RT-003 completion report.

#include <exception>
#include <string>
#include <vector>

namespace sceneswitch {

// Mirrors SceneContract.h's SceneTransitionPhase one-to-one (SceneManager
// maps between them); duplicated only because that header is not OF-free.
enum class Phase {
	Idle,
	FadingOut,
	Loading,
	FadingIn,
	Failed
};

enum class Direction {
	Next,
	Previous
};

enum class RequestResult {
	Accepted,
	RejectedTransitionActive, // phase != Idle (includes Failed) — suppressed, never queued
	RejectedNoOtherScene,     // registry has fewer than two entries
	RejectedNoOwner           // no active owner to switch away from
};

// What the caller must publish into this frame's HudFrameData scene-side
// fields (status / effects / capabilities).
enum class Publication {
	Live,           // pull from the active owner as normal
	FrozenOutgoing, // keep the last values pulled from the outgoing owner; pull nothing
	Neutral         // no scene identity: transition status, effects = nullopt, no capabilities
};

struct FramePlan {
	Publication publication = Publication::Live;
	bool runActiveScene = true;                // update + status pull + draw this frame
	bool activationSucceededThisFrame = false; // caller queries + caches capabilities now
	bool ownershipReleasedThisFrame = false;   // caller drops outgoing status/effects now
	bool failedThisFrame = false;
};

class LifecycleTarget {
public:
	virtual ~LifecycleTarget() = default;
	virtual void setupEntry(int index) = 0;
	virtual void activateEntry(int index) = 0;
	virtual void deactivateEntry(int index) = 0;
};

struct Config {
	int fadeOutFrames = 12;
	int fadeInFrames = 12;
};

class Controller {
public:
	void configure(int entryCount, Config config) {
		entryCount_ = entryCount < 0 ? 0 : entryCount;
		config_ = config;
		if (config_.fadeOutFrames < 1) config_.fadeOutFrames = 1;
		if (config_.fadeInFrames < 1) config_.fadeInFrames = 1;
		didSetup_.assign(static_cast<size_t>(entryCount_), false);
		phase_ = Phase::Idle;
		owner_ = -1;
		pending_ = -1;
		framesInPhase_ = 0;
		progress_ = 0.0f;
		failureMessage_.clear();
	}

	// Startup ownership — the startup scene's setup()/activate() are performed
	// by SceneManager::setup()/activateScene() (the unchanged startup path);
	// the controller only records the results.
	void setOwner(int index) { owner_ = validIndex(index) ? index : -1; }
	void markSetupDone(int index) {
		if (validIndex(index)) didSetup_[static_cast<size_t>(index)] = true;
	}

	// Ring traversal: Next = +1, Previous = -1, wrapping. For the two-entry
	// Blob + Temporal registry both directions reach the other scene.
	int targetIndexFor(Direction direction) const {
		if (entryCount_ < 2 || owner_ < 0) return -1;
		int step = (direction == Direction::Next) ? 1 : -1;
		return ((owner_ + step) % entryCount_ + entryCount_) % entryCount_;
	}

	RequestResult requestSwitch(Direction direction, LifecycleTarget& target) {
		if (phase_ != Phase::Idle) return RequestResult::RejectedTransitionActive;
		if (entryCount_ < 2) return RequestResult::RejectedNoOtherScene;
		if (owner_ < 0) return RequestResult::RejectedNoOwner;

		pending_ = targetIndexFor(direction);
		phase_ = Phase::FadingOut;
		framesInPhase_ = 0;
		progress_ = 0.0f;
		failureMessage_.clear();
		try {
			target.deactivateEntry(owner_);
		} catch (const std::exception& e) {
			fail(std::string("outgoing deactivate() threw: ") + e.what());
		} catch (...) {
			fail("outgoing deactivate() threw a non-std exception");
		}
		return RequestResult::Accepted;
	}

	// Call exactly once at the start of every runtime frame, before any scene
	// update. Advances the transition and performs the lifecycle calls that
	// belong to this frame.
	FramePlan beginFrame(LifecycleTarget& target) {
		FramePlan plan;
		switch (phase_) {
			case Phase::Idle:
				plan.publication = Publication::Live;
				plan.runActiveScene = owner_ >= 0;
				return plan;

			case Phase::FadingOut:
				framesInPhase_++;
				if (framesInPhase_ <= config_.fadeOutFrames) {
					progress_ = static_cast<float>(framesInPhase_) / static_cast<float>(config_.fadeOutFrames);
					plan.publication = Publication::FrozenOutgoing;
					plan.runActiveScene = false;
					return plan;
				}
				return enterLoading(target);

			case Phase::Loading:
				return activatePending(target);

			case Phase::FadingIn:
				if (framesInPhase_ >= config_.fadeInFrames) {
					phase_ = Phase::Idle;
					framesInPhase_ = 0;
					progress_ = 0.0f;
				} else {
					framesInPhase_++;
					progress_ = static_cast<float>(framesInPhase_) / static_cast<float>(config_.fadeInFrames);
				}
				plan.publication = Publication::Live;
				plan.runActiveScene = true;
				return plan;

			case Phase::Failed:
			default:
				plan.publication = Publication::Neutral;
				plan.runActiveScene = false;
				return plan;
		}
	}

	Phase phase() const { return phase_; }
	float progress() const { return progress_; } // per-phase, 0..1
	int ownerIndex() const { return owner_; }     // -1 = no owner (Loading / Failed)
	int pendingIndex() const { return pending_; } // -1 = none
	bool didSetup(int index) const { return validIndex(index) && didSetup_[static_cast<size_t>(index)]; }
	const std::string& failureMessage() const { return failureMessage_; }
	int entryCount() const { return entryCount_; }
	const Config& config() const { return config_; }

private:
	bool validIndex(int index) const { return index >= 0 && index < entryCount_; }

	FramePlan enterLoading(LifecycleTarget& target) {
		FramePlan plan;
		phase_ = Phase::Loading;
		framesInPhase_ = 1;
		progress_ = 1.0f;
		owner_ = -1; // ownership released: neither outgoing nor incoming is current
		plan.publication = Publication::Neutral;
		plan.runActiveScene = false;
		plan.ownershipReleasedThisFrame = true;
		if (!didSetup(pending_)) {
			try {
				target.setupEntry(pending_);
				didSetup_[static_cast<size_t>(pending_)] = true;
			} catch (const std::exception& e) {
				fail(std::string("incoming setup() threw: ") + e.what());
				plan.failedThisFrame = true;
			} catch (...) {
				fail("incoming setup() threw a non-std exception");
				plan.failedThisFrame = true;
			}
		}
		return plan;
	}

	FramePlan activatePending(LifecycleTarget& target) {
		FramePlan plan;
		int incoming = pending_;
		std::string error;
		bool ok = true;
		try {
			target.activateEntry(incoming);
		} catch (const std::exception& e) {
			ok = false;
			error = std::string("incoming activate() threw: ") + e.what();
		} catch (...) {
			ok = false;
			error = "incoming activate() threw a non-std exception";
		}
		if (!ok) {
			// Best effort: never leave a partially activated incoming scene
			// running unpublished.
			try {
				target.deactivateEntry(incoming);
			} catch (...) {
			}
			fail(error);
			plan.publication = Publication::Neutral;
			plan.runActiveScene = false;
			plan.failedThisFrame = true;
			return plan;
		}
		owner_ = incoming;
		pending_ = -1;
		phase_ = Phase::FadingIn;
		framesInPhase_ = 1;
		progress_ = 1.0f / static_cast<float>(config_.fadeInFrames);
		plan.publication = Publication::Live;
		plan.runActiveScene = true;
		plan.activationSucceededThisFrame = true;
		return plan;
	}

	void fail(const std::string& message) {
		phase_ = Phase::Failed;
		owner_ = -1;
		framesInPhase_ = 0;
		progress_ = 0.0f;
		failureMessage_ = message;
	}

	int entryCount_ = 0;
	Config config_;
	std::vector<bool> didSetup_;
	Phase phase_ = Phase::Idle;
	int owner_ = -1;
	int pending_ = -1;
	int framesInPhase_ = 0;
	float progress_ = 0.0f;
	std::string failureMessage_;
};

} // namespace sceneswitch
