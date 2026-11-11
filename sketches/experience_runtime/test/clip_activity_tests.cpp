// Precomputed clip activity (ClipActivity.h): curve lookup and the per-frame feeder (bare compiler).
#include "ClipActivity.h"
#include <cstdio>
using namespace clipactivity;
using scenevisual::State;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
static bool near(double a, double b, double e = 1e-6) { return std::fabs(a - b) < e; }

static Curve ramp() { Curve c; c.stepSeconds = 0.1; c.durationSeconds = 0.4; c.raw = {0.0f, 0.1f, 0.2f, 0.3f}; return c; }

int main() {
	// -- Index ---------------------------------------------------------------------------------------
	ClipActivityIndex idx;
	CHECK(idx.empty());
	CHECK(idx.add("a.mp4", ramp()) && idx.has("a.mp4") && idx.size() == 1);
	{ Curve bad; bad.raw = {}; CHECK(!idx.add("e.mp4", bad)); }
	{ Curve bad = ramp(); bad.stepSeconds = 0.0; CHECK(!idx.add("s.mp4", bad)); }
	{ Curve bad = ramp(); bad.raw[1] = -0.5f; CHECK(!idx.add("n.mp4", bad)); }
	{ Curve bad = ramp(); bad.raw[1] = std::nanf(""); CHECK(!idx.add("nan.mp4", bad)); }
	CHECK(!idx.add("", ramp()));
	CHECK(idx.size() == 1);

	CHECK(!idx.rawAt("missing.mp4", 0.1));                  // unknown clip: not a measurement
	CHECK(!idx.rawAt("a.mp4", -1.0) && !idx.rawAt("a.mp4", std::nan(""))); // bad position
	CHECK(near(*idx.rawAt("a.mp4", 0.05), 0.0));            // centre of step 0
	CHECK(near(*idx.rawAt("a.mp4", 0.15), 0.1));            // centre of step 1
	CHECK(near(*idx.rawAt("a.mp4", 0.10), 0.05));           // midway between step centres: linear
	CHECK(near(*idx.rawAt("a.mp4", 0.0), 0.0));             // clamped at the start
	CHECK(near(*idx.rawAt("a.mp4", 99.0), 0.3));            // clamped at the end (loop overshoot)
	{ Curve one; one.raw = {0.2f}; idx.add("one.mp4", one); CHECK(near(*idx.rawAt("one.mp4", 5.0), 0.2)); }

	// -- Feeder --------------------------------------------------------------------------------------
	ClipActivityFeeder f(idx);
	CHECK(f.publish(0.0)->state == State::Unavailable);      // before any generation / data: honest unavailable

	f.beginGeneration(1, "blob");
	f.update(1.0, true, "a.mp4", 0.15);
	auto s = f.publish(1.0);
	CHECK(s->state == State::Valid && s->generation == 1 && s->sceneId == "blob");
	CHECK(near(s->raw, 0.1, 1e-5));
	CHECK(near(s->level, sceneactivity::fixedLogLevel(0.1), 1e-5));

	// Within one period: no new observation, same snapshot object.
	const auto* before = f.publish(1.0).get();
	f.update(1.05, true, "a.mp4", 0.25);
	CHECK(f.publish(1.05).get() == before);
	// After a period: new observation.
	f.update(1.10, true, "a.mp4", 0.25);
	CHECK(near(f.publish(1.10)->raw, 0.2, 1e-5));

	// A genuine zero is a present value, not unavailable.
	f.update(1.30, true, "a.mp4", 0.05);
	CHECK(f.publish(1.30)->state == State::Valid && f.publish(1.30)->level == 0.0f);

	// Stale when feeding stops (hitch): evaluated against `now`.
	CHECK(f.publish(2.0)->state == State::Stale);

	// Transition static frame: Transition, never a retained value.
	f.update(2.1, false, "a.mp4", 0.15);
	CHECK(f.publish(2.1)->state == State::Transition);
	// First live frame after it is fed immediately.
	f.update(2.2, true, "a.mp4", 0.15);
	CHECK(f.publish(2.2)->state == State::Valid);

	// No clip ready / unknown clip / no position -> Unavailable, not zero.
	f.update(2.3, true, "", std::nullopt);
	CHECK(f.publish(2.3)->state == State::Unavailable);
	f.update(2.4, true, "never-measured.mp4", 0.5);
	CHECK(f.publish(2.4)->state == State::Unavailable);
	f.update(2.5, true, "a.mp4", std::nullopt);
	CHECK(f.publish(2.5)->state == State::Unavailable);
	// Recovers as soon as a measured clip is ready again.
	f.update(2.6, true, "a.mp4", 0.35);
	CHECK(f.publish(2.6)->state == State::Valid);

	// A new generation drops the prior observation.
	f.beginGeneration(2, "temporal");
	CHECK(f.publish(2.6)->state == State::Unavailable && f.publish(2.6)->generation == 2);
	f.update(2.7, true, "a.mp4", 0.35);
	CHECK(f.publish(2.7)->state == State::Valid && f.publish(2.7)->generation == 2 && f.publish(2.7)->sceneId == "temporal");

	std::printf(fails == 0 ? "clip_activity_tests: all passed\n" : "clip_activity_tests: %d FAILED\n", fails);
	return fails == 0 ? 0 : 1;
}
