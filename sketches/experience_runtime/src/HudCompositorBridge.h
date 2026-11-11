#pragma once

#include "HudFocusNavigator.h"
#include "HudFrameData.h"
#include "HudRegionInteraction.h"
#include "HudSkinLoader.h"
#include "HudSkinOfImageBackend.h"
#include "HudWireframeRenderer.h"

#include <string>

// Scene/HUD Contract v1 §12: HUD asset paths reach the HudCompositor from
// ExperienceRuntime through their own object, never through SceneServices.
// HUD-004 first loader needs exactly one path; nothing else is added here.
struct HudAssetPaths {
	std::string skinsRoot; // canonical: <repo>/assets/hud/skins
};

// HudCompositorBridge — Engineering Session 2's replacement for
// HudCompositorStub (see that class's own header comment, now marked
// superseded — it remains on disk only because this environment's `rm` is
// consistently denied at the permission layer, not because it is still
// used; see this session's completion report, "stray files needing manual
// cleanup").
//
// This is a thin bridge, NOT a second HUD renderer: it owns exactly one
// hudpresent::HudWireframeRenderer — the SAME renderer class
// hud_validation_studio hosts against fake data — and does nothing this
// class's own logic couldn't be inlined into ExperienceRuntime, except
// keep that class's header from needing to include the HUD Runtime
// domain's ~15-header include chain directly. No HUD drawing logic lives
// in this file; draw() is a one-line forward.
//
// Ownership/consumption rules this class exists to preserve (Scene-HUD-
// Contract-v1.md §4, and this repo's root .claude/CLAUDE.md architecture
// diagram): it receives one assembled HudFrameData by const& per frame
// from ExperienceRuntime and never polls SceneManager/scenes/
// RuntimeServices/VideoPlaybackService/effect services itself — everything
// it draws comes from the HudFrameData snapshot handed to update().
//
// Canvas assumption: HudWireframeRenderer::draw() draws in a fixed
// 1280x720 canonical canvas coordinate space (HudRegionCatalog's
// normalized region bounds * canvasWidth_/canvasHeight_ — see that
// class's pixelBoundsFor()). ExperienceRuntime's window is already
// 1280x720 (sketches/experience_runtime/src/main.cpp) specifically so this
// bridge never needs a destRect/scale transform the way HudCompositorStub
// did for its own, unrelated (native-scene-size-based) rectangle — this
// bridge draws directly at (0,0)-(1280,720), matching the window 1:1.
class HudCompositorBridge {
public:
	void setup();

	// All per-frame resolution work — see
	// HudWireframeRenderer::update(float, const HudFrameData&)'s own
	// comment for exactly what this does (scene-epoch/frame-freshness/
	// generation handling, all from already-approved fields only).
	void update(float dt, const HudFrameData& frame);

	// Draws the last update()'d state at the canonical 1280x720 canvas
	// origin. No-op (but still leaves prior state intact) when !visible_,
	// matching HudCompositorStub's own toggle behavior.
	void draw() const;

	// Wired to the existing development-only RuntimeCommand::ToggleHud
	// binding (see InputRouter) — same role HudCompositorStub::setVisible()
	// played, preserved so ExperienceRuntime.cpp's existing call site needs
	// no behavioral change beyond the member's type.
	void setVisible(bool visible) { visible_ = visible; }
	bool isVisible() const { return visible_; }

	// Read-only inspection surface — tests/telemetry only, never used to
	// feed decisions back into update()/draw() (draw() is const and reads
	// only what update() already resolved, per HudWireframeRenderer's own
	// design).
	const hudpresent::HudWireframeRenderer& renderer() const { return renderer_; }
	// DIAGNOSTIC ONLY (OrbitInjectHarness): inject a deterministic series in place
	// of the (not yet published) scene-colour source. nullptr restores the normal path.
	void setOrbitSeriesOverrideForDiagnostics(const hudpresent::OrbitSeriesView* v) { renderer_.setOrbitSeriesOverride(v); }

	// Architecture-Closure Session: ground-truth counter proving "exactly
	// one production HUD draw per runtime frame" — incremented in draw(),
	// even when !visible_ (a skipped draw is still one decision made per
	// frame, not a second draw), so GlRestorationHarness can assert
	// drawCallCount == frameCount after N frames rather than trusting
	// single-call-site code inspection alone.
	uint64_t drawCallCount() const { return drawCallCount_; }

	// -- HUD-004 skin packages (first shell-only loader) -----------------------
	// Configure once at setup: where packages live and the CALLER-supplied
	// expected Blueprint pair (the validator has no default).
	void configureSkins(const HudAssetPaths& paths, const hudskin::SkinManifestValidationPolicy& expectedBlueprint);

	// Validate → decode → stage → atomically activate one package. On any
	// failure the current presentation (active skin, or wireframe) is kept
	// and the candidate is released. Logs a one-line diagnostic.
	hudskin::SkinLoadOutcome requestSkin(const std::string& selection);

	// Releases all skin GPU resources; the HUD returns to wireframe. Call
	// while the GL context is still alive (ExperienceRuntime::exit()).
	void shutdownSkins();

	// -- Playable HUD Menu v0.1: focus over the existing controls -------------
	// Focus is HUD presentation state only; it never dispatches anything.
	// Moves focus (first call engages it). Returns false if no control is visible.
	bool stepFocus(int delta);
	// The user activated the current focus: it becomes the intended control
	// (cancels restoration of an earlier one — see HudFocusNavigator).
	void confirmFocus() { focus_.confirmFocus(); }
	// The focused control's region ID if it is enabled right now, else nullopt.
	std::optional<std::string> focusActivationTarget() const { return focus_.activationTarget(); }
	const HudFocusNavigator& focus() const { return focus_; }

