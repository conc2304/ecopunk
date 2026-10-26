#include "TypographyHarness.h"

#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofImage.h"
#include "ofJson.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {
constexpr const char* kCaptureDir = "captures/typography";
const char* const kTextTargets[] = {"controls.scene_previous", "controls.scene_next", "controls.media_previous", "controls.media_next"};
} // namespace

bool TypographyHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_TYPOGRAPHY_HARNESS") != nullptr;
}

TypographyHarness::TypographyHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	buildScript();
}

void TypographyHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void TypographyHarness::note(const std::string& line) {
	const std::string stamped = "[t=" + ofToString(ofGetElapsedTimef(), 2) + "s] " + line;
	ofLogNotice("TypographyHarness") << stamped;
	lines_.push_back(stamped);
}

void TypographyHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + ".png");
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + ".png (" + fontName() + ", " + ofToString(pg()->sizePx())
		+ " px, spacing " + ofToString(pg()->letterSpacing(), 2) + ", glow " + ofToString(pg()->glow(), 2) + ")");
}

std::string TypographyHarness::fontName() const {
	return pg() ? pg()->fonts()[size_t(pg()->fontIndex())].name : std::string("(no playground)");
}

bool TypographyHarness::cycleTo(const std::string& nameStart) {
	if (fontName().rfind(nameStart, 0) == 0) return true;
	if (framesInStep_ % 3 == 0) runtime_.keyPressed('.');
	return false;
}

// Every text label's drawn glyph box is centred on its canonical region.
void TypographyHarness::checkCentred(const std::string& where) {
	const auto& boxes = pg()->lastLabelBoxes();
	const auto& renderer = runtime_.hudCompositorBridgeForTesting().renderer();
	float worst = 0.0f;
	int seen = 0;
	std::string overflow;
	for (const char* region : kTextTargets) {
		auto it = boxes.find(region);
		if (it == boxes.end()) continue;
		++seen;
		const ofRectangle b = renderer.pixelBoundsFor(region);
		worst = std::max({worst, std::fabs(it->second.getCenter().x - b.getCenter().x), std::fabs(it->second.getCenter().y - b.getCenter().y)});
		if (it->second.width > b.width) overflow += std::string(" ") + region;
	}
	check(where + ": all 4 text labels centred on their canonical control bounds (real glyph box)", seen == 4 && worst <= 0.75f,
		ofToString(seen) + " labels, worst offset " + ofToString(worst, 2) + " px" + (overflow.empty() ? "" : "; WIDER than control:" + overflow));
}

