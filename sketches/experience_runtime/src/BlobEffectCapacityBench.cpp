#include "BlobEffectCapacityBench.h"

#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofFileUtils.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

using namespace blobcapacity;

namespace {

	constexpr const char * kLog = "BlobCapacity";

	const char * env(const char * name) {
		const char * v = std::getenv(name);
		return (v != nullptr && *v != '\0') ? v : nullptr;
	}

	// Cheap sysfs reads, once per 5 s window. "NA" where unavailable (macOS,
	// or a Pi kernel without the firmware node — use vcgencmd instead).
	std::string readFirstLine(const char * path) {
		std::ifstream in(path);
		std::string line;
		if (!in || !std::getline(in, line)) {
			return "NA";
		}
		return line;
	}

	std::string temperatureC() {
		std::string raw = readFirstLine("/sys/class/thermal/thermal_zone0/temp");
		if (raw == "NA") {
			return raw;
		}
		return ofToString(std::atoi(raw.c_str()) / 1000.0, 1);
	}

	std::string throttled() {
		return readFirstLine("/sys/devices/platform/soc/soc:firmware/get_throttled");
	}

	std::string joinSet(const std::set<std::string> & s) {
		std::string out;
		for (const std::string & e : s) {
			out += (out.empty() ? "" : "+") + e;
		}
		return out.empty() ? std::string("-") : out;
	}

	const char * kTsvHeader =
		"timestamp\tscript_step\tload\tcase_id\tmode\teffect_set\teffects\tmedia_id\tmarked"
		"\tshader_fragment_target\tactual_total_fragments\tactual_shader_fragments\tactual_original_fragments"
		"\tdistinct_shader_count\teffects_used\tframes\tfps\tframe_ms\tscene_draw_ms\tregion_render_ms"
		"\tbackground_render_ms\tdetector_ms\ttracker_ms\tregion_update_ms\tdirect_draws\tlegacy_draws"
		"\tunshaded_draws\tshader_binds\tshader_switches\tstalls\tmax_stall_ms\ttemp_c\tthrottled";

} // namespace

bool BlobEffectCapacityBench::isRequested() {
	return env("EXPERIENCE_RUNTIME_BLOB_EFFECT_CAPACITY") != nullptr;
}

void BlobEffectCapacityBench::setup() {
	if (const char * m = env("EXPERIENCE_RUNTIME_BLOB_CAPACITY_MEDIA")) {
		wantedMedia_ = m;
	}
	if (const char * fx = env("EXPERIENCE_RUNTIME_BLOB_CAPACITY_EFFECTS")) {
		for (const std::string & id : splitList(fx, ',')) {
			if (findEffect(id) != nullptr) {
				customEffects_.push_back(id);
			} else {
				ofLogError(kLog) << "EXPERIENCE_RUNTIME_BLOB_CAPACITY_EFFECTS: unknown effect '" << id << "' ignored";
			}
		}
	}
	if (const char * load = env("EXPERIENCE_RUNTIME_BLOB_CAPACITY_LOAD")) {
		syntheticLoad_ = std::string(load) != "tracked";
	}
	if (const char * n = env("EXPERIENCE_RUNTIME_BLOB_CAPACITY_FRAGMENTS")) {
		syntheticCount_ = std::max(0, std::min(kSlots, std::atoi(n)));
	}
	if (const char * sc = env("EXPERIENCE_RUNTIME_BLOB_CAPACITY_REGION_SCALE")) {
		regionScale_ = std::max(0.25f, std::min(2.0f, static_cast<float>(std::atof(sc))));
	}
	if (const char * w = env("EXPERIENCE_RUNTIME_BLOB_CAPACITY_WARMUP")) {
		scriptWarmupSeconds_ = std::max(0.0f, static_cast<float>(std::atof(w)));
	}
	openTsv();

	ofLogNotice(kLog) << "EXPERIMENT ONLY — Blob multi-effect capacity bench enabled (EXPERIENCE_RUNTIME_BLOB_EFFECT_CAPACITY)";
	ofLogNotice(kLog) << "keys: F1-F7 presets A-G | , . shader slots -/+ | - = distinct -/+ | q w e r t y u effect sets"
					  << " | j l single-effect focus | m assignment mode (single/round_robin/weighted/budget) | g tracked/synthetic load | z x synthetic count -/+"
					  << " | b mark window + capture | i show config";

	Plan initial;
	if (const char * script = env("EXPERIENCE_RUNTIME_BLOB_CAPACITY_SCRIPT")) {
		std::string error;
		if (parseScript(script, script_, error)) {
			scriptIndex_ = 0;
			ofLogNotice(kLog) << "SCRIPT " << script_.size() << " steps (warmup " << scriptWarmupSeconds_ << " s each): " << script;
			applyPlan(script_[0].plan, "script step 1/" + ofToString(script_.size()) + " " + script_[0].spec);
			summaries_.push_back({ script_[0].spec, script_[0].plan.fields() + " load=" + loadLabel() });
			return;
		}
		ofLogError(kLog) << "EXPERIENCE_RUNTIME_BLOB_CAPACITY_SCRIPT invalid (" << error << ") — interactive mode";
	}
	const char * start = env("EXPERIENCE_RUNTIME_BLOB_EFFECT_CAPACITY");
	char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(start[0])));
	if (!preset(letter, initial)) {
		preset('A', initial);
	}
	applyPlan(initial, "startup");
}

