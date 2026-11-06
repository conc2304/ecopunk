#include "OrbitInjectHarness.h"

#include "InputRouter.h"
#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "ofJson.h"
#include "HudCompositorBridge.h"
#include "OrbitDraw.h"

namespace { constexpr const char* kDir = "captures/orbit-inject"; }

bool OrbitInjectHarness::isOc005() {
	const char* v = std::getenv("EXPERIENCE_RUNTIME_ORBIT_INJECT_HARNESS");
	return v && std::string(v) == "oc005";
}

bool OrbitInjectHarness::isRequested() { return std::getenv("EXPERIENCE_RUNTIME_ORBIT_INJECT_HARNESS") != nullptr; }

OrbitInjectHarness::OrbitInjectHarness(ExperienceRuntime& runtime) : runtime_(runtime) {}

namespace {
void hsv(float h, float s, float v, float& r, float& g, float& b) {
	const float c = v * s, x = c * (1 - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1)), m = v - c;
	float rr = 0, gg = 0, bb = 0;
	if (h < 60) { rr = c; gg = x; } else if (h < 120) { rr = x; gg = c; } else if (h < 180) { gg = c; bb = x; }
	else if (h < 240) { gg = x; bb = c; } else if (h < 300) { rr = x; bb = c; } else { rr = c; bb = x; }
	r = rr + m; g = gg + m; b = bb + m;
}
}

// Deterministic: sample i at t0 + 0.5*slot. Hue sweeps 0..300 deg, luminance
// swings between ~0.15 and ~0.95. Slots 20 and 21 are INVALID samples; slots
// 45..47 are missing entirely (a sampling pause).
void OrbitInjectHarness::buildSeries(int slots, double t0, std::vector<hudpresent::OrbitSample>& out) const {
	out.clear();
	for (int i = 0; i < slots; ++i) {
		if (i >= 45 && i <= 47) continue; // pause: no sample at all
		hudpresent::OrbitSample s;
		s.t = t0 + 0.5 * i;
		if (i == 20 || i == 21) { s.valid = false; out.push_back(s); continue; }
		const float u = float(i) / float(std::max(1, slots - 1));
		const float v = 0.55f + 0.40f * std::sin(u * 6.2831853f * 1.5f);
		hsv(300.0f * u, 0.85f, std::clamp(v, 0.15f, 0.95f), s.r, s.g, s.b);
		s.luma = 0.2126f * s.r + 0.7152f * s.g + 0.0722f * s.b;
		s.valid = true;
		out.push_back(s);
	}
}

void OrbitInjectHarness::capture(const std::string& name) {
	std::filesystem::create_directories(ofToDataPath(kDir, true));
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kDir) + "/" + name + ".png");
	const auto& m = runtime_.hudCompositorBridgeForTesting().renderer().orbitModelForDiagnostics();
	size_t valid = 0, gap = 0, miss = 0;
	for (size_t i = 0; i < m.history().count(); ++i) {
		const auto k = m.history().seg(i).kind;
		valid += k == hudpresent::SegKind::Valid; gap += k == hudpresent::SegKind::Gap; miss += k == hudpresent::SegKind::Missing;
	}
	lines_.push_back(name + ": bin/data/" + std::string(kDir) + "/" + name + ".png window " + ofToString(ofGetWidth()) + "x"
		+ ofToString(ofGetHeight()) + " skin=" + runtime_.currentSkinForTesting() + " gen=" + ofToString(m.generation())
		+ " state=" + ofToString(int(m.state())) + " segments valid=" + ofToString(valid) + " gap=" + ofToString(gap)
		+ " missing=" + ofToString(miss) + " chartSegs(R)=" + ofToString(m.chartSegCount(0)));
	ofLogNotice("OrbitInjectHarness") << lines_.back();
}

