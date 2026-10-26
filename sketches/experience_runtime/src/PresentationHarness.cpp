#include "PresentationHarness.h"

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
constexpr const char* kCaptureDir = "captures/rt007";
constexpr float kStepTimeoutSeconds = 45.0f;
constexpr int kSettleFrames = 90;

float luma(const ofPixels& px, int x, int y) {
	x = std::clamp(x, 0, int(px.getWidth()) - 1);
	y = std::clamp(y, 0, int(px.getHeight()) - 1);
	const ofColor c = px.getColor(x, y);
	return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

} // namespace

bool PresentationHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_RT007_HARNESS") != nullptr;
}

PresentationHarness::PresentationHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	startedFullscreen_ = fullscreenOverrideFromEnv().value_or(false);
	note(std::string("start mode requested: ") + (startedFullscreen_ ? "EXPERIENCE_RUNTIME_FULLSCREEN=1" : "windowed (no override)")
		+ "; screen " + ofToString(ofGetScreenWidth()) + "x" + ofToString(ofGetScreenHeight()));
	buildScript();
}

void PresentationHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void PresentationHarness::note(const std::string& line) {
	const std::string stamped = "[t=" + ofToString(ofGetElapsedTimef(), 1) + "s] " + line;
	ofLogNotice("PresentationHarness") << stamped;
	lines_.push_back(stamped);
}

void PresentationHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + ".png");
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + ".png (" + ofToString(img.getWidth()) + "x"
		+ ofToString(img.getHeight()) + ", " + describeTransform() + ", focus=" + focused() + ")");
}

bool PresentationHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId
		&& f.scene.sceneId == sceneId;
}

std::string PresentationHarness::focused() const {
	const auto& f = runtime_.hudCompositorBridgeForTesting().focus().focused();
	return f ? *f : std::string("none");
}

std::string PresentationHarness::mediaId() const {
	const auto& v = runtime_.currentHudFrameData().video;
	return (v && v->mediaId) ? *v->mediaId : std::string("(none)");
}

std::string PresentationHarness::describeTransform() const {
	const PresentationTransform& t = runtime_.presentationTransform();
	return "fb " + ofToString(t.framebufferWidth) + "x" + ofToString(t.framebufferHeight) + " scale " + ofToString(t.scale, 4)
		+ " offset (" + ofToString(t.offsetX, 1) + "," + ofToString(t.offsetY, 1) + ") "
		+ (ofGetWindowMode() == OF_FULLSCREEN ? "fullscreen" : "windowed");
}

// Same frame: the canonical composition FBO (what the HUD drew at 1280x720)
// vs the physical framebuffer, sampled every 8 canvas px through the final
// transform (bilinear scaling softens edges, so a small residual is
// expected) and through the pre-RT-007 unscaled top-left mapping.
PresentationHarness::Alignment PresentationHarness::measureAlignment() {
	Alignment a;
	ofPixels fbo;
	if (!runtime_.readPresentationFboPixelsForTesting(fbo)) return a;
	ofImage screen;
	screen.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	const ofPixels& fb = screen.getPixels();
	const PresentationTransform& t = runtime_.presentationTransform();
	double sumT = 0.0, sumTL = 0.0;
	for (int cy = 4; cy < 720; cy += 8) {
		for (int cx = 4; cx < 1280; cx += 8) {
			const float ref = luma(fbo, cx, cy);
			const int fx = int(std::floor(t.toFramebufferX(cx + 0.5f)));
			const int fy = int(std::floor(t.toFramebufferY(cy + 0.5f)));
			sumT += std::fabs(luma(fb, fx, fy) - ref);
			sumTL += std::fabs(luma(fb, cx, cy) - ref);
			++a.samples;
		}
	}
	a.transformed = float(sumT / std::max(1, a.samples));
	a.topLeft = float(sumTL / std::max(1, a.samples));
	return a;
}

void PresentationHarness::checkComposed(const std::string& where) {
	const PresentationTransform& t = runtime_.presentationTransform();
	const float expected = std::min(ofGetWidth() / 1280.0f, ofGetHeight() / 720.0f);
	check(where + ": transform from the ACTUAL framebuffer, contain scale + centring",
		t.framebufferWidth == ofGetWidth() && t.framebufferHeight == ofGetHeight() && std::fabs(t.scale - expected) < 1e-4f
			&& std::fabs(t.offsetX - (ofGetWidth() - 1280.0f * t.scale) * 0.5f) < 0.01f
			&& std::fabs(t.offsetY - (ofGetHeight() - 720.0f * t.scale) * 0.5f) < 0.01f,
		describeTransform());
	const auto& c = runtime_.presentationCountersForTesting();
	check(where + ": composed path (one canonical composition, scaled once)",
		c.composedPresentationFrames > composedFramesMark_, ofToString(c.composedPresentationFrames - composedFramesMark_) + " composed frames");
	const Alignment a = measureAlignment();
	check(where + ": framebuffer matches the canonical composition through the transform",
		a.samples > 0 && a.transformed < 10.0f && (t.isIdentity() || a.transformed < a.topLeft * 0.5f),
		"mean |dLuma| " + ofToString(a.transformed, 2) + " (old top-left mapping " + ofToString(a.topLeft, 2) + ", "
			+ ofToString(a.samples) + " samples)");
}

