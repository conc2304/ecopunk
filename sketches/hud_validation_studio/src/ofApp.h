#pragma once

#include "ofMain.h"

// Deliberately ONLY shared/src/hud-compositor/ headers (plus, as of HUD
// Runtime Engineering Session 2, HudFrameData.h itself — see below) — no
// ExperienceRuntime, no SceneManager, no IEcopunkScene. Per this task's
// frozen constraints, this app is not merged into the installation runtime
// and does not depend on any not-yet-built runtime type.
//
// UPDATED, Engineering Session 2: this app now ALSO exercises
// HudWireframeRenderer::update(float, const HudFrameData&) — "the
// Validation Studio must remain a second host for the same renderer path"
// (this session's own framing) means literally that: the same renderer
// instance, same draw(), fed occasionally by a hand-built, deterministic
// HudFrameData (see buildRealFrameCases() in ofApp.cpp) instead of only
// FakeHudFrameData. HudFrameData.h transitively pulls in SceneContract.h
// (for SceneFrame/SceneHudStatus/SceneManagerStatus/SceneCapabilities) and
// VideoPlaybackStatus.h — both already reachable via
// HudWireframeRenderer.h's own real-path include chain
// (HudRealFrameResolver.h), so this is not a new dependency this app
// didn't already build against; it is now also constructed directly here,
// deterministically (no wall-clock time, no live SceneManager/scene of any
// kind — every field is a literal, fixed value chosen in this file).
#include "HudFrameData.h"
#include "HudWireframeRenderer.h"
#include "fake/FakeHudScenarioBase.h"
#include "fake/FakeHudScenarioRegistry.h"
#include "widgets/BindingPlaceholderWidget.h"

#include <memory>
#include <string>
#include <vector>

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

private:
	void switchScene(const std::string& sceneId);
	void jumpToPhase(hudpresent::FakeCasePhase phase);
	void drawInspectorPanel() const;
	void drawRegionOutlines() const;
	void drawDebugBindingErrorDemo() const;
	std::string screenshotName() const;
	void captureOne(const std::string& suffix);
	void runBaselineCapture();

	// Engineering Session 2 additions — real-HudFrameData baseline
	// coverage (see ofApp.h's own header comment). buildSyntheticMediaTexture()
	// generates one small, deterministic (not loaded from disk — no asset
	// dependency, no non-determinism from a missing/changed file) checkerboard
	// pattern once in setup(), reused by every real-frame case that wants a
	// non-null, visibly-textured MediaViewportMesh inspection screenshot.
	void buildSyntheticMediaTexture();
	struct RealFrameCase;
	std::vector<RealFrameCase> buildRealFrameCases() const;

	// Architecture-Closure Session: profile-switch allocation-spike
	// measurement (closure plan §8.2) — Blob->Temporal->Quadrant->Blueprint,
	// repeated, using the same alloccounter global-new/delete-override
	// counter sketches/experience_runtime/src/AllocationCounter.{h,cpp}
	// already established (symlinked into src/ here, same pattern as
	// EffectActivityStatus.cpp) rather than a second instrumentation
	// mechanism. Runs once, after the screenshot batch, only when
	// HUD_STUDIO_AUTOCAPTURE is set (same gate as the batch capture
	// itself) — logs via ofLogNotice, no persistent state.
	void runProfileSwitchAllocationMeasurement();

	hudpresent::HudWireframeRenderer renderer_;
	hudpresent::FakeHudScenarioRegistry scenarioRegistry_;
	std::unique_ptr<hudpresent::IFakeHudScenario> scenario_;
	std::string currentSceneId_;

	float elapsedSeconds_ = 0.0f;
	static constexpr float kFixedDt = 1.0f / 60.0f;
	bool paused_ = false;

	bool showInspector_ = true;
	bool showRegionOutlines_ = false;
	bool debugMode_ = false; // studio/placeholder mode — see HudWireframeRenderer::setStudioMode()

	hudpresent::BindingPlaceholderWidget debugPlaceholderWidget_; // demo-only, see drawDebugBindingErrorDemo()

	// Batch baseline-capture scenario list — same fixed-dt-then-
	// ofSaveScreen pattern as sketches/hud_validation_harness/src/ofApp.cpp,
	// reused per docs/probes/hud-runtime-validation-studio-probe.md §10's
	// recommendation to copy that pattern directly rather than extend the
	// widget-level harness.
	struct BaselineCase {
		std::string sceneId;
		hudpresent::FakeCasePhase phase;
		float offsetWithinPhase;
		std::string name;
	};
	std::vector<BaselineCase> baselineCases_;
	size_t baselineIndex_ = 0;
	bool capturing_ = false;

	// Engineering Session 2: real-HudFrameData baseline cases (manager
	// transition, effects absent/present, media unavailable/loading/ready
	// x3 title-fallback tiers, typography overflow, textured
	// MediaViewportMesh inspection) — captured AFTER baselineCases_ and
	// the binding-error demo, in runBaselineCapture()'s same batch. Each
	// entry owns one complete, literal HudFrameData — no scene, no
	// scenario, no elapsed-time dependency, so re-running this capture
	// twice produces byte-identical PNGs (same determinism guarantee as
	// the fake-scenario baselines above).
	struct RealFrameCase {
		std::string name;
		HudFrameData frame;
		// Architecture-Closure Session: true for exactly one deliberate
		// tooling/fixture demo case (see buildRealFrameCases()'s
		// "effects_compatibility_fallback_demo") — see
		// HudRealFrameResolver::setActiveEffectsCompatibilityFallbackEnabled()'s
		// own comment for why this defaults to false everywhere else.
		bool compatibilityFallback = false;
	};
	std::vector<RealFrameCase> realFrameCases_;
	size_t realFrameIndex_ = 0;
	bool boundsOverlayCaptured_ = false; // see draw()'s "bounds_safe_margin_overlay" capture step
	ofTexture syntheticMediaTexture_; // see buildSyntheticMediaTexture()

	// Set when the HUD_STUDIO_AUTOCAPTURE environment variable is present
	// at startup — lets this same interactive app be driven
	// non-interactively (batch-capture every baseline, then exit) for
	// automated verification, without adding a second binary or an argv-
	// parsing dependency. Normal interactive runs never set this and the
	// app behaves exactly as an interactive tool (this task's §12).
	bool autoCaptureThenExit_ = false;
};
