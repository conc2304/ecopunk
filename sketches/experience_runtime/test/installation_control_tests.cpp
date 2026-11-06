// Installation Controls & ENERGY v0.1 — installation::Controller (OF-free).

#include "InstallationControl.h"

#include <cmath>
#include <iostream>
#include <string>
#include <thread>

using namespace installation;

namespace {

int g_total = 0;
int g_failures = 0;

void check(bool cond, const std::string& what) {
	g_total++;
	if (!cond) {
		g_failures++;
		std::cerr << "FAIL: " << what << "\n";
	}
}

Request req(Request::Kind k) { Request r; r.kind = k; return r; }

void test_defaults_and_load_validation() {
	Controller c;
	check(!c.settings().sceneAutoCycle.enabled && c.settings().mediaAutoCycle.enabled, "defaults: scene cycle off, media cycle on");
	check(c.settings().energyTarget == 0.5f && c.energySmoothed() == 0.5f, "default ENERGY 0.5");

	Settings bad;
	bad.sceneAutoCycle = {true, 2.0f};         // below minimum
	bad.mediaAutoCycle = {false, NAN};         // not finite
	bad.energyTarget = 1.5f;                   // out of range
	bad.launchFullscreen = true;
	auto rejected = c.load(bad);
	check(rejected.size() == 3, "load rejects 3 invalid fields");
	check(c.settings().sceneAutoCycle.enabled && c.settings().sceneAutoCycle.durationSeconds == 120.0f,
		"invalid scene duration keeps default, enabled flag kept");
	check(!c.settings().mediaAutoCycle.enabled && c.settings().mediaAutoCycle.durationSeconds == 30.0f, "invalid media duration keeps default");
	check(c.settings().energyTarget == 0.5f && c.settings().launchFullscreen, "invalid energy keeps default; launchFullscreen loaded");
}

void test_runtime_updates_validated() {
	Controller c;
	Request r = req(Request::Kind::SetSceneAutoCycle);
	r.flag = true; r.durationSeconds = 5.0f;
	check(!c.submit(r).accepted && !c.settings().sceneAutoCycle.enabled, "scene duration below 10 s rejected, nothing applied");
	r.durationSeconds = 45.0f;
	check(c.submit(r).accepted && c.settings().sceneAutoCycle.enabled && c.settings().sceneAutoCycle.durationSeconds == 45.0f,
		"valid scene cycle applied");
	check(c.takeSettingsChanged() && !c.takeSettingsChanged(), "settings-changed flag set once for persistence");
	Request m = req(Request::Kind::SetMediaAutoCycle);
	m.flag = false;
	check(c.submit(m).accepted && !c.settings().mediaAutoCycle.enabled && c.settings().sceneAutoCycle.enabled,
		"media automation independent of scene automation");
	Request e = req(Request::Kind::SetEnergyTarget);
	e.value = -0.1f;
	check(!c.submit(e).accepted, "ENERGY outside [0,1] rejected");
}

void test_scene_cycle_schedule() {
	Controller c;
	Request r = req(Request::Kind::SetSceneAutoCycle);
	r.flag = true; r.durationSeconds = 10.0f;
	c.submit(r);
	int fires = 0;
	for (int i = 0; i < 599; ++i) fires += c.tickSceneCycle(1.0f / 60.0f, true);
	check(fires == 0, "no switch before the period elapses");
	fires += c.tickSceneCycle(1.0f / 60.0f, true);
	check(fires == 1, "exactly one switch at 10 s");

	// Not idle (transition/loading): time does not accrue, nothing fires.
	for (int i = 0; i < 2000; ++i) fires += c.tickSceneCycle(1.0f / 60.0f, false);
	check(fires == 1 && c.sceneCycleElapsed() == 0.0f, "no accrual or switch during transitions");

	// Long frame: capped contribution, never a burst.
	fires = 0;
	fires += c.tickSceneCycle(30.0f, true);
	check(fires == 0 && c.sceneCycleElapsed() == kMaxScheduleStepSeconds, "a 30 s stall counts as 1 s, no immediate switch");
	for (int i = 0; i < 9; ++i) fires += c.tickSceneCycle(1.0f, true);
	check(fires == 1 && c.sceneCycleElapsed() == 0.0f, "period completes once, restarts at zero (no catch-up remainder)");

	// Manual navigation restarts the period.
	for (int i = 0; i < 8; ++i) c.tickSceneCycle(1.0f, true);
	c.noteSceneChanged();
	fires = 0;
	for (int i = 0; i < 9; ++i) fires += c.tickSceneCycle(1.0f, true);
	check(fires == 0, "manual scene change resets the cadence");

	// Disabled: never fires.
	r.flag = false; r.durationSeconds.reset();
	c.submit(r);
	fires = 0;
	for (int i = 0; i < 100; ++i) fires += c.tickSceneCycle(1.0f, true);
	check(fires == 0, "disabled scene automation never switches");
}

void test_energy_smoothing_and_encoder() {
	Controller c;
	check(!c.submit(req(Request::Kind::StepEnergy)).accepted, "encoder steps refused outside ENERGY adjustment");
	c.submit(req(Request::Kind::BeginEnergyAdjust));
	Request s = req(Request::Kind::StepEnergy);
	s.steps = 40; // far past the top
	c.submit(s);
	check(c.settings().energyTarget == 1.0f, "steps clamp at 1.0");
	s.steps = -3;
	c.submit(s);
	check(std::fabs(c.settings().energyTarget - 0.85f) < 1e-5f, "three detents down = -0.15");
	check(c.submit(req(Request::Kind::EndEnergyAdjust)).accepted && !c.energyAdjusting(), "exit adjustment");

	// Frame-rate independence: 1 s at 30 fps vs 1 s at 144 fps.
	Controller a, b;
	Request t = req(Request::Kind::SetEnergyTarget);
	t.value = 1.0f;
	a.submit(t); b.submit(t);
	for (int i = 0; i < 30; ++i) a.tickEnergy(1.0f / 30.0f);
	for (int i = 0; i < 144; ++i) b.tickEnergy(1.0f / 144.0f);
	check(std::fabs(a.energySmoothed() - b.energySmoothed()) < 1e-4f, "smoothing is frame-rate independent");
	const float expected = 1.0f - 0.5f * std::exp(-1.0f / kEnergySmoothingTau);
	check(std::fabs(a.energySmoothed() - expected) < 1e-4f, "exponential approach with tau 0.5 s");
	for (int i = 0; i < 600; ++i) a.tickEnergy(1.0f / 60.0f);
	check(a.energySmoothed() <= 1.0f && a.energySmoothed() > 0.999f, "converges to target, never overshoots");
	a.tickEnergy(-5.0f);
	check(a.energySmoothed() <= 1.0f, "negative dt ignored");
}

void test_queue_from_other_thread() {
	Controller c;
	std::thread producer([&c] {
		Request r; r.kind = Request::Kind::SetEnergyTarget; r.value = 0.2f;
		c.enqueue(r);
		Request bad; bad.kind = Request::Kind::SetEnergyTarget; bad.value = 7.0f;
		c.enqueue(bad);
		Request f; f.kind = Request::Kind::ToggleFullscreen;
		c.enqueue(f);
	});
	producer.join();
	check(c.settings().energyTarget == 0.5f, "queued requests do not apply until the runtime thread drains");
	auto results = c.drainQueue();
	check(results.size() == 3 && results[0].accepted && !results[1].accepted && results[1].code == "invalid_energy" && results[2].accepted,
		"drained in order with per-request validation");
	check(c.settings().energyTarget == 0.2f, "valid queued ENERGY applied");
	check(c.takeFullscreenToggle() && !c.takeFullscreenToggle(), "fullscreen toggle is a one-shot request");
	check(!c.settings().launchFullscreen, "one-shot toggle never changes the persistent launch preference");
}

} // namespace

