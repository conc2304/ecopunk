#include "ExperienceV03Harness.h"

#include "HudRealFrameResolver.h"
#include "HudVocabularyResolver.h"

#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>
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
constexpr const char* kCaptureDir = "captures/v03";
constexpr const char* kImmersive = "controls.reseed"; // frozen region ID; IMMERSIVE control since v0.3
constexpr float kStepTimeoutSeconds = 45.0f;
constexpr int kSettleFrames = 90;
constexpr float kMotionWindowSeconds = 12.0f;
constexpr float kHighThresh = 1.5f; // quadrantcrosshair::CrosshairSystem::HIGH_THRESH
constexpr float kDashHysteresis = 0.8f;

} // namespace

bool ExperienceV03Harness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_V03_HARNESS") != nullptr;
}

ExperienceV03Harness::ExperienceV03Harness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	if (const char* s = std::getenv("EXPERIENCE_RUNTIME_V03_OCCUPANCY_SECONDS")) occupancySeconds_ = std::max(10.0f, float(std::atof(s)));
	const auto ids = runtime_.sceneManagerForTesting().registeredSceneIdsForTesting();
	check("full roster of five, startup Blob", ids.size() == 5 && ids[0] == kBlobId);
	buildScript();
}

void ExperienceV03Harness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void ExperienceV03Harness::note(const std::string& line) {
	const std::string stamped = "[t=" + ofToString(ofGetElapsedTimef(), 1) + "s] " + line;
	ofLogNotice("ExperienceV03Harness") << stamped;
	lines_.push_back(stamped);
}

void ExperienceV03Harness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + ".png");
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + ".png (" + ofToString(img.getWidth()) + "x"
		+ ofToString(img.getHeight()) + ", focus=" + focused() + ")");
}

void ExperienceV03Harness::captureScene(const std::string& name) {
	const std::string path = std::string(kCaptureDir) + "/" + name + ".png";
	if (runtime_.saveSceneFrameCaptureForTesting(ofToDataPath(path, true))) note("scene capture: bin/data/" + path);
}

bool ExperienceV03Harness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId
		&& f.scene.sceneId == sceneId;
}

std::string ExperienceV03Harness::focused() const {
	const auto& f = runtime_.hudCompositorBridgeForTesting().focus().focused();
	return f ? *f : std::string("none");
}

std::string ExperienceV03Harness::mediaId() const {
	const auto& v = runtime_.currentHudFrameData().video;
	return (v && v->mediaId) ? *v->mediaId : std::string("(none)");
}

// The real production resolver against the real frame — the same text the
// HUD binding draws (identifiers through the HUD vocabulary).
std::string ExperienceV03Harness::resolvedText(const std::string& sourceId) const {
	static const hudpresent::HudRealFrameResolver resolver;
	static const hudpresent::HudVocabularyResolver vocabulary;
	const auto v = resolver.resolve(sourceId, runtime_.currentHudFrameData());
	if (!v || !v->present) return "(missing)";
	if (v->valueType == hudpresent::HudSourceValueType::Identifier) {
		return vocabulary.resolve(runtime_.currentHudFrameData().scene.sceneId, v->textValue);
	}
	if (v->valueType == hudpresent::HudSourceValueType::Text) return v->textValue;
	return ofToString(v->numberValue, 3);
}

bool ExperienceV03Harness::focusRegion(const std::string& regionId) {
	if (focused() == regionId) return true;
	if (framesInStep_ % 2 == 0) runtime_.keyPressed(OF_KEY_RIGHT);
	return false;
}

