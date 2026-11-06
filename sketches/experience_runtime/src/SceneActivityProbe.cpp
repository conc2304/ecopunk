// OC-004 diagnostic probe -- see SceneActivityProbe.h.
// Extra switch: EXPERIENCE_RUNTIME_SCENE_ACTIVITY_GPU_TIMER=1 wraps the luma and diff passes in GL_TIME_ELAPSED
// queries (desktop only), logged as gpuLumaMs / gpuDiffMs. Compiled in always, inert unless
// EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE=<dir> is set.
#include "SceneActivityProbe.h"
#include "SceneColorSampler.h" // StageStat only

#include "ofFbo.h"
#include "ofGraphics.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofPixels.h"
#include "ofShader.h"
#include "ofAppRunner.h"
#include "ofGLUtils.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace sceneactivity {

namespace {
double nowMs() {
	using namespace std::chrono;
	return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}
const char* envOr(const char* name) { const char* v = std::getenv(name); return (v && *v) ? v : nullptr; }
double envNum(const char* name, double def) { const char* v = envOr(name); return v ? std::atof(v) : def; }
std::string safeId(std::string s) {
	for (char& c : s) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) c = '_';
	return s;
}

// Pass 1: SceneFBO -> 64x36. Each output texel is the box mean of a block of source texels (sampled at
// texel centres, at most 16x16 taps per block, strided for larger blocks). Per source texel:
//   e   = rgb * alpha                      composited over black, display-encoded
//   enc = dot(w709, e)                     luma of the displayed values   -> .r
//   lin = dot(w709, eotf(e))               relative luminance             -> .g
std::string lumaFragment(bool programmable, bool rect) {
	std::string f;
	f += programmable ? "#version 150\nout vec4 outputColor;\n" : "#version 120\n";
	f += rect ? "uniform sampler2DRect tex;\n" : "uniform sampler2D tex;\n";
	f += "uniform vec2 srcSize;\nuniform vec2 block;\nuniform float quant8;\n";
	f += "const vec3 w = vec3(0.2126, 0.7152, 0.0722);\n";
	f += "vec3 eotf(vec3 c){ return mix(pow((c + 0.055) / 1.055, vec3(2.4)), c / 12.92, step(c, vec3(0.04045))); }\n";
	f += "void main(){\n";
	f += "  vec2 o = floor(gl_FragCoord.xy);\n";
	f += "  vec2 start = floor(o * block);\n";
	f += "  vec2 stop = min(srcSize, max(start + 1.0, floor((o + 1.0) * block + 0.5)));\n";
	f += "  vec2 stride = max(vec2(1.0), ceil((stop - start) / 16.0));\n";
	f += "  float enc = 0.0, lin = 0.0, n = 0.0;\n";
	f += "  for (int j = 0; j < 16; j++) { float y = start.y + float(j) * stride.y; if (y >= stop.y) break;\n";
	f += "    for (int i = 0; i < 16; i++) { float x = start.x + float(i) * stride.x; if (x >= stop.x) break;\n";
	f += rect ? "      vec4 c = " + std::string(programmable ? "texture" : "texture2DRect") + "(tex, vec2(x + 0.5, y + 0.5));\n"
	          : "      vec4 c = " + std::string(programmable ? "texture" : "texture2D") + "(tex, vec2((x + 0.5) / srcSize.x, (y + 0.5) / srcSize.y));\n";
	f += "      vec3 e = c.rgb * c.a;\n";
	f += "      enc += dot(w, e); lin += dot(w, eotf(e)); n += 1.0; } }\n";
	f += "  vec2 m = vec2(enc, lin) / max(n, 1.0);\n";
	f += "  if (quant8 > 0.5) m = floor(m * 255.0 + 0.5) / 255.0;\n";
	f += std::string(programmable ? "  outputColor" : "  gl_FragColor") + " = vec4(m, 0.0, 1.0);\n}\n";
	return f;
}
// Pass 2/3: block mean of |A - B| (useB) or of A (already a difference).  64x36 -> 8x6 -> 1x1.
std::string diffFragment(bool programmable) {
	std::string f;
	f += programmable ? "#version 150\nout vec4 outputColor;\n" : "#version 120\n";
	f += "uniform sampler2D texA;\nuniform sampler2D texB;\nuniform vec2 srcSize;\nuniform vec2 block;\nuniform float useB;\n";
	f += "void main(){\n";
	f += "  vec2 o = floor(gl_FragCoord.xy);\n  vec2 start = floor(o * block);\n  vec2 acc = vec2(0.0); float n = 0.0;\n";
	f += "  for (int j = 0; j < 8; j++) { float y = start.y + float(j); if (j >= int(block.y + 0.5)) break;\n";
	f += "    for (int i = 0; i < 8; i++) { float x = start.x + float(i); if (i >= int(block.x + 0.5)) break;\n";
	f += "      vec2 uv = vec2((x + 0.5) / srcSize.x, (y + 0.5) / srcSize.y);\n";
	std::string tex = programmable ? "texture" : "texture2D";
	f += "      vec2 a = " + tex + "(texA, uv).rg;\n";
	f += "      if (useB > 0.5) a = abs(a - " + tex + "(texB, uv).rg);\n";
	f += "      acc += a; n += 1.0; } }\n";
	f += std::string(programmable ? "  outputColor" : "  gl_FragColor") + " = vec4(acc / max(n, 1.0), 0.0, 1.0);\n}\n";
	return f;
}
std::string passVertex(bool programmable) {
	return programmable
		? "#version 150\nuniform mat4 modelViewProjectionMatrix;\nin vec4 position;\nvoid main(){ gl_Position = modelViewProjectionMatrix * position; }\n"
		: "#version 120\nvoid main(){ gl_Position = ftransform(); }\n";
}
void put(std::ostringstream& o, double v) {
	if (std::isnan(v)) { o << ','; return; }
	char b[32]; std::snprintf(b, sizeof(b), "%.6f", v); o << ',' << b;
}
} // namespace

