// Orbit Console Scene Sampling v0.1 -- SceneColorSeries.h (OF-free core).
#include "SceneColorSeries.h"

#include <cmath>
#include <iostream>
#include <string>

using namespace scenecolorsampling;

namespace {
int g_total = 0, g_failures = 0;
void check(bool cond, const std::string& what) {
	g_total++;
	if (!cond) { g_failures++; std::cerr << "FAIL: " << what << "\n"; }
}
bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

Sample valid(double t, uint64_t gen, float v = 0.5f) {
	Sample s; s.t = t; s.generation = gen; s.r = s.g = s.b = v; s.luma = luma709(v, v, v); s.valid = true; return s;
}
}

int main() {
	// -- linear-light averaging / relative luminance / alpha
	{
		uint8_t white[16 * 4]; for (auto& b : white) b = 255;
		float r, g, b;
		check(averageLinearRgba8OverBlack(white, 16, r, g, b) && near(r, 1) && near(g, 1) && near(b, 1), "opaque white -> 1,1,1");
		check(near(luma709(1, 1, 1), 1.0f), "luma of white == 1");
		check(near(luma709(1, 0, 0), 0.2126f) && near(luma709(0, 1, 0), 0.7152f) && near(luma709(0, 0, 1), 0.0722f), "Rec.709 weights");

		// Colour-space correctness: sRGB code 128 is 0.2158 in linear light (NOT 0.502).
		uint8_t mid[16 * 4]; for (int i = 0; i < 16; ++i) { mid[i*4]=mid[i*4+1]=mid[i*4+2]=128; mid[i*4+3]=255; }
		check(averageLinearRgba8OverBlack(mid, 16, r, g, b) && near(r, 0.21586f, 2e-4f) && near(g, r) && near(b, r), "sRGB 128 -> linear 0.2158 (EOTF applied)");
		check(near(srgbToLinear(0.04045f), 0.04045f / 12.92f, 1e-6f) && near(srgbToLinear(0.0f), 0.0f) && near(srgbToLinear(1.0f), 1.0f), "EOTF endpoints/toe");
		// Averaging happens in LINEAR light: half black / half white -> 0.5 linear (encoded averaging would give 0.2140 after decoding 127.5).
		uint8_t bw[16 * 4]; for (int i = 0; i < 16; ++i) { const uint8_t v = i < 8 ? 255 : 0; bw[i*4]=bw[i*4+1]=bw[i*4+2]=v; bw[i*4+3]=255; }
		check(averageLinearRgba8OverBlack(bw, 16, r, g, b) && near(r, 0.5f) && near(g, 0.5f), "black/white average is 0.5 in linear light");
		check(near(luma709(r, g, b), 0.5f), "relative luminance of the linear average");
		// Rec.709 on linear: pure sRGB red (255,0,0) -> Y = 0.2126.
		uint8_t red[16 * 4]; for (int i = 0; i < 16; ++i) { red[i*4]=255; red[i*4+1]=0; red[i*4+2]=0; red[i*4+3]=255; }
		check(averageLinearRgba8OverBlack(red, 16, r, g, b) && near(luma709(r, g, b), 0.2126f), "pure red -> Y 0.2126");

		uint8_t half[16 * 4]; for (int i = 0; i < 16; ++i) { half[i*4]=255; half[i*4+1]=0; half[i*4+2]=0; half[i*4+3]=(i < 8) ? 255 : 0; }
		check(averageLinearRgba8OverBlack(half, 16, r, g, b) && near(r, 0.5f) && near(g, 0) && near(b, 0), "transparent texels count as black");
		uint8_t clear[16 * 4] = {}; for (int i = 0; i < 16; ++i) { clear[i*4]=200; clear[i*4+3]=0; }
		check(averageLinearRgba8OverBlack(clear, 16, r, g, b) && r == 0 && g == 0 && b == 0, "alpha 0 composites to black regardless of rgb");
		// Valid black stays a VALID zero, distinct from "no data".
		uint8_t black[16 * 4] = {}; for (int i = 0; i < 16; ++i) black[i*4+3] = 255;
		check(averageLinearRgba8OverBlack(black, 16, r, g, b) && r == 0 && g == 0 && b == 0 && luma709(r, g, b) == 0, "opaque black measures as valid zero");
		check(!averageLinearRgba8OverBlack(nullptr, 16, r, g, b) && !averageLinearRgba8OverBlack(white, 0, r, g, b), "null/empty rejected (no synthetic black)");
	}
	// -- cadence + states
	{
		Series s;
		check(!s.due(0.0), "no generation: never due");
		check(s.state(0.0) == State::Unavailable, "no samples: Unavailable");
		s.beginGeneration(1, 10.0);
		check(s.due(10.0), "first sample due immediately");
		s.markIssued(10.0);
		check(!s.due(10.4) && s.due(10.5), "2 Hz cadence");
		s.markIssued(20.0); // long frame: no catch-up burst
		check(!s.due(20.4) && s.due(20.5), "no catch-up after a stall");
		check(s.push(valid(10.0, 1)), "push current generation");
		check(s.state(10.0) == State::Valid && s.state(11.5) == State::Valid, "valid within 3 periods");
		check(s.state(11.51) == State::Stale, "stale after 3 periods");
		check(s.push(valid(12.0, 1)) && s.state(12.0) == State::Valid, "fresh sample clears staleness");
	}
	// -- gaps never become black samples
	{
		Series s; s.beginGeneration(1, 0.0);
		s.push(valid(0.0, 1, 0.8f));
		Sample gap; gap.t = 0.5; gap.generation = 1; gap.valid = false;
		s.push(gap);
		check(s.size() == 2 && !s.at(1).valid, "gap stored as invalid");
		check(s.newestValid() && near(s.newestValid()->r, 0.8f), "newestValid skips gaps");
	}
	// -- generation identity
	{
		Series s; s.beginGeneration(1, 0.0);
		s.push(valid(0.0, 1)); s.push(valid(0.5, 1));
		const auto rev = s.revision();
		s.beginGeneration(2, 1.0);
		check(s.size() == 0 && s.generation() == 2 && s.revision() > rev, "new generation clears history, bumps revision");
		check(s.state(1.0) == State::Unavailable, "new generation: Unavailable until first valid");
		check(!s.push(valid(1.0, 1)), "late result from old generation rejected");
		check(s.size() == 0, "rejected sample not stored");
		check(s.push(valid(1.0, 2)) && s.size() == 1, "current generation accepted");
		check(s.at(0).generation == 2, "history holds only one generation");
	}
	// -- bounded ring, oldest->newest order
	{
		Series s; s.beginGeneration(1, 0.0);
		for (std::size_t i = 0; i < kCapacity + 30; ++i) s.push(valid(i * 0.5, 1, 0.5f));
		check(s.size() == kCapacity, "history bounded at capacity");
		check(near(static_cast<float>(s.at(0).t), 30 * 0.5f), "oldest is the 31st pushed");
		check(near(static_cast<float>(s.at(kCapacity - 1).t), (kCapacity + 29) * 0.5f), "newest last");
		bool mono = true; for (std::size_t i = 1; i < s.size(); ++i) mono = mono && s.at(i).t > s.at(i - 1).t;
		check(mono, "timestamps strictly increasing oldest->newest");
	}
	// -- snapshot publication: immutable, revision-driven, stale detection, gaps, valid zero
	{
		Series s; SnapshotPublisher pub;
		check(pub.publish(s, 0.0, "x") == nullptr, "no generation begun: snapshot stays null (never zeros)");
		s.beginGeneration(1, 10.0);
		auto p0 = pub.publish(s, 10.0, "blob-region-prototype");
		check(p0 && p0->state == scenecolor::State::Unavailable && p0->count == 0 && p0->generation == 1 && p0->sceneId == "blob-region-prototype", "new generation: Unavailable, empty, identified");
		check(pub.publish(s, 10.1, "blob-region-prototype") == p0 && pub.rebuilds() == 1, "unchanged series returns the SAME immutable object (no rebuild)");
		Sample black = valid(10.0, 1, 0.0f); // valid zero
		s.push(black);
		auto p1 = pub.publish(s, 10.0, "blob-region-prototype");
		check(p1 != p0 && p1->revision > p0->revision && p1->state == scenecolor::State::Valid && p1->count == 1, "new sample rebuilds, revision increases");
		check(p1->history[0].valid && p1->history[0].r == 0 && p1->history[0].luma == 0, "valid black is a valid zero, not a gap");
		check(p0->count == 0, "previously published snapshot is untouched (immutable)");
		Sample gap; gap.t = 10.5; gap.generation = 1; gap.valid = false; s.push(gap);
		auto p2 = pub.publish(s, 10.5, "blob-region-prototype");
		check(p2->count == 2 && !p2->history[1].valid && p2->state == scenecolor::State::Valid, "gap published as invalid, state still Valid");
		auto p3 = pub.publish(s, 12.1, "blob-region-prototype"); // no new samples; 3 periods elapsed since last VALID (10.0)
		check(p3 != p2 && p3->state == scenecolor::State::Stale && p3->revision > p2->revision, "stale transition republishes without a new sample");
		check(pub.publish(s, 12.2, "blob-region-prototype") == p3, "stays stale: no further rebuild");
		s.beginGeneration(2, 13.0);
		auto p4 = pub.publish(s, 13.0, "temporal-fields");
		check(p4->generation == 2 && p4->count == 0 && p4->state == scenecolor::State::Unavailable && p4->sceneId == "temporal-fields" && p4->revision > p3->revision, "scene change: empty new generation, new identity, monotonic revision");
		check(p3->generation == 1 && p3->count == 2, "old generation snapshot unchanged");
		bool mono = true; Series b; b.beginGeneration(1, 0.0); SnapshotPublisher pb;
		for (std::size_t i = 0; i < kCapacity + 30; ++i) b.push(valid(i * 0.5, 1, 0.5f));
		auto pf = pb.publish(b, (kCapacity + 29) * 0.5, "s");
		check(pf->count == kCapacity, "snapshot history bounded at capacity");
		for (std::size_t i = 1; i < pf->count; ++i) mono = mono && pf->history[i].t > pf->history[i - 1].t;
		check(mono, "snapshot timestamps strictly increasing oldest->newest");
		check(pf->colorSpace == scenecolor::ColorSpace::LinearSrgb, "colour space recorded explicitly");
	}
	// -- no allocation: the type is trivially a fixed array
	static_assert(sizeof(Series) < 16 * 1024, "Series is a small fixed-size value");

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