void PresentationHarness::buildScript() {
	auto framesAtLeast = [this](int n) { return [this, n] { return framesInStep_ >= n; }; };
	// Window size settled: unchanged for 20 consecutive frames.
	auto settledAt = [this](std::function<bool()> target) {
		return [this, target] {
			const int w = ofGetWidth(), h = ofGetHeight();
			stableFrames_ = (w == lastW_ && h == lastH_) ? stableFrames_ + 1 : 0;
			lastW_ = w;
			lastH_ = h;
			return target() && stableFrames_ >= 20;
		};
	};
	auto scenePass = [this](const std::string& id, const std::string& name, bool last) {
		return Step{name, [this, id] { return idleOn(id) && framesInStep_ >= kSettleFrames; }, [this, name, last] {
			checkComposed(name);
			capture("r03_1600x900_" + name);
			if (!last) runtime_.keyPressed(']');
		}};
	};

	script_ = {
		{"start", [this] { return idleOn(kBlobId) && framesInStep_ >= kSettleFrames; }, [this] {
			const auto& c = runtime_.presentationCountersForTesting();
			sceneFboAllocsAtStart_ = c.sceneFboAllocations;
			switchReallocsAtStart_ = c.sceneFboSwitchReallocations;
			mediaAtStart_ = mediaId();
			if (startedFullscreen_) {
				check("native fullscreen startup: window REPORTS fullscreen (read back, not assumed)", ofGetWindowMode() == OF_FULLSCREEN,
					describeTransform());
				check("native fullscreen startup: framebuffer covers the display, not the 1280x720 default",
					ofGetWidth() != 1280 || ofGetHeight() != 720, describeTransform());
				checkComposed("fullscreen startup");
				capture("r00_fullscreen_startup_blob");
			} else {
				check("windowed start: 1280x720 framebuffer, direct path, no composition FBO allocated",
					ofGetWidth() == 1280 && ofGetHeight() == 720 && runtime_.presentationTransform().isIdentity()
						&& c.directPresentationFrames > 0 && c.composedPresentationFrames == 0 && c.presentationFboAllocations == 0,
					describeTransform() + ", direct " + ofToString(c.directPresentationFrames));
				capture("r01_windowed_1280x720_blob");
			}
			ofSetFullscreen(false);
			ofSetWindowShape(1600, 900);
		}},
		{"resized to 1600x900", settledAt([] { return ofGetWidth() == 1600 && ofGetHeight() == 900 && ofGetWindowMode() == OF_WINDOW; }),
			[this] {
				composedFramesMark_ = 0;
				checkComposed("runtime resize 1600x900");
				check("1600x900: scale 1.25, no letterbox", std::fabs(runtime_.presentationTransform().scale - 1.25f) < 1e-4f
					&& runtime_.presentationTransform().offsetX == 0.0f && runtime_.presentationTransform().offsetY == 0.0f,
					describeTransform());
				const auto& c = runtime_.presentationCountersForTesting();
				check("composition FBO allocated exactly once; scene FBO NOT reallocated by the resize",
					c.presentationFboAllocations == 1 && c.sceneFboAllocations == sceneFboAllocsAtStart_,
					ofToString(c.presentationFboAllocations) + " composition alloc(s), scene FBO allocs " + ofToString(sceneFboAllocsAtStart_)
						+ " -> " + ofToString(c.sceneFboAllocations));
				check("scene and media continuous across the resize", idleOn(kBlobId) && mediaId() == mediaAtStart_,
					mediaAtStart_ + " -> " + mediaId());
				runtime_.pointerPressed(800.0f, 450.0f);
				const glm::vec2 p = runtime_.lastPointerCanvasForTesting();
				runtime_.pointerPressed(0.0f, 0.0f);
				const glm::vec2 q = runtime_.lastPointerCanvasForTesting();
				runtime_.pointerPressed(1599.0f, 899.0f);
				const glm::vec2 r = runtime_.lastPointerCanvasForTesting();
				check("pointer: framebuffer -> canvas through the inverse transform (centre, corners)",
					std::fabs(p.x - 640.0f) < 0.01f && std::fabs(p.y - 360.0f) < 0.01f && q.x == 0.0f && q.y == 0.0f
						&& std::fabs(r.x - 1279.2f) < 0.01f && std::fabs(r.y - 719.2f) < 0.01f,
					"(800,450)->(" + ofToString(p.x, 1) + "," + ofToString(p.y, 1) + "), (1599,899)->(" + ofToString(r.x, 1) + ","
						+ ofToString(r.y, 1) + ")");
				capture("r02_1600x900_blob");
				runtime_.keyPressed(OF_KEY_RIGHT); // focus brackets on PREV SCENE
			}},
		{"focus brackets", framesAtLeast(10), [this] {
			check("keyboard focus unchanged by scaling (first step still engages PREV SCENE)", focused() == "controls.scene_previous", focused());
			capture("r02b_1600x900_focus_brackets");
			runtime_.keyPressed(OF_KEY_RIGHT);
			runtime_.keyPressed(OF_KEY_RIGHT); // -> MODE control (HUD-006 v2)
		}},
		{"mode panel focus", framesAtLeast(10), [this] {
			check("focus walks to the MODE control", focused() == "controls.mode", focused());
			capture("r02c_1600x900_mode_panel_focus");
			runtime_.keyPressed(']');
		}},
		scenePass(kTemporalId, "temporal", false),
		scenePass(kBlueprintId, "blueprint", false),
		scenePass(kFragmentId, "fragment", false),
		scenePass(kQuadrantId, "quadrant", true),
		{"to fullscreen", framesAtLeast(5), [this] {
			composedFramesMark_ = runtime_.presentationCountersForTesting().composedPresentationFrames;
			ofSetFullscreen(true); // the same call the Installation Controls 'f' toggle path makes
		}},
		{"fullscreen", settledAt([] { return ofGetWindowMode() == OF_FULLSCREEN; }), [this] {
			check("fullscreen state read back from the window (not inferred from the request)", ofGetWindowMode() == OF_FULLSCREEN,
				describeTransform());
			checkComposed("runtime fullscreen");
			const auto& c = runtime_.presentationCountersForTesting();
			check("still one composition FBO allocation; scene FBO untouched by fullscreen",
				c.presentationFboAllocations == 1 && c.sceneFboSwitchReallocations == switchReallocsAtStart_,
				ofToString(c.presentationFboAllocations) + " alloc(s)");
			check("installation status reports the real fullscreen state", runtime_.currentHudFrameData().installation
				&& runtime_.currentHudFrameData().installation->fullscreen);
			capture("r04_fullscreen_quadrant");
			runtime_.keyPressed(OF_KEY_RETURN); // MODE control Enter -> NextMode at fullscreen
		}},
		{"leave fullscreen", framesAtLeast(30), [this] {
			ofSetFullscreen(false);
		}},
		{"windowed again", settledAt([] { return ofGetWindowMode() == OF_WINDOW; }), [] { ofSetWindowShape(1280, 720); }},
		{"back to 1280x720", settledAt([] { return ofGetWidth() == 1280 && ofGetHeight() == 720 && ofGetWindowMode() == OF_WINDOW; }),
			[this] { directFramesMark_ = runtime_.presentationCountersForTesting().directPresentationFrames;
				composedFramesMark_ = runtime_.presentationCountersForTesting().composedPresentationFrames; }},
		{"direct again", framesAtLeast(10), [this] {
			const auto& c = runtime_.presentationCountersForTesting();
			check("1280x720 window again: direct path resumes, composition target no longer used",
				runtime_.presentationTransform().isIdentity() && c.directPresentationFrames >= directFramesMark_ + 9
					&& c.composedPresentationFrames == composedFramesMark_,
				describeTransform());
			check("no scene FBO reallocations caused by any display change (all scenes native 1280x720)",
				c.sceneFboSwitchReallocations == switchReallocsAtStart_, ofToString(c.sceneFboSwitchReallocations));
			capture("r05_windowed_1280x720_again");
		}},
		{"done", framesAtLeast(2), [this] { finish(); }},
	};
}

void PresentationHarness::step(float dt) {
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
		stableFrames_ = 0;
		if (stepIndex_ >= script_.size()) finish();
	} else if (secondsInStep_ > kStepTimeoutSeconds) {
		check("step '" + s.label + "' reached", false, "timed out — " + describeTransform());
		finish();
	}
}

void PresentationHarness::finish() {
	if (done_) return;
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