struct SceneActivityProbe::Impl {
	enum class Mode { Cpu, Gpu, Verify, Off };
	struct Pending {
		bool active = false;
		int slot = 0, framesLeft = 0;
		bool havePrev = false;
		double t = 0.0, dt = 0.0;
		uint64_t gen = 0;
		std::string frame;
		double submitMs = 0.0, issueMs = 0.0; // OC-003/A: submit cost + issue time, so deferred `total` covers the whole sample
		void* fence = nullptr;                // GLsync (fence strategy)
	};

	bool enabled = false, setupTried = false, setupOk = false, warned = false;
	Mode mode = Mode::Cpu;
	bool deferred = true, quant8 = false;
	double period = kNominalPeriodSeconds, frameEvery = 0.0;
	int segments = 12;
	std::string dir;
	std::ofstream csv, framesCsv;

	ofShader lumaRect, luma2D, diffShader;
	std::array<ofFbo, 2> grid;
	ofFbo diff1, diff1b;
	ofFbo diff2;
	ofFloatPixels pxGrid, pxDiff;

	// stream state
	uint64_t gen = 0;
	std::string sceneId;
	int cur = 0;                        // slot the next grid is drawn into
	bool prevValid = false;             // grids[1-cur] holds the previous valid grid of this generation
	std::array<std::vector<float>, 2> cpuGrid; // [prev/cur by slot] RG interleaved copies for the CPU path
	double prevT = 0.0, lastIssueT = -1e9, lastEventT = 0.0, lastMissingRow = -1e9, lastSmoothT = -1.0;
	double frameNextAt = 0.0;
	int frameIdx = 0;
	bool haveScene = false, liveSinceGen = false;
	Pending pending;

	Normalizer norm;
	std::array<Smoother, 3> smooth;

	// OC-004 production publication (EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY=1, default OFF): the measurement events
	// below also feed this tracker, which ExperienceRuntime turns into HudFrameData::sceneVisualActivity.
	bool publish = false;
	VisualActivityTracker tracker;

