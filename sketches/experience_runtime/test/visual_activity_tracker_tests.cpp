// OC-004 tracker/publisher tests (bare compiler). Covers snapshot validity+generation, optional publication
// (publisher caching), valid zero vs unavailable, missing/stale, generation reset, transition, fixed-log level.
#include "SceneVisualActivitySnapshot.h"
#include "SceneVisualActivityTracker.h"
#include <cstdio>
using namespace sceneactivity;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
// OC-004 static-boundary regression. A scripted driver that uses StaticBoundary + VisualActivityTracker exactly the way
// SceneActivityProbe does (static entry: tracker.onStatic + drop pending + forget comparison grid, once; live frame
// ends the interval; completion publishes only while the issued (generation, epoch) is current and not static).
struct ProbeModel {
	struct Pending { bool active = false; uint64_t gen = 0, epoch = 0; bool havePrev = false; double t = 0.0; };
	StaticBoundary stat; VisualActivityTracker tr;
	uint64_t gen = 0; bool prevValid = false; Pending pend;
	int gpuResets = 0, discarded = 0;
	void clear() { prevValid = false; pend.active = false; ++gpuResets; }
	void beginGeneration(uint64_t g, const char* id) { gen = g; tr.beginGeneration(g, id); stat.resetForGeneration(); clear(); }
	void staticFrame() { if (stat.enter()) { tr.onStatic(); clear(); } }
	void liveFrame() { stat.exitOnLive(); }
	Pending issue(double t) { pend = {true, gen, stat.epoch(), prevValid, t}; prevValid = true; return pend; }
	void complete(const Pending& p, double raw) { // the poll() path
		if (!p.active) return;
		if (pend.active && pend.t == p.t) pend.active = false;
		if (p.gen != gen || !stat.eligible(p.epoch)) { ++discarded; return; }
		if (p.havePrev) tr.onSample(p.t, raw); else tr.onLiveNoMeasurement();
	}
};

void staticBoundaryRegression() {
	ProbeModel m;
	m.beginGeneration(1, "blob");
	m.liveFrame();
	auto base = m.issue(1.00); m.complete(base, 0.0); // first grid of the generation: baseline only
	CHECK(m.tr.evaluate(1.01) == scenevisual::State::Unavailable);
	auto p1 = m.issue(1.10); m.complete(p1, 0.02);
	CHECK(m.tr.evaluate(1.11) == scenevisual::State::Valid);
	// A readback is pending when the runtime enters the static interval (inside the 10 Hz period: the old rate limit would have kept it).
	auto pend = m.issue(1.20);
	const ProbeModel::Pending stale = pend;
	const int resetsBefore = m.gpuResets;
	m.staticFrame(); // first static frame, 33 ms after the issue
	CHECK(m.tr.evaluate(1.233) == scenevisual::State::Transition);
	CHECK(!m.pend.active);                         // pending dropped immediately
	CHECK(!m.prevValid);                           // comparison grid ineligible
	CHECK(!m.stat.eligible(stale.epoch));          // the issued epoch is no longer eligible
	CHECK(m.gpuResets == resetsBefore + 1);
	// Delayed completion of that observation (even a stale copy) must not publish Valid.
	m.complete(stale, 0.5);
	CHECK(m.discarded == 1);
	CHECK(m.tr.evaluate(1.266) == scenevisual::State::Transition);
	CHECK(m.tr.build(1.266)->state == scenevisual::State::Transition && m.tr.build(1.266)->level == 0.0f);
	// Repeated static frames: no further resets, still Transition.
	for (int i = 0; i < 20; ++i) m.staticFrame();
	CHECK(m.gpuResets == resetsBefore + 1);
	CHECK(m.stat.inStatic() && m.tr.evaluate(1.9) == scenevisual::State::Transition);
	m.complete(stale, 0.5);
	CHECK(m.tr.evaluate(1.9) == scenevisual::State::Transition);
	// New generation: first new frame is a baseline (no Valid), later same-generation frames are Valid.
	m.beginGeneration(2, "temporal");
	m.liveFrame();
	CHECK(m.tr.evaluate(2.0) == scenevisual::State::Unavailable);
	auto b2 = m.issue(2.00); m.complete(b2, 0.9);   // would be a cross-scene spike if differenced; havePrev is false
	CHECK(m.tr.evaluate(2.01) == scenevisual::State::Unavailable);
	CHECK(!b2.havePrev);
	auto v2 = m.issue(2.10); m.complete(v2, 0.03);
	CHECK(m.tr.evaluate(2.11) == scenevisual::State::Valid && m.tr.build(2.11)->generation == 2);
	// A pending observation from generation 2 is also rejected after a generation change (completion-generation guard intact).
	auto g2 = m.issue(2.20);
	m.beginGeneration(3, "blueprint"); m.liveFrame();
	const int d0 = m.discarded;
	m.complete(g2, 0.4);
	CHECK(m.discarded == d0 + 1 && m.tr.evaluate(2.3) == scenevisual::State::Unavailable);
	// A live frame ends the static interval: the next static interval is a new entry (one more reset).
	const int r = m.gpuResets;
	m.staticFrame(); m.staticFrame(); m.liveFrame(); m.staticFrame();
	CHECK(m.gpuResets == r + 2);
	// StaticBoundary in isolation.
	StaticBoundary b;
	const uint64_t before = b.epoch(); // an observation issued during the preceding live interval
	CHECK(b.eligible(before));
	CHECK(b.enter() && !b.enter() && b.inStatic() && !b.eligible(before) && !b.eligible(b.epoch()));
	b.exitOnLive();
	CHECK(!b.inStatic() && !b.eligible(before) && b.eligible(b.epoch()));
}