void OrbitInjectHarness::step(float) {
	if (done_) return;
	runtime_.draw();
	++frame_;
	if (isOc005()) { stepOc005(); return; }
	auto& bridge = runtime_.hudCompositorBridgeMutableForTesting();
	auto next = [&](int s) { stage_ = s; frame_ = 0; };
	switch (stage_) {
		case 0: // settle on the real runtime first (scene, skin, fonts)
			if (frame_ < 90) return;
			buildSeries(100, 1000.0, series_);
			view_ = {hudpresent::OrbitState::Valid, 1, 1, series_.data(), uint32_t(series_.size())};
			bridge.setOrbitSeriesOverrideForDiagnostics(&view_);
			next(1); return;
		case 1: if (frame_ < 10) return; capture("valid"); view_.state = hudpresent::OrbitState::Stale; view_.revision = 1; next(2); return;
		case 2: if (frame_ < 10) return; capture("stale"); // generation reset: partial startup history (12 samples)
			buildSeries(12, 5000.0, series_);
			view_ = {hudpresent::OrbitState::Valid, 2, 2, series_.data(), uint32_t(series_.size())};
			next(3); return;
		case 3: if (frame_ < 10) return; capture("reset"); bridge.setOrbitSeriesOverrideForDiagnostics(nullptr); next(4); return;
		case 4: if (frame_ < 10) return; capture("unavail"); next(5); return;
		default: {
			std::ofstream f(ofToDataPath(std::string(kDir) + "/results.txt", true));
			for (const auto& l : lines_) f << l << "\n";
			ofLogNotice("OrbitInjectHarness") << "done";
			done_ = true;
			ofExit(0);
		}
	}
}