	scenecolorsampling::StageStat submit, wait, diffCpu, total, frameDt, probeFrame, gpuLuma, gpuDiff;
	// Workstream A attribution: `gpuRead` times the 1x1 diff readback in EVERY mode (before, verify mode left it untimed and
	// the first CPU grid read absorbed the GPU completion wait for both paths); `latency` = issue -> fence observed signalled.
	scenecolorsampling::StageStat gpuRead, latency;
	bool useFence = false;
	int deferFrames = 2;
	uint64_t notReadyPolls = 0, forcedBlocking = 0;
	static constexpr double kFenceTimeoutMs = 250.0;
	void releaseFence(Pending& q) {
#ifndef TARGET_OPENGLES
		if (q.fence) glDeleteSync(static_cast<GLsync>(q.fence));
#endif
		q.fence = nullptr;
	}
	void chooseDefer() {
		std::string want = "auto";
		if (const char* e = envOr("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_DEFER")) want = e;
		bool syncOk = false;
#ifndef TARGET_OPENGLES
		if (glFenceSync && glClientWaitSync && glDeleteSync) {
			auto r = ofGetGLRenderer();
			const int major = r ? r->getGLVersionMajor() : 0, minor = r ? r->getGLVersionMinor() : 0;
			if (major > 3 || (major == 3 && minor >= 2)) syncOk = true;
			else if (const GLubyte* ext = glGetString(GL_EXTENSIONS)) syncOk = std::strstr(reinterpret_cast<const char*>(ext), "GL_ARB_sync") != nullptr;
		}
#endif
		useFence = false;
		deferFrames = 2;
		if (want.rfind("frames", 0) == 0) {
			const auto c = want.find(':');
			if (c != std::string::npos) deferFrames = std::min(30, std::max(2, std::atoi(want.c_str() + c + 1)));
		} else if (syncOk && (want == "auto" || want == "fence")) {
			useFence = true;
		}
		ofLogNotice("SceneActivity") << "deferred readback: syncObjects=" << (syncOk ? "yes" : "no") << " strategy=" << (useFence ? "fence" : "frames") << " deferFrames=" << deferFrames;
	}
	// Optional GL timer queries (EXPERIENCE_RUNTIME_SCENE_ACTIVITY_GPU_TIMER=1, desktop only): one per slot per pass group.
	bool gpuTimer = false;
	std::array<std::array<GLuint, 2>, 2> queries{}; // [slot][0 = luma pass, 1 = diff passes]
	std::array<bool, 2> diffTimed{};
	uint64_t issued = 0, completed = 0, droppedBusy = 0, statics = 0, firsts = 0, wides = 0;
	double maxGpuCpuDelta = 0.0;
	double probeMsThisFrame = 0.0, lastPollMs = -1.0, lastStatsAt = 0.0;
	int issuedThisFrame = 0, collectedThisFrame = 0;

	void init() {
		const char* d = envOr("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE");
		const char* pub = envOr("EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY");
		publish = pub && std::string(pub) == "1";
		if (!d && !publish) return; // production default: fully inert
		enabled = true;
		if (d) dir = d;
		if (const char* m = envOr("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_MODE")) {
			const std::string s = m;
			mode = s == "gpu" ? Mode::Gpu : s == "verify" ? Mode::Verify : s == "off" ? Mode::Off : Mode::Cpu;
		}
		if (const char* r = envOr("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_READBACK")) deferred = std::string(r) != "sync";
		quant8 = envOr("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_QUANT8") != nullptr;
		gpuTimer = envOr("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_GPU_TIMER") != nullptr;
		const double hz = envNum("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_HZ", 1.0 / kNominalPeriodSeconds);
		if (hz > 0.0) period = 1.0 / hz;
		tracker.setPeriod(period);
		frameEvery = d ? envNum("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_FRAME_SECONDS", 0.0) : 0.0;
		segments = std::max(1, static_cast<int>(envNum("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_SEGMENTS", 12)));
		NormParams np;
		np.fixedRef = envNum("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_FIXED_REF", np.fixedRef);
		np.gamma = envNum("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_GAMMA", np.gamma);
		np.floorRaw = envNum("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_FLOOR", np.floorRaw);
		norm.setParams(np);
		if (!d) { // publish-only: no CSV diagnostics
			ofLogNotice("SceneActivity") << "OC-004 visual-activity PUBLICATION ON (scene.visual.activity) period=" << period << "s";
			return;
		}
		csv.open(dir + "/activity.csv", std::ios::trunc);
		csv << "t,gen,scene,state,dt,rawEnc,rawLin,cpuEnc,cpuLin,gpuCpuDelta,fixed,rolling,bounded,rollingRef,boundedRef,"
		       "sFixed,sRolling,sBounded,segFixed,segRolling,segBounded,segN,frame\n";
		framesCsv.open(dir + "/frames.csv", std::ios::trunc);
		framesCsv << "t,frameDtMs,probeMs,issued,collected\n";
		ofLogNotice("SceneActivity") << "OC-004 probe ON dir=" << dir << " mode=" << (mode == Mode::Cpu ? "cpu" : mode == Mode::Gpu ? "gpu" : mode == Mode::Verify ? "verify" : "off")
			<< " readback=" << (deferred ? "deferred" : "sync") << " period=" << period << "s segments=" << segments
			<< " quant8=" << quant8 << " fixedRef=" << np.fixedRef << " gamma=" << np.gamma;
	}

