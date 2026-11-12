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
constexpr const char* kModePanel = "universal.media_title"; // the one-line "Mode: <name>   x/y" text (spill checks)
constexpr const char* kModeControl = "controls.mode";       // HUD-006 v2 NEXT MODE control (focus/activation)
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

int ModeNavigationHarness::navIndex() {
	return modeIndex();
}

int ModeNavigationHarness::navNextIndex(int before) {
	return (before + 1) % 3; // ghost_layers -> perpetual -> editorial_collage -> ghost_layers
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

std::string ModeNavigationHarness::mediaId() const {
	const auto& v = runtime_.currentHudFrameData().video;
	return (v && v->mediaId) ? *v->mediaId : std::string("(none)");
}

std::string ModeNavigationHarness::position() const {
	const auto& s = runtime_.currentHudFrameData().sceneManager;
	if (!s.activeSceneIndex) return std::string();
	return ofToString(*s.activeSceneIndex + 1) + "/" + ofToString(s.availableSceneCount);
}

bool ModeNavigationHarness::loading() const {
	return runtime_.currentHudFrameData().sceneManager.transitionPhase == SceneTransitionPhase::Loading;
}

bool ModeNavigationHarness::restorationPending() const {
	return runtime_.hudCompositorBridgeForTesting().focus().restorationPending();
}

// Live-text pixels (the HUD's teal primary/muted text) inside a region vs in
// the band just outside its right edge, read from the frame just drawn.
ModeNavigationHarness::TextSpill ModeNavigationHarness::textSpill(const std::string& regionId, float bandWidth) {
	const ofRectangle b = runtime_.hudCompositorBridgeForTesting().renderer().pixelBoundsFor(regionId);
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	const ofPixels& px = img.getPixels();
	auto isText = [&](int x, int y) {
		const ofColor c = px.getColor(x, y);
		return c.g > 150 && c.b > 150 && c.r < 170;
	};
	TextSpill t;
	for (int y = int(b.y); y < int(b.getBottom()); ++y) {
		for (int x = int(b.x); x < int(b.getRight()); ++x) t.inside += isText(x, y);
		for (int x = int(b.getRight()) + 1; x < int(b.getRight() + bandWidth) && x < int(px.getWidth()); ++x) t.outside += isText(x, y);
	}
	return t;
}

void ModeNavigationHarness::buildScript() {
	auto framesAtLeast = [this](int n) { return [this, n] { return framesInStep_ >= n; }; };
	auto panelEnabled = [this]() { return runtime_.hudCompositorBridgeForTesting().renderer().controlEnabled(kModeControl); };
	auto temporalRunning = [this]() {
		return runtime_.temporalSceneForTesting().coreForTesting().compositionPhase() == TFComposition::CyclePhase::RUNNING;
	};
	// Steps RIGHT through the focus ring (one press every other frame) until
	// the mode panel holds focus — the real focus path, not a direct set.
	auto focusModePanel = [this]() {
		if (focused() == kModeControl) return true;
		if (framesInStep_ % 2 == 0) runtime_.keyPressed(OF_KEY_RIGHT);
		return false;
	};
	auto pressM = [this]() {
		indexBefore_ = navIndex();
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
			indexBefore_ = navIndex();
			serialBefore_ = runtime_.lastFocusActivationForTesting().serial;
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"temporal focus activation", framesAtLeast(2), [this] {
			const auto& rec = runtime_.lastFocusActivationForTesting();
			check("Temporal: Enter on the focused mode panel dispatches SceneCommand::NextMode, accepted",
				rec.serial == serialBefore_ + 1 && rec.region == kModeControl && rec.command == "SceneCommand::NextMode"
					&& rec.accepted && !rec.refused,
				rec.command);
			check("Temporal: focus path stepped to the next pattern", modeIndex() == (indexBefore_ + 1) % 9, modeId());
			check("Temporal: automatic 90 s advance restarted from the new pattern",
				runtime_.temporalSceneForTesting().coreForTesting().compositionPhaseElapsed() < 5.0f);
			capture("06_temporal_bands_focus_activation");
		}},
		// Long names: step with 'm' (only while no transition plays) to
		// ECOLOGICAL SUCCESSION (7/9), then TEMPORAL TIDES (MODE 9/9).
		{"temporal to ecological succession", [this, temporalRunning] {
				if (modeIndex() == 6 && temporalRunning() && framesInStep_ >= 30) return true;
				if (modeIndex() != 6 && temporalRunning() && framesInStep_ % 10 == 0) runtime_.keyPressed('m');
				return false;
			},
			[this] {
				const TextSpill t = textSpill(kModePanel, 24.0f);
				check("Temporal: ECOLOGICAL SUCCESSION (7/9) drawn inside the mode line (universal.media_title), no spill",
					modeId() == "ecological_succession" && t.inside > 0 && t.outside == 0,
					ofToString(t.inside) + " px inside, " + ofToString(t.outside) + " px in the 24 px gap");
				capture("06b_temporal_ecological_succession");
			}},
		{"temporal to 9/9", [this, temporalRunning] {
				if (modeIndex() == 8 && temporalRunning() && framesInStep_ >= 30) return true;
				if (modeIndex() != 8 && temporalRunning() && framesInStep_ % 10 == 0) runtime_.keyPressed('m');
				return false;
			},
			[this] {
				check("Temporal: last pattern reads MODE 9/9 (TEMPORAL TIDES)", modeIndex() == 8 && modeCount() == 9, modeId());
				const TextSpill title = textSpill("universal.scene_title", 20.0f);
				check("scene title + '2/5' suffix stay inside universal.scene_title", title.inside > 0 && title.outside == 0,
					ofToString(title.inside) + " px inside, " + ofToString(title.outside) + " px past the right edge");
				capture("06c_temporal_mode_9_of_9");
			}},
		{"to blueprint", [this, temporalRunning] { return temporalRunning() && framesInStep_ >= 30; }, [this] {
			check("mode panel focused before the switch", focused() == kModeControl, focused());
			runtime_.keyPressed(']');
		}},
		{"temporal->blueprint loading", [this] { return loading(); }, [this] {
			check("Loading: panel hidden, focus fell back automatically, restoration pending",
				focused() != kModeControl && restorationPending(), focused());
			capture("07a_loading_focus_fallback");
		}},

		// -- Blueprint (3/5): two cycle modes, manual NEXT MODE ----------------
		{"blueprint live", [this] { return idleOn(kBlueprintId) && framesInStep_ >= kSettleFrames && modeIndex() >= 0; },
			[this, panelEnabled, pressM] {
				check("Blueprint: scene position 3/5", position() == "3/5", position());
				check("Blueprint: 3 modes shown", modeCount() == 3 && (modeId() == "ghost_layers" || modeId() == "perpetual" || modeId() == "editorial_collage"),
					modeId());
				const auto enabled = panelEnabled();
				check("Blueprint: NEXT MODE advertised and enabled", enabled.has_value() && *enabled);
				check("Blueprint: focus restored to the mode panel after Loading", focused() == kModeControl && !restorationPending(),
					focused());
				capture("07_blueprint_mode_before_next");
				mediaBefore_ = mediaId();
				pressM();
			}},
		{"blueprint mode toggled", framesAtLeast(3), [this, pressM] {
			check("Blueprint: 'm' advances to the next mode", navIndex() == navNextIndex(indexBefore_), modeId());
			check("Blueprint: manual mode change keeps the shared media", mediaId() == mediaBefore_, mediaBefore_ + " -> " + mediaId());
		}},
		{"blueprint mode held", framesAtLeast(60), [this, pressM] {
			check("Blueprint: manual mode holds (no immediate revert)", navIndex() == navNextIndex(indexBefore_), modeId());
			capture("07b_blueprint_mode_after_next");
			pressM();
		}},
		{"blueprint mode wrapped", framesAtLeast(3), [this] {
			check("Blueprint: second 'm' steps again (3-step cycle)", navIndex() == navNextIndex(indexBefore_), modeId());
			check("Blueprint: still the same shared media", mediaId() == mediaBefore_, mediaId());
		}},
		{"blueprint focus mode panel", focusModePanel, [this] {
			indexBefore_ = navIndex();
			serialBefore_ = runtime_.lastFocusActivationForTesting().serial;
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"blueprint focus activation", framesAtLeast(2), [this] {
			const auto& rec = runtime_.lastFocusActivationForTesting();
			check("Blueprint: Enter on the mode panel dispatches SceneCommand::NextMode, accepted",
				rec.serial == serialBefore_ + 1 && rec.region == kModeControl && rec.command == "SceneCommand::NextMode"
					&& rec.accepted && !rec.refused, rec.command);
			check("Blueprint: focus path switched the mode", navIndex() == navNextIndex(indexBefore_), modeId());
			capture("08_blueprint_next_mode_focus_activation");
		}},
		{"blueprint regenerate", framesAtLeast(30), [this] {
			seedBefore_ = runtime_.blueprintSceneForTesting().coreForTesting().composition().getCycleSeed();
			runtime_.keyPressed('2'); // Regenerate: separate command, restarts the whole cycle
			const auto& composition = runtime_.blueprintSceneForTesting().coreForTesting().composition();
			check("Blueprint: Regenerate still restarts the cycle (new seed, clock reset), independent of NEXT MODE",
				composition.getCycleSeed() != seedBefore_ && composition.getCycleElapsedSeconds() == 0.0f);
		}},
		{"blueprint after regenerate", framesAtLeast(3), [this] {
			check("Blueprint: after Regenerate the HUD reports a valid (randomly re-rolled) mode",
				modeCount() == 3 && (modeId() == "ghost_layers" || modeId() == "perpetual" || modeId() == "editorial_collage"), modeId());
			runtime_.keyPressed(']');
		}},
		{"blueprint->fragment loading", [this] { return loading(); }, [this] {
			runtime_.keyPressed(OF_KEY_LEFT); // deliberate navigation while the panel is hidden
			check("explicit LEFT during Loading replaces the intended control (restoration cancelled)",
				!restorationPending() && focused() != kModeControl, focused());
		}},

		// -- Fragment (4/5): four content modes, wraparound -----------------
		{"fragment live", [this] { return idleOn(kFragmentId) && framesInStep_ >= kSettleFrames && modeIndex() >= 0; },
			[this, pressM] {
				check("Fragment: scene position 4/5", position() == "4/5", position());
				check("Fragment: 4 content modes, starts on EFFECT_VARIED", modeCount() == 4 && modeId() == "effect_varied",
					modeId());
				check("Fragment: no focus stealing — focus stayed where the user put it", focused() != kModeControl, focused());
				capture("09_fragment_effect_varied");
				pressM();
			}},
		{"fragment param variant", framesAtLeast(3), [this] {
			check("Fragment: 'm' -> EFFECT_PARAM_VARIANT", modeIndex() == 2 && modeId() == "effect_param_variant", modeId());
		}},
		{"fragment param variant settled", framesAtLeast(150), [this, pressM] {
			const TextSpill t = textSpill(kModePanel, 24.0f);
			check("Fragment: EFFECT PARAM VARIANT drawn inside the mode line (universal.media_title), no spill", t.inside > 0 && t.outside == 0,
				ofToString(t.inside) + " px inside, " + ofToString(t.outside) + " px in the gap");
			capture("10_fragment_effect_param_variant");
			pressM();
		}},
		{"fragment backdrop effect", framesAtLeast(3), [this] {
			check("Fragment: 'm' -> BACKDROP_EFFECT", modeIndex() == 3 && modeId() == "backdrop_effect", modeId());
		}},
		{"fragment backdrop effect settled", framesAtLeast(150), [this, pressM] {
			capture("10b_fragment_backdrop_effect");
			pressM();
		}},
		{"fragment time slice (wrapped)", framesAtLeast(3), [this] {
			check("Fragment: 'm' wraps 4 -> 1, TIME_SLICE", modeIndex() == 0 && modeId() == "time_slice", modeId());
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
		{"blob focus mode panel", focusModePanel, [this] {
			runtime_.keyPressed('['); // PreviousScene, the other direction, panel focused
		}},
		{"blob->quadrant loading", [this] { return loading(); }, [this] {
			check("PreviousScene Loading: fallback with restoration pending", focused() != kModeControl && restorationPending(),
				focused());
		}},
		{"quadrant again", [this] { return idleOn(kQuadrantId) && framesInStep_ >= kSettleFrames && modeIndex() >= 0; }, [this] {
			check("PreviousScene ring wraps 1/5 -> 5/5", position() == "5/5", position());
			check("Quadrant: revisit resets to DRIFT (activation reset policy)", modeId() == "motion_drift", modeId());
			check("focus restored to the mode panel after PreviousScene Loading", focused() == kModeControl && !restorationPending(),
				focused());
			capture("15_quadrant_revisit_focus_restored");
		}},
		{"quadrant->fragment", framesAtLeast(30), [this] { runtime_.keyPressed('['); }},
		{"fragment->blueprint", [this] { return idleOn(kFragmentId) && framesInStep_ >= 30; }, [this] { runtime_.keyPressed('['); }},
		{"blueprint revisit", [this] { return idleOn(kBlueprintId) && framesInStep_ >= kSettleFrames && modeIndex() >= 0; },
			[this, panelEnabled, pressM] {
				const auto enabled = panelEnabled();
				check("Blueprint revisit: fresh cycle reports a valid mode, NEXT MODE enabled",
					modeCount() == 3 && (modeId() == "ghost_layers" || modeId() == "perpetual" || modeId() == "editorial_collage") && enabled.has_value() && *enabled,
					modeId());
				capture("16_blueprint_revisit");
				pressM();
			}},
		{"blueprint revisit toggled", framesAtLeast(3), [this] {
			check("Blueprint revisit: NEXT MODE works on the new cycle", navIndex() == navNextIndex(indexBefore_), modeId());
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