void BlobEffectCapacityBench::openTsv() {
	std::string dir = ofToDataPath("captures/blob_capacity", true);
	ofDirectory::createDirectory(dir, false, true);
	tsvPath_ = dir + "/blob_capacity_" + ofGetTimestampString("%Y%m%d-%H%M%S") + ".tsv";
	tsv_.open(tsvPath_);
	if (tsv_) {
		tsv_ << kTsvHeader << "\n";
		tsv_.flush();
		ofLogNotice(kLog) << "TSV: " << tsvPath_;
	} else {
		ofLogError(kLog) << "could not open " << tsvPath_ << " — log lines only";
	}
}

std::string BlobEffectCapacityBench::loadLabel() const {
	return syntheticLoad_ ? "synthetic:" + ofToString(syntheticCount_) + "@x" + ofToString(regionScale_, 2) : std::string("tracked");
}

void BlobEffectCapacityBench::logBanner() const {
	std::istringstream lines(plan_.banner());
	std::string line;
	while (std::getline(lines, line)) {
		ofLogNotice(kLog) << line;
	}
	ofLogNotice(kLog) << "load=" << loadLabel();
}

const std::vector<VideoRegion> & BlobEffectCapacityBench::syntheticRegions(float t) {
	// 5 x 2 grid in normalized source space; each region drifts slowly
	// (crops move like real blobs) and neighbours occasionally overlap.
	synthetic_.resize(static_cast<size_t>(syntheticCount_));
	float w = 0.18f * regionScale_;
	float h = 0.24f * regionScale_;
	for (int i = 0; i < syntheticCount_; i++) {
		float cx = 0.1f + 0.2f * static_cast<float>(i % 5) + 0.025f * std::sin(t * 0.5f + i);
		float cy = (i < 5 ? 0.3f : 0.7f) + 0.03f * std::cos(t * 0.4f + i * 1.3f);
		VideoRegion & r = synthetic_[i];
		r.id = i + 1;
		r.normalizedSourceBounds = ofRectangle(cx - w * 0.5f, cy - h * 0.5f, w, h);
		r.smoothedNormalizedBounds = r.normalizedSourceBounds;
		r.confidence = 1.0f;
	}
	return synthetic_;
}