void TypographyHarness::buildScript() {
	auto framesAtLeast = [this](int n) { return [this, n] { return framesInStep_ >= n; }; };
	script_ = {
		{"start", [this] {
				const HudFrameData& f = runtime_.currentHudFrameData();
				return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && framesInStep_ >= 90;
			},
			[this] {
				check("playground enabled by EXPERIENCE_RUNTIME_TYPOGRAPHY_PLAYGROUND=1", pg() != nullptr);
				if (!pg()) { finish(); return; }
				int present = 0, missing = 0;
				for (const auto& f : pg()->fonts()) {
					if (f.builtinBitmap) continue;
					(f.present ? present : missing)++;
				}
				note("registry: " + ofToString(present) + " font file(s) present, " + ofToString(missing) + " missing (reported, not substituted)");
				check("starts on the HUD bitmap baseline (preset font empty) — unchanged labels", pg()->fonts()[size_t(pg()->fontIndex())].builtinBitmap
					&& pg()->fontLoads() == 0, fontName());
				capture("t01_baseline_bitmap");
				loadsMark_ = pg()->fontLoads();
			}},
		{"to Argaka", [this] { return cycleTo("Argaka"); }, [this] {
			check("switch to a real TTF without recompiling (one load)", pg()->fontLoads() == loadsMark_ + 1, fontName());
		}},
		{"argaka drawn", framesAtLeast(5), [this] {
			checkCentred("Argaka");
			check("TTF labels drawn every frame from the cache", pg()->labelDraws() > 0, ofToString(pg()->labelDraws()) + " label draws");
			capture("t02_argaka_fashion");
		}},
		{"to Liberation", [this] { return cycleTo("Liberation"); }, [this] {
			check("second real TTF loaded once", pg()->fontLoads() == loadsMark_ + 2, fontName());
		}},
		{"liberation drawn", framesAtLeast(5), [this] {
			checkCentred("Liberation Mono");
			capture("t03_liberation_mono");
			runtime_.keyPressed('.'); // next: the deliberately broken entry -> must be skipped
		}},
		{"invalid skipped", framesAtLeast(3), [this] {
			bool flagged = false;
			for (const auto& f : pg()->fonts()) flagged = flagged || (f.invalid && f.name.find("Broken") != std::string::npos);
			check("invalid font file fails gracefully: flagged, skipped, app keeps running", flagged && pg()->fontLoadFailures() == 1
				&& fontName().find("Broken") == std::string::npos, "now on " + fontName());
		}},
		{"back to Argaka", [this] { return cycleTo("Argaka"); }, [this] {
			check("returning to an already-loaded font is a cache hit (no new load)", pg()->fontLoads() == loadsMark_ + 2,
				ofToString(pg()->fontLoads() - loadsMark_) + " loads total");
			sizeBefore_ = pg()->sizePx();
			spacingBefore_ = pg()->letterSpacing();
			const int loads = pg()->fontLoads();
			runtime_.keyPressed('0');
			runtime_.keyPressed('0');
			runtime_.keyPressed('0');
			runtime_.keyPressed('k');
			runtime_.keyPressed('k');
			runtime_.keyPressed('g');
			check("spacing, colour and glow changes never reload the font", pg()->fontLoads() == loads,
				"spacing " + ofToString(pg()->letterSpacing(), 2) + ", color #" + ofToString(pg()->colorIndex()) + ", glow " + ofToString(pg()->glow(), 2));
			runtime_.keyPressed('=');
			runtime_.keyPressed('=');
			check("each new size rasterises once (2 steps = 2 loads); glyphs then cached", pg()->sizePx() == sizeBefore_ + 2 && pg()->fontLoads() == loads + 2,
				ofToString(sizeBefore_) + " -> " + ofToString(pg()->sizePx()) + " px");
		}},
		{"styled", framesAtLeast(5), [this] {
			check("letter spacing applied", std::fabs(pg()->letterSpacing() - (spacingBefore_ + 0.15f)) < 1e-3f, ofToString(pg()->letterSpacing(), 2));
			checkCentred("Argaka styled");
			capture("t04_argaka_styled_size_spacing_color_glow");
			runtime_.keyPressed(OF_KEY_RIGHT);
			runtime_.keyPressed(OF_KEY_RIGHT); // focus NEXT SCENE
		}},
		{"focus", framesAtLeast(10), [this] {
			const auto& f = runtime_.hudCompositorBridgeForTesting().focus().focused();
			check("navigation/focus untouched by the playground (arrows still move focus)", f && *f == "controls.scene_next", f ? *f : "none");
			capture("t05_focus_brackets_over_ttf_label");
			loadsMark_ = pg()->fontLoads();
		}},
		{"render soak", framesAtLeast(120), [this] {
			check("no font loading during ordinary rendering (120 frames)", pg()->fontLoads() == loadsMark_,
				ofToString(pg()->fontLoads() - loadsMark_) + " loads");
			check("save preset (w)", pg()->save());
			std::ifstream in(pg()->presetPath());
			const ofJson j = ofJson::parse(in, nullptr, false);
			check("saved preset holds the live settings (relative font reference)",
				j.is_object() && j.value("font", std::string()).rfind("Argaka", 0) == 0 && j.value("size", 0) == pg()->sizePx()
					&& j.contains("glow"),
				j.is_object() ? j.dump() : std::string("unparseable"));
			const int loadsBeforeRevisit = pg()->fontLoads();
			runtime_.keyPressed('-');
			runtime_.keyPressed('=');
			check("revisiting an already-rasterised size is a cache hit", pg()->fontLoads() == loadsBeforeRevisit,
				ofToString(pg()->fontLoads() - loadsBeforeRevisit) + " loads");
			runtime_.keyPressed('k');
			runtime_.keyPressed('-');
			runtime_.keyPressed('.');
			runtime_.keyPressed('r'); // reload from disk
		}},
		{"reloaded", framesAtLeast(5), [this] {
			check("reload (r) restores the saved preset", fontName().rfind("Argaka", 0) == 0 && pg()->sizePx() == sizeBefore_ + 2
				&& std::fabs(pg()->letterSpacing() - (spacingBefore_ + 0.15f)) < 1e-3f,
				fontName() + " " + ofToString(pg()->sizePx()) + " px");
			capture("t06_after_reload");
		}},
		// -- Gallery: every usable font in one neutral style, captured -------
		{"gallery setup", framesAtLeast(2), [this] {
			// 15 px, spacing 1.00, warm ivory (registry colour #1 after the HUD teal baseline), no glow.
			pg()->setStyleForTesting(15, 1.0f, 1, 0.0f);
			galleryVisited_.clear();
			galleryLastIndex_ = -1;
			while (!pg()->fonts()[size_t(pg()->fontIndex())].builtinBitmap) runtime_.keyPressed('.'); // back to the baseline
		}},
		{"gallery", [this] {
				const int idx = pg()->fontIndex();
				if (framesInStep_ % 8 == 5 && idx != galleryLastIndex_) {
					galleryLastIndex_ = idx;
					const auto& f = pg()->fonts()[size_t(idx)];
					std::string slug;
					for (char ch : f.name) slug += std::isalnum(static_cast<unsigned char>(ch)) ? char(std::tolower(static_cast<unsigned char>(ch))) : '_';
					slug = slug.substr(0, 28);
					if (f.builtinBitmap) {
						capture("g00_bitmap_baseline");
					} else {
						checkCentred("gallery " + f.name);
						capture("g" + std::string(idx < 10 ? "0" : "") + ofToString(idx) + "_" + slug);
					}
					galleryVisited_.push_back(idx);
				}
				if (framesInStep_ % 8 == 7 && !galleryVisited_.empty()) {
					runtime_.keyPressed('.');
					if (pg()->fontIndex() == 0) return true; // wrapped back to the baseline
				}
				return false;
			},
			[this] {
				note("gallery: " + ofToString(galleryVisited_.size()) + " look(s) captured (bitmap baseline + every usable font)");
			}},
		{"done", framesAtLeast(3), [this] { finish(); }},
	};
}

void TypographyHarness::step(float /*dt*/) {
	if (done_) return;
	runtime_.draw();
	framesInStep_++;
	Step& s = script_[stepIndex_];
	if (s.ready()) {
		s.act();
		if (done_) return;
		stepIndex_++;
		framesInStep_ = 0;
		if (stepIndex_ >= script_.size()) finish();
	} else if (framesInStep_ > 60 * 30) {
		check("step '" + s.label + "' reached", false, "timed out");
		finish();
	}
}

void TypographyHarness::finish() {
	if (done_) return;
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