void test_automation_suspension() {
	Controller c;
	Settings s;
	s.sceneAutoCycle = {true, 10.0f};
	c.load(s);
	for (int i = 0; i < 8; ++i) c.tickSceneCycle(1.0f, true);
	check(c.sceneCycleElapsed() > 7.0f, "scene period accrues normally");
	c.setAutomationSuspended(true);
	bool fired = false;
	for (int i = 0; i < 60; ++i) fired = c.tickSceneCycle(1.0f, true) || fired;
	check(!fired && c.automationSuspended(), "suspended: a minute of ticks never fires the scene cycle");
	c.setAutomationSuspended(false);
	check(c.sceneCycleElapsed() == 0.0f, "resume starts a fresh period (no catch-up)");
	check(!c.tickSceneCycle(1.0f, true), "first tick after resume does not fire");
	Request r = req(Request::Kind::SetClock24Hour);
	r.flag = false;
	check(c.submit(r).accepted && !c.settings().clock24Hour && c.takeSettingsChanged(), "SetClock24Hour is a validated, persisted setting");
	Settings loaded;
	loaded.clock24Hour = false;
	c.load(loaded);
	check(!c.settings().clock24Hour, "clock24Hour survives load()");
}

// OC-005 Stage 2: committed Orbit observations in the installation settings.
static void test_orbit_observations() {
	Controller c;
	check(c.settings().orbitA == Settings::OrbitSelection{"fx", "avg"} && c.settings().orbitB == Settings::OrbitSelection{"raw", "avg"}, "defaults: A = fx/avg, B = raw/avg");
	{ // load: valid values survive; nothing rejected
		Settings s;
		s.orbitA = {"raw", "vib"};
		s.orbitB = {"fx", "dom"};
		check(c.load(s).empty() && c.settings().orbitA == Settings::OrbitSelection{"raw", "vib"} && c.settings().orbitB == Settings::OrbitSelection{"fx", "dom"}, "valid saved selection restores both observations");
	}
	{ // invalid saved fields fall back to the documented defaults, per field, and are reported
		Settings s;
		s.orbitA = {"banana", "vib"};
		s.orbitB = {"fx", "median"};
		const auto rejected = c.load(s);
		check(rejected.size() == 2 && rejected[0] == "orbitObservations.a.source" && rejected[1] == "orbitObservations.b.metric", "invalid orbit fields are reported by name");
		check(c.settings().orbitA == Settings::OrbitSelection{"fx", "vib"}, "A: bad source -> default fx, valid metric kept");
		check(c.settings().orbitB == Settings::OrbitSelection{"fx", "avg"}, "B: valid source kept, bad metric -> default avg");
		Settings empty; empty.orbitA = {"", ""}; empty.orbitB = {"", ""};
		check(c.load(empty).size() == 4 && c.settings().orbitA == Settings::OrbitSelection{"fx", "avg"} && c.settings().orbitB == Settings::OrbitSelection{"raw", "avg"}, "empty values fall back to the defaults");
	}
	c.load(Settings{});
	c.takeSettingsChanged();
	{ // committing one observation leaves the other alone and marks the settings changed (one write)
		Request r = req(Request::Kind::SetOrbitObservation);
		r.text = "raw.vib";
		check(c.submit(r).accepted && c.settings().orbitA == Settings::OrbitSelection{"raw", "vib"} && c.takeSettingsChanged(), "commit A");
		check(c.settings().orbitB == Settings::OrbitSelection{"raw", "avg"}, "B untouched by an A commit");
		r.flag = true;
		r.text = "fx.dom";
		check(c.submit(r).accepted && c.settings().orbitB == Settings::OrbitSelection{"fx", "dom"} && c.takeSettingsChanged(), "commit B");
		check(c.settings().orbitA == Settings::OrbitSelection{"raw", "vib"}, "A untouched by a B commit");
	}
	{ // re-committing the same value is accepted but writes nothing
		Request r = req(Request::Kind::SetOrbitObservation);
		r.text = "raw.vib";
		check(c.submit(r).accepted && !c.takeSettingsChanged(), "unchanged commit: no settings change, no disk write");
	}
	{ // invalid requests are rejected and change nothing
		for (const char* bad : {"", "fx", "fx.", ".avg", "xx.avg", "fx.median", "FX.AVG", "fx.avg.extra"}) {
			Request r = req(Request::Kind::SetOrbitObservation);
			r.text = bad;
			check(!c.submit(r).accepted && !c.takeSettingsChanged(), std::string("rejected: '") + bad + "'");
		}
		check(c.settings().orbitA == Settings::OrbitSelection{"raw", "vib"} && c.settings().orbitB == Settings::OrbitSelection{"fx", "dom"}, "rejected requests leave both observations as committed");
	}
	{ // unrelated settings are untouched by orbit commits
		Controller d;
		Settings s; s.clock24Hour = false; s.systemMode = "auto"; s.energyTarget = 0.75f;
		d.load(s);
		Request r = req(Request::Kind::SetOrbitObservation);
		r.text = "raw.dom";
		d.submit(r);
		check(!d.settings().clock24Hour && d.settings().energyTarget == 0.75f && d.settings().systemMode == "auto", "orbit commit leaves unrelated installation settings unchanged");
	}
}

int main() {
	test_orbit_observations();
	test_automation_suspension();
	test_defaults_and_load_validation();
	test_runtime_updates_validated();
	test_scene_cycle_schedule();
	test_energy_smoothing_and_encoder();
	test_queue_from_other_thread();
	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed.\n";
	return g_failures == 0 ? 0 : 1;
}