	bool setup() {
		if (setupTried) return setupOk;
		setupTried = true;
#ifdef TARGET_OPENGLES
		ofLogNotice("SceneActivity") << "GLES: probe unavailable (needs float render targets); desktop-only diagnostic";
		return false;
#else
		const bool programmable = ofIsGLProgrammableRenderer();
		bool ok = true;
		for (int rect = 0; rect < 2 && ok; ++rect) {
			ofShader& sh = rect ? lumaRect : luma2D;
			ok = sh.setupShaderFromSource(GL_VERTEX_SHADER, passVertex(programmable))
				&& sh.setupShaderFromSource(GL_FRAGMENT_SHADER, lumaFragment(programmable, rect != 0))
				&& (!programmable || sh.bindDefaults()) && sh.linkProgram();
		}
		ok = ok && diffShader.setupShaderFromSource(GL_VERTEX_SHADER, passVertex(programmable))
			&& diffShader.setupShaderFromSource(GL_FRAGMENT_SHADER, diffFragment(programmable))
			&& (!programmable || diffShader.bindDefaults()) && diffShader.linkProgram();
		if (!ok) { ofLogNotice("SceneActivity") << "shader build failed: probe idle"; return false; }
		ofFbo::Settings st;
		st.internalformat = GL_RGBA32F;
		st.textureTarget = GL_TEXTURE_2D;
		st.minFilter = GL_NEAREST;
		st.maxFilter = GL_NEAREST;
		st.useDepth = false; st.useStencil = false; st.numSamples = 0;
		st.width = kGridW; st.height = kGridH;
		for (auto& f : grid) f.allocate(st);
		st.width = 8; st.height = 6;
		diff1.allocate(st);
		st.width = 1; st.height = 1;
		diff2.allocate(st);
		setupOk = grid[0].isAllocated() && grid[1].isAllocated() && diff1.isAllocated() && diff2.isAllocated();
#ifndef TARGET_OPENGLES
		if (setupOk && gpuTimer) for (auto& q : queries) glGenQueries(2, q.data());
#endif
		if (!setupOk) ofLogNotice("SceneActivity") << "RGBA32F render targets unavailable: probe idle";
		else ofLogNotice("SceneActivity") << "GPU luminance reduction ready (RGBA32F 64x36 x2, 8x6, 1x1)";
		return setupOk;
#endif
	}

	uint64_t discardedAtClear = 0, discardedGenMismatch = 0;
	void clearStream(const char* why = nullptr) {
		if (why && pending.active) { // OC-004 verification: an in-flight readback is being invalidated
			++discardedAtClear;
			ofLogNotice("SceneActivity") << "DISCARD pending readback issued gen=" << pending.gen << " at t=" << pending.t << " (current gen=" << gen << ") reason=" << why;
		}
		prevValid = false;
		releaseFence(pending);
		pending.active = false;
	}

