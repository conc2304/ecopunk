// OC-003A diagnostic half of SceneColorSampler: predominant-colour comparison. Compiled in always, inert
// unless EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM=<dir> is set. It owns no part of the ratified series.
#include "SceneColorSampler.h"
#include "PredominantColor.h"

#include "ofGraphics.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofPixels.h"
#include "ofTexture.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace scenecolorsampling {

namespace {
double nowMs() {
	using namespace std::chrono;
	return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}
std::string safeId(std::string s) {
	for (char& c : s) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) c = '_';
	return s;
}
constexpr const char* kHeader =
	"t,gen,scene,valid,mixedFrac,"
	"a16r,a16g,a16b,a160r,a160g,a160b,"
	"d16r,d16g,d16b,d16bin,d16share,d160r,d160g,d160b,d160bin,d160share,"
	"v16ok,v16r,v16g,v16b,v16bin,v16pop,v16share,v160ok,v160r,v160g,v160b,v160bin,v160pop,v160share,"
	"e16r,e16g,e16b,h16r,h16g,h16b,h16bin,e160r,e160g,e160b,h160r,h160g,h160b,h160bin,"
	"ve160r,ve160g,ve160b,vh160r,vh160g,vh160b,vh160bin,vhe160r,vhe160g,vhe160b,"
	"ms_submit,ms_read16,ms_read160,us_an16,us_an160,us_vib,frame";
constexpr int kFields = 66;
}

struct SceneColorSampler::PredomDiag {
	struct Track { bool have = false; predominant::Rgb ema; int held = -1; };
	std::string dir;
	double period = 0.5, frameEvery = 10.0;
	double nextDue = -1.0, nextFrameAt = -1.0;
	uint64_t gen = 0;
	bool haveGen = false, warned = false;
	std::ofstream csv;
	int frameIdx = 0;
	predominant::Tuning tu;
	predominant::Histogram h16, h160;
	Track t16, t160;
	struct VTrack { bool haveE = false, haveHE = false; predominant::Rgb e, he; int held = -1; };
	VTrack vt; // vibrant temporal state at 160x90
	ofFloatPixels px16, px160;
};

SceneColorSampler::SceneColorSampler() = default;
SceneColorSampler::~SceneColorSampler() = default;

namespace {
void put(std::ostringstream& o, double v) { char b[32]; std::snprintf(b, sizeof(b), "%.5f", v); o << ',' << b; }
void putc(std::ostringstream& o, const predominant::Cluster& c, bool withPop) {
	put(o, c.valid ? c.rgb.r : NAN); put(o, c.valid ? c.rgb.g : NAN); put(o, c.valid ? c.rgb.b : NAN);
	o << ',' << c.bin;
	if (withPop) o << ',' << c.population;
	put(o, c.share);
}
}

void SceneColorSampler::diagStaticFrame(double now, const std::string& sceneId, uint64_t generation) {
	static const char* dirEnv = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM");
	if (!dirEnv || !*dirEnv) return;
	if (!diag_ || !diag_->csv.is_open()) return; // nothing logged yet
	PredomDiag& d = *diag_;
	if (d.nextDue >= 0.0 && now < d.nextDue) return;
	d.nextDue = now + d.period;
	d.t16 = {}; d.t160 = {}; d.vt = {}; // a retained frame breaks continuity: smoothing/hysteresis never bridge it
	std::ostringstream o;
	char b[48]; std::snprintf(b, sizeof(b), "%.4f", now);
	o << b << ',' << generation << ',' << safeId(sceneId) << ",0";
	for (int i = 4; i < kFields; ++i) o << ',';
	d.csv << o.str() << '\n';
	d.csv.flush();
}

