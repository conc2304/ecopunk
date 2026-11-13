// Standalone tests for ShredEngine.h (no openFrameworks / GL). Build: make -C test -f Makefile.tests shred_engine_tests
#include "../src/ShredEngine.h"

#include <cstdio>
#include <set>
#include <string>

using namespace shred;

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::vector<std::string> fx() { return {"invert", "solarize", "recolor", "heatmap_recolor", "hue_rotate", "threshold"}; }

static void testModes() {
	std::set<std::string> keys;
	for (int m = 0; m < kModeCount; ++m) {
		const ModeSpec& s = spec(ModeId(m));
		CHECK(int(s.id) == m, "mode order");
		keys.insert(s.key);
		for (int d = 0; d < 2; ++d) CHECK(s.dial[d].label && s.dial[d].lo <= s.dial[d].hi && s.dial[d].def >= 0 && s.dial[d].def <= 1, "dial spec %s", s.key);
	}
	CHECK(int(keys.size()) == 13, "13 unique mode ids");
	uint32_t all = 0; for (int m = 0; m < kModeCount; ++m) all |= spec(ModeId(m)).policyMask;
	CHECK(all == (1u << kPolicyCount) - 1, "all 10 policies have at least one compatible mode");
}

static void testDials() {
	Engine e; e.configure(1280, 720, false, 7);
	e.setDial(ModeId::ECHO_WAVE, 0, 5.f); CHECK(e.dial(ModeId::ECHO_WAVE, 0) == 1.f, "clamp hi");
	e.setDial(ModeId::ECHO_WAVE, 0, -2.f); CHECK(e.dial(ModeId::ECHO_WAVE, 0) == 0.f, "clamp lo");
	CHECK(std::fabs(e.dialPhys(ModeId::ECHO_WAVE, 0) - spec(ModeId::ECHO_WAVE).dial[0].lo) < 1e-5f, "maps to lo");
	e.setDial(ModeId::ECHO_WAVE, 1, 0.8f);
	e.setMode(ModeId::SCAN); e.setMode(ModeId::ECHO_WAVE);
	CHECK(e.dial(ModeId::ECHO_WAVE, 1) == 0.8f, "dial persists across mode transitions");
	e.resetDials(); CHECK(e.dial(ModeId::ECHO_WAVE, 1) == spec(ModeId::ECHO_WAVE).dial[1].def, "reset restores defaults");
	// observable effect: ECHO span drives crop offsets
	auto maxCrop = [&](float d) { Engine g; g.configure(1280, 720, false, 3); g.setAutoCycle(false); g.setMode(ModeId::ECHO_WAVE); g.setDial(ModeId::ECHO_WAVE, 0, d); float m = 0; for (int i = 0; i < 1500; ++i) { g.update(1.f / 60); for (auto& f : g.fragments()) m = std::max(m, std::fabs(f.pcx)); } return m; };
	CHECK(maxCrop(1.f) > maxCrop(0.f) * 1.5f, "ECHO wave span changes crop amplitude");
	auto vac = [&](float d) { (void)d; return 0; }; (void)vac;
	// SCAN width drives the resolved fraction
	auto resolved = [&](float d) { Engine g; g.configure(1280, 720, false, 5); g.setAutoCycle(false); g.setMode(ModeId::SCAN); g.setDial(ModeId::SCAN, 0, d); float best = 0; for (int i = 0; i < 1800; ++i) { g.update(1.f / 60); best = std::max(best, g.telemetry().coherence); } return best; };
	CHECK(resolved(1.f) > resolved(0.f), "SCAN width raises peak coherence");
}

static void testDeterminism() {
	auto run = [](uint64_t seed) { Engine e; e.configure(1280, 720, false, seed); e.setEffectCandidates(fx()); for (int i = 0; i < 3000; ++i) e.update(1.f / 60); double h = 0; for (auto& f : e.fragments()) h += f.dx * 1.3 + f.dy * 0.7 + f.u0 * 5 + f.alpha; return h; };
	CHECK(run(11) == run(11), "same seed replays identically");
	CHECK(run(11) != run(12), "different seed differs");
}

static void testUvBounds() {
	for (int m = 0; m < kModeCount; ++m) {
		Engine e; e.configure(1280, 720, false, 20 + m); e.setAutoCycle(false); e.setMode(ModeId(m));
		for (int i = 0; i < 3600; ++i) {
			e.update(1.f / 60);
			for (auto& f : e.fragments()) {
				if (!f.visible) continue;
				float a = std::min(f.u0, f.u1), b = std::max(f.u0, f.u1), c = std::min(f.v0, f.v1), d = std::max(f.v0, f.v1);
				CHECK(a >= -1e-4f && b <= 1.0001f && c >= -1e-4f && d <= 1.0001f, "uv bounds mode %d: %f %f %f %f", m, a, b, c, d);
				CHECK(f.dx >= -0.01f && f.dx + f.dw <= 1280.01f && f.dy >= -0.01f && f.dy + f.dh <= 720.01f, "dest within canvas");
			}
		}
	}
}