void BlobEffectCapacityBench::applyPlan(const Plan & plan, const std::string & why) {
	plan_ = plan;
	resetRequested_ = true;
	window_ = Window {};
	warmupRemaining_ = scriptIndex_ >= 0 ? scriptWarmupSeconds_ : interactiveWarmupSeconds_;
	ofLogNotice(kLog) << "---- configuration change (" << why << ") ----";
	logBanner();
	ofLogNotice(kLog) << "warmup " << warmupRemaining_ << " s (fragments re-acquire), then " << kWindowSeconds << " s windows";
}

bool BlobEffectCapacityBench::takeResetRequest() {
	bool r = resetRequested_;
	resetRequested_ = false;
	return r;
}

bool BlobEffectCapacityBench::takeCaptureRequest(std::string & label) {
	if (captureLabel_.empty()) {
		return false;
	}
	label = captureLabel_;
	captureLabel_.clear();
	return true;
}

void BlobEffectCapacityBench::focusEffect(int step) {
	const auto & table = effectTable();
	int n = static_cast<int>(table.size());
	focusIndex_ = focusIndex_ < 0 ? (step > 0 ? 0 : n - 1) : ((focusIndex_ + step) % n + n) % n;
	Plan p = plan_;
	p.caseId = "focus:" + table[focusIndex_].id;
	p.setName = "single:" + table[focusIndex_].id;
	p.effectSet = { table[focusIndex_].id };
	p.distinct = 1;
	p.mode = AssignmentMode::Single;
	if (p.shaderSlots == 0) {
		p.shaderSlots = kSlots;
	}
	applyPlan(p, "focus effect");
}

bool BlobEffectCapacityBench::handleKey(int key) {
	bool configKey = false;
	Plan p = plan_;
	std::string why;

	if (key >= OF_KEY_F1 && key <= OF_KEY_F7) {
		configKey = preset(static_cast<char>('A' + (key - OF_KEY_F1)), p);
		why = "preset";
	} else {
		switch (key) {
			case ',':
			case '<':
				p.shaderSlots = std::max(0, p.shaderSlots - 1);
				configKey = true;
				why = "shader slots";
				break;
			case '.':
			case '>':
				p.shaderSlots = std::min(kSlots, p.shaderSlots + 1);
				configKey = true;
				why = "shader slots";
				break;
			case '-':
			case '_':
				p.distinct = std::max(1, std::min(p.distinct, static_cast<int>(p.effectSet.size())) - 1);
				configKey = true;
				why = "distinct";
				break;
			case '=':
			case '+':
				p.distinct = std::min(static_cast<int>(p.effectSet.size()), p.distinct + 1);
				configKey = true;
				why = "distinct";
				break;
			case 'q': case 'w': case 'e': case 'r': case 't': case 'y':
				for (const EffectSet & s : effectSets()) {
					if (s.key == key) {
						p.setName = s.name;
						p.effectSet = s.effects;
						p.distinct = std::min(std::max(1, p.distinct), static_cast<int>(s.effects.size()));
					}
				}
				configKey = true;
				why = "effect set";
				break;
			case 'u':
				if (customEffects_.empty()) {
					ofLogNotice(kLog) << "no custom set — set EXPERIENCE_RUNTIME_BLOB_CAPACITY_EFFECTS=a,b,c";
					return true;
				}
				p.setName = "custom";
				p.effectSet = customEffects_;
				p.distinct = static_cast<int>(customEffects_.size());
				configKey = true;
				why = "effect set";
				break;
			case 'j':
			case 'l':
				if (scriptIndex_ >= 0) {
					ofLogNotice(kLog) << "script running — config keys ignored";
					return true;
				}
				focusEffect(key == 'l' ? 1 : -1);
				return true;
			case 'm': {
				p.mode = p.mode == AssignmentMode::Single ? AssignmentMode::RoundRobin
					: p.mode == AssignmentMode::RoundRobin ? AssignmentMode::Weighted
					: p.mode == AssignmentMode::Weighted ? AssignmentMode::Budget
					: AssignmentMode::Single;
				if (p.mode == AssignmentMode::Weighted) {
					if (const char * w = env("EXPERIENCE_RUNTIME_BLOB_CAPACITY_WEIGHTS")) {
						p.weights = parseWeights(w);
					}
					if (p.weights.empty()) {
						p.weights = { { "original", 5 }, { "recolor", 5 } };
					}
				}
				configKey = true;
				why = "assignment mode";
				break;
			}
			case 'b':
			case 'B':
				markNext_ = true;
				captureLabel_ = "mark_" + plan_.caseId + "_" + ofGetTimestampString("%H%M%S");
				ofLogNotice(kLog) << "MARK armed: next completed window is marked=1; capture " << captureLabel_;
				return true;
			case 'i':
			case 'I':
				logBanner();
				ofLogNotice(kLog) << "TSV: " << tsvPath_;
				return true;
			case 'g':
			case 'G':
				syntheticLoad_ = !syntheticLoad_;
				configKey = true;
				why = "load source";
				break;
			case 'z':
			case 'x':
				syntheticCount_ = std::max(0, std::min(kSlots, syntheticCount_ + (key == 'x' ? 1 : -1)));
				configKey = true;
				why = "synthetic count";
				break;
			default:
				return false;
		}
	}
	if (!configKey) {
		return false;
	}
	if (scriptIndex_ >= 0) {
		ofLogNotice(kLog) << "script running — config keys ignored";
		return true;
	}
	if (why != "preset" && p.caseId.rfind("manual", 0) != 0) {
		p.caseId = "manual";
	}
	focusIndex_ = -1;
	applyPlan(p, why);
	return true;
}

