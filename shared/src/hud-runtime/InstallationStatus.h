#pragma once

// ============================================================================
// InstallationStatus.h — Installation Controls & ENERGY v0.1 snapshot.
//
// Runtime-owned installation settings and observed state, published once per
// frame in HudFrameData::installation so the HUD (and any future status
// endpoint) reads the same values the runtime acts on. OF-free.
// ============================================================================

struct InstallationStatus {
	bool sceneAutoCycleEnabled = false;
	float sceneAutoCycleSeconds = 0.0f;
	float sceneAutoCycleElapsedSeconds = 0.0f; // progress of the current period (0 while not idle/disabled)

	bool mediaAutoCycleEnabled = false;      // the setting
	bool mediaAutoAdvanceActive = false;     // observed: Shared Video timer on (off in boundary-driven scenes)
	float mediaAutoCycleSeconds = 0.0f;      // minimum dwell applied to Shared Video

	bool fullscreen = false;                 // observed window mode
	bool launchFullscreen = false;           // persistent startup preference

	float energyTarget = 0.5f;               // [0,1]
	float energy = 0.5f;                     // smoothed value scenes receive
	bool energyAdjusting = false;            // encoder steps adjust ENERGY, not focus
};