static void testCoherenceCeilingAndInterruption() {
	for (int m = 0; m < kModeCount; ++m) {
		Engine e; e.configure(1280, 720, false, 40 + m); e.setAutoCycle(false); e.setMode(ModeId(m));
		// extreme dials toward maximum coherence
		e.setDial(ModeId(m), 0, m == int(ModeId::INTERRUPT) ? 1.f : 0.f); e.setDial(ModeId::SHRED, 0, 1.f);
		float mx = 0;
		for (int i = 0; i < 7200; ++i) { e.update(1.f / 60); mx = std::max(mx, e.telemetry().coherence); }
		CHECK(mx <= kCoherenceCeiling + 1e-4f, "mode %d coherence %.4f above ceiling", m, mx);
		CHECK(e.telemetry().maxCoherence <= kCoherenceCeiling + 1e-4f, "mode %d maxCoherence", m);
	}
	// SHRED near threshold must interrupt
	Engine e; e.configure(1280, 720, false, 9); e.setAutoCycle(false); e.setMode(ModeId::SHRED); e.setDial(ModeId::SHRED, 0, 1.f);
	for (int i = 0; i < 7200; ++i) e.update(1.f / 60);
	CHECK(e.telemetry().interrupts > 0, "interruptions occur");
}

static void testEffectCapStress() {
	Engine e; e.configure(1280, 720, false, 77); e.setEffectCandidates(fx()); e.setAutoCycle(false);
	Rng r(5); float worst = 0; int maxDistinct = 0; bool sawZero = false, sawOne = false, sawTwo = false;
	for (int i = 0; i < 60 * 60 * 12; ++i) { // 12 simulated minutes, rapid mode switches
		if (i % 97 == 0 && r.chance(0.5f)) e.setMode(ModeId(r.irange(0, 12)));
		if (i % 811 == 0 && r.chance(0.3f)) e.onMediaChanged();
		e.setDial(e.mode(), r.irange(0, 1), r.f() * (i % 5 == 0));
		e.update(1.f / 60);
		const Telemetry& t = e.telemetry();
		worst = std::max(worst, t.effectAreaFrac); maxDistinct = std::max(maxDistinct, t.effectCount);
		sawZero |= t.effectCount == 0; sawOne |= t.effectCount == 1; sawTwo |= t.effectCount == 2;
		// at most two distinct IDs among slots that hold fragments
		std::set<std::string> ids; for (auto& f : e.fragments()) if (f.effectSlot >= 0) ids.insert(e.slotInfo(f.effectSlot).effectId);
		CHECK(ids.size() <= 2, "distinct ids %zu", ids.size());
		CHECK(t.effectAreaFrac <= kEffectAreaCap + 1e-4f, "effect area %.4f", t.effectAreaFrac);
		// desaturation / vacancy never counted as effects
		for (auto& f : e.fragments()) if (f.effectSlot < 0) CHECK(f.effAlpha == 0.f, "no effect alpha without slot");
	}
	std::printf("effect stress: worst area %.4f  maxDistinct %d  zero:%d one:%d two:%d  forcedReleases %d\n", worst, maxDistinct, sawZero, sawOne, sawTwo, e.telemetry().forcedEffectReleases);
	CHECK(worst <= kEffectAreaCap, "worst area");
	CHECK(maxDistinct <= 2, "max distinct");
	CHECK(sawZero && sawOne, "0 and 1 effect states occur");
}

static void testGovernorSafetyNet() {
	Engine e; e.configure(1280, 720, false, 3); e.setAutoCycle(false); e.setMode(ModeId::SHRED);
	for (int i = 0; i < 240; ++i) e.update(1.f / 60);
	e.forceAssignForTesting(0, 1000); // deliberately over-assign everything
	e.update(1.f / 60);
	CHECK(e.telemetry().effectAreaFrac <= kEffectAreaCap, "safety net clamps over-assignment: %.3f", e.telemetry().effectAreaFrac);
	CHECK(e.telemetry().forcedEffectReleases > 0, "safety net reports its correction");
}

static void testBoundedGrowth() {
	Engine e; e.configure(1280, 720, false, 31); e.setEffectCandidates(fx()); e.setAutoCycle(true);
	size_t mx = 0, mg = 0; Rng r(1);
	for (int i = 0; i < 60 * 60 * 30; ++i) { if (i % 53 == 0) e.setMode(ModeId(r.irange(0, 12))); e.update(1.f / 60); mx = std::max(mx, e.fragments().size()); mg = std::max(mg, e.ghostFragments().size()); }
	std::printf("bounded growth: max fragments %zu, max ghosts %zu\n", mx, mg);
	CHECK(mx < 400 && mg <= 700, "fragment counts bounded");
}

static void testPiProfile() {
	CHECK(!effectEligible(true, true, -1, true), "unclassified is not Pi-eligible");
	CHECK(effectEligible(true, true, -1, false), "unclassified OK on desktop");
	CHECK(!effectEligible(true, true, 0, false), "piSafe=false excluded");
	CHECK(effectEligible(true, true, 1, true), "piSafe=true eligible on Pi");
	CHECK(!effectEligible(false, true, 1, true), "stateful excluded");
	Engine a, b; a.configure(1280, 720, false, 1); b.configure(1280, 720, true, 1);
	CHECK(b.fragments().size() < a.fragments().size(), "Pi profile uses fewer fragments");
}

static void testAspectAndDt() {
	Engine e; e.configure(720, 1280, false, 2); // portrait canvas
	for (int i = 0; i < 600; ++i) e.update(i % 7 == 0 ? 0.2f : 0.004f);
	CHECK(e.telemetry().effectAreaFrac <= kEffectAreaCap, "portrait ok");
	for (auto& f : e.fragments()) if (f.visible) CHECK(f.u1 <= 1.0001f && f.u0 >= -1e-4f, "portrait uv");
}

int main() {
	testModes(); testDials(); testDeterminism(); testUvBounds(); testCoherenceCeilingAndInterruption();
	testEffectCapStress(); testGovernorSafetyNet(); testBoundedGrowth(); testPiProfile(); testAspectAndDt();
	std::printf(g_fail ? "FAILED (%d)\n" : "ALL PASSED\n", g_fail);
	return g_fail ? 1 : 0;
}