	void writeRow(double t, State st, double dt, double rawEnc, double rawLin, double cpuEnc, double cpuLin, double delta, const std::string& frame) {
		if (!csv.is_open()) { lastEventT = t; return; } // publish-only: the CSV-side normalisers/smoothers are diagnostic
		const bool raw = hasRaw(st);
		Normalised n;
		double x[3] = {0, 0, 0};
		if (raw) {
			n = norm.push(t, rawEnc);
			x[0] = n.fixed; x[1] = n.rolling; x[2] = n.bounded;
		}
		if (st == State::SceneChange) { for (auto& s : smooth) s.reset(); lastSmoothT = -1.0; }
		double sm[3] = {0, 0, 0};
		const double sdt = lastSmoothT < 0.0 ? 0.0 : t - lastSmoothT;
		if (st != State::SceneChange) for (int i = 0; i < 3; ++i) sm[i] = smooth[i].push(x[i], sdt); // non-raw states decay toward 0
		lastSmoothT = t;
		std::ostringstream o;
		char tb[48]; std::snprintf(tb, sizeof(tb), "%.4f", t);
		o << tb << ',' << gen << ',' << safeId(sceneId) << ',' << stateName(st);
		put(o, dt);
		put(o, raw ? rawEnc : NAN); put(o, raw ? rawLin : NAN); put(o, cpuEnc); put(o, cpuLin); put(o, delta);
		put(o, raw ? n.fixed : NAN); put(o, raw ? n.rolling : NAN); put(o, raw ? n.bounded : NAN);
		put(o, raw ? n.rollingRef : NAN); put(o, raw ? n.boundedRef : NAN);
		put(o, sm[0]); put(o, sm[1]); put(o, sm[2]);
		o << ',' << segmentCount(sm[0], segments) << ',' << segmentCount(sm[1], segments) << ',' << segmentCount(sm[2], segments)
		  << ',' << segments << ',' << frame;
		csv << o.str() << '\n';
		csv.flush();
		lastEventT = t;
	}

	void passTo(ofFbo& dst, const ofTexture& a, const ofTexture* b, float srcW, float srcH, float bw, float bh) {
		dst.begin();
		ofClear(0, 0, 0, 255);
		diffShader.begin();
		diffShader.setUniformTexture("texA", a, 0);
		diffShader.setUniformTexture("texB", b ? *b : a, 1);
		diffShader.setUniform2f("srcSize", srcW, srcH);
		diffShader.setUniform2f("block", bw, bh);
		diffShader.setUniform1f("useB", b ? 1.0f : 0.0f);
		ofDrawRectangle(0, 0, dst.getWidth(), dst.getHeight());
		diffShader.end();
		dst.end();
	}

	void submitGrid(const ofTexture& scene, int slot) {
		const float sw = scene.getWidth(), sh = scene.getHeight();
		const bool rect = scene.getTextureData().textureTarget == GL_TEXTURE_RECTANGLE;
		ofShader& sd = rect ? lumaRect : luma2D;
		ofPushStyle();
		ofDisableAlphaBlending();
		ofSetColor(255);
#ifndef TARGET_OPENGLES
		if (gpuTimer) glBeginQuery(GL_TIME_ELAPSED, queries[slot][0]);
#endif
		grid[slot].begin();
		ofClear(0, 0, 0, 255);
		sd.begin();
		sd.setUniformTexture("tex", scene, 0);
		sd.setUniform2f("srcSize", sw, sh);
		sd.setUniform2f("block", sw / kGridW, sh / kGridH);
		sd.setUniform1f("quant8", quant8 ? 1.0f : 0.0f);
		ofDrawRectangle(0, 0, kGridW, kGridH);
		sd.end();
		grid[slot].end();
#ifndef TARGET_OPENGLES
		if (gpuTimer) glEndQuery(GL_TIME_ELAPSED);
#endif
		diffTimed[slot] = false;
		if (prevValid && (mode == Mode::Gpu || mode == Mode::Verify)) {
#ifndef TARGET_OPENGLES
			if (gpuTimer) { glBeginQuery(GL_TIME_ELAPSED, queries[slot][1]); diffTimed[slot] = true; }
#endif
			passTo(diff1, grid[slot].getTexture(), &grid[1 - slot].getTexture(), kGridW, kGridH, 8, 6);
			passTo(diff2, diff1.getTexture(), nullptr, 8, 6, 8, 6);
#ifndef TARGET_OPENGLES
			if (gpuTimer) glEndQuery(GL_TIME_ELAPSED);
#endif
		}
		ofPopStyle();
	}