	// -- OC-005 Orbit observations: HUD-owned configuration + editor ----------------
	// The observations are two extra focus stops after the HUD-006 row ("orbit.a" =
	// region #7, "orbit.b" = region #8). Activating a focused one opens its editor; while
	// the editor is open the SAME device-neutral focus input (step / activate) and the
	// existing back event drive it. Pure presentation state: nothing is dispatched to
	// scenes, media or the runtime.
	static bool isOrbitFocusId(const std::string& id) {
		return id == hudpresent::HudWireframeRenderer::kOrbitFocusA || id == hudpresent::HudWireframeRenderer::kOrbitFocusB;
	}
	hudpresent::OrbitConfigEditor& orbitUi() { return renderer_.orbitUi(); }
	const hudpresent::OrbitConfigEditor& orbitUi() const { return renderer_.orbitUi(); }
	bool orbitEditorOpen() const { return renderer_.orbitUi().isOpen(); }
	// Opens the editor of the focused observation. False if focus is not on an observation.
	bool openOrbitEditorForFocus();
	// Editor input. Each returns true when the editor consumed it (always, while open).
	bool orbitEditorStep(int delta);
	bool orbitEditorActivate(); // select/preview an option, or commit on DONE; true if it just closed
	bool orbitEditorBack();     // cancel and restore
	// DIAGNOSTIC ONLY (OC-005 harness): deterministic RAW series; nullptr restores the normal path.
	void setOrbitRawSeriesOverrideForDiagnostics(const hudpresent::OrbitSeriesView* v) { renderer_.setOrbitRawSeriesOverride(v); }
	// -- Generic interactive region (scene opt-in via its presentation profile) ---------------------
	// ONE focus stop (kInteractiveFocusId) for the whole region. Presentation state only: the
	// bridge/renderer never change a scene value. A value step is reported to ExperienceRuntime,
	// which forwards it through its region-option port. Unavailable (no option port, region not
	// rendering, HUD hidden, scene changed) => hidden from focus traversal and never active.
	static bool isInteractiveFocusId(const std::string& id) { return id == hudpresent::HudWireframeRenderer::kInteractiveFocusId; }
	// Number of option axes the active scene advertises AND the runtime can dispatch (0 = none/no port).
	void setInteractiveOptionAxes(int n) { interactiveAxisCount_ = n; }
	bool interactiveRegionAvailable() const {
		const auto* d = renderer_.interactiveRegion();
		return d != nullptr && interactiveAxisCount_ >= d->rowCount && visible_ && renderer_.interactiveRegionPresent();
	}
	bool interactiveActive() const { return region_.active(); }
	int interactiveRow() const { return region_.row(); }
	const HudRegionInteraction& interactiveState() const { return region_; }
	bool enterInteractiveForFocus(); // false unless focus is on the available region
	bool exitInteractive() { return region_.exit(); }
	bool interactiveSelectRow(int delta) { return region_.selectRow(delta); }
	bool interactiveAdvanceRowOrExit() { return region_.advanceRowOrExit(); }
	hudpresent::HudInteractionPhase interactionPhase() const { return renderer_.interactionPhase(); }
	const hudpresent::HudWireframeRenderer& rendererForTesting() const { return renderer_; }
	// Shows the activation flash for the result ExperienceRuntime got back.
	void reportActivation(bool accepted) { renderer_.triggerActivationFeedback(accepted); }
	// Day/night text palette (skin schedule); default = night.
	void setColors(const hudpresent::HudWidgetColors& colors) { renderer_.setColors(colors); }
	void setDayPeriod(bool daytime) { renderer_.setDayPeriod(daytime); }
	void setButtonLabelMode(int mode) { renderer_.setButtonLabelMode(mode); }
	void setViewportStyle(const ofColor& backing, const ofColor& border, bool borderOn) { renderer_.setViewportStyle(backing, border, borderOn); }
	// Minimal look: drop the active skin (shell, button faces) — the plain wireframe HUD remains.
	void clearSkin() { shutdownSkins(); }
	// Development-only Live Typography Playground (null = production default).
	void setLabelTypography(const hudpresent::IHudLabelTypography* typography) { renderer_.setLabelTypography(typography); }

	// Inspection / harness surface.
	const hudskin::HudSkinLoader& skinLoader() const { return skinLoader_; }
	const HudSkinOfImageBackend& skinBackend() const { return skinBackend_; }
	HudSkinOfImageBackend& skinBackendForTesting() { return skinBackend_; }
	void setSkinsRootForTesting(const std::string& root);

private:
	void syncShellTexture();
	void refreshFocus();
	void refreshInteraction();

	// Traversal order = the HUD-006 controls row, left to right, then the
	// mode panel (flexible.secondary_a: Enter -> NEXT MODE). Hidden stops
	// (no "enabled" source this frame) are skipped like any other control.
	// HUD-006 v2: the six-button row, left to right (the mode panel is
	// informational and no longer a focus stop).
	HudFocusNavigator focus_{{"controls.scene_previous", "controls.scene_next", "controls.mode",
		"controls.media_previous", "controls.media_next", "controls.fullscreen", "controls.menu",
		hudpresent::HudWireframeRenderer::kInteractiveFocusId, // generic interactive region: ONE stop
		hudpresent::HudWireframeRenderer::kOrbitFocusA, hudpresent::HudWireframeRenderer::kOrbitFocusB}};

	hudpresent::HudWireframeRenderer renderer_;
	HudRegionInteraction region_;
	int interactiveAxisCount_ = 0;
	bool visible_ = true;
	mutable uint64_t drawCallCount_ = 0;

	HudSkinOfImageBackend skinBackend_;
	hudskin::HudSkinLoader skinLoader_{skinBackend_};
};