// Visible-occupancy method (Blueprint density): read the scene FBO, crop to
// the part the HUD media viewport shows (cover-fit of the 1280x720 canvas
// into the frozen 0.60x0.70 region of a 16:9 window -> central 1097x720),
// divide into 16x16 px cells, and count a cell as occupied when >= 30% of its
// pixels are brighter than the frame's background (10th-percentile
// luminance) by > 28/255. Thin grid/measurement lines (1-2 px in a 16 px
// cell, < 15%) don't count; fragment imagery and filled widgets do.
float ExperienceV03Harness::visibleOccupancy() {
	ofPixels px;
	if (!runtime_.readSceneFboPixelsForTesting(px) || px.getWidth() == 0) return -1.0f;
	const int W = int(px.getWidth()), H = int(px.getHeight());
	const float aspect = (0.60f * 16.0f) / (0.70f * 9.0f);
	const int visW = std::min(W, int(std::round(H * aspect)));
	const int x0 = (W - visW) / 2;
	const int ch = int(px.getNumChannels());
	auto lum = [&](int x, int y) {
		const unsigned char* p = px.getData() + (size_t(y) * W + x) * ch;
		return 0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2];
	};
	std::vector<float> sample;
	sample.reserve(size_t(visW / 4) * size_t(H / 4));
	for (int y = 0; y < H; y += 4)
		for (int x = x0; x < x0 + visW; x += 4) sample.push_back(lum(x, y));
	std::nth_element(sample.begin(), sample.begin() + sample.size() / 10, sample.end());
	const float bg = sample[sample.size() / 10];
	const int cell = 16;
	int cells = 0, occupied = 0;
	for (int cy = 0; cy + cell <= H; cy += cell) {
		for (int cx = x0; cx + cell <= x0 + visW; cx += cell) {
			int bright = 0;
			for (int y = cy; y < cy + cell; ++y)
				for (int x = cx; x < cx + cell; ++x) bright += lum(x, y) > bg + 28.0f;
			++cells;
			occupied += bright >= int(0.30f * cell * cell);
		}
	}
	return cells ? float(occupied) / float(cells) : -1.0f;
}

void ExperienceV03Harness::sampleMotion(MotionWindow& w, float dt) {
	const auto s = runtime_.quadrantSceneForTesting().coreForTesting().crosshair().getState();
	if (dt <= 0.0f) return;
	w.seconds += dt;
	// "raw" = the pre-v0.3 definition: per-frame pixel distance, unsmoothed
	// — what used to drive line width and the dashed/solid switch.
	float raw = 0.0f;
	if (w.havePrev) raw = std::hypot(s.cx - w.prevX, s.cy - w.prevY);
	w.prevX = s.cx; w.prevY = s.cy;
	if (!w.havePrev) { w.havePrev = true; return; }
	w.frames++;
	w.sumSpeed += s.speed;
	w.sumRawSpeed += raw;
	w.sumRawPer60 += raw / (dt * 60.0f);
	if (w.lastSpeed >= 0.0f) {
		w.sumSpeedDelta += std::fabs(s.speed - w.lastSpeed);
		w.sumRawSpeedDelta += std::fabs(raw - w.lastRaw);
		w.sumWidthDelta += std::fabs(s.chWidth - w.lastWidth);
	}
	w.lastSpeed = s.speed; w.lastRaw = raw; w.lastWidth = s.chWidth;
	const bool dash = w.dash ? s.speed > kHighThresh * kDashHysteresis : s.speed > kHighThresh;
	if (dash != w.dash) { w.dashToggles++; w.dash = dash; }
	const bool rawDash = raw > kHighThresh;
	if (rawDash != w.rawDash) { w.rawDashToggles++; w.rawDash = rawDash; }
}

std::string ExperienceV03Harness::describe(const MotionWindow& w) const {
	const double n = std::max(1, w.frames);
	return ofToString(w.frames) + " frames/" + ofToString(w.seconds, 1) + " s (" + ofToString(w.frames / std::max(0.001f, w.seconds), 1)
		+ " fps): speed mean " + ofToString(w.sumSpeed / n, 2) + " (60Hz units), frame-to-frame |dspeed| "
		+ ofToString(w.sumSpeedDelta / n, 3) + " vs raw per-frame " + ofToString(w.sumRawSpeedDelta / n, 3)
		+ "; raw per-frame speed mean " + ofToString(w.sumRawSpeed / n, 2) + "; |dwidth| " + ofToString(w.sumWidthDelta / n, 3)
		+ " px/frame; dashed/solid switches " + ofToString(w.dashToggles) + " (raw rule would give " + ofToString(w.rawDashToggles) + ")";
}