int main() {
	// fixed-log mapping bounds 3e-4 .. 0.30
	CHECK(fixedLogLevel(0.0) == 0.0 && fixedLogLevel(3e-4) == 0.0 && fixedLogLevel(1e-5) == 0.0);
	CHECK(fixedLogLevel(0.30) > 0.999999 && fixedLogLevel(5.0) == 1.0);
	CHECK(std::fabs(fixedLogLevel(std::sqrt(3e-4 * 0.30)) - 0.5) < 1e-9);

	VisualActivityTracker t;
	// before any generation / observation: Unavailable (never a zero)
	CHECK(t.evaluate(0.0) == scenevisual::State::Unavailable);
	t.beginGeneration(1, "blob");
	auto s = t.build(0.0);
	CHECK(s->state == scenevisual::State::Unavailable && s->generation == 1 && s->sceneId == "blob");

	// valid zero is Valid with level 0 (not Unavailable)
	t.onSample(1.0, 0.0);
	s = t.build(1.05);
	CHECK(s->state == scenevisual::State::Valid && s->raw == 0.0f && s->level == 0.0f && s->rawT == 1.0);
	// a real value
	t.onSample(1.1, 0.05);
	s = t.build(1.12);
	CHECK(s->state == scenevisual::State::Valid && std::fabs(s->raw - 0.05f) < 1e-7 && s->level > 0.7f && s->level < 0.9f);
	CHECK(s->nominalPeriodSeconds == 0.1 && s->stalePeriods == 3);

	// stale after 3 periods: last observation retained but flagged
	CHECK(t.evaluate(1.1 + 0.29) == scenevisual::State::Valid);
	CHECK(t.evaluate(1.1 + 0.31) == scenevisual::State::Stale);
	s = t.build(2.0);
	CHECK(s->state == scenevisual::State::Stale && std::fabs(s->raw - 0.05f) < 1e-7);

	// invalid samples (NaN / negative) are rejected, never stored as 0
	VisualActivityTracker u; u.beginGeneration(1, "x");
	u.onSample(0.1, NAN); u.onSample(0.2, -1.0);
	CHECK(u.evaluate(0.2) == scenevisual::State::Unavailable);

	// transition: static frame invalidates the prior observation; it is not zero activity
	t.onSample(3.0, 0.02);
	t.onStatic();
	s = t.build(3.01);
	CHECK(s->state == scenevisual::State::Transition && s->level == 0.0f);
	t.onLiveNoMeasurement(); // first grid after the static run: nothing to difference
	CHECK(t.evaluate(3.2) == scenevisual::State::Unavailable);
	t.onSample(3.3, 0.01);
	CHECK(t.evaluate(3.3) == scenevisual::State::Valid);

	// generation change drops the prior observation and bumps generation
	t.beginGeneration(2, "temporal");
	s = t.build(3.31);
	CHECK(s->state == scenevisual::State::Unavailable && s->generation == 2 && s->sceneId == "temporal");

	// unsupported GPU path -> Unavailable
	t.onSample(4.0, 0.03); t.onUnsupported();
	CHECK(t.evaluate(4.0) == scenevisual::State::Unavailable);

	// publisher: unchanged state republishes the same pointer; revision or state change rebuilds
	VisualActivityTracker p; VisualActivityPublisher pub;
	p.beginGeneration(1, "q"); p.onSample(1.0, 0.01);
	auto a = pub.publish(p, 1.05);
	auto b = pub.publish(p, 1.06);
	CHECK(a.get() == b.get());
	auto c = pub.publish(p, 1.5); // Valid -> Stale without a new event
	CHECK(c.get() != a.get() && c->state == scenevisual::State::Stale);
	p.onSample(1.6, 0.02);
	auto d = pub.publish(p, 1.61);
	CHECK(d.get() != c.get() && d->state == scenevisual::State::Valid && d->revision > c->revision);
	staticBoundaryRegression();
	std::printf(fails ? "FAILED\n" : "visual_activity_tracker_tests: all passed\n");
	return fails;
}