	void complete(const Pending& p) {
		const double w0 = nowMs();
#ifndef TARGET_OPENGLES
		if (gpuTimer) { // read AFTER the grid/diff readback below would have waited anyway; deferred mode: already finished
			GLuint64 ns = 0;
			glGetQueryObjectui64v(queries[p.slot][0], GL_QUERY_RESULT, &ns);
			gpuLuma.add(static_cast<double>(ns) * 1e-6);
			if (diffTimed[p.slot]) { glGetQueryObjectui64v(queries[p.slot][1], GL_QUERY_RESULT, &ns); gpuDiff.add(static_cast<double>(ns) * 1e-6); }
		}
#endif
		double cpuEnc = NAN, cpuLin = NAN, gEnc = NAN, gLin = NAN;
		const bool needCpu = mode == Mode::Cpu || mode == Mode::Verify;
		const bool needGpu = (mode == Mode::Gpu || mode == Mode::Verify) && p.havePrev;
		double w1 = w0;
		if (needCpu) {
			grid[p.slot].readToPixels(pxGrid);
			w1 = nowMs();
			wait.add(w1 - w0);
			const std::size_t n = static_cast<std::size_t>(pxGrid.getWidth()) * pxGrid.getHeight();
			const int ch = static_cast<int>(pxGrid.getNumChannels());
			auto& mine = cpuGrid[p.slot];
			mine.resize(n * 2);
			const float* d = pxGrid.getData();
			for (std::size_t i = 0; i < n; ++i) { mine[i * 2] = d[i * ch]; mine[i * 2 + 1] = d[i * ch + 1]; }
			if (p.havePrev) {
				const auto& prev = cpuGrid[1 - p.slot];
				if (prev.size() == mine.size()) {
					cpuEnc = meanAbsDiff(mine.data(), prev.data(), n, 2, 2);
					cpuLin = meanAbsDiff(mine.data() + 1, prev.data() + 1, n, 2, 2);
				}
			}
			diffCpu.add(nowMs() - w1);
		}
		if (needGpu) {
			const double g0 = nowMs();
			diff2.readToPixels(pxDiff);
			gpuRead.add(nowMs() - g0);
			if (!needCpu) wait.add(nowMs() - g0);
			gEnc = pxDiff.getData()[0];
			gLin = pxDiff.getData()[1];
		}
		const bool gpuOnly = mode == Mode::Gpu;
		const double rawEnc = gpuOnly ? gEnc : cpuEnc, rawLin = gpuOnly ? gLin : cpuLin; // verify reports the CPU reference as raw
		double delta = NAN;
		if (mode == Mode::Verify && !std::isnan(gEnc) && !std::isnan(cpuEnc)) {
			delta = std::max(std::fabs(gEnc - cpuEnc), std::fabs(gLin - cpuLin));
			maxGpuCpuDelta = std::max(maxGpuCpuDelta, delta);
		}
		State st = State::First;
		if (p.havePrev && !std::isnan(rawEnc)) st = p.dt > kWidePeriods * period ? State::Wide : State::Valid;
		if (publish) {
			if (hasRaw(st)) tracker.onSample(p.t, rawEnc);
			else tracker.onLiveNoMeasurement();
		}
		if (st == State::First) ++firsts;
		if (st == State::Wide) ++wides;
		writeRow(p.t, st, p.havePrev ? p.dt : NAN, rawEnc, rawLin, mode == Mode::Verify ? cpuEnc : NAN, mode == Mode::Verify ? cpuLin : NAN, delta, p.frame);
		++completed;
		++collectedThisFrame;
	}
};

SceneActivityProbe::SceneActivityProbe() : impl_(std::make_unique<Impl>()) { impl_->init(); }
SceneActivityProbe::~SceneActivityProbe() = default;

bool SceneActivityProbe::publishing() const { return impl_->enabled && impl_->publish; }
const VisualActivityTracker& SceneActivityProbe::tracker() const { return impl_->tracker; }

bool SceneActivityProbe::requested() {
	static const bool r = envOr("EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE") != nullptr;
	return r;
}

