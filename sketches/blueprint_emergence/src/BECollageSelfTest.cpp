// Developer-only logic test for the evolving Editorial Collage (standalone sketch).
// Run: BE_COLLAGE_SELFTEST=1 ./blueprint_emergence   -> logs PASS/FAIL lines and exits.
// Not part of the runtime build.

#include "BECollage.h"
#include "BESettings.h"
#include "ofLog.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <set>

namespace {

int g_fail = 0;
void check(const char * name, bool ok, const std::string & detail = {}) {
	if (ok)
		ofLogNotice("selftest") << "PASS " << name << (detail.empty() ? "" : " — " + detail);
	else {
		g_fail++;
		ofLogError("selftest") << "FAIL " << name << (detail.empty() ? "" : " — " + detail);
	}
}

struct SimStats {
	int maxFrag = 0, maxGfx = 0, maxElems = 0, maxQueue = 0, maxTransitions = 0;
	bool idsUnique = true, cropsInRange = true, orphan = false;
	std::vector<float> eventTimes; // successful non-structural events
	int farCrops = 0, totalCrops = 0;
	std::set<int> turnsSeen;
	bool flipXSeen = false, flipYSeen = false;
};

// Emulates BEComposition's phase callbacks around the collage: a new cycle every
// `cycleLen` seconds, DENSITY at +8, DISSOLVE at +cycleLen-15 (gated 4.5 s).
void drive(BECollage & c, uint32_t seed, float seconds, float dt, SimStats * st, float cycleLen = 45.0f) {
	float t = 0.0f, cycleT = cycleLen; // force a begin on the first step
	int cycle = 0;
	bool dens = false, diss = false;
	uint64_t lastCount = c.eventCount();
	while (t < seconds) {
		if (cycleT >= cycleLen) {
			c.beginLayout(seed * 1664525u + static_cast<uint32_t>(++cycle) * 1013904223u);
			c.setPhase(CollagePhase::Placement);
			cycleT = 0.0f;
			dens = diss = false;
		}
		if (!dens && cycleT >= 8.0f) { c.setPhase(CollagePhase::Density); dens = true; }
		if (!diss && cycleT >= cycleLen - 15.0f) { c.setPhase(CollagePhase::Dissolve); diss = true; }
		c.update(dt);
		t += dt;
		cycleT += dt;
		if (!st) continue;
		const auto & els = c.elements();
		int nf = 0, ng = 0;
		std::set<uint32_t> ids;
		for (const auto & e : els) {
			if (!ids.insert(e.id).second) st->idsUnique = false;
			if (e.state == ElemState::Exiting || e.exitAt >= 0.0f) continue;
			(e.video ? nf : ng)++;
		}
		st->maxFrag = std::max(st->maxFrag, nf);
		st->maxGfx = std::max(st->maxGfx, ng);
		st->maxElems = std::max(st->maxElems, static_cast<int>(els.size()));
		st->maxQueue = std::max(st->maxQueue, static_cast<int>(c.queueSize()));
		st->maxTransitions = std::max(st->maxTransitions, c.inTransitionCount());
		for (const auto & e : els) {
			if (e.state == ElemState::Exiting && e.stateT > e.exitDur + 0.5f) st->orphan = true;
			if (e.state == ElemState::Pending && e.spawnAt < c.directorTime() - 1.0f) st->orphan = true;
			if (!e.video) continue;
			const ofRectangle uv = c.cropUv(e, 16.0f / 9.0f);
			if (uv.x < -1e-4f || uv.y < -1e-4f || uv.x + uv.width > 1.0001f || uv.y + uv.height > 1.0001f) st->cropsInRange = false;
		}
		if (c.eventCount() != lastCount) {
			lastCount = c.eventCount();
			st->eventTimes.push_back(t);
		}
	}
}

} // namespace

