#include "PredominantHarness.h"

#include "HudCompositorBridge.h"
#include "OrbitDraw.h"
#include "ofAppRunner.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
constexpr const char* kDir = "captures/predominant";
const char* kSceneName[5] = {"blob-region-prototype", "temporal-fields", "blueprint-emergence", "fragment-trail", "quadrant-crosshair"};
double nowS() { return ofGetElapsedTimef(); }
}

bool PredominantHarness::isRequested() { return std::getenv("EXPERIENCE_RUNTIME_PREDOMINANT_HARNESS") != nullptr; }

PredominantHarness::PredominantHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	if (const char* v = std::getenv("EXPERIENCE_RUNTIME_PREDOMINANT_HARNESS")) { const float s = float(std::atof(v)); if (s >= 5.0f) sceneSeconds_ = s; }
	predExpected_ = rawvideo::videoColorEnabledFromEnv(std::getenv("EXPERIENCE_RUNTIME_PREDOMINANT_COLOR"));
	rawExpected_ = rawvideo::videoColorEnabledFromEnv(std::getenv("EXPERIENCE_RUNTIME_VIDEO_COLOR"));
	csv_ = "t,scene,fxGen,fxState,fxCount,fxAvg,fxDom,fxVib,rawGen,rawState,rawAvg,rawDom,rawVib,fxDomRGB,fxVibRGB,rawDomRGB,rawVibRGB,slotStates\n";
	std::filesystem::create_directories(ofToDataPath(kDir, true));
	ofLogNotice("PredominantHarness") << "start: " << sceneSeconds_ << " s/scene, predominant=" << predExpected_ << " raw=" << rawExpected_;
}

void PredominantHarness::check(const std::string& name, bool ok, const std::string& detail) {
	(ok ? passes_ : failures_)++;
	lines_.push_back(std::string(ok ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " - " + detail));
	ofLogNotice("PredominantHarness") << lines_.back();
}

void PredominantHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kDir) + "/" + name + ".png");
}

namespace {
std::string rgbs(bool ok, float r, float g, float b) {
	if (!ok) return "-";
	char buf[48];
	std::snprintf(buf, sizeof(buf), "%.3f/%.3f/%.3f", r, g, b);
	return buf;
}
}

void PredominantHarness::row() {
	const auto& fd = runtime_.currentHudFrameData();
	const auto* fx = fd.sceneColor.get();
	const auto* rw = fd.sceneColorRaw.get();
	const auto& od = runtime_.hudCompositorBridgeForTesting().renderer().orbitDraw();
	const auto& mdl = od.model;
	Agg& a = aggs_.back().second;
	if (!fx || fx->count == 0) return;
	const auto& n = fx->history[fx->count - 1];
	a.rows++;
	a.avg += n.valid;
	a.dom += n.valid && n.domValid;
	a.vib += n.valid && n.vibValid;
	a.vibInvalidAvgValid += n.valid && !n.vibValid;
	a.domValidVibInvalid += n.valid && n.domValid && !n.vibValid;
	a.domInvalidAvgValid += n.valid && !n.domValid;
	if (!fx->predominantProvided) a.notProvided++;
	// The Orbit model must carry exactly this snapshot's generation in every FX slot (no previous-generation leakage).
	for (size_t s : {size_t(0), size_t(2), size_t(4)}) {
		const auto& m = mdl.slot(s);
		if (s == 0 || m.provided) {
			if (m.generation != fx->generation) a.slotGenMismatch++;
			if (m.hasLatest && m.latest.t < fx->history[0].t - 1e-9) a.leak++;
		}
	}
	std::string raw = "-,-,-,-,-";
	std::string rd = "-", rv = "-";
	if (rw && rw->count > 0) {
		const auto& rn = rw->history[rw->count - 1];
		a.rawRows++; a.rawAvg += rn.valid; a.rawDom += rn.valid && rn.domValid; a.rawVib += rn.valid && rn.vibValid;
		a.rawVibInvalidAvgValid += rn.valid && !rn.vibValid;
		for (size_t s : {size_t(1), size_t(3), size_t(5)}) {
			const auto& m = mdl.slot(s);
			if (s == 1 || m.provided) { if (m.generation != rw->generation) a.slotGenMismatch++; if (m.hasLatest && m.latest.t < rw->history[0].t - 1e-9) a.leak++; }
		}
		raw = ofToString(rw->generation) + "," + std::to_string(int(rw->state)) + "," + std::to_string(int(rn.valid)) + "," + std::to_string(int(rn.domValid)) + "," + std::to_string(int(rn.vibValid));
		rd = rgbs(rn.domValid, rn.domR, rn.domG, rn.domB);
		rv = rgbs(rn.vibValid, rn.vibR, rn.vibG, rn.vibB);
	}
	std::ostringstream o;
	o << ofToString(nowS() - t0_, 2) << ',' << sceneId_ << ',' << fx->generation << ',' << int(fx->state) << ',' << fx->count << ',' << int(n.valid) << ',' << int(n.domValid) << ',' << int(n.vibValid)
	  << ',' << raw << ',' << rgbs(n.domValid, n.domR, n.domG, n.domB) << ',' << rgbs(n.vibValid, n.vibR, n.vibG, n.vibB) << ',' << rd << ',' << rv << ',';
	for (size_t s = 0; s < hudpresent::kOrbitSlotCount; ++s) o << (mdl.slot(s).provided ? int(mdl.slot(s).state) : -1) << (s + 1 < hudpresent::kOrbitSlotCount ? ":" : "");
	csv_ += o.str() + "\n";
}