void SceneActivityProbe::beginGeneration(uint64_t generation, const std::string& sceneId, double now) {
	Impl& d = *impl_;
	if (!d.enabled) return;
	d.gen = generation;
	d.sceneId = sceneId;
	if (d.pending.active) ofLogNotice("SceneActivity") << "generation " << d.gen << " -> " << generation << " begins with a readback in flight";
	if (d.publish) d.tracker.beginGeneration(generation, sceneId);
	d.clearStream("generation-change");
	d.norm.reset();
	d.lastIssueT = -1e9;
	d.lastMissingRow = -1e9;
	d.frameNextAt = now + 1.0;
	d.writeRow(now, State::SceneChange, NAN, NAN, NAN, NAN, NAN, NAN, "");
}

void SceneActivityProbe::onStaticFrame(double now) {
	Impl& d = *impl_;
	if (!d.enabled || d.mode == Impl::Mode::Off) return;
	if (d.publish) d.tracker.onStatic(); // every static frame, not rate-limited: a retained frame is never a measurement
	if (now - d.lastIssueT < d.period) return;
	const double t0 = nowMs();
	d.lastIssueT = now;
	d.clearStream("static-frame"); // a retained frame is not a measurement; the next live grid has nothing to difference against
	d.writeRow(now, State::Static, NAN, NAN, NAN, NAN, NAN, NAN, "");
	++d.statics;
	d.probeMsThisFrame += nowMs() - t0;
}

void SceneActivityProbe::onLiveFrame(const ofTexture& tex, double now) {
	Impl& d = *impl_;
	if (!d.enabled || d.mode == Impl::Mode::Off) return;
	if (now - d.lastIssueT < d.period - 0.002) return; // slack so a 60 Hz frame boundary never skips a due sample
	const double t0 = nowMs();
	if (d.pending.active) { ++d.droppedBusy; return; }
	if (!tex.isAllocated()) return;
	const GLint fmt = tex.getTextureData().glInternalFormat;
	if (!d.setup() || !(fmt == GL_RGBA || fmt == GL_RGBA8)) {
		if (!d.warned) { d.warned = true; ofLogNotice("SceneActivity") << "unsupported source or GPU path: recording unsupported rows"; }
		d.lastIssueT = now;
		d.clearStream();
		if (d.publish) d.tracker.onUnsupported();
		d.writeRow(now, State::Unsupported, NAN, NAN, NAN, NAN, NAN, NAN, "");
		return;
	}
	const double dt = d.prevValid ? now - d.prevT : NAN;
	d.lastIssueT = now;
	const int slot = d.cur;
	static bool deferChosen = false;
	if (!deferChosen) { deferChosen = true; d.chooseDefer(); }
	d.submitGrid(tex, slot);
	Impl::Pending p;
#ifndef TARGET_OPENGLES
	if (d.deferred && d.useFence) p.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#endif
	d.submit.add(nowMs() - t0);
	p.submitMs = nowMs() - t0;
	p.issueMs = t0;
	p.active = true;
	p.slot = slot;
	p.framesLeft = d.deferred ? d.deferFrames : 0;
	p.havePrev = d.prevValid;
	p.t = now;
	p.dt = dt;
	p.gen = d.gen;
	if (d.frameEvery > 0.0 && now >= d.frameNextAt) {
		d.frameNextAt = now + d.frameEvery;
		ofPixels full;
		tex.readToPixels(full);
		char nm[96]; std::snprintf(nm, sizeof(nm), "act_%s_%03d.bmp", safeId(d.sceneId).c_str(), d.frameIdx++);
		ofSaveImage(full, d.dir + "/" + nm);
		p.frame = nm;
	}
	d.prevT = now;
	d.prevValid = true; // the grid just drawn is the next sample's "previous"
	d.cur = 1 - slot;
	++d.issued;
	++d.issuedThisFrame;
	if (!d.deferred) {
		d.complete(p);
		d.total.add(nowMs() - t0);
	} else {
		d.pending = p;
	}
	d.probeMsThisFrame += nowMs() - t0;
}