// ---- OC-005 ---------------------------------------------------------------------------------------
void OrbitInjectHarness::check(const std::string& name, bool ok, const std::string& detail) {
	(ok ? oc005Passes_ : oc005Failures_)++;
	lines_.push_back(std::string(ok ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
	ofLogNotice("OrbitInjectHarness") << lines_.back();
}

void OrbitInjectHarness::buildOc005() {
	using namespace hudpresent;
	auto& bridge = runtime_.hudCompositorBridgeMutableForTesting();
	auto key = [this](int k) { runtime_.keyPressed(k); };
	auto cfgText = [&bridge]() {
		const auto& c = bridge.orbitUi().config();
		return std::string("A=") + orbitSourceText(c.a.source) + "/" + orbitMetricText(c.a.metric) + " B=" + orbitSourceText(c.b.source) + "/" + orbitMetricText(c.b.metric);
	};
	auto focused = [&bridge]() { const auto& f = bridge.focus().focused(); return f ? *f : std::string("none"); };
	auto is = [&bridge](OrbitSource as, OrbitMetric am, OrbitSource bs, OrbitMetric bm) {
		const auto& c = bridge.orbitUi().config();
		return c.a.source == as && c.a.metric == am && c.b.source == bs && c.b.metric == bm;
	};
	using S = OrbitSource; using M = OrbitMetric;
	const int kWait = 12; // > the 0.25 s OK debounce at 30 fps
	oc005_ = {
		{"settle", 1, [this, &bridge] {
			buildSeries(100, 1000.0, series_);
			view_ = {OrbitState::Valid, 1, 1, series_.data(), uint32_t(series_.size())};
			bridge.setOrbitSeriesOverrideForDiagnostics(&view_);
			rawSeries_ = series_; // RAW: independent colours (channels rotated) so FX vs RAW is visible
			for (auto& r : rawSeries_) { const float a = r.r; r.r = r.g; r.g = r.b; r.b = a; r.luma = 0.2126f * r.r + 0.7152f * r.g + 0.0722f * r.b; }
			rawView_ = {OrbitState::Valid, 1, 1, rawSeries_.data(), uint32_t(rawSeries_.size())};
			bridge.setOrbitRawSeriesOverrideForDiagnostics(&rawView_);
		}},
		{"01 default", 12, [this, is, &bridge, focused] {
			capture("oc005_01_default");
			check("defaults A=FX/AVG B=RAW/AVG", is(S::Fx, M::Avg, S::Raw, M::Avg));
			check("no focus before input", focused() == "none" && !bridge.orbitEditorOpen());
			rawView_.state = OrbitState::Unavailable; rawView_.count = 0; rawView_.samples = nullptr; rawView_.revision = 2; // RAW goes unavailable
		}},
		{"02 raw unavailable", 12, [this, &bridge] {
			capture("oc005_02_raw_unavailable");
			check("unavailable RAW draws no RAW data", !bridge.renderer().orbitModelForDiagnostics().raw().hasLatest);
			rawView_ = {OrbitState::Valid, 1, 3, rawSeries_.data(), uint32_t(rawSeries_.size())};
		}},
		{"03 focus A", 12, [this, key] { for (int i = 0; i < 8; ++i) key(OF_KEY_RIGHT); }}, // engage + 7 steps -> orbit.a
		{"03 focus A check", 12, [this, focused, &bridge] {
			capture("oc005_03_focus_a");
			check("arrows reach observation A as the 8th stop", focused() == "orbit.a", focused());
			check("A focus flag set, B not", bridge.renderer().orbitDraw().focusA && !bridge.renderer().orbitDraw().focusB);
		}},
		{"04 enter opens editor A", kWait, [this, key] { key(OF_KEY_RETURN); }},
		{"04 check", 12, [this, &bridge, focused] {
			capture("oc005_04_edit_a");
			check("Enter opens the editor for A", bridge.orbitEditorOpen() && bridge.orbitUi().target() == hudpresent::OrbitObs::A);
			check("overlay is region B", bridge.orbitUi().overlayRegion() == hudpresent::OrbitObs::B);
			check("focus stays on A while editing", focused() == "orbit.a", focused());
		}},
		{"05 arrow to RAW", kWait, [this, key] { key(OF_KEY_RIGHT); }},
		{"05 select RAW (preview)", kWait, [this, key] { key(OF_KEY_RETURN); }},
		{"05 check", 12, [this, is, cfgText] {
			capture("oc005_05_edit_a_preview_raw");
			check("selection previews immediately: A=RAW/AVG (identical to B)", is(S::Raw, M::Avg, S::Raw, M::Avg), cfgText());
		}},
		{"06 Tab x2 -> DOM, select", kWait, [this, key] { key('\t'); key('\t'); }},
		{"06 select DOM", kWait, [this, key] { key(OF_KEY_RETURN); }},
		{"06 check", 12, [this, is, cfgText, &bridge] {
			capture("oc005_06_edit_a_dom_unavailable");
			check("A=RAW/DOM previews (explicit unavailable)", is(S::Raw, M::Dom, S::Raw, M::Avg), cfgText());
			check("Tab moved the cursor (DOM)", bridge.orbitUi().cursor() == hudpresent::OrbitEditItem::MetDom);
		}},
		{"07 Esc cancels", kWait, [this] { runtime_.keyPressed(OF_KEY_ESC); }},
		{"07 check", 12, [this, is, cfgText, &bridge, focused] {
			capture("oc005_07_cancel_restored");
			check("Esc cancelled: prior configuration restored", is(S::Fx, M::Avg, S::Raw, M::Avg), cfgText());
			check("editor closed, runtime still running", !bridge.orbitEditorOpen());
			check("focus returned to the edited observation A", focused() == "orbit.a", focused());
		}},
		{"08 reopen A", kWait + 20, [this, key] { key(OF_KEY_RETURN); }}, // wait out the 0.6 s closing-Enter guard
		{"08 RAW", kWait, [this, key] { key(OF_KEY_RIGHT); }},
		{"08 select RAW", kWait, [this, key] { key(OF_KEY_RETURN); }},
		{"08 to VIB", kWait, [this, key] { key(OF_KEY_RIGHT); key(OF_KEY_RIGHT); key(OF_KEY_RIGHT); }},
		{"08 select VIB", kWait, [this, key] { key(OF_KEY_RETURN); }},
		{"08 to DONE", kWait, [this, key] { key(OF_KEY_DOWN); }},
		{"08 DONE commits", kWait, [this, key] { key(OF_KEY_RETURN); }},
		{"08 check", 12, [this, is, cfgText, &bridge, focused] {
			capture("oc005_08_mixed_committed");
			check("DONE commits: A=RAW/VIB B=RAW/AVG", is(S::Raw, M::Vib, S::Raw, M::Avg), cfgText());
			check("editor closed; focus on A", !bridge.orbitEditorOpen() && focused() == "orbit.a", focused());
		}},
		{"09 focus B", 40, [this, key] { key(OF_KEY_RIGHT); }},
		{"09 open B", kWait, [this, key] { key(OF_KEY_RETURN); }},
		{"09 check", 12, [this, &bridge] {
			capture("oc005_09_edit_b");
			check("Enter opens the editor for B; overlay in region A", bridge.orbitUi().target() == hudpresent::OrbitObs::B && bridge.orbitUi().overlayRegion() == hudpresent::OrbitObs::A);
		}},
		{"09 select FX", kWait, [this, key] { key(OF_KEY_UP); key(OF_KEY_UP); }}, // cursor RAW -> FX... (RAW -> FX is -1; extra -1 wraps to DONE)
		{"09 fix cursor", kWait, [this, key] { key(OF_KEY_RIGHT); }},                 // DONE -> FX
		{"09 select FX2", kWait, [this, key] { key(OF_KEY_RETURN); }},
		{"09 check2", 12, [this, is, cfgText] {
			capture("oc005_10_edit_b_preview_fx");
			check("B previews FX/AVG while A keeps RAW/VIB", is(S::Raw, M::Vib, S::Fx, M::Avg), cfgText());
		}},
		{"10 scene change while editing", kWait, [this] { runtime_.keyPressed(']'); }},
		{"10 wait", 60, [] {}},
		{"10 check", 12, [this, &bridge, is, cfgText] {
			capture("oc005_11_scene_change_while_editing");
			check("editor survives a scene change; configuration unchanged", bridge.orbitEditorOpen() && is(S::Raw, M::Vib, S::Fx, M::Avg), cfgText());
		}},
		{"11 Esc", kWait, [this] { runtime_.keyPressed(OF_KEY_ESC); }},
		{"11 check", 12, [this, &bridge, is, cfgText] {
			check("Esc restores B to its committed RAW/AVG", !bridge.orbitEditorOpen() && is(S::Raw, M::Vib, S::Raw, M::Avg), cfgText());
		}},
		{"12 immersive cancels editor", 40, [this, key] { key(OF_KEY_RETURN); }},
		{"12 press i", kWait + 20, [this] { runtime_.keyPressed('i'); }},
		{"12 check", 12, [this, &bridge, is, cfgText] {
			check("entering IMMERSIVE cancelled the editor", runtime_.immersiveForTesting() && !bridge.orbitEditorOpen() && is(S::Raw, M::Vib, S::Raw, M::Avg), cfgText());
			runtime_.keyPressed('x'); // any key exits immersive
		}},
		{"13 hide HUD cancels editor", 20, [this, key] { key(OF_KEY_RETURN); }},
		{"13 open", kWait + 20, [this, key] { key(OF_KEY_RETURN); }},
		{"13 hide", kWait, [this] { runtime_.keyPressed('h'); }},
		{"13 check", 12, [this, &bridge] {
			check("hiding the HUD cancelled the editor", !bridge.orbitEditorOpen() && !bridge.isVisible());
			runtime_.keyPressed('h');
		}},
		{"14 router", 4, [this] {
			InputRouter r;
			const auto fwd = r.translateToFocusInput('\t'), back = r.translateToFocusInput('\t', true);
			check("Tab = step +1, Shift+Tab = step -1", fwd && back && fwd->delta == 1 && back->delta == -1);
			check("Esc maps to the device-neutral Back", r.translateToDpad(OF_KEY_ESC) && *r.translateToDpad(OF_KEY_ESC) == DpadInput::Back);
		}},
		{"15 final", 12, [this] { capture("oc005_12_final"); }},
	};
}

void OrbitInjectHarness::stepOc005() {
	if (oc005_.empty()) {
		if (frame_ < 90) return;
		const char* ph = std::getenv("EXPERIENCE_RUNTIME_OC005_PHASE");
		if (ph) buildOc005Phase(ph); else buildOc005();
		oc005Index_ = 0;
		frame_ = 0;
	}
	if (oc005Index_ >= oc005_.size()) {
		std::filesystem::create_directories(ofToDataPath(kDir, true));
		std::ofstream f(ofToDataPath(std::string(kDir) + "/oc005-results.txt", true));
		for (const auto& l : lines_) f << l << "\n";
		f << "passes=" << oc005Passes_ << " failures=" << oc005Failures_ << "\n";
		ofLogNotice("OrbitInjectHarness") << "oc005 done: passes=" << oc005Passes_ << " failures=" << oc005Failures_;
		done_ = true;
		ofExit(oc005Failures_ ? 1 : 0);
		return;
	}
	Oc005Step& st = oc005_[oc005Index_];
	if (frame_ == 1) st.act();
	if (frame_ >= st.waitFrames + 1) { ++oc005Index_; frame_ = 0; }
}

// ---- OC-005 Stage 2 --------------------------------------------------------------------------------
std::string OrbitInjectHarness::cfgText() const {
	const auto& c = runtime_.hudCompositorBridgeForTesting().orbitUi().config();
	return std::string("A=") + hudpresent::orbitSourceText(c.a.source) + "/" + hudpresent::orbitMetricText(c.a.metric) + " B="
		+ hudpresent::orbitSourceText(c.b.source) + "/" + hudpresent::orbitMetricText(c.b.metric);
}
std::string OrbitInjectHarness::focusedId() const {
	const auto& f = runtime_.hudCompositorBridgeForTesting().focus().focused();
	return f ? *f : std::string("none");
}
std::string OrbitInjectHarness::settingsFile() const {
	const char* path = std::getenv("EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS");
	if (!path) return "(no path)";
	std::ifstream in(path);
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}
bool OrbitInjectHarness::cfgIs(hudpresent::OrbitSource as, hudpresent::OrbitMetric am, hudpresent::OrbitSource bs, hudpresent::OrbitMetric bm) const {
	const auto& c = runtime_.hudCompositorBridgeForTesting().orbitUi().config();
	return c.a.source == as && c.a.metric == am && c.b.source == bs && c.b.metric == bm;
}
void OrbitInjectHarness::installFixtures() {
	auto& bridge = runtime_.hudCompositorBridgeMutableForTesting();
	buildSeries(100, 1000.0, series_);
	view_ = {hudpresent::OrbitState::Valid, 1, 1, series_.data(), uint32_t(series_.size())};
	bridge.setOrbitSeriesOverrideForDiagnostics(&view_);
	rawSeries_ = series_;
	for (auto& r : rawSeries_) { const float a = r.r; r.r = r.g; r.g = r.b; r.b = a; r.luma = 0.2126f * r.r + 0.7152f * r.g + 0.0722f * r.b; }
	rawView_ = {hudpresent::OrbitState::Valid, 1, 1, rawSeries_.data(), uint32_t(rawSeries_.size())};
	bridge.setOrbitRawSeriesOverrideForDiagnostics(&rawView_);
}

void OrbitInjectHarness::buildOc005Phase(const std::string& phase) {
	using namespace hudpresent;
	using S = OrbitSource; using M = OrbitMetric;
	const int kWait = 36, kGuard = 90; // frames: > the 0.25 s OK debounce and > the 0.6 s closing-Enter guard even at ~120 fps (the frame rate is not pinned)
	auto key = [this](int k) { runtime_.keyPressed(k); };
	auto bridge = [this]() -> HudCompositorBridge& { return runtime_.hudCompositorBridgeMutableForTesting(); };
	auto cap = [this, phase](const std::string& n) { capture("oc005s2_" + phase + "_" + n); };
	auto hasKey = [this](const std::string& needle) { return settingsFile().find(needle) != std::string::npos; };

	if (phase == "write") {
		oc005_ = {
			{"fixtures", 1, [this] { installFixtures(); }},
			{"focus A", kWait, [key] { for (int i = 0; i < 8; ++i) key(OF_KEY_RIGHT); }},
			{"record file", 1, [this] { fileBefore_ = settingsFile(); }},
			{"open A", kWait, [key] { key(OF_KEY_RETURN); }},
			{"A: RAW", kWait, [key] { key(OF_KEY_RIGHT); }},
			{"A: select RAW", kWait, [key] { key(OF_KEY_RETURN); }},
			{"A: to VIB", kWait, [key] { key(OF_KEY_RIGHT); key(OF_KEY_RIGHT); key(OF_KEY_RIGHT); }},
			{"A: select VIB", kWait, [key] { key(OF_KEY_RETURN); }},
			{"preview check", 12, [this, cap] {
				cap("01_preview");
				check("preview shows A=RAW/VIB", cfgIs(S::Raw, M::Vib, S::Raw, M::Avg), cfgText());
				check("no persistence during preview (file unchanged)", settingsFile() == fileBefore_);
			}},
			{"Esc", kWait, [key] { key(OF_KEY_ESC); }},
			{"cancel check", 12, [this] {
				check("Esc rolls back to the defaults", cfgIs(S::Fx, M::Avg, S::Raw, M::Avg), cfgText());
				check("Esc does not persist (file unchanged)", settingsFile() == fileBefore_);
			}},
			{"reopen A", kGuard, [key] { key(OF_KEY_RETURN); }},
			{"A: RAW", kWait, [key] { key(OF_KEY_RIGHT); }},
			{"A: select RAW", kWait, [key] { key(OF_KEY_RETURN); }},
			{"A: to VIB", kWait, [key] { key(OF_KEY_RIGHT); key(OF_KEY_RIGHT); key(OF_KEY_RIGHT); }},
			{"A: select VIB", kWait, [key] { key(OF_KEY_RETURN); }},
			{"A: to DONE", kWait, [key] { key(OF_KEY_RIGHT); }},
			{"A: DONE", kWait, [key] { key(OF_KEY_RETURN); }},
			{"A committed", 20, [this, hasKey] {
				check("Done committed A=RAW/VIB", cfgIs(S::Raw, M::Vib, S::Raw, M::Avg), cfgText());
				const std::string f = settingsFile();
				check("Done persisted A = raw/vib and B = raw/avg (the other is preserved)",
					f.find("\"orbitObservations\"") != std::string::npos && f.find("\"raw\"") != std::string::npos && f.find("\"vib\"") != std::string::npos, f);
				fileBefore_ = f;
			}},
			{"focus B", kGuard, [key] { key(OF_KEY_RIGHT); }},
			{"open B", kWait, [key] { key(OF_KEY_RETURN); }},
			{"B: to FX", kWait, [key] { key(OF_KEY_LEFT); }},
			{"B: select FX", kWait, [key] { key(OF_KEY_RETURN); }},
			{"B: to DONE", kWait, [key] { key(OF_KEY_LEFT); }},
			{"B: DONE", kWait, [key] { key(OF_KEY_RETURN); }},
			{"B committed", 20, [this, cap] {
				cap("02_mixed_committed");
				check("Done committed B=FX/AVG; A keeps RAW/VIB", cfgIs(S::Raw, M::Vib, S::Fx, M::Avg), cfgText());
				const std::string f = settingsFile();
				ofJson j = ofJson::parse(f, nullptr, false);
				const bool ok = j.is_object() && j.contains("orbitObservations")
					&& j["orbitObservations"]["a"]["source"] == "raw" && j["orbitObservations"]["a"]["metric"] == "vib"
					&& j["orbitObservations"]["b"]["source"] == "fx" && j["orbitObservations"]["b"]["metric"] == "avg";
				check("file holds A = raw/vib and B = fx/avg", ok, f);
				fileBefore_ = f;
			}},
			{"scene change", kWait, [this, key] { key(']'); }},
			{"scene settled", 90, [] {}},
			{"after scene", 12, [this, cap] {
				cap("03_after_scene_change");
				check("scene change keeps the committed selection", cfgIs(S::Raw, M::Vib, S::Fx, M::Avg), cfgText());
				check("scene change writes nothing", settingsFile() == fileBefore_);
			}},
		};
	} else if (phase == "verify") {
		oc005_ = {
			{"fixtures", 1, [this] { installFixtures(); }},
			{"restored", 12, [this, cap] {
				cap("01_restored_after_restart");
				check("restart restored A=RAW/VIB, B=FX/AVG from the installation settings", cfgIs(S::Raw, M::Vib, S::Fx, M::Avg), cfgText());
				check("nothing is being edited after startup", !runtime_.hudCompositorBridgeForTesting().orbitEditorOpen());
				{
					const auto& d = runtime_.hudCompositorBridgeForTesting().renderer().orbitDraw();
					check("restored A = RAW/VIB resolves to NO source (explicit unavailable, never AVG, never FX)", hudpresent::orbitBoundSource(d, OrbitObs::A) == nullptr);
					check("restored B = FX/AVG resolves to the FX source model", hudpresent::orbitBoundSource(d, OrbitObs::B) == &d.model.fx());
				}
				fileBefore_ = settingsFile();
			}},
			{"nine-stop walk", 12, [this, key, cap] {
				std::string order;
				for (int i = 0; i < 9; ++i) { key(OF_KEY_RIGHT); order += (i ? " > " : "") + focusedId(); }
				check("nine-stop ring from the first engage; Orbit A = stop 8, Orbit B = stop 9",
					order == "controls.scene_previous > controls.scene_next > controls.mode > controls.media_previous > controls.media_next > controls.fullscreen > controls.menu > orbit.a > orbit.b", order);
				cap("02_focus_b_stop9");
				key(OF_KEY_RIGHT);
				check("wrap-around is deterministic: stop 10 = stop 1", focusedId() == "controls.scene_previous", focusedId());
				key(OF_KEY_LEFT);
				check("reverse wrap: stop 1 - 1 = Orbit B", focusedId() == "orbit.b", focusedId());
				key(OF_KEY_LEFT);
				check("reverse: Orbit B - 1 = Orbit A", focusedId() == "orbit.a", focusedId());
			}},
			{"isolation: open A", 12, [this, key] { mediaBefore_ = runtime_.currentHudFrameData().video && runtime_.currentHudFrameData().video->mediaId ? *runtime_.currentHudFrameData().video->mediaId : ""; genBefore_ = runtime_.lastFocusActivationForTesting().serial; key(OF_KEY_RETURN); }},
			{"isolation: navigate inside editor", kWait, [key] { key(OF_KEY_RIGHT); key(OF_KEY_DOWN); key('\t'); key(OF_KEY_UP); key(OF_KEY_LEFT); }},
			{"isolation: select", kWait, [key] { key(OF_KEY_RETURN); }},
			{"isolation check", 12, [this, bridge, cap] {
				const std::string m = runtime_.currentHudFrameData().video && runtime_.currentHudFrameData().video->mediaId ? *runtime_.currentHudFrameData().video->mediaId : "";
				check("editor navigation did not leak into media/scene controls", m == mediaBefore_ && runtime_.lastFocusActivationForTesting().serial == genBefore_ + 1 && runtime_.lastFocusActivationForTesting().command == "HudOrbit::OpenEditor",
					runtime_.lastFocusActivationForTesting().command);
				check("editor is still open and focus stayed on A", bridge().orbitEditorOpen() && focusedId() == "orbit.a");
				cap("03_editing_a");
			}},
			{"Esc", kWait, [key] { key(OF_KEY_ESC); }},
			{"Esc check", 12, [this] {
				check("Esc restored the persisted selection", cfgIs(S::Raw, M::Vib, S::Fx, M::Avg), cfgText());
				check("Esc wrote nothing", settingsFile() == fileBefore_);
			}},
			{"Settings coexistence: open editor", kGuard, [key] { key(OF_KEY_RETURN); }},
			{"Settings coexistence: F1", kWait, [key] { key(OF_KEY_F1); }},
			{"Settings check", 12, [this, bridge] {
				check("opening Settings cancelled the editor (Stage 1 behaviour) and the page owns input", !bridge().orbitEditorOpen() && runtime_.settingsPageForTesting().isOpen());
				check("selection still the persisted one", cfgIs(S::Raw, M::Vib, S::Fx, M::Avg), cfgText());
			}},
			{"Settings: Esc closes page", kWait, [key] { key(OF_KEY_ESC); }},
			{"Settings closed", 12, [this] {
				check("Settings closed with Esc; orbit editor is not open", !runtime_.settingsPageForTesting().isOpen() && !runtime_.hudCompositorBridgeForTesting().orbitEditorOpen());
			}},
		};
	} else if (phase == "invalid") {
		oc005_ = {
			{"fixtures", 1, [this] { installFixtures(); }},
			{"check", 12, [this, cap] {
				cap("01_invalid_file");
				const char* want = std::getenv("EXPERIENCE_RUNTIME_OC005_EXPECT");
				const std::string w = want ? want : "A=FX/AVG B=RAW/AVG";
				check("invalid/missing saved values fall back to the documented defaults: expected " + w, cfgText() == w, cfgText());
				check("runtime is alive and nothing is being edited", !runtime_.hudCompositorBridgeForTesting().orbitEditorOpen());
			}},
		};
	} else if (phase == "real") {
		// Real runtime telemetry: NO fixtures. The scene colour sampler (FX) is on by default; RAW needs EXPERIENCE_RUNTIME_VIDEO_COLOR=1.
		const bool rawOn = std::getenv("EXPERIENCE_RUNTIME_VIDEO_COLOR") != nullptr;
		oc005_ = {
			{"wait for samples", 360, [] {}},
			{"telemetry", 1, [this, rawOn, cap] {
				const auto& m = runtime_.hudCompositorBridgeForTesting().renderer().orbitModelForDiagnostics();
				check("REAL FX scene-colour series has a latest valid sample", m.fx().hasLatest && m.fx().state == hudpresent::OrbitState::Valid,
					"fx r,g,b = " + ofToString(m.fx().latest.r, 3) + "," + ofToString(m.fx().latest.g, 3) + "," + ofToString(m.fx().latest.b, 3));
				if (rawOn) check("REAL RAW series (VIDEO_COLOR=1) has a latest sample", m.raw().hasLatest, "raw r,g,b = " + ofToString(m.raw().latest.r, 3) + "," + ofToString(m.raw().latest.g, 3) + "," + ofToString(m.raw().latest.b, 3));
				else check("RAW off (default): RAW series Unavailable, never inferred from FX", !m.raw().hasLatest && m.raw().state == hudpresent::OrbitState::Unavailable);
				check("defaults: A=FX/AVG B=RAW/AVG", cfgIs(S::Fx, M::Avg, S::Raw, M::Avg), cfgText());
				cap("01_real_default");
				genBefore_ = m.fx().generation;
			}},
			// A -> RAW/AVG (B stays RAW/AVG): identical source+metric, real telemetry
			{"focus A", kWait, [key] { for (int i = 0; i < 8; ++i) key(OF_KEY_RIGHT); }},
			{"open A", kWait, [key] { key(OF_KEY_RETURN); }},
			{"A: RAW", kWait, [key] { key(OF_KEY_RIGHT); }},
			{"A: select RAW", kWait, [key] { key(OF_KEY_RETURN); }},
			{"identical check", 12, [this, bridge, cap] {
				const auto& d = runtime_.hudCompositorBridgeForTesting().renderer().orbitDraw();
				const auto* a = hudpresent::orbitBoundSource(d, OrbitObs::A);
				const auto* b = hudpresent::orbitBoundSource(d, OrbitObs::B);
				check("A and B both RAW/AVG bind to the same RAW source model", a && b && a == b && a == &d.model.raw(), cfgText());
				check("identical A/B resolves to the same measurement", a && b && a->hasLatest == b->hasLatest && a->latest.r == b->latest.r && a->latest.g == b->latest.g);
				cap("02_real_identical_raw_raw");
			}},
			// A -> FX, then different sources
			{"A: FX", kWait, [key] { key(OF_KEY_LEFT); }},
			{"A: select FX", kWait, [key] { key(OF_KEY_RETURN); }},
			{"A: to DONE", kWait, [key] { key(OF_KEY_LEFT); }},
			{"A: DONE", kWait, [key] { key(OF_KEY_RETURN); }},
			{"different check", 12, [this, rawOn, cap] {
				const auto& d = runtime_.hudCompositorBridgeForTesting().renderer().orbitDraw();
				const auto* a = hudpresent::orbitBoundSource(d, OrbitObs::A);
				const auto* b = hudpresent::orbitBoundSource(d, OrbitObs::B);
				check("A=FX/AVG and B=RAW/AVG bind to different source models", a == &d.model.fx() && b == &d.model.raw() && a != b, cfgText());
				check("FX is never filled into the RAW observation", b && (rawOn || !b->hasLatest));
				if (rawOn && a->hasLatest && b->hasLatest) check("FX and RAW are independent real measurements (distinct values)", a->latest.r != b->latest.r || a->latest.g != b->latest.g || a->latest.b != b->latest.b,
					"fx " + ofToString(a->latest.r, 3) + "," + ofToString(a->latest.g, 3) + "," + ofToString(a->latest.b, 3) + " raw " + ofToString(b->latest.r, 3) + "," + ofToString(b->latest.g, 3) + "," + ofToString(b->latest.b, 3));
				cap("03_real_fx_raw");
			}},
			{"B -> FX (same FX/FX)", kGuard, [key] { key(OF_KEY_RIGHT); }},
			{"open B", kWait, [key] { key(OF_KEY_RETURN); }},
			{"B: to FX", kWait, [key] { key(OF_KEY_LEFT); }},
			{"B: select FX", kWait, [key] { key(OF_KEY_RETURN); }},
			{"B: DONE", kWait, [key] { key(OF_KEY_LEFT); key(OF_KEY_RETURN); }},
			{"fx/fx check", 20, [this, cap] {
				const auto& d = runtime_.hudCompositorBridgeForTesting().renderer().orbitDraw();
				check("A and B both FX/AVG bind to the real FX source", hudpresent::orbitBoundSource(d, OrbitObs::A) == &d.model.fx() && hudpresent::orbitBoundSource(d, OrbitObs::B) == &d.model.fx(), cfgText());
				cap("04_real_identical_fx_fx");
			}},
			{"scene change", kWait, [key] { key(']'); }},
			{"scene settled + samples", 330, [] {}},
			{"after scene", 1, [this, cap] {
				const auto& m = runtime_.hudCompositorBridgeForTesting().renderer().orbitModelForDiagnostics();
				check("scene change: committed selection kept", cfgIs(S::Fx, M::Avg, S::Fx, M::Avg), cfgText());
				check("scene change: FX series generation advanced (new scene = new series)", m.fx().generation != genBefore_, ofToString(genBefore_) + " -> " + ofToString(m.fx().generation));
				check("scene change: FX valid again after fresh samples", m.fx().hasLatest);
				cap("05_real_after_scene_change");
			}},
		};
	}
}
