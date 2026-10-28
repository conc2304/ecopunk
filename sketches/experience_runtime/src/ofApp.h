#pragma once

#include "ofMain.h"
#include "BlobLifecycleHarness.h"
#include "ExperienceRuntime.h"
#include "GlRestorationHarness.h"
#include "Rt004StallDriver.h"
#include "Rt004FramePhaseProbe.h"
#include "SceneSwitchHarness.h"
#include "SkinLoaderHarness.h"
#include "HudArtReferenceExporter.h"
#include "FocusMenuHarness.h"
#include "BlueprintRosterHarness.h"
#include "BlueprintHudHarness.h"
#include "ModeNavigationHarness.h"
#include "InstallationHarness.h"
#include "ExperienceV03Harness.h"
#include "PresentationHarness.h"
#include "ReactiveControlHarness.h"
#include "TypographyHarness.h"
#include "Hud006v2Harness.h"
#include "SettingsPageHarness.h"
#include "SkinCycleHarness.h"
#include "ParitySoakHarness.h"
#include "ResumeWindowHarness.h"
#include "CadenceHarness.h"
#include "TemporalWarmHarness.h"
#include "ThreeSceneCircuitHarness.h"
#include "TemporalLifecycleHarness.h"
#include "TwoSceneAcceptanceHarness.h"

#include <memory>

// Thin ofApp — forwards the openFrameworks callback surface into
// ExperienceRuntime and nothing else. Per the implementation prompt: "ofApp
// may own the openFrameworks callback surface, but it must not become the
// long-term location for scene management, FBO ownership, command routing,
// or telemetry logic."
//
// The exception is wiring up the opt-in harnesses (see those classes) when
// their respective environment variables are set — that's test-harness
// plumbing, not app/runtime logic, kept here because each needs to
// intercept the normal update/draw callback sequence. All are mutually
// exclusive with each other and with the normal run (see setup()): the GL
// harness always exercises FakeScene, the Blob lifecycle/soak harnesses
// always exercise the installed BlobProductionScene, the Temporal lifecycle
// harness always exercises the installed TemporalProductionScene, the RT-003
// scene-switch harness drives real Blob <-> Temporal switching, and a normal
// run registers both production scenes (RT-003) starting in Blob (the
// default) OR Temporal (EXPERIENCE_RUNTIME_TEMPORAL_SCENE=1), switchable at
// runtime with ']' (NextScene) / '[' (PreviousScene).
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;

	void keyPressed(int key) override;
	void mousePressed(int x, int y, int button) override;

private:
	ExperienceRuntime runtime;
	std::unique_ptr<GlRestorationHarness> harness;
	std::unique_ptr<BlobLifecycleHarness> blobHarness;
	std::unique_ptr<TemporalLifecycleHarness> temporalHarness;
	std::unique_ptr<SceneSwitchHarness> switchHarness;
	std::unique_ptr<TwoSceneAcceptanceHarness> acceptanceHarness; // RT-002
	std::unique_ptr<SkinLoaderHarness> skinHarness; // HUD-004 first shell loader proof
	std::unique_ptr<HudArtReferenceExporter> artExporter; // HUD Art reference PNG export (tooling)
	std::unique_ptr<FocusMenuHarness> focusHarness; // Playable HUD Menu v0.1 demonstration
	std::unique_ptr<BlueprintRosterHarness> blueprintHarness; // BLUE-001 roster/switch verification
	std::unique_ptr<BlueprintHudHarness> blueprintHudHarness; // HUD check with Blueprint as third scene
	std::unique_ptr<ParitySoakHarness> paritySoakHarness; // three-scene production-parity soak
	std::unique_ptr<ResumeWindowHarness> resumeWindowHarness; // Shared Video long-clip resume windows
	std::unique_ptr<CadenceHarness> cadenceHarness; // Blueprint cycle-driven media cadence
	std::unique_ptr<TemporalWarmHarness> temporalWarmHarness; // Temporal first-activation stall
	std::unique_ptr<ThreeSceneCircuitHarness> threeSceneHarness; // RT-005 three-scene circuits
	std::unique_ptr<ModeNavigationHarness> modeHarness; // Scene & Pattern Navigation, full roster
	std::unique_ptr<InstallationHarness> installationHarness; // Installation Controls & ENERGY, full roster
	std::unique_ptr<ExperienceV03Harness> v03Harness; // HUD & Scene Experience v0.3, full roster
	std::unique_ptr<PresentationHarness> presentationHarness; // RT-007 fullscreen presentation, full roster
	std::unique_ptr<ReactiveControlHarness> reactiveHarness; // Reactive Control Artwork v0.1, full roster
	std::unique_ptr<TypographyHarness> typographyHarness; // dev Live Typography Playground, full roster
	std::unique_ptr<Hud006v2Harness> hud006v2Harness; // HUD-006 v2 six-button navigation, full roster
	std::unique_ptr<SettingsPageHarness> settingsPageHarness; // Experience Settings page, keyboard-only
	std::unique_ptr<SkinCycleHarness> skinCycleHarness; // day/night skin schedule + 'v' switching
	std::unique_ptr<Rt004StallDriver> stallDriver; // RT-004 diagnostic, normal path only
	std::unique_ptr<Rt004FramePhaseProbe> framePhaseProbe; // RT-004 frame-phase diagnostic (EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE)
};
