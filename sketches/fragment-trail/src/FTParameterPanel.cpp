#include "FTParameterPanel.h"

void FTParameterPanel::setup() {
	spawnGroup.setName("Spawn");
	spawnGroup.add(spawnMinDistance.set("Min Distance (px)", 70.f, 10.f, 300.f));
	spawnGroup.add(spawnMinInterval.set("Min Interval (s)", 0.10f, 0.02f, 1.0f));
	spawnGroup.add(idlePulseEnabled.set("Idle Pulse", false));
	spawnGroup.add(idlePulseInterval.set("Idle Pulse Interval (s)", 1.2f, 0.3f, 10.f));

	fragmentGroup.setName("Fragments");
	// Total lifetime is Sustain + Decay: sustain holds full opacity, decay
	// then fades it to zero — split out of the old single "Lifespan" slider
	// (which had no sustain at all — every fragment started decaying the
	// instant it spawned, reported as fading out too soon).
	fragmentGroup.add(sustainSeconds.set("Sustain (s)", 1.6f, 0.f, 8.f));
	fragmentGroup.add(decaySeconds.set("Decay (s)", 1.6f, 0.1f, 8.f));
	// 18 per the brief's original default — explicitly unbenchmarked on Pi
	// 3B hardware (Section 8); this is a placeholder pending real testing.
	fragmentGroup.add(maxFragments.set("Max Fragments", 18, 1, 40));
	fragmentGroup.add(minSize.set("Min Size (px)", 90.f, 20.f, 300.f));
	fragmentGroup.add(maxSize.set("Max Size (px)", 220.f, 20.f, 400.f));

	responseGroup.setName("Response");
	responseGroup.add(speedResponse.set("Speed Response", 0.6f, 0.f, 1.f));
	responseGroup.add(modeDwellDuration.set("Mode Dwell (s)", 12.f, 2.f, 60.f));

	crosshairGroup.setName("Crosshair Motion");
	crosshairGroup.add(crosshairSpeed.set("Speed", 1.0f, 0.2f, 3.0f));
	crosshairGroup.add(crosshairSmoothness.set("Smoothness", 0.0f, 0.0f, 0.95f));
	crosshairGroup.add(crosshairSpeedVariance.set("Speed Variance", 0.4f, 0.0f, 0.95f));
	crosshairGroup.add(crosshairVarianceRate.set("Variance Rate", 0.03f, 0.005f, 0.2f));
	// Index into CrosshairSystem's presets: 0 DRIFT, 1 SCAN, 2 HUNT,
	// 3 NERVOUS, 4 ORBIT — default matches ofApp::setup()'s setPreset(2).
	crosshairGroup.add(crosshairPattern.set("Pattern", 2, 0, 4));
	crosshairGroup.add(crosshairAutoCycle.set("Auto Cycle Patterns", false));
	crosshairGroup.add(crosshairAutoCycleInterval.set("Auto Cycle Interval (s)", 15.f, 3.f, 60.f));

	rootGroup.setName("Fragment Trail");
	rootGroup.add(spawnGroup);
	rootGroup.add(fragmentGroup);
	rootGroup.add(responseGroup);
	rootGroup.add(crosshairGroup);

	panel.setup(rootGroup);
}

void FTParameterPanel::draw() {
	if (visible) panel.draw();
}