void PredominantHarness::step(float dt) {
	if (done_) return;
	runtime_.draw();
	++frame_;
	if (frame_ == 1) t0_ = nowS();
	if (frame_ > 120) frameMs_.push_back(dt * 1000.0f);
	if (frame_ < 120) return; // settle: scene, skin, fonts, first samples
	if (aggs_.empty()) {
		sceneStart_ = nowS();
		sceneId_ = runtime_.sceneManagerForTesting().status().activeSceneId;
		aggs_.push_back({sceneId_, {}});
	}
	if (!inCombos_) {
		if (nowS() - lastRow_ >= 0.5) { lastRow_ = nowS(); row(); }
		const bool idle = runtime_.sceneManagerForTesting().status().transitionPhase == SceneTransitionPhase::Idle;
		if (nowS() - sceneStart_ >= sceneSeconds_ && idle) {
			capture("scene_" + std::to_string(sceneIdx_) + "_" + sceneId_);
			if (sceneIdx_ >= 4) { inCombos_ = true; comboIdx_ = 0; comboStart_ = 0; return; }
			++sceneIdx_;
			runtime_.keyPressed(']');
			sceneStart_ = nowS();
			sceneId_ = "?";
		}
		const std::string cur = runtime_.sceneManagerForTesting().status().activeSceneId;
		if (sceneId_ == "?" && cur != aggs_.back().first && runtime_.sceneManagerForTesting().status().transitionPhase == SceneTransitionPhase::Idle) {
			sceneId_ = cur;
			aggs_.push_back({cur, {}});
			sceneStart_ = nowS();
		}
		return;
	}
	// ---- Orbit A/B: all six (source, metric) combinations through the real observation configuration ----------
	using namespace hudpresent;
	auto& bridge = runtime_.hudCompositorBridgeMutableForTesting();
	const OrbitSource srcs[2] = {OrbitSource::Fx, OrbitSource::Raw};
	const OrbitMetric mets[3] = {OrbitMetric::Avg, OrbitMetric::Dom, OrbitMetric::Vib};
	if (comboIdx_ < 6) {
		if (comboStart_ == 0) {
			const OrbitObservationConfig A { srcs[comboIdx_ % 2], mets[comboIdx_ / 2] };
			const int nb = (comboIdx_ + 3) % 6; // B = a different combination
			const OrbitObservationConfig B { srcs[nb % 2], mets[nb / 2] };
			OrbitConsoleConfig c; c.a = A; c.b = B;
			bridge.orbitUi().setCommitted(c);
			comboStart_ = nowS();
			return;
		}
		if (nowS() - comboStart_ < 2.0) return;
		const OrbitConsoleConfig c = bridge.orbitUi().config();
		const auto& od = bridge.renderer().orbitDraw();
		auto tag = [&](const OrbitObservationConfig& o) { return std::string(orbitSourceText(o.source)) + "/" + orbitMetricText(o.metric); };
		for (OrbitObs ob : {OrbitObs::A, OrbitObs::B}) {
			const OrbitObservationConfig& o = c.at(ob);
			const OrbitSourceModel* bound = orbitBoundSource(od, ob);
			const OrbitSourceModel& slot = od.model.slot(orbitSlotOf(o.source, o.metric));
			const bool avg = o.metric == OrbitMetric::Avg;
			const bool shouldBeProvided = avg || (predExpected_ && (o.source == OrbitSource::Fx || rawExpected_));
			const bool sourceExists = o.source == OrbitSource::Fx || rawExpected_;
			std::string what = std::string(orbitObsText(ob)) + "=" + tag(o);
			check(what + " binds to its own slot (never AVG, never the other source)", bound == (shouldBeProvided ? &slot : nullptr) || (avg && bound == &slot), what);
			if (shouldBeProvided && sourceExists) {
				check(what + " state/data: provided and reports " + (slot.hasLatest ? "a live colour" : "no colour"), bound != nullptr,
					"state=" + std::to_string(int(slot.state)) + " hasLatest=" + std::to_string(slot.hasLatest) + " gen=" + std::to_string(slot.generation) +
					" rgb(enc)=" + rgbs(slot.hasLatest, slot.latest.r, slot.latest.g, slot.latest.b));
			} else {
				check(what + " is NOT AVAILABLE / no source as expected", !avg ? bound == nullptr : !slot.hasLatest, "predExpected=" + std::to_string(predExpected_) + " rawExpected=" + std::to_string(rawExpected_));
			}
		}
		std::string fname = "orbit_combo_" + std::to_string(comboIdx_) + "_A_" + tag(c.a) + "_B_" + tag(c.b);
		std::replace(fname.begin(), fname.end(), '/', '-');
		capture(fname);
		++comboIdx_;
		comboStart_ = 0;
		return;
	}
	finish();
}

