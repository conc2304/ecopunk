// OC-003B RAW VIDEO sampling logic: eligibility gate, generations, format validation,
// feature flag, and independence from the FX series. OF-free (RawVideoGate.h + SceneColorSeries.h).
// The decoded-activation counter itself is covered in
// shared/src/video-playback/test/video_activation_count_tests.cpp (make test-activation).

#include "RawVideoGate.h"
#include "SceneColorSeries.h"

#include <iostream>
#include <string>

using namespace rawvideo;

namespace {
int g_total = 0, g_failures = 0;
void check(bool cond, const std::string& what) { g_total++; if (!cond) { g_failures++; std::cerr << "FAIL: " << what << "\n"; } }

Observation ready(uint64_t activation, uint64_t scene, bool frameNew, bool tex = true) {
	Observation o;
	o.healthReady = true; o.hasMediaId = true;
	o.textureAllocated = tex; o.texW = tex ? 1280 : 0; o.texH = tex ? 720 : 0; o.srcW = 1280; o.srcH = 720;
	o.frameNew = frameNew; o.activationCount = activation; o.sceneGeneration = scene;
	return o;
}
Observation at(Observation o, double now) { o.nowSeconds = now; return o; }
Observation notReady(uint64_t activation, uint64_t scene) { Observation o; o.activationCount = activation; o.sceneGeneration = scene; return o; }

scenecolorsampling::Sample valid(double t, uint64_t gen) {
	scenecolorsampling::Sample s; s.t = t; s.generation = gen; s.r = s.g = s.b = s.luma = 0.5f; s.valid = true; return s;
}
}

