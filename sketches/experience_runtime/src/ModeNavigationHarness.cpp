#include "ModeNavigationHarness.h"

#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

const std::string kBlobId = BlobProductionScene::kSceneId;
const std::string kTemporalId = TemporalProductionScene::kSceneId;
const std::string kBlueprintId = BlueprintProductionScene::kSceneId;
const std::string kFragmentId = FragmentProductionScene::kSceneId;
const std::string kQuadrantId = QuadrantProductionScene::kSceneId;
constexpr const char* kCaptureDir = "captures/mode-navigation";
constexpr const char* kModePanel = "flexible.secondary_a";
constexpr float kStepTimeoutSeconds = 45.0f;
constexpr int kSettleFrames = 90;

} // namespace

bool ModeNavigationHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_MODE_HARNESS") != nullptr;
}

ModeNavigationHarness::ModeNavigationHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	const auto ids = runtime_.sceneManagerForTesting().registeredSceneIdsForTesting();
	check("full roster [Blob, Temporal, Blueprint, Fragment, Quadrant], startup Blob",
		ids.size() == 5 && ids[0] == kBlobId && ids[1] == kTemporalId && ids[2] == kBlueprintId
			&& ids[3] == kFragmentId && ids[4] == kQuadrantId);
	buildScript();
}

void ModeNavigationHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void ModeNavigationHarness::note(const std::string& line) {
	ofLogNotice("ModeNavigationHarness") << line;
	lines_.push_back(line);
}

void ModeNavigationHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + ".png");
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + ".png (" + ofToString(img.getWidth()) + "x"
		+ ofToString(img.getHeight()) + ", scene " + position() + ", mode " + modeId() + " "
		+ ofToString(modeIndex() + 1) + "/" + ofToString(modeCount()) + ", focus=" + focused() + ")");
}

bool ModeNavigationHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId
		&& f.scene.sceneId == sceneId;
}

int ModeNavigationHarness::modeIndex() const {
	const auto& m = runtime_.currentHudFrameData().scene.mode;
	return m ? m->index : -1;
}

int ModeNavigationHarness::modeCount() const {
	const auto& m = runtime_.currentHudFrameData().scene.mode;
	return m ? m->count : 0;
}

std::string ModeNavigationHarness::modeId() const {
	const auto& m = runtime_.currentHudFrameData().scene.mode;
	return m ? m->modeId : std::string("none");
}

std::string ModeNavigationHarness::focused() const {
	const auto& f = runtime_.hudCompositorBridgeForTesting().focus().focused();
	return f ? *f : std::string("none");
}

std::string ModeNavigationHarness::position() const {
	const auto& s = runtime_.currentHudFrameData().sceneManager;
	if (!s.activeSceneIndex) return std::string();
	return ofToString(*s.activeSceneIndex + 1) + "/" + ofToString(s.availableSceneCount);
}