void SceneColorSampler::diagFrame(const ofTexture& sceneTexture, double now, const std::string& sceneId, uint64_t generation) {
	static const char* dirEnv = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM");
	if (!dirEnv || !*dirEnv) return;
	if (!diag_) {
		diag_ = std::make_unique<PredomDiag>();
		diag_->dir = dirEnv;
		if (const char* hz = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM_HZ")) { const double v = std::atof(hz); if (v > 0.0) diag_->period = 1.0 / v; }
		if (const char* fe = std::getenv("EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM_FRAME_SECONDS")) { const double v = std::atof(fe); if (v > 0.0) diag_->frameEvery = v; }
		diag_->csv.open(diag_->dir + "/predominant.csv", std::ios::trunc);
		diag_->csv << kHeader << '\n';
		ofLogNotice("SceneColorPredom") << "diagnostic ON dir=" << diag_->dir << " period=" << diag_->period << "s (EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM)";
	}
	PredomDiag& d = *diag_;
	if (d.nextDue >= 0.0 && now < d.nextDue) return;
	d.nextDue = now + d.period;
	if (!d.haveGen || generation != d.gen) { // scene change: history and smoothing state start over
		d.gen = generation; d.haveGen = true; d.t16 = {}; d.t160 = {}; d.vt = {};
		d.nextFrameAt = now + 2.0;
	}
	if (!sceneTexture.isAllocated() || !validateSource(sceneTexture) || !setupLinearPath()) {
		if (!d.warned) { d.warned = true; ofLogNotice("SceneColorPredom") << "GPU linear path unavailable: diagnostic idle"; }
		return;
	}

	const double a = nowMs();
	submitLinear(sceneTexture, 0, true);
	const double b = nowMs();
	linBs_[0].readToPixels(d.px16);   // GPU completion wait + 16x9 transfer (same cost the series pays)
	const double c = nowMs();
	linA_.readToPixels(d.px160);      // INCREMENTAL: the 160x90 float transfer (GPU already finished)
	const double e = nowMs();
	const std::size_t n16 = static_cast<std::size_t>(d.px16.getWidth()) * d.px16.getHeight();
	const std::size_t n160 = static_cast<std::size_t>(d.px160.getWidth()) * d.px160.getHeight();
	const int ch16 = static_cast<int>(d.px16.getNumChannels()), ch160 = static_cast<int>(d.px160.getNumChannels());
	predominant::accumulate(d.px16.getData(), n16, ch16, d.tu, d.h16);
	const double f = nowMs();
	predominant::accumulate(d.px160.getData(), n160, ch160, d.tu, d.h160);
	const double g = nowMs();

	// "Mixed" 16x9 cells: a cell whose 10x10 source pixels (the 160x90 target) disagree strongly, i.e. its
	// average is a colour that barely exists in the frame.
	double mixedFrac = NAN;
	const int w16 = d.px16.getWidth(), h16 = d.px16.getHeight(), w160 = d.px160.getWidth(), h160 = d.px160.getHeight();
	if (w16 > 0 && h16 > 0 && w160 % w16 == 0 && h160 % h16 == 0) {
		const int bw = w160 / w16, bh = h160 / h16;
		int mixed = 0;
		const float* p = d.px160.getData();
		for (int cy = 0; cy < h16; ++cy) for (int cx = 0; cx < w16; ++cx) {
			double s[3] = {0, 0, 0}, s2[3] = {0, 0, 0};
			for (int y = 0; y < bh; ++y) for (int x = 0; x < bw; ++x) {
				const float* q = p + (static_cast<std::size_t>(cy * bh + y) * w160 + (cx * bw + x)) * ch160;
				for (int k = 0; k < 3; ++k) { const double v = predominant::encode(q[k]); s[k] += v; s2[k] += v * v; }
			}
			const double m = bw * bh; double sd = 0;
			for (int k = 0; k < 3; ++k) sd = std::max(sd, std::sqrt(std::max(0.0, s2[k] / m - (s[k] / m) * (s[k] / m))));
			if (sd > 0.12) ++mixed;
		}
		mixedFrac = static_cast<double>(mixed) / (w16 * h16);
	}

	const predominant::Cluster d16 = predominant::dominant(d.h16), d160 = predominant::dominant(d.h160);
	const predominant::Cluster v16 = predominant::vibrant(d.h16, d.tu);
	// Vibrant temporal variants at 160x90 (timed together: this is the incremental CPU of the stabilisation).
	// Every variant restarts after an invalid observation; nothing is substituted or carried across it.
	const double vb0 = nowMs();
	const predominant::Cluster v160 = predominant::vibrant(d.h160, d.tu);
	predominant::Rgb vEma; bool vEmaOk = false;
	if (v160.valid) { d.vt.e = predominant::emaStep(d.vt.e, d.vt.haveE, v160.rgb, d.tu.emaAlpha); d.vt.haveE = true; vEma = d.vt.e; vEmaOk = true; }
	else d.vt.haveE = false;
	d.vt.held = predominant::vibrantWithHysteresis(d.h160, d.vt.held, d.tu);
	const predominant::Cluster vHyst = predominant::vibrantOfBin(d.h160, d.vt.held, d.tu);
	predominant::Rgb vComb; bool vCombOk = false;
	if (vHyst.valid) { d.vt.he = predominant::emaStep(d.vt.he, d.vt.haveHE, vHyst.rgb, d.tu.emaAlpha); d.vt.haveHE = true; vComb = d.vt.he; vCombOk = true; }
	else d.vt.haveHE = false;
	const double vibUs = (nowMs() - vb0) * 1000.0;
	auto track = [&](PredomDiag::Track& t, const predominant::Histogram& h, const predominant::Cluster& dom, predominant::Cluster& hyst, predominant::Rgb& ema, bool& emaOk) {
		emaOk = dom.valid;
		if (dom.valid) { t.ema = predominant::emaStep(t.ema, t.have, dom.rgb, d.tu.emaAlpha); t.have = true; ema = t.ema; }
		else t.have = false;
		t.held = predominant::dominantWithHysteresis(h, t.held, d.tu);
		hyst = predominant::dominantOfBin(h, t.held);
	};
	predominant::Cluster hy16, hy160; predominant::Rgb em16, em160; bool ok16 = false, ok160 = false;
	track(d.t16, d.h16, d16, hy16, em16, ok16);
	track(d.t160, d.h160, d160, hy160, em160, ok160);

	std::string frameName;
	if (now >= d.nextFrameAt) {
		d.nextFrameAt = now + d.frameEvery;
		ofPixels full;
		sceneTexture.readToPixels(full);
		char nm[96]; std::snprintf(nm, sizeof(nm), "scene_%s_%03d.png", safeId(sceneId).c_str(), d.frameIdx++);
		ofSaveImage(full, d.dir + "/" + nm);
		frameName = nm;
	}

	std::ostringstream o;
	char tb[48]; std::snprintf(tb, sizeof(tb), "%.4f", now);
	o << tb << ',' << generation << ',' << safeId(sceneId) << ",1";
	put(o, mixedFrac);
	put(o, d.h16.average.r); put(o, d.h16.average.g); put(o, d.h16.average.b);
	put(o, d.h160.average.r); put(o, d.h160.average.g); put(o, d.h160.average.b);
	putc(o, d16, false); putc(o, d160, false);
	o << ',' << (v16.valid ? 1 : 0); putc(o, v16, true);
	o << ',' << (v160.valid ? 1 : 0); putc(o, v160, true);
	put(o, ok16 ? em16.r : NAN); put(o, ok16 ? em16.g : NAN); put(o, ok16 ? em16.b : NAN);
	put(o, hy16.valid ? hy16.rgb.r : NAN); put(o, hy16.valid ? hy16.rgb.g : NAN); put(o, hy16.valid ? hy16.rgb.b : NAN); o << ',' << hy16.bin;
	put(o, ok160 ? em160.r : NAN); put(o, ok160 ? em160.g : NAN); put(o, ok160 ? em160.b : NAN);
	put(o, hy160.valid ? hy160.rgb.r : NAN); put(o, hy160.valid ? hy160.rgb.g : NAN); put(o, hy160.valid ? hy160.rgb.b : NAN); o << ',' << hy160.bin;
	put(o, vEmaOk ? vEma.r : NAN); put(o, vEmaOk ? vEma.g : NAN); put(o, vEmaOk ? vEma.b : NAN);
	put(o, vHyst.valid ? vHyst.rgb.r : NAN); put(o, vHyst.valid ? vHyst.rgb.g : NAN); put(o, vHyst.valid ? vHyst.rgb.b : NAN); o << ',' << vHyst.bin;
	put(o, vCombOk ? vComb.r : NAN); put(o, vCombOk ? vComb.g : NAN); put(o, vCombOk ? vComb.b : NAN);
	put(o, b - a); put(o, c - b); put(o, e - c); put(o, (f - e) * 1000.0); put(o, (g - f) * 1000.0); put(o, vibUs); // an16/an160/vib in microseconds
	o << ',' << frameName;
	d.csv << o.str() << '\n';
	d.csv.flush();
}

} // namespace scenecolorsampling
