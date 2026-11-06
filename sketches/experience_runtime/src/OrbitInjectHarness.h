#pragma once

#include "ExperienceRuntime.h"
#include "OrbitConsoleModel.h"

#include <functional>
#include <string>
#include <vector>

// OrbitInjectHarness — DEVELOPMENT/DIAGNOSTIC ONLY (EXPERIENCE_RUNTIME_ORBIT_INJECT_HARNESS=1).
// Renders the complete Orbit Console through the REAL HudCompositorBridge ->
// HudWireframeRenderer path in the real runtime (production fonts, real HUD
// region rectangles, the active skin) from a deterministic, injected
// OrbitSeriesView. It never touches the runtime scene-color sampler, never
// publishes anything and is not reachable in a production run. Captures:
//   valid   - hue sweep, changing luminance, 2-sample invalid gap, 3-slot pause
//   stale   - same series, state Stale
//   reset   - scene-generation reset: new generation, partial startup history
//   unavail - Unavailable (no series)
// to bin/data/captures/orbit-inject/ and writes results.txt there.
class OrbitInjectHarness {
public:
	static bool isRequested();
	explicit OrbitInjectHarness(ExperienceRuntime& runtime);
	void step(float dt);
	// OC-005 (EXPERIENCE_RUNTIME_ORBIT_INJECT_HARNESS=oc005): drives the configurable Orbit observations through the
	// REAL key path (arrows / Tab / Enter / Esc) in the real runtime and captures the HUD; writes oc005-results.txt.
	static bool isOc005();

private:
	void capture(const std::string& name);
	void buildSeries(int validCount, double t0, std::vector<hudpresent::OrbitSample>& out) const;
	ExperienceRuntime& runtime_;
	int frame_ = 0, stage_ = 0;
	std::vector<hudpresent::OrbitSample> series_;
	hudpresent::OrbitSeriesView view_;
	std::vector<std::string> lines_;
	bool done_ = false;

	struct Oc005Step {
		std::string name;
		int waitFrames;
		std::function<void()> act;
	};
	void stepOc005();
	void buildOc005();
	// OC-005 Stage 2 phases (EXPERIENCE_RUNTIME_OC005_PHASE): write | verify | invalid | real. Persistence goes through
	// EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS (a scratch file); "real" uses the runtime's own FX/RAW telemetry
	// (no injected series), every other phase injects deterministic fixtures for the visuals only.
	void buildOc005Phase(const std::string& phase);
	void installFixtures();
	std::string cfgText() const;
	std::string focusedId() const;
	std::string settingsFile() const;
	bool cfgIs(hudpresent::OrbitSource as, hudpresent::OrbitMetric am, hudpresent::OrbitSource bs, hudpresent::OrbitMetric bm) const;
	std::string fileBefore_;
	uint64_t genBefore_ = 0;
	std::string mediaBefore_;
	void check(const std::string& name, bool ok, const std::string& detail = "");
	std::vector<Oc005Step> oc005_;
	size_t oc005Index_ = 0;
	int oc005Failures_ = 0, oc005Passes_ = 0;
	std::vector<hudpresent::OrbitSample> rawSeries_;
	hudpresent::OrbitSeriesView rawView_;
};