void ModeNavigationHarness::buildScript() {
	auto framesAtLeast = [this](int n) { return [this, n] { return framesInStep_ >= n; }; };
	auto panelEnabled = [this]() { return runtime_.hudCompositorBridgeForTesting().renderer().controlEnabled(kModePanel); };
	auto temporalRunning = [this]() {
		return runtime_.temporalSceneForTesting().coreForTesting().compositionPhase() == TFComposition::CyclePhase::RUNNING;
	};
	// Steps RIGHT through the focus ring (one press every other frame) until
	// the mode panel holds focus — the real focus path, not a direct set.
	auto focusModePanel = [this]() {
		if (focused() == kModePanel) return true;
		if (framesInStep_ % 2 == 0) runtime_.keyPressed(OF_KEY_RIGHT);
		return false;
	};
	auto pressM = [this]() {
		indexBefore_ = modeIndex();
		runtime_.keyPressed('m');
	};

	script_ = {
		// -- Blob (1/5): three background modes, 'm' key, wraparound --------
		{"blob live", [this] { return idleOn(kBlobId) && framesInStep_ >= kSettleFrames && modeIndex() >= 0; },
			[this, panelEnabled, pressM] {
				check("Blob: scene position 1/5 from SceneManager's registry", position() == "1/5", position());
				check("Blob: 3 modes, starts on Desaturated Background (production model)",
					modeCount() == 3 && modeId() == "desaturated_background", modeId());
				const auto enabled = panelEnabled();
				check("Blob: NEXT MODE enabled (advertised, 3 modes)", enabled.has_value() && *enabled);
				capture("01_blob_desaturated_background");
				pressM();
			}},
		{"blob no background", framesAtLeast(3), [this, pressM] {
			check("Blob: 'm' -> No Background (2 -> 3)", modeIndex() == 2 && modeId() == "no_background", modeId());
			capture("02_blob_no_background");
			pressM();
		}},
		{"blob natural (wrapped)", framesAtLeast(3), [this] {
			check("Blob: 'm' wraps 3 -> 1, Natural Background", modeIndex() == 0 && modeId() == "natural_background", modeId());
			capture("03_blob_natural_background_wrapped");
			runtime_.keyPressed(']');
		}},

		// -- Temporal (2/5): nine patterns; key + focus paths; refusal -------
		{"temporal live", [this, temporalRunning] {
				return idleOn(kTemporalId) && framesInStep_ >= kSettleFrames && modeIndex() >= 0 && temporalRunning();
			},
			[this, panelEnabled, pressM] {
				check("Temporal: scene position 2/5", position() == "2/5", position());
				check("Temporal: 9 patterns, activation starts on BSP", modeCount() == 9 && modeIndex() == 0 && modeId() == "bsp",
					modeId());
				const auto enabled = panelEnabled();
				check("Temporal: NEXT MODE enabled", enabled.has_value() && *enabled);
				capture("04_temporal_bsp");
				pressM();
			}},
		{"temporal stepped", framesAtLeast(2), [this, pressM, temporalRunning] {
			check("Temporal: 'm' -> BLOB_GRID via forceNextPattern()", modeIndex() == 1 && modeId() == "blob_grid", modeId());
			// Press again at once. Refused only if a transition is actually
			// still playing (a hard-cut style returns to RUNNING immediately).
			transitioningAtPress_ = !temporalRunning();
			pressM();
		}},
		{"temporal second press", framesAtLeast(2), [this] {
			const int expected = transitioningAtPress_ ? indexBefore_ : (indexBefore_ + 1) % 9;
			check(std::string("Temporal: second NEXT MODE ") + (transitioningAtPress_ ? "refused (transition still playing)"
				: "accepted (no transition playing)"), modeIndex() == expected, modeId());
		}},
		{"temporal settled", [this, temporalRunning] { return temporalRunning() && framesInStep_ >= 30; }, [this] {
			capture("05_temporal_blob_grid");
		}},
		{"focus mode panel", focusModePanel, [this] {
			indexBefore_ = modeIndex();
			serialBefore_ = runtime_.lastFocusActivationForTesting().serial;
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"temporal focus activation", framesAtLeast(2), [this] {
			const auto& rec = runtime_.lastFocusActivationForTesting();
			check("Temporal: Enter on the focused mode panel dispatches SceneCommand::NextMode, accepted",
				rec.serial == serialBefore_ + 1 && rec.region == kModePanel && rec.command == "SceneCommand::NextMode"
					&& rec.accepted && !rec.refused,
				rec.command);
			check("Temporal: focus path stepped to the next pattern", modeIndex() == (indexBefore_ + 1) % 9, modeId());
			check("Temporal: automatic 90 s advance restarted from the new pattern",
				runtime_.temporalSceneForTesting().coreForTesting().compositionPhaseElapsed() < 5.0f);
			capture("06_temporal_bands_focus_activation");
		}},
		{"to blueprint", [this, temporalRunning] { return temporalRunning() && framesInStep_ >= 30; }, [this] {
			runtime_.keyPressed(']');
		}},

		// -- Blueprint (3/5): two random cycle modes, displayed read-only ----
		{"blueprint live", [this] { return idleOn(kBlueprintId) && framesInStep_ >= kSettleFrames && modeIndex() >= 0; },
			[this, panelEnabled, pressM] {
				check("Blueprint: scene position 3/5", position() == "3/5", position());
				check("Blueprint: 2 cycle modes shown", modeCount() == 2 && (modeId() == "ghost_layers" || modeId() == "perpetual"),
					modeId());
				const auto enabled = panelEnabled();
				check("Blueprint: NEXT MODE visible but disabled (not advertised)", enabled.has_value() && !*enabled);
				// The panel hides during the Loading transition (neutral status has
				// no mode), so the existing focus rule moved focus to a visible
				// control — same as a hidden RESEED.
				check("Blueprint: focus left the panel while it was hidden mid-switch", focused() != kModePanel, focused());
				capture("07_blueprint_mode_read_only");
				pressM();
			}},
		{"blueprint key refused", framesAtLeast(3), [this] {
			check("Blueprint: 'm' rejected, cycle mode unchanged", modeIndex() == indexBefore_, modeId());
		}},
		{"blueprint focus mode panel", focusModePanel, [this] {
			check("Blueprint: disabled mode panel is still reachable in the focus ring", focused() == kModePanel, focused());
			serialBefore_ = runtime_.lastFocusActivationForTesting().serial;
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"blueprint focus refused", framesAtLeast(1), [this] {
			const auto& rec = runtime_.lastFocusActivationForTesting();
			check("Blueprint: Enter on the disabled mode panel is refused, nothing dispatched",
				rec.serial == serialBefore_ + 1 && rec.refused && rec.command.empty() && rec.region == kModePanel);
			capture("08_blueprint_next_mode_refused_flash");
			runtime_.keyPressed(']');
		}},

		// -- Fragment (4/5): three content modes, wraparound -----------------
		{"fragment live", [this] { return idleOn(kFragmentId) && framesInStep_ >= kSettleFrames && modeIndex() >= 0; },
			[this, pressM] {
				check("Fragment: scene position 4/5", position() == "4/5", position());
				check("Fragment: 3 content modes, starts on EFFECT_VARIED", modeCount() == 3 && modeId() == "effect_varied",
					modeId());
				capture("09_fragment_effect_varied");
				pressM();
			}},
		{"fragment param variant", framesAtLeast(3), [this] {
			check("Fragment: 'm' -> EFFECT_PARAM_VARIANT", modeIndex() == 2 && modeId() == "effect_param_variant", modeId());
		}},
		{"fragment param variant settled", framesAtLeast(150), [this, pressM] {
			capture("10_fragment_effect_param_variant");
			pressM();
		}},
		{"fragment time slice (wrapped)", framesAtLeast(3), [this] {
			check("Fragment: 'm' wraps 3 -> 1, TIME_SLICE", modeIndex() == 0 && modeId() == "time_slice", modeId());
		}},
		{"fragment time slice settled", framesAtLeast(150), [this] {
			capture("11_fragment_time_slice");
			runtime_.keyPressed(']');
		}},

		// -- Quadrant (5/5): five crosshair motion strategies -----------------
		{"quadrant live", [this] { return idleOn(kQuadrantId) && framesInStep_ >= kSettleFrames && modeIndex() >= 0; },
			[this, pressM] {
				check("Quadrant: scene position 5/5", position() == "5/5", position());
				check("Quadrant: 5 motion strategies, starts on DRIFT", modeCount() == 5 && modeId() == "motion_drift", modeId());
				capture("12_quadrant_drift");
				const quadrantcrosshair::CrosshairState cs = runtime_.quadrantSceneForTesting().coreForTesting().crosshair().getState();
				crosshairX_ = cs.cx;
				crosshairY_ = cs.cy;
				pressM();
			}},
		{"quadrant scan", framesAtLeast(1), [this] {
			check("Quadrant: 'm' -> SCAN", modeIndex() == 1 && modeId() == "motion_scan", modeId());
			const quadrantcrosshair::CrosshairState cs = runtime_.quadrantSceneForTesting().coreForTesting().crosshair().getState();
			const float jump = std::hypot(cs.cx - crosshairX_, cs.cy - crosshairY_);
			check("Quadrant: crosshair glides into the new strategy (no teleport on the switch frame)", jump < 40.0f,
				ofToString(jump, 1) + " px");
		}},
		{"quadrant scan settled", framesAtLeast(150), [this] {
			capture("13_quadrant_scan");
			runtime_.keyPressed(']');
		}},

		// -- Back to Blob (ring wrap): mode persisted ------------------------
		{"blob again", [this] { return idleOn(kBlobId) && framesInStep_ >= kSettleFrames && modeIndex() >= 0; }, [this] {
			check("NextScene ring wraps 5/5 -> 1/5", position() == "1/5", position());
			check("Blob: background mode persisted across the visit (Natural Background)", modeId() == "natural_background",
				modeId());
			capture("14_blob_natural_background_persisted");
		}},
	};
}

void ModeNavigationHarness::step(float dt) {
	if (done_) return;
	runtime_.draw();
	framesInStep_++;
	secondsInStep_ += dt;

	Step& s = script_[stepIndex_];
	if (s.ready()) {
		s.act();
		if (done_) return;
		stepIndex_++;
		framesInStep_ = 0;
		secondsInStep_ = 0.0f;
		if (stepIndex_ >= script_.size()) finish();
	} else if (secondsInStep_ > kStepTimeoutSeconds) {
		check("step '" + s.label + "' reached", false, "timed out");
		finish();
	}
}

void ModeNavigationHarness::finish() {
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