int runCollageSelfTest() {
	g_fail = 0;
	ofLogNotice("selftest") << "── Editorial Collage logic tests ──";

	// 1. UV transform: 16 combinations collapse to the 8 dihedral orientations; corners
	//    always come from the crop's own four corners; destination independent of crop.
	{
		std::set<std::string> distinct;
		bool cornersOk = true;
		for (int t = 0; t < 4; t++)
			for (int fx = 0; fx < 2; fx++)
				for (int fy = 0; fy < 2; fy++) {
					const CollageUvQuad q = makeCollageUvQuad(0.2f, 0.1f, 0.7f, 0.9f, static_cast<QuarterTurn>(t), fx, fy);
					std::string key;
					for (int i = 0; i < 4; i++) {
						const bool xOk = std::fabs(q.c[i].x - 0.2f) < 1e-5f || std::fabs(q.c[i].x - 0.7f) < 1e-5f;
						const bool yOk = std::fabs(q.c[i].y - 0.1f) < 1e-5f || std::fabs(q.c[i].y - 0.9f) < 1e-5f;
						cornersOk = cornersOk && xOk && yOk;
						key += std::to_string(static_cast<int>(q.c[i].x * 100)) + "," + std::to_string(static_cast<int>(q.c[i].y * 100)) + ";";
					}
					distinct.insert(key);
				}
		check("uv: every corner is a crop corner", cornersOk);
		check("uv: 16 turn/flip combos give the 8 distinct orientations", distinct.size() == 8, std::to_string(distinct.size()));
		const CollageUvQuad id = makeCollageUvQuad(0.2f, 0.1f, 0.7f, 0.9f, QuarterTurn::Deg0, false, false);
		check("uv: identity maps TL->(u0,v0) and BR->(u1,v1)",
			std::fabs(id.c[0].x - 0.2f) < 1e-5f && std::fabs(id.c[0].y - 0.1f) < 1e-5f && std::fabs(id.c[2].x - 0.7f) < 1e-5f && std::fabs(id.c[2].y - 0.9f) < 1e-5f);
	}

	// 2. Palettes: at most two chromatic accents per curated family; neutrals stay neutral.
	{
		bool ok = true;
		std::string detail;
		for (const auto & p : hud::curatedCollagePalettes()) {
			std::vector<ofColor> chroma;
			auto consider = [&](const ofColor & c) {
				if (c.getSaturation() < 60) return;
				for (const auto & k : chroma)
					if (std::abs(k.r - c.r) + std::abs(k.g - c.g) + std::abs(k.b - c.b) < 12) return;
				chroma.push_back(c);
			};
			consider(p.background);
			consider(p.primary);
			consider(p.resolve(hud::PaletteToken::Secondary));
			consider(p.dark);
			consider(p.light);
			if (chroma.size() > 2) {
				ok = false;
				detail += std::string(p.name) + "=" + std::to_string(chroma.size()) + " ";
			}
		}
		check("palette: <=2 chromatic accents in every curated family", ok, detail.empty() ? std::to_string(hud::curatedCollagePalettes().size()) + " families" : detail);
	}

	const std::string font = "";
	auto make = [&](const CollageTuning & tun) {
		auto c = std::make_unique<BECollage>();
		c->setup(1280, 720, font, 13);
		c->setTuning(tun);
		return c;
	};

	// 3. Seeded determinism: same seed + same dt sequence -> identical state and events.
	{
		auto a = make(CollageTuning::balanced()), b = make(CollageTuning::balanced()), d = make(CollageTuning::balanced());
		drive(*a, 4242u, 240.0f, 1.0f / 30.0f, nullptr);
		drive(*b, 4242u, 240.0f, 1.0f / 30.0f, nullptr);
		drive(*d, 4243u, 240.0f, 1.0f / 30.0f, nullptr);
		check("determinism: same seed + dt => identical state hash", a->stateHash() == b->stateHash() && a->eventCount() == b->eventCount(),
			std::to_string(a->eventCount()) + " events");
		check("determinism: different seed => different evolution", a->stateHash() != d->stateHash());
	}

	// 4-7, 10. Long BALANCED run (20 simulated minutes): bounds, ids, cleanup, cadence, coverage.
	{
		auto c = make(CollageTuning::balanced());
		SimStats st;
		drive(*c, 99u, 1200.0f, 1.0f / 20.0f, &st);
		const auto & t = c->tuning();
		check("bounds: fragments never exceed maxFragments", st.maxFrag <= t.maxFragments, "max " + std::to_string(st.maxFrag) + "/" + std::to_string(t.maxFragments));
		check("bounds: graphics never exceed maxGraphics", st.maxGfx <= t.maxGraphics, "max " + std::to_string(st.maxGfx) + "/" + std::to_string(t.maxGraphics));
		check("bounds: registry size bounded (no growth over cycles)", st.maxElems <= t.maxFragments + t.maxGraphics + 40, "max " + std::to_string(st.maxElems));
		check("bounds: event queue bounded", st.maxQueue <= t.maxQueue, "max " + std::to_string(st.maxQueue));
		check("ids: no collisions", st.idsUnique);
		check("cleanup: no orphaned exiting/pending elements", !st.orphan);
		check("uv: every crop stays inside the source frame", st.cropsInRange);
		check("cadence: swells occurred", c->swellCount() >= 20, std::to_string(c->swellCount()));
		check("cadence: recompositions occurred", c->recomposeCount() >= 20, std::to_string(c->recomposeCount()));
		check("mutations: split exercised", c->eventCountOf(CollageEvent::Split) > 0, std::to_string(c->eventCountOf(CollageEvent::Split)));
		check("mutations: merge exercised", c->eventCountOf(CollageEvent::Merge) > 0, std::to_string(c->eventCountOf(CollageEvent::Merge)));
		check("mutations: crop swap / transform / replace / slide exercised",
			c->eventCountOf(CollageEvent::CropSwap) > 0 && c->eventCountOf(CollageEvent::TransformChange) > 0
				&& c->eventCountOf(CollageEvent::ReplaceFragment) > 0 && c->eventCountOf(CollageEvent::BlockSlide) > 0);
		// No long freeze: longest gap between successful events after the establishing period.
		float worst = 0.0f, prev = 12.0f;
		for (float et : st.eventTimes) {
			if (et < 12.0f) continue;
			worst = std::max(worst, et - prev);
			prev = et;
		}
		check("scheduling: no stall — longest event gap < 6 s (BALANCED)", worst < 6.0f, std::to_string(worst) + " s over " + std::to_string(st.eventTimes.size()) + " events");
		check("concurrency: simultaneous transitions stay modest", st.maxTransitions <= 24, "max " + std::to_string(st.maxTransitions));
	}

	// 6. QUIET still evolves (no stall) and the three profiles order by activity.
	{
		auto q = make(CollageTuning::quiet()), b = make(CollageTuning::balanced()), k = make(CollageTuning::kinetic());
		SimStats sq, sb, sk;
		drive(*q, 7u, 600.0f, 1.0f / 20.0f, &sq);
		drive(*b, 7u, 600.0f, 1.0f / 20.0f, &sb);
		drive(*k, 7u, 600.0f, 1.0f / 20.0f, &sk);
		float worst = 0.0f, prev = 12.0f;
		for (float et : sq.eventTimes) {
			if (et < 12.0f) continue;
			worst = std::max(worst, et - prev);
			prev = et;
		}
		check("profiles: QUIET keeps evolving — longest event gap < 12 s", worst < 12.0f, std::to_string(worst) + " s");
		check("profiles: event rate QUIET < BALANCED < KINETIC", q->eventCount() < b->eventCount() && b->eventCount() < k->eventCount(),
			std::to_string(q->eventCount()) + " / " + std::to_string(b->eventCount()) + " / " + std::to_string(k->eventCount()));
		check("profiles: KINETIC respects its own caps", sk.maxFrag <= k->tuning().maxFragments && sk.maxGfx <= k->tuning().maxGraphics);
	}

	// 7b. Pi fallback keeps evolving within tighter caps.
	{
		CollageTuning lo = CollageTuning::balanced();
		lo.applyLowQuality();
		auto c = make(lo);
		SimStats st;
		drive(*c, 31u, 300.0f, 1.0f / 20.0f, &st);
		check("low-quality: tighter caps respected and still evolving", st.maxFrag <= lo.maxFragments && st.maxGfx <= lo.maxGraphics && c->eventCount() > 100,
			std::to_string(c->eventCount()) + " events");
	}

	// 8. Switch away/back: forced restart resets owned objects; lock/unlock safe; dt storm bounded.
	{
		auto c = make(CollageTuning::balanced());
		drive(*c, 5u, 90.0f, 1.0f / 30.0f, nullptr);
		const size_t before = c->elements().size();
		c->lock(true);
		const uint64_t h = c->stateHash();
		c->update(1.0f);
		check("switch: locked collage does not advance", c->stateHash() == h);
		c->lock(false);
		c->beginLayout(77u, true); // like deactivate -> reactivate: clean registry
		uint32_t minId = 1u << 30;
		for (const auto & e : c->elements())
			minId = std::min(minId, e.id);
		check("switch: forced restart rebuilds a clean registry", minId == 1 && c->elements().size() > 0 && c->queueSize() == 0, std::to_string(before) + " -> " + std::to_string(c->elements().size()));
		const uint64_t ev0 = c->eventCount();
		c->update(500.0f); // resume after a long suspend
		check("dt: a 500 s hitch is clamped (no event storm)", c->eventCount() - ev0 <= 4, std::to_string(c->eventCount() - ev0));
	}

	// 9. Lifecycle: dissolve gate opens, then a new cycle recomposes around a retained anchor.
	{
		auto c = make(CollageTuning::balanced());
		c->beginLayout(11u);
		for (int i = 0; i < 600; i++)
			c->update(0.05f);
		const uint32_t anchor = [&] {
			for (const auto & e : c->elements())
				if (e.video && e.anchor) return e.id;
			return 0u;
		}();
		c->setPhase(CollagePhase::Dissolve);
		check("lifecycle: dissolve gate closed at start", !c->dissolveFinished());
		for (int i = 0; i < 100; i++)
			c->update(0.05f);
		check("lifecycle: dissolve gate opens after the selective clear", c->dissolveFinished());
		int alive = 0;
		for (const auto & e : c->elements())
			if (e.state != ElemState::Exiting && e.exitAt < 0.0f) alive++;
		check("lifecycle: dissolve leaves residue (not a dead screen)", alive >= 3, std::to_string(alive) + " alive");
		c->beginLayout(12u);
		bool kept = false;
		for (const auto & e : c->elements())
			if (e.id == anchor && e.state != ElemState::Exiting) kept = true;
		check("lifecycle: recomposition retains the anchor", anchor == 0 || kept);
	}

	// 11. Transform distribution roughly matches the curated mix across many layouts.
	{
		int n = 0, deg[4] = { 0, 0, 0, 0 }, fx = 0, fy = 0;
		for (uint32_t s = 1; s <= 400; s++) {
			const CollageLayout L = BECollage::generateLayout(s, 1280, 720);
			for (const auto & f : L.fragments) {
				n++;
				deg[static_cast<int>(f.rotation)]++;
				fx += f.flipX;
				fy += f.flipY;
			}
		}
		const float p0 = 100.0f * deg[0] / n, p90 = 100.0f * deg[1] / n, p270 = 100.0f * deg[3] / n;
		check("transforms: 0 deg is the plurality and all four turns + both flips occur",
			p0 > p90 && p0 > p270 && deg[1] > 0 && deg[2] > 0 && deg[3] > 0 && fx > 0 && fy > 0,
			"0:" + std::to_string(static_cast<int>(p0)) + "% 90:" + std::to_string(static_cast<int>(p90)) + "% 270:" + std::to_string(static_cast<int>(p270)) + "% n=" + std::to_string(n));
	}

	ofLogNotice("selftest") << (g_fail == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(g_fail));
	return g_fail;
}
