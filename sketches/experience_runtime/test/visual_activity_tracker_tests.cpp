// OC-004 tracker/publisher tests (bare compiler). Covers snapshot validity+generation, optional publication
// (publisher caching), valid zero vs unavailable, missing/stale, generation reset, transition, fixed-log level.
#include "SceneVisualActivitySnapshot.h"
#include "SceneVisualActivityTracker.h"
#include <cstdio>
using namespace sceneactivity;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
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
	std::printf(fails ? "FAILED\n" : "visual_activity_tracker_tests: all passed\n");
	return fails;
}