int main() {
	// -- feature flag: disabled by default ------------------------------------------------
	check(!videoColorEnabledFromEnv(nullptr), "flag off when the variable is unset");
	check(!videoColorEnabledFromEnv("") && !videoColorEnabledFromEnv("0") && !videoColorEnabledFromEnv("true") && !videoColorEnabledFromEnv("2"), "flag off unless exactly \"1\"");
	check(videoColorEnabledFromEnv("1"), "flag on for \"1\"");

	// -- format validation ----------------------------------------------------------------
	check(sourceFormatSupported(SourceKind::SceneFbo, kGlRgba, false) && sourceFormatSupported(SourceKind::SceneFbo, kGlRgba8, false), "FX accepts RGBA/RGBA8");
	check(!sourceFormatSupported(SourceKind::SceneFbo, kGlRgb, false) && !sourceFormatSupported(SourceKind::SceneFbo, kGlRgb8, false), "FX validation unchanged: rejects RGB/RGB8");
	check(sourceFormatSupported(SourceKind::DecodedVideo, kGlRgba, false) && sourceFormatSupported(SourceKind::DecodedVideo, kGlRgba8, false), "RAW accepts RGBA/RGBA8");
	check(sourceFormatSupported(SourceKind::DecodedVideo, kGlRgb, false) && sourceFormatSupported(SourceKind::DecodedVideo, kGlRgb8, false), "RAW accepts opaque RGB/RGB8");
	check(!sourceFormatSupported(SourceKind::DecodedVideo, 0x822E /*GL_R32F*/, false), "RAW rejects other formats");
	check(!sourceFormatSupported(SourceKind::DecodedVideo, kGlRgb8, true) && !sourceFormatSupported(SourceKind::SceneFbo, kGlRgba8, true), "framebuffer sRGB rejects both");

	// -- first successful activation: unavailable until a decoded frame ----------------------
	{
		Gate g;
		auto d = g.observe(ready(1, 1, /*frameNew*/ false));
		check(d.newGeneration && d.rawGeneration == 1, "first observation begins RAW generation 1");
		check(d.verdict == Verdict::AwaitingFirstFrame, "Ready with a texture but no decoded frame yet: RAW unavailable (old/placeholder texture rejected)");
		d = g.observe(ready(1, 1, false));
		check(d.verdict == Verdict::AwaitingFirstFrame && !d.newGeneration, "still waiting, no new generation");
		d = g.observe(ready(1, 1, true));
		check(d.verdict == Verdict::Eligible && !d.newGeneration && g.frameLatched(), "first decoded frame latches: eligible");
		d = g.observe(ready(1, 1, false));
		check(d.verdict == Verdict::Eligible, "latch holds on frames without a new decoded frame");
	}
	// -- the activation frame never latches (stale isFrameNew from the outgoing media) -----------
	{
		Gate g;
		g.observe(ready(1, 1, true)); g.observe(ready(1, 1, true));
		auto d = g.observe(ready(2, 1, /*frameNew carried over*/ true));
		check(d.newGeneration && d.rawGeneration == 2, "media change advances the RAW generation");
		check(d.verdict == Verdict::AwaitingFirstFrame && !g.frameLatched(), "latch resets on activation; the activation frame's isFrameNew is ignored");
		d = g.observe(ready(2, 1, true));
		check(d.verdict == Verdict::Eligible, "next real decoded frame latches");
	}
	// -- same-media reactivation is a new generation --------------------------------------------
	{
		Gate g;
		g.observe(ready(1, 1, true)); g.observe(ready(1, 1, true));
		const auto before = g.rawGeneration();
		auto d = g.observe(ready(2, 1, false)); // same media, activation counter +1
		check(d.newGeneration && d.rawGeneration == before + 1 && d.verdict == Verdict::AwaitingFirstFrame, "same-media reactivation: new generation, latch reset");
	}
	// -- failed activation: counter unchanged, playback not Ready --------------------------------
	{
		Gate g;
		g.observe(ready(1, 1, true)); g.observe(ready(1, 1, true));
		auto d = g.observe(notReady(1, 1)); // Failed: no counter bump
		check(d.verdict == Verdict::Unavailable && d.newGeneration, "leaving Ready invalidates RAW (new generation clears history)");
		d = g.observe(notReady(1, 1));
		check(d.verdict == Verdict::Unavailable && !d.newGeneration, "stays unavailable without churning generations");
		d = g.observe(ready(2, 1, false)); // recovery activation
		check(d.newGeneration && d.verdict == Verdict::AwaitingFirstFrame, "recovery activation starts a clean generation");
	}
	// -- Degraded/Loading are not eligible (caller maps health != Ready to healthReady=false) ------
	{
		Gate g;
		Observation o = ready(1, 1, true); o.healthReady = false; // Degraded / Loading
		check(g.observe(o).verdict == Verdict::Unavailable, "non-Ready health is ineligible even with a texture and a new frame");
		o = ready(1, 1, true); o.hasMediaId = false;
		check(g.observe(o).verdict == Verdict::Unavailable, "missing mediaId is ineligible");
	}
	// -- texture checks ------------------------------------------------------------------------
	{
		Gate g;
		g.observe(ready(1, 1, true)); g.observe(ready(1, 1, true));
		check(g.observe(ready(1, 1, false, /*tex*/ false)).verdict == Verdict::Gap, "latched but texture missing: gap, not a guess");
		Observation o = ready(1, 1, false); o.texW = 640; // size mismatch
		check(g.observe(o).verdict == Verdict::Gap, "texture size != source size: gap");
		check(g.observe(ready(1, 1, false)).verdict == Verdict::Eligible, "recovers when the texture matches again");
		Gate g2;
		check(g2.observe(ready(1, 1, true, false)).verdict == Verdict::AwaitingFirstFrame, "a new-frame flag without an allocated texture never latches");
	}
	// -- decoder stall: no new frame for > kStallSeconds records gaps, then recovers --------------------
	{
		Gate g;
		g.observe(at(ready(1, 1, true), 10.0)); // activation frame
		check(g.observe(at(ready(1, 1, true), 10.1)).verdict == Verdict::Eligible, "decoding: eligible");
		check(g.observe(at(ready(1, 1, false), 10.1 + kStallSeconds - 0.1)).verdict == Verdict::Eligible, "a short pause is still eligible");
		check(g.observe(at(ready(1, 1, false), 10.1 + kStallSeconds + 0.1)).verdict == Verdict::Gap, "no new frame for > kStallSeconds: gap, not a re-measurement of the same frame");
		check(g.observe(at(ready(1, 1, true), 20.0)).verdict == Verdict::Eligible, "decoding resumes: eligible again, same generation");
	}
	// -- scene activation advances RAW generation independently of the media ----------------------
	{
		Gate g;
		g.observe(ready(1, 1, true)); g.observe(ready(1, 1, true));
		auto d = g.observe(ready(1, 2, false)); // new scene generation, same media/activation
		check(d.newGeneration && d.rawGeneration == 2, "scene activation advances the RAW generation");
		check(d.verdict == Verdict::Eligible, "media latch is NOT reset by a scene change (same media, still decoding)");
	}

	// -- stale in-flight readback rejected; RAW and FX independent ------------------------------------
	{
		scenecolorsampling::Series raw, fx;
		raw.beginGeneration(1, 0.0);
		fx.beginGeneration(1, 0.0);
		check(raw.push(valid(1.0, 1)) && fx.push(valid(1.0, 1)), "both series accept their generation");
		raw.beginGeneration(2, 2.0); // media activation
		check(!raw.push(valid(1.5, 1)), "a readback completing from the previous RAW generation is rejected");
		check(raw.size() == 0 && raw.state(2.1) == scenecolor::State::Unavailable, "RAW history cleared and Unavailable after the new generation");
		check(fx.size() == 1 && fx.state(1.1) == scenecolor::State::Valid && fx.generation() == 1, "FX series untouched by the RAW generation change");
		check(raw.push(valid(2.6, 2)) && raw.state(2.7) == scenecolor::State::Valid, "RAW valid again independently while FX keeps its own cadence");
		// RAW stale does not touch FX.
		check(raw.state(2.6 + 3.1 * scenecolorsampling::kPeriodSeconds * 1.0 + 1.0) == scenecolor::State::Stale, "RAW goes Stale on its own clock");
		check(fx.push(valid(10.0, 1)) && fx.state(10.1) == scenecolor::State::Valid, "FX stays Valid while RAW is Stale");
		// Staggered due times: RAW offset by half a period from FX.
		scenecolorsampling::Series a, b;
		a.beginGeneration(1, 5.0);
		b.beginGeneration(1, 5.0 + scenecolorsampling::kPeriodSeconds * 0.5);
		check(a.due(5.0) && !b.due(5.0) && b.due(5.25), "RAW first sample is due half a period after FX (staggered)");
	}

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures ? 1 : 0;
}