void SceneActivityProbe::poll(double now) {
	Impl& d = *impl_;
	if (!d.enabled) return;
	const double t0 = nowMs();
	if (d.pending.active && d.mode != Impl::Mode::Off) {
		bool ready = true;
		if (d.pending.fence) { // fence strategy: never block; read only a grid the GPU has finished
#ifndef TARGET_OPENGLES
			const double age = t0 - d.pending.issueMs;
			const GLenum r = glClientWaitSync(static_cast<GLsync>(d.pending.fence), 0, 0);
			if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED) d.latency.add(age);
			else if (age < Impl::kFenceTimeoutMs) { ++d.notReadyPolls; ready = false; }
			else ++d.forcedBlocking; // bounded: the blocking read below still completes the sample
#endif
		} else {
			if (d.pending.framesLeft > 0) --d.pending.framesLeft;
			ready = d.pending.framesLeft == 0;
		}
		if (ready) {
			Impl::Pending p = d.pending;
			d.pending.active = false;
			d.pending.fence = nullptr;
			d.releaseFence(p);
			if (p.gen == d.gen) d.complete(p); // a deferred sample never crosses a scene change
			else { ++d.discardedGenMismatch; ofLogNotice("SceneActivity") << "DISCARD completed readback issued gen=" << p.gen << " (current gen=" << d.gen << ") reason=completion-gen-mismatch"; }
			d.total.add(p.submitMs + (nowMs() - t0));
		}
	}
	// A live scene that stopped producing samples is reported, not silently held.
	if (d.mode != Impl::Mode::Off && d.prevValid && !d.pending.active && now - d.lastEventT > kStalePeriods * d.period
		&& now - d.lastMissingRow >= d.period) {
		d.lastMissingRow = now;
		d.writeRow(now, State::Missing, NAN, NAN, NAN, NAN, NAN, NAN, "");
	}
	d.probeMsThisFrame += nowMs() - t0;
	// frames.csv: whole-frame cadence, with the probe's own CPU time inside the frame.
	const double m = nowMs();
	if (d.lastPollMs >= 0.0) {
		const double fdt = m - d.lastPollMs;
		d.frameDt.add(fdt);
		d.probeFrame.add(d.probeMsThisFrame);
		char b[160];
		std::snprintf(b, sizeof(b), "%.4f,%.4f,%.4f,%d,%d\n", now, fdt, d.probeMsThisFrame, d.issuedThisFrame, d.collectedThisFrame);
		d.framesCsv << b;
	}
	d.lastPollMs = m;
	d.probeMsThisFrame = 0.0;
	d.issuedThisFrame = d.collectedThisFrame = 0;
	if (now - d.lastStatsAt >= 10.0) {
		d.lastStatsAt = now;
		d.framesCsv.flush();
		ofLogNotice("SceneActivity") << "scene=" << d.sceneId << " gen=" << d.gen << " issued=" << d.issued << " done=" << d.completed
			<< " busy=" << d.droppedBusy << " static=" << d.statics << " first=" << d.firsts << " wide=" << d.wides
			<< " maxGpuCpuDelta=" << d.maxGpuCpuDelta
			<< " ms[p50/p95/max] submit=" << d.submit.pct(0.5) << "/" << d.submit.pct(0.95) << "/" << d.submit.max
			<< " wait=" << d.wait.pct(0.5) << "/" << d.wait.pct(0.95) << "/" << d.wait.max
			<< " diffCpu=" << d.diffCpu.pct(0.5) << "/" << d.diffCpu.pct(0.95) << "/" << d.diffCpu.max
			<< " gpuLumaMs=" << d.gpuLuma.pct(0.5) << "/" << d.gpuLuma.pct(0.95) << "/" << d.gpuLuma.max
			<< " gpuDiffMs=" << d.gpuDiff.pct(0.5) << "/" << d.gpuDiff.pct(0.95) << "/" << d.gpuDiff.max
			<< " gpuRead=" << d.gpuRead.pct(0.5) << "/" << d.gpuRead.pct(0.95) << "/" << d.gpuRead.max
			<< " latency=" << d.latency.pct(0.5) << "/" << d.latency.pct(0.95) << "/" << d.latency.max
			<< " notReady=" << d.notReadyPolls << " forced=" << d.forcedBlocking
			<< " total=" << d.total.pct(0.5) << "/" << d.total.pct(0.95) << "/" << d.total.max;
	}
}

} // namespace sceneactivity