void PredominantHarness::finish() {
	using namespace hudpresent;
	std::ostringstream o;
	o << "== OC-003 desktop harness (REAL runtime, no injected series) ==\n";
	o << "predominant requested=" << predExpected_ << " raw requested=" << rawExpected_ << " sceneSeconds=" << sceneSeconds_ << "\n\n";
	const auto& fxS = runtime_.sceneColorForTesting().stats();
	const auto& rwS = runtime_.videoColorForTesting().stats();
	const auto& fxSampler = runtime_.sceneColorForTesting();
	const auto& rwSampler = runtime_.videoColorForTesting();
	for (const auto& [id, a] : aggs_) {
		o << "scene " << id << ": rows=" << a.rows << " FX avgValid=" << a.avg << " domValid=" << a.dom << " vibValid=" << a.vib
		  << " | vibInvalid&avgValid=" << a.vibInvalidAvgValid << " domValid&vibInvalid=" << a.domValidVibInvalid << " domInvalid&avgValid=" << a.domInvalidAvgValid
		  << " | RAW rows=" << a.rawRows << " avg=" << a.rawAvg << " dom=" << a.rawDom << " vib=" << a.rawVib << " vibInvalid&avgValid=" << a.rawVibInvalidAvgValid
		  << " | slotGenMismatch=" << a.slotGenMismatch << " prevGenLeak=" << a.leak << " snapshotNotProvided=" << a.notProvided << "\n";
		check("scene " + id + ": no Orbit slot carries another generation", a.slotGenMismatch == 0);
		check("scene " + id + ": no previous-generation DOM/VIB/AVG in the Orbit slots", a.leak == 0);
		check("scene " + id + ": FX/AVG measured", a.rows > 0 && a.avg > 0);
		if (predExpected_) check("scene " + id + ": FX DOM measured (published)", a.rows > 0 && a.dom > 0, "domValid=" + std::to_string(a.dom) + "/" + std::to_string(a.rows));
		else check("scene " + id + ": predominant OFF => snapshot says not provided and no DOM/VIB", a.notProvided == a.rows && a.dom == 0 && a.vib == 0);
	}
	auto stat = [&](std::vector<float> v) {
		if (v.empty()) return std::string("n/a");
		std::sort(v.begin(), v.end());
		double m = 0; for (float x : v) m += x;
		char b[160]; std::snprintf(b, sizeof(b), "n=%zu mean=%.3f p50=%.3f p95=%.3f p99=%.3f max=%.3f ms", v.size(), m / v.size(), v[v.size() / 2], v[size_t(v.size() * 0.95)], v[size_t(v.size() * 0.99)], v.back());
		return std::string(b);
	};
	o << "\nframe time: " << stat(frameMs_) << "\n";
	auto src = [&](const char* name, const scenecolorsampling::SceneColorSampler& sampler) {
		const scenecolorsampling::Stats& s = sampler.stats();
		o << name << ": issued=" << s.issued << " completed=" << s.completed << " gaps=" << s.gaps << " gpuReadbacks=" << s.gpuReadbacks << " readbackBytes=" << s.gpuReadbackBytes
		  << " readbacksPerCompleted=" << (s.completed ? double(s.gpuReadbacks) / double(s.completed) : 0.0) << " bytesPerReadback=" << (s.gpuReadbacks ? double(s.gpuReadbackBytes) / double(s.gpuReadbacks) : 0.0)
		  << " | wait p50/p95/max=" << s.wait.pct(0.5) << "/" << s.wait.pct(0.95) << "/" << s.wait.max << " ms convert p50/p95/max=" << s.convert.pct(0.5) << "/" << s.convert.pct(0.95) << "/" << s.convert.max
		  << " ms | predominantCpu n=" << s.predominantCpu.n << " p50/p95/max=" << s.predominantCpu.pct(0.5) << "/" << s.predominantCpu.pct(0.95) << "/" << s.predominantCpu.max << " ms"
		  << " | domValid=" << s.domValid << " vibValid=" << s.vibValid << " vibInvalid=" << s.vibInvalid
		  << " | " << sampler.readbackSummary() << " | submit p50/p95/max=" << s.submit.pct(0.5) << "/" << s.submit.pct(0.95) << "/" << s.submit.max
		  << " latency p50/p95/max=" << s.latency.pct(0.5) << "/" << s.latency.pct(0.95) << "/" << s.latency.max << " total p50/p95/max=" << s.total.pct(0.5) << "/" << s.total.pct(0.95) << "/" << s.total.max
		  << " dropGen=" << s.droppedGeneration << " discardTimeout=" << s.discardedTimeout << " discardWaitFailed=" << s.discardedWaitFailed << "\n";
	};
	src("FX  sampler", fxSampler);
	src("RAW sampler", rwSampler);
	(void)fxS; (void)rwS;
	o << "\nchecks: " << passes_ << " passed, " << failures_ << " failed\n";
	for (const auto& l : lines_) o << l << "\n";
	std::ofstream(ofToDataPath(std::string(kDir) + "/results.txt", true)) << o.str();
	std::ofstream(ofToDataPath(std::string(kDir) + "/rows.csv", true)) << csv_;
	ofLogNotice("PredominantHarness") << "done: " << passes_ << " passed, " << failures_ << " failed";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