void ExperienceV03Harness::buildScript() {
	auto framesAtLeast = [this](int n) { return [this, n] { return framesInStep_ >= n; }; };
	auto ctl = [this](const char* region) {
		const auto e = runtime_.hudCompositorBridgeForTesting().renderer().controlEnabled(region);
		return e.has_value() && *e;
	};

	script_ = {
		// -- Phase 1: navigation row ------------------------------------
		{"blob live", [this] { return idleOn(kBlobId) && framesInStep_ >= kSettleFrames; },
			[this, ctl] {
				check("title suffix from the registry: [Scene 1/5]", resolvedText("scene.position") == "[Scene 1/5]",
					resolvedText("scene.position"));
				const bool prevMediaVisible =
					runtime_.hudCompositorBridgeForTesting().renderer().controlEnabled("controls.media_previous").has_value();
				check("row: PREV SCENE, NEXT SCENE, NEXT MEDIA, IMMERSIVE enabled; PREV MEDIA visible (disabled at history start)",
					ctl("controls.scene_previous") && ctl("controls.scene_next") && prevMediaVisible
						&& ctl("controls.media_next") && ctl(kImmersive));
				capture("v01_blob_navigation_row");
			}},
		{"focus NEXT SCENE", [this] { return focusRegion("controls.scene_next"); }, [this] {
			sceneBefore_ = runtime_.currentHudFrameData().sceneManager.activeSceneId;
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"temporal via NEXT SCENE", [this] { return idleOn(kTemporalId) && framesInStep_ >= 30; }, [this] {
			check("NEXT SCENE (focus + Enter) -> Temporal, [Scene 2/5]", resolvedText("scene.position") == "[Scene 2/5]",
				resolvedText("scene.position"));
			check("focus kept on NEXT SCENE across the switch", focused() == "controls.scene_next", focused());
		}},
		{"focus PREV SCENE", [this] { return focusRegion("controls.scene_previous"); }, [this] {
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"blob via PREV SCENE", [this] { return idleOn(kBlobId) && framesInStep_ >= kSettleFrames; }, [this] {
			check("PREV SCENE (focus + Enter) -> Blob, [Scene 1/5]", resolvedText("scene.position") == "[Scene 1/5]",
				resolvedText("scene.position"));
		}},

		// -- Phase 2: IMMERSIVE -----------------------------------------
		{"focus IMMERSIVE", [this] { return focusRegion(kImmersive); }, [this] {
			sceneBefore_ = runtime_.currentHudFrameData().sceneManager.activeSceneId;
			mediaBefore_ = mediaId();
			liveFramesBefore_ = runtime_.presentationCountersForTesting().liveSceneFrames;
			const int w = ofGetWidth(), h = ofGetHeight();
			runtime_.keyPressed(OF_KEY_RETURN);
			check("IMMERSIVE (focus + Enter) entered", runtime_.immersiveForTesting());
			check("IMMERSIVE is not OS fullscreen: window mode/size unchanged",
				ofGetWindowMode() == OF_WINDOW && ofGetWidth() == w && ofGetHeight() == h);
		}},
		{"immersive running", framesAtLeast(60), [this] {
			const uint64_t live = runtime_.presentationCountersForTesting().liveSceneFrames;
			check("scene keeps rendering live while immersive", live >= liveFramesBefore_ + 50,
				ofToString(live - liveFramesBefore_) + " live frames in 60");
			capture("v02_immersive_blob");
			runtime_.keyPressed(']'); // first input: must only exit, not switch scenes
		}},
		{"key exit consumed", framesAtLeast(30), [this] {
			check("any key exits IMMERSIVE", !runtime_.immersiveForTesting());
			check("the exiting key was consumed (no NextScene)",
				runtime_.currentHudFrameData().sceneManager.activeSceneId == sceneBefore_
					&& runtime_.currentHudFrameData().sceneManager.transitionPhase == SceneTransitionPhase::Idle,
				runtime_.currentHudFrameData().sceneManager.activeSceneId);
			check("HUD focus restored to IMMERSIVE", focused() == kImmersive, focused());
			check("media continuous across IMMERSIVE", mediaId() == mediaBefore_, mediaBefore_ + " -> " + mediaId());
			capture("v03_after_immersive_exit");
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"immersive again", framesAtLeast(10), [this] {
			check("IMMERSIVE re-entered", runtime_.immersiveForTesting());
			runtime_.pointerPressed();
		}},
		{"pointer exit", framesAtLeast(10), [this] {
			check("pointer input exits IMMERSIVE, scene unchanged",
				!runtime_.immersiveForTesting() && runtime_.currentHudFrameData().sceneManager.activeSceneId == sceneBefore_);
		}},

		// -- Phase 3: media progress ------------------------------------
		{"media progress", [this] {
				const auto& v = runtime_.currentHudFrameData().video;
				return (v && v->durationSeconds && *v->durationSeconds > 0.0f && v->positionSeconds) || secondsInStep_ > 20.0f;
			},
			[this] {
				const auto& v = runtime_.currentHudFrameData().video;
				const std::string t = resolvedText("media.playback.time");
				const bool known = v && v->durationSeconds && *v->durationSeconds > 0.0f;
				check("media slot time from Shared Video (current / total)",
					known && t.find(" / ") != std::string::npos && t.find("--") == std::string::npos, t);
				check("media slot progress from Shared Video", v && v->playbackProgress.has_value(),
					v && v->playbackProgress ? ofToString(*v->playbackProgress, 3) : std::string("(none)"));
				check("media slot title kept alongside the progress bar", resolvedText("media.title") != "(missing)",
					resolvedText("media.title"));
				capture("v04_media_progress");
				runtime_.keyPressed(']');
			}},

		// -- Phase 6: Blueprint occupancy -------------------------------
		{"temporal pass", [this] { return idleOn(kTemporalId) && framesInStep_ >= 30; }, [this] { runtime_.keyPressed(']'); }},
		{"blueprint live", [this] { return idleOn(kBlueprintId) && framesInStep_ >= 10; }, [this] {
			occupancy_.clear();
			occupancyPhase_.clear();
			occupancyFragments_.clear();
			occupancyNext_ = 0.0f;
			note("Blueprint occupancy sampling for " + ofToString(occupancySeconds_, 0) + " s (1 Hz)");
		}},
		{"blueprint occupancy", [this] {
				if (secondsInStep_ >= occupancyNext_) {
					occupancy_.push_back(visibleOccupancy());
					const BECompositionState st = runtime_.blueprintSceneForTesting().coreForTesting().composition().getState();
					occupancyPhase_.push_back(st.currentPhase);
					occupancyFragments_.push_back(st.fragmentCount);
					occupancyNext_ += 1.0f;
					if (occupancy_.size() == size_t(occupancySeconds_ * 0.5f)) captureScene("v05_blueprint_mid");
				}
				return secondsInStep_ >= occupancySeconds_;
			},
			[this] {
				// Two figures: whole run (every cycle stage, including the
				// intentional BLANK / DISSOLVE / RESET_HOLD gaps) and the composed
				// stages (PLACEMENT + DENSITY) after the first 30 s of slot stagger.
				// The 30-75% target applies to the composed stages.
				static const char* kPhase[] = {"B", "P", "D", "X", "R"}; // BLANK PLACEMENT DENSITY DISSOLVE RESET_HOLD
				auto stats = [](const std::vector<float>& v) {
					if (v.empty()) return std::string("n/a");
					float mn = 1, mx = 0, mean = 0;
					for (float o : v) { mn = std::min(mn, o); mx = std::max(mx, o); mean += o; }
					mean /= float(v.size());
					return "mean " + ofToString(mean * 100, 1) + "%, min " + ofToString(mn * 100, 1) + "%, max "
						+ ofToString(mx * 100, 1) + "% (" + ofToString(v.size()) + " samples)";
				};
				std::vector<float> all, composed;
				for (size_t i = 0; i < occupancy_.size(); ++i) {
					if (occupancy_[i] < 0.0f) continue;
					all.push_back(occupancy_[i]);
					const int ph = occupancyPhase_[i];
					if (i >= 30 && (ph == 1 || ph == 2)) composed.push_back(occupancy_[i]);
				}
				std::string series;
				for (size_t i = 0; i < occupancy_.size(); i += 5) {
					const int ph = occupancyPhase_[i];
					series += ofToString(int(std::round(occupancy_[i] * 100))) + "/" + ofToString(occupancyFragments_[i])
						+ (ph >= 0 && ph < 5 ? kPhase[ph] : "?") + " ";
				}
				note("Blueprint occupancy every 5 s (%/fragments+stage B,P,D,X,R): " + series);
				note("Blueprint occupancy, whole run: " + stats(all));
				float composedMean = 0.0f;
				for (float o : composed) composedMean += o;
				if (!composed.empty()) composedMean /= float(composed.size());
				check("Blueprint visible occupancy within 30-75% during composed stages (PLACEMENT/DENSITY)",
					!composed.empty() && composedMean >= 0.30f && composedMean <= 0.75f, stats(composed));
				captureScene("v06_blueprint_end");
				capture("v06_blueprint_end_hud");
				runtime_.keyPressed(']');
			}},

		// -- Phase 5: Fragment radar ------------------------------------
		{"fragment live", [this] { return idleOn(kFragmentId) && secondsInStep_ >= 15.0f; }, [this] {
			auto& core = runtime_.fragmentSceneForTesting().coreForTesting();
			const auto& overlay = core.overlay();
			const ofRectangle safe = overlay.safeAreaRect();
			const ofVec2f c = overlay.radarCenter();
			const float r = overlay.radarRingRadius() + 6.0f; // + tick half-length
			const bool inside = safe.getWidth() > 0.0f && c.x - r >= safe.x && c.x + r <= safe.getRight() && c.y - r >= safe.y
				&& c.y + r <= safe.getBottom();
			check("radar ring fully inside the visible (cover-cropped) area", inside,
				"ring x " + ofToString(c.x - r, 0) + ".." + ofToString(c.x + r, 0) + " in safe " + ofToString(safe.x, 0) + ".."
					+ ofToString(safe.getRight(), 0));
			int live = 0;
			for (const auto& f : core.pool().getFragments()) live += f.getAlpha() > 0.02f;
			const auto& contacts = overlay.radarContacts();
			const int primaries = int(std::count_if(contacts.begin(), contacts.end(), [](const hud::RadarContact& k) { return k.primary; }));
			check("radar contacts come from the live fragment pool (one per visible fragment, one primary)",
				int(contacts.size()) == live && primaries == (live > 0 ? 1 : 0),
				ofToString(contacts.size()) + " contacts, " + ofToString(live) + " live fragments, " + ofToString(primaries) + " primary");
			captureScene("v07_fragment_radar_scene");
			capture("v07_fragment_radar_hud");
			runtime_.keyPressed(']');
		}},

		// -- Phase 4: Quadrant motion + state badge ---------------------
		{"quadrant live", [this] { return idleOn(kQuadrantId) && framesInStep_ >= kSettleFrames; }, [this] {
			const std::string detail = resolvedText("scene.state.secondary");
			const bool expanding = runtime_.quadrantSceneForTesting().coreForTesting().expansionDirector().isActive();
			const bool strategy = detail == "DRIFT" || detail == "SCAN" || detail == "HUNT" || detail == "NERVOUS" || detail == "ORBIT";
			check("state badge detail = expanding quadrant's effect, else the active strategy",
				expanding ? detail != "(missing)" : strategy, detail + (expanding ? " (expansion active)" : ""));
			check("state badge value = Quadrant phase", resolvedText("scene.state.primary") != "(missing)",
				resolvedText("scene.state.primary"));
			capture("v08_quadrant_status");
			motion60_ = MotionWindow{};
			ofSetFrameRate(60);
		}},
		{"quadrant 60 fps", [this] {
				sampleMotion(motion60_, lastDt_);
				return motion60_.seconds >= kMotionWindowSeconds;
			},
			[this] {
				note("Quadrant motion @60: " + describe(motion60_));
				motion30_ = MotionWindow{};
				ofSetFrameRate(30);
			}},
		{"quadrant 30 fps", [this] {
				sampleMotion(motion30_, lastDt_);
				return motion30_.seconds >= kMotionWindowSeconds;
			},
			[this] {
				ofSetFrameRate(60);
				note("Quadrant motion @30: " + describe(motion30_));
				const double n60 = std::max(1, motion60_.frames), n30 = std::max(1, motion30_.frames);
				const double d60 = motion60_.sumSpeedDelta / n60, r60 = motion60_.sumRawSpeedDelta / n60;
				check("speed signal smoothed: frame-to-frame |dspeed| well below the raw per-frame signal at 60 fps",
					d60 <= r60 * 0.5, ofToString(d60, 3) + " vs " + ofToString(r60, 3));
				const double s60 = motion60_.sumSpeed / n60, s30 = motion30_.sumSpeed / n30;
				const double raw60 = motion60_.sumRawSpeed / n60, raw30 = motion30_.sumRawSpeed / n30;
				note("frame-rate independence: speed (60Hz units) " + ofToString(s60, 2) + " @60 vs " + ofToString(s30, 2)
					+ " @30; raw per-frame (old definition) " + ofToString(raw60, 2) + " @60 vs " + ofToString(raw30, 2) + " @30");
				// Units check (robust to the noise path differing between the two
				// windows): the published speed must equal the per-frame distance
				// rescaled to 60 Hz frames at BOTH rates — i.e. px per 1/60 s,
				// not px per rendered frame (the old definition doubles at 30 fps).
				const double u60 = (motion60_.sumRawPer60 / n60) / std::max(1e-6, s60);
				const double u30 = (motion30_.sumRawPer60 / n30) / std::max(1e-6, s30);
				const double old30 = raw30 / std::max(1e-6, s30);
				check("speed is in frame-rate-independent units (rescaled per-frame distance / published speed ~1.0 at 60 and 30 fps)",
					std::fabs(u60 - 1.0) <= 0.2 && std::fabs(u30 - 1.0) <= 0.2,
					ofToString(u60, 2) + " @60, " + ofToString(u30, 2) + " @30 (old per-frame definition would read "
						+ ofToString(old30, 2) + " @30)");
				check("dashed/solid arm switching calmer than the raw rule at both rates",
					motion60_.dashToggles <= motion60_.rawDashToggles && motion30_.dashToggles <= motion30_.rawDashToggles,
					ofToString(motion60_.dashToggles) + "/" + ofToString(motion60_.rawDashToggles) + " @60, "
						+ ofToString(motion30_.dashToggles) + "/" + ofToString(motion30_.rawDashToggles) + " @30");
				capture("v09_quadrant_after_motion");
			}},
		{"done", framesAtLeast(2), [this] { finish(); }},
	};
}

void ExperienceV03Harness::step(float dt) {
	if (done_) return;
	runtime_.draw();
	framesInStep_++;
	secondsInStep_ += dt;
	lastDt_ = dt;

	Step& s = script_[stepIndex_];
	const float timeout = s.label == "blueprint occupancy" ? occupancySeconds_ + kStepTimeoutSeconds : kStepTimeoutSeconds;
	if (s.ready()) {
		s.act();
		if (done_) return;
		stepIndex_++;
		framesInStep_ = 0;
		secondsInStep_ = 0.0f;
		if (stepIndex_ >= script_.size()) finish();
	} else if (secondsInStep_ > timeout) {
		check("step '" + s.label + "' reached", false, "timed out");
		finish();
	}
}

void ExperienceV03Harness::finish() {
	if (done_) return;
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