void BlobEffectCapacityBench::recordFrame(const FrameSample & s) {
	float dt = s.frameSeconds;
	if (scriptIndex_ >= 0) {
		advanceScript(dt);
	}
	if (warmupRemaining_ > 0.0f) {
		warmupRemaining_ -= dt;
		return;
	}

	Window & w = window_;
	w.elapsedSeconds += dt;
	w.mediaId = s.mediaId;
	if (dt > kStallSeconds) {
		// Media-load stalls stay out of the renderer averages.
		w.stalls++;
		w.maxStallMs = std::max(w.maxStallMs, static_cast<double>(dt) * 1000.0);
	} else {
		w.frames++;
		w.frameMs += dt * 1000.0;
		w.sceneDrawMs += s.sceneDrawMs;
		w.detectorMs += s.detectorMs;
		w.trackerMs += s.trackerMs;
		w.regionUpdateMs += s.regionUpdateMs;
		const auto & f = s.fragment;
		w.regionRenderMs += f.sourceMs + f.shaderDirectMs + f.shaderResultMs + f.compositeMs;
		const auto & b = s.background;
		w.backgroundMs += b.sourceMs + b.shaderDirectMs + b.shaderResultMs + b.compositeMs;
		w.directDraws += f.directDraws;
		w.legacyDraws += f.legacyDraws;
		w.unshadedDraws += f.unshadedDraws;
		w.shaderBinds += f.shaderBinds;
		w.shaderSwitches += f.shaderSwitches;
		w.distinctUsed += f.distinctPrograms;
		if (s.fragmentEffects != nullptr) {
			int original = 0;
			for (const std::string & e : *s.fragmentEffects) {
				if (e.empty()) {
					original++;
				} else {
					w.effectsUsed.insert(e);
				}
			}
			int total = static_cast<int>(s.fragmentEffects->size());
			w.fragments += total;
			w.originalFragments += original;
			w.shaderFragments += total - original;
		}
	}
	if (w.elapsedSeconds >= kWindowSeconds) {
		closeWindow();
	}
}

void BlobEffectCapacityBench::closeWindow() {
	Window & w = window_;
	if (w.frames == 0) {
		window_ = Window {};
		return;
	}
	double n = w.frames;
	double fps = w.frameMs > 0.0 ? 1000.0 * n / w.frameMs : 0.0;
	std::string temp = temperatureC();
	std::string thr = throttled();
	std::string step = scriptIndex_ >= 0 ? ofToString(scriptIndex_ + 1) : std::string("-");
	bool marked = markNext_;
	markNext_ = false;

	ofLogNotice(kLog) << "WINDOW ts=" << ofGetTimestampString("%Y-%m-%dT%H:%M:%S") << " step=" << step << " " << plan_.fields()
					  << " load=" << loadLabel()
					  << " media=" << (w.mediaId.empty() ? std::string("-") : w.mediaId)
					  << " fragments=" << ofToString(w.fragments / n, 2)
					  << " shader=" << ofToString(w.shaderFragments / n, 2)
					  << " original=" << ofToString(w.originalFragments / n, 2)
					  << " distinctUsed=" << ofToString(w.distinctUsed / n, 2)
					  << " effectsUsed=" << joinSet(w.effectsUsed)
					  << " frames=" << w.frames
					  << " fps=" << ofToString(fps, 2)
					  << " frameMs=" << ofToString(w.frameMs / n, 3)
					  << " sceneDrawMs=" << ofToString(w.sceneDrawMs / n, 3)
					  << " regionRenderMs=" << ofToString(w.regionRenderMs / n, 3)
					  << " backgroundMs=" << ofToString(w.backgroundMs / n, 3)
					  << " detectorMs=" << ofToString(w.detectorMs / n, 3)
					  << " trackerMs=" << ofToString(w.trackerMs / n, 3)
					  << " directDraws=" << ofToString(w.directDraws / n, 2)
					  << " legacyDraws=" << ofToString(w.legacyDraws / n, 2)
					  << " unshadedDraws=" << ofToString(w.unshadedDraws / n, 2)
					  << " shaderBinds=" << ofToString(w.shaderBinds / n, 2)
					  << " shaderSwitches=" << ofToString(w.shaderSwitches / n, 2)
					  << " stalls=" << w.stalls << " maxStallMs=" << ofToString(w.maxStallMs, 0)
					  << " tempC=" << temp << " throttled=" << thr
					  << (marked ? " MARKED" : "");

	if (tsv_) {
		std::vector<std::string> effects;
		if (plan_.mode == AssignmentMode::Weighted) {
			for (const auto & p : plan_.weights) effects.push_back(p.first + ":" + ofToString(p.second));
		} else if (plan_.plannedShaderSlots() > 0) {
			effects = plan_.activeEffects();
		}
		tsv_ << ofGetTimestampString("%Y-%m-%dT%H:%M:%S") << "\t" << step << "\t" << loadLabel() << "\t" << plan_.caseId << "\t" << toString(plan_.mode)
			 << "\t" << plan_.setName << "\t" << (effects.empty() ? std::string("-") : ofJoinString(effects, "+")) << "\t" << w.mediaId << "\t" << (marked ? 1 : 0)
			 << "\t" << plan_.plannedShaderSlots() << "\t" << w.fragments / n << "\t" << w.shaderFragments / n << "\t" << w.originalFragments / n
			 << "\t" << w.distinctUsed / n << "\t" << joinSet(w.effectsUsed) << "\t" << w.frames << "\t" << fps << "\t" << w.frameMs / n
			 << "\t" << w.sceneDrawMs / n << "\t" << w.regionRenderMs / n << "\t" << w.backgroundMs / n << "\t" << w.detectorMs / n
			 << "\t" << w.trackerMs / n << "\t" << w.regionUpdateMs / n << "\t" << w.directDraws / n << "\t" << w.legacyDraws / n
			 << "\t" << w.unshadedDraws / n << "\t" << w.shaderBinds / n << "\t" << w.shaderSwitches / n << "\t" << w.stalls
			 << "\t" << w.maxStallMs << "\t" << temp << "\t" << thr << "\n";
		tsv_.flush();
	}

	if (scriptIndex_ >= 0 && scriptIndex_ < static_cast<int>(summaries_.size())) {
		StepSummary & sum = summaries_[scriptIndex_];
		sum.windows++;
		sum.fps += fps;
		sum.frameMs += w.frameMs / n;
		sum.regionRenderMs += w.regionRenderMs / n;
		sum.backgroundMs += w.backgroundMs / n;
		sum.fragments += w.fragments / n;
		sum.shaderFragments += w.shaderFragments / n;
		sum.distinctUsed += w.distinctUsed / n;
		sum.shaderSwitches += w.shaderSwitches / n;
		sum.stalls += w.stalls;
	}
	window_ = Window {};
}

void BlobEffectCapacityBench::advanceScript(float dt) {
	const ScriptStep & step = script_[scriptIndex_];
	stepElapsed_ += dt;
	// Capture just before the step ends, after its last full window, so the
	// PNG-save stall never lands inside a measured window.
	if (!stepCaptured_ && stepElapsed_ >= step.seconds - 0.25f) {
		stepCaptured_ = true;
		captureLabel_ = "step" + ofToString(scriptIndex_ + 1) + "_" + step.spec;
		std::replace(captureLabel_.begin(), captureLabel_.end(), '@', '_');
		std::replace(captureLabel_.begin(), captureLabel_.end(), '#', '_');
	}
	if (stepElapsed_ < step.seconds) {
		return;
	}
	window_ = Window {}; // partial window discarded
	scriptIndex_++;
	stepElapsed_ = 0.0f;
	stepCaptured_ = false;
	if (scriptIndex_ >= static_cast<int>(script_.size())) {
		finishScript();
		return;
	}
	const ScriptStep & next = script_[scriptIndex_];
	summaries_.push_back({ next.spec, next.plan.fields() + " load=" + loadLabel() });
	applyPlan(next.plan, "script step " + ofToString(scriptIndex_ + 1) + "/" + ofToString(script_.size()) + " " + next.spec);
}

void BlobEffectCapacityBench::finishScript() {
	std::string path = tsvPath_.substr(0, tsvPath_.size() - 4) + "_summary.tsv";
	std::ofstream out(path);
	out << "step\tspec\tplan\twindows\tfps\tframe_ms\tregion_render_ms\tbackground_render_ms\tfragments\tshader_fragments\tdistinct_used\tshader_switches\tstalls\n";
	ofLogNotice(kLog) << "==================== SCRIPT SUMMARY (means of steady windows) ====================";
	for (size_t i = 0; i < summaries_.size(); i++) {
		const StepSummary & s = summaries_[i];
		double n = std::max(1, s.windows);
		ofLogNotice(kLog) << "SUMMARY step=" << (i + 1) << " spec=" << s.spec << " windows=" << s.windows
						  << " fps=" << ofToString(s.fps / n, 2) << " frameMs=" << ofToString(s.frameMs / n, 3)
						  << " regionRenderMs=" << ofToString(s.regionRenderMs / n, 3)
						  << " backgroundMs=" << ofToString(s.backgroundMs / n, 3)
						  << " fragments=" << ofToString(s.fragments / n, 2) << " shader=" << ofToString(s.shaderFragments / n, 2)
						  << " distinctUsed=" << ofToString(s.distinctUsed / n, 2)
						  << " shaderSwitches=" << ofToString(s.shaderSwitches / n, 2) << " stalls=" << s.stalls
						  << " | " << s.fields;
		out << (i + 1) << "\t" << s.spec << "\t" << s.fields << "\t" << s.windows << "\t" << s.fps / n << "\t" << s.frameMs / n
			<< "\t" << s.regionRenderMs / n << "\t" << s.backgroundMs / n << "\t" << s.fragments / n << "\t" << s.shaderFragments / n
			<< "\t" << s.distinctUsed / n << "\t" << s.shaderSwitches / n << "\t" << s.stalls << "\n";
	}
	ofLogNotice(kLog) << "SCRIPT COMPLETE — windows: " << tsvPath_ << " summary: " << path << " tempC=" << temperatureC()
					  << " throttled=" << throttled();
	scriptIndex_ = -1;
	ofExit();
}
