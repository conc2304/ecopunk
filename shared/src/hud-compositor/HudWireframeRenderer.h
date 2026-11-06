#pragma once

#include "HudLabelTypography.h"

// ============================================================================
// HudWireframeRenderer.h — the renderer-private orchestrator that ties
// every piece in this domain together into one minimal wireframe
// presentation (this task's §13).
//
// Deliberately NOT named "HudCompositor" — that name is reserved by the
// frozen Scene/HUD Contract v1 for the real, not-yet-built sibling system
// under ExperienceRuntime (Contract §4's sibling-system diagram). This
// class is a renderer-private stand-in scoped to this task: it consumes a
// FakeHudFrameData (not a real SceneFrame/HudFrameData), has no
// relationship to ExperienceRuntime/SceneManager, and is not a proposal
// for what the real HudCompositor's internals should look like beyond
// "this is one way to wire the pieces this task built together".
//
// Owns exactly the services this task's earlier sections defined —
// HudSourceResolver, HudProfileCompiler, HudVocabularyResolver,
// HudFormattingService (static), HudMissingDataController,
// HudHistoryStore, HudWidgetRegistry — and nothing else. It does not read
// scene implementation objects (it only ever sees FakeHudFrameData, an
// immutable value) and does not poll mutable services during draw (all
// resolution/formatting/policy work happens in update(), draw() only
// reads the results update() already computed and calls widget draw()).
// ============================================================================

#include "FakeHudSemanticTypes.h"
#include "HudCompiledProfile.h"
#include "HudHistoryStore.h"
#include "HudMissingDataController.h"
#include "HudPresentationProfile.h"
#include "HudProfileCompiler.h"
#include "HudVitalsDraw.h"
#include "HudVisualActivityBallistics.h"
#include "OrbitDraw.h"

#include <array>
#include "HudRealFrameResolver.h" // production path — real HudFrameData (see below)
#include "HudRegionCatalog.h"
#include "HudSourceResolver.h"
#include "HudVocabularyResolver.h"
#include "HudWidgetInput.h"
#include "MediaViewportMesh.h"
#include "widgets/HudWidgetBase.h"
#include "widgets/HudWidgetRegistry.h"

#include "ofRectangle.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace hudpresent {

class HudWireframeRenderer {
public:
	enum class RenderScope {
		// This task's §13 first vertical slice: scene title, media title,
		// primary state badge, overall activity, controls, one flexible
		// primary widget, health overlay, transition overlay.
		MinimalSlice,
		// Every compiled binding, including the deferred effect summary/
		// timing/traces/secondary-flexible content — exists so those
		// widget types are still exercisable/testable/screenshot-able in
		// this increment, per this task's own instruction to fully
		// implement all 11 widget types, not just the first-slice subset.
		FullProfile
	};

	HudWireframeRenderer();

	void setup(float canvasWidth, float canvasHeight);
	void setCanvasSize(float canvasWidth, float canvasHeight);

	// Recompiles the profile for `sceneId` and resets history to a new
	// scene epoch — the ONLY two things that happen on a scene switch.
	// Per this task's §15 ("Compile profiles only at setup or
	// scene-profile change"), NEVER call this from update()/draw().
	void setScene(const std::string& sceneId);
	const std::string& currentScene() const { return currentSceneId_; }

	void selectVocabularyPack(const std::string& packId) { vocabulary_.selectPack(packId); }
	const std::string& selectedVocabularyPack() const { return vocabulary_.selectedPack(); }

	// false (default) = production behavior: an invalid binding is
	// skipped, logged once. true = Validation Studio behavior: an invalid
	// binding renders as a visible BindingPlaceholder instead.
	void setStudioMode(bool enabled) { studioMode_ = enabled; }
	bool studioMode() const { return studioMode_; }

	void setRenderScope(RenderScope scope) { renderScope_ = scope; }
	RenderScope renderScope() const { return renderScope_; }

	// Architecture-Closure Session passthrough — see
	// HudRealFrameResolver::setActiveEffectsCompatibilityFallbackEnabled()'s
	// own comment. Off by default; only an explicitly-labeled tooling/
	// fixture case should ever set this true.
	void setActiveEffectsCompatibilityFallbackEnabled(bool enabled) {
		realResolver_.setActiveEffectsCompatibilityFallbackEnabled(enabled);
	}
	bool activeEffectsCompatibilityFallbackEnabled() const {
		return realResolver_.activeEffectsCompatibilityFallbackEnabled();
	}

	// All per-frame data work — resolves every compiled binding's sources
	// against `frame`, applies HudMissingDataController policy, feeds
	// HudHistoryStore, caches lastValid per binding/role. No drawing
	// happens here. Validation Studio / fake-scenario path.
	void update(float dt, const FakeHudFrameData& frame);

	// Engineering Session 2's production path: the SAME renderer, same
	// draw(), same compiled profile/widget pipeline, resolving against
	// the REAL shared/src/hud-runtime/HudFrameData instead — this is what
	// "the Validation Studio must remain a second host for the same
	// renderer path" (this task's own framing) means concretely: there is
	// exactly one draw() implementation below, shared by both update()
	// overloads.
	//
	// Also handles scene epoch / frame freshness, using ONLY existing
	// approved fields (this task's §3 — no new public field names
	// invented): a change in frame.sceneManager.activeSceneId is treated
	// as a scene activation (triggers setScene(), which recompiles the
	// profile and resets HudHistoryStore's epoch — "histories reset… on
	// scene activation"); frame.scene.semantic's own `timing.generation`
	// remains a same-scene reseed/reconfiguration event (drives
	// HudHistoryStore::markGenerationChange(), never a full epoch reset —
	// "generation remains a reseed event, not an activation substitute");
	// frame.sceneFrame.frameNumber is recorded and exposed via
	// lastObservedFrameNumber()/isFrameFresh() as this task's "freshness
	// distinguishes a new capture from retained display state" signal —
	// see that method's own comment for exactly what it does and does not
	// guarantee in this increment.
	void update(float dt, const HudFrameData& frame);

	// Draws every scope-filtered compiled binding at its region's pixel
	// bounds (derived from HudRegionCatalog's normalized placeholder
	// bounds + the current canvas size) via HudWidgetRegistry, plus the
	// media_viewport region's MediaViewportMesh (textured if the most
	// recent update(HudFrameData) call supplied an allocated texture;
	// placeholder-filled otherwise — see MediaViewportMesh::draw()).
	// Reads only what update() already computed.
	void draw() const;

	// True only immediately after an update(HudFrameData) call whose
	// frame.sceneFrame.frameNumber differed from the previously observed
	// one — i.e. "this was a genuinely new capture, not the same frame
	// handed to update() twice." Does NOT gate whether draw() actually
	// renders (draw() always renders the last resolved state, matching
	// the "HUD does not poll owners directly, consumes one immutable
	// HudFrameData per draw" requirement) — this is an INSPECTION signal
	// only, for the Validation Studio and tests, not a control-flow gate.
	bool isFrameFresh() const { return lastFrameWasFresh_; }
	uint64_t lastObservedFrameNumber() const { return lastObservedFrameNumber_; }

	const MediaViewportMesh& mediaViewportMesh() const { return mediaViewportMesh_; }

	// HUD-004 first shell-only loader seam. A non-owning pointer to the
	// active skin's full-canvas shell texture (1280x720, straight-alpha
	// RGBA8), or nullptr for the functional wireframe (the default, and the
	// only state the Validation Studio path ever uses). draw() composites it
	// directly after MediaViewportMesh and before every widget/overlay
	// binding — spec §4 order: mesh-clipped scene, shell, runtime widgets,
	// runtime overlays — using source-over for straight alpha. The owner
	// (HudCompositorBridge's skin loader) must clear or replace this pointer
	// before releasing the texture. No skin-ID logic lives here.
	void setShellTexture(const ofTexture* shell) { shellTexture_ = shell; }
	// Minimal (no-artwork) look: the colour behind the scene video inside the
	// media screen and an optional 1 px line around it (the viewport outline).
	void setViewportStyle(const ofColor& backing, const ofColor& border, bool borderOn) {
		viewportBacking_ = backing; viewportBorder_ = border; viewportBorderOn_ = borderOn;
	}
	const ofTexture* shellTexture() const { return shellTexture_; }

	// Reactive Control Artwork v0.1 (HUD-004 optional extension). A non-
	// owning pointer to the active skin's optional pressed face for ONE
	// control region (today only controls.scene_next — see
	// HudReactiveControlArt.h), or nullptr when the skin has none. Drawn
	// above the shell and below every widget/label and the focus brackets,
	// only while the EXISTING accepted activation feedback runs on that
	// focused control; plus a subtle edge illumination while it is focused.
	// Same ownership rule as the shell: the skin owner clears/replaces this
	// before releasing the texture. No skin-ID logic.
	void setPressedArtwork(const std::string& regionId, const ofTexture* texture) {
		pressedRegionId_ = regionId;
		pressedTexture_ = texture;
	}
	const ofTexture* pressedArtwork() const { return pressedTexture_; }

	// Shared navigation-button faces (HUD Art hud-state-buttons): idle /
	// focused / active textures for ALL controls, any of which may be null.
	// Non-owning, same ownership rule as the shell. Drawn above the shell and
	// below every label, focus bracket and the legacy pressed overlay:
	// focused face while a control holds focus, otherwise idle; the active
	// face fades over it during the existing accepted-activation interval.
	void setButtonFaces(const ofTexture* idle, const ofTexture* focused, const ofTexture* active) {
		buttonIdleTexture_ = idle;
		buttonFocusedTexture_ = focused;
		buttonActiveTexture_ = active;
	}
	bool hasButtonFaces() const { return buttonIdleTexture_ || buttonFocusedTexture_ || buttonActiveTexture_; }
	// Active-face opacity for a control this frame (0 = not drawn).
	float buttonActiveOpacity(const std::string& regionId) const;

	// Development-only Live Typography Playground (HudLabelTypography.h):
	// non-owning; nullptr (default, production) = native bitmap labels.
	void setLabelTypography(const IHudLabelTypography* typography) { labelTypography_ = typography; }
	// Day/night text palette for every widget, focus bracket and flash
	// (HudWidgetColors; default = the night palette). Set by the runtime's
	// skin schedule alongside the shell.
	void setColors(const HudWidgetColors& colors) { colors_ = colors; colors_.daytime = daytime_; colors_.buttonLabelMode = buttonLabelMode_; }
	// Day/night system state for the clock icon; independent of the colours/skin in use.
	void setDayPeriod(bool daytime) { daytime_ = daytime; colors_.daytime = daytime; }
	void setButtonLabelMode(int mode) { buttonLabelMode_ = mode; colors_.buttonLabelMode = mode; }
	const HudWidgetColors& colors() const { return colors_; }
	// The pressed artwork's opacity for the current state (0 = not drawn).
	float pressedArtworkOpacity() const;

	// Playable HUD Menu v0.1 — focus presentation for existing controls.
	// controlEnabled() reports a control region's state from the bindings the
	// last update() ALREADY resolved, using exactly LabelWidget's rule:
	// nullopt = hidden (unsupported), false = disabled (dimmed), true =
	// enabled — so focus never disagrees with what is drawn.
	std::optional<bool> controlEnabled(const std::string& regionId) const;
	// Region to outline as focused, or nullopt for none (the default — the
	// HUD is unchanged until something sets focus). Drawn after all widgets.
	void setFocusedControl(const std::optional<std::string>& regionId) {
		focusedControl_ = regionId;
		orbitDraw_.focusA = regionId && *regionId == kOrbitFocusA; // OC-005: observation focus is presentation state
		orbitDraw_.focusB = regionId && *regionId == kOrbitFocusB;
	}
	const std::optional<std::string>& focusedControl() const { return focusedControl_; }
	// One short activation flash on the focused control (accepted = accent,
	// refused = warning), fading over kActivationFlashSeconds.
	// The interval starts at the NEXT update() — the frame that first shows
	// it — not at the input event: an accepted NEXT/PREV SCENE switches
	// scenes synchronously and that frame can take longer than the whole
	// interval (0.59 s measured), which used to expire the feedback unseen.
	void triggerActivationFeedback(bool accepted) {
		activationPending_ = true;
		activationStart_ = -1.0f;
		activationAccepted_ = accepted;
	}
	static constexpr float kActivationFlashSeconds = 0.30f;
	// Seconds since the last activation feedback started (< 0 = never).
	float activationFeedbackAgeSeconds() const { return activationStart_ < 0.0f ? -1.0f : elapsedSeconds_ - activationStart_; }

	struct FrameStats {
		int activeWidgetCount = 0;
		int historySignalCount = 0;
		size_t historyBytes = 0;
		double totalDrawMicros = 0.0;
		std::vector<std::pair<std::string, double>> perWidgetMicros; // bindingId -> micros
	};
	// Populated by the most recent draw() call — draw() is const, so this
	// is the one piece of "mutable during a const call" state in this
	// class, used purely for read-only instrumentation, never for
	// resolution/formatting/policy decisions.
	const FrameStats& lastFrameStats() const { return stats_; }

	// System Telemetry v1 instrumentation/tests: geometry rebuild counters for
	// a vitals binding (null if the binding has no vitals cache).
	// Slots currently allocated in a region (hidden bindings in visibility-aware regions excluded).
	int regionSlotCount(const std::string& regionId) const;
	// Orbit Console v0.1: test/validation access to the cached orbit draw data.
	const OrbitDrawData& orbitDraw() const { return orbitDraw_; }
	// Validation Studio / tests only: replaces the series the orbit reads (non-owning;
	// nullptr = production). Production has NO other source until HudFrameData carries
	// the ratified scene-colour snapshot, so the orbit shows Unavailable.
	void setOrbitSeriesOverride(const OrbitSeriesView* view) { orbitOverride_ = view; }
	const OrbitConsoleModel& orbitModelForDiagnostics() const { return orbitDraw_.model; }
	// OC-005: HUD-owned Orbit observation configuration + editor (presentation state only).
	// Focus ids of the two observations (extra stops after the HUD-006 controls).
	static constexpr const char* kOrbitFocusA = "orbit.a";
	static constexpr const char* kOrbitFocusB = "orbit.b";
	OrbitConfigEditor& orbitUi() { return orbitDraw_.ui; }
	const OrbitConfigEditor& orbitUi() const { return orbitDraw_.ui; }
	// True once the orbit regions have a built layout (the observations are drawn and may take focus).
	bool orbitObservationsPresent() const { return orbitDraw_.upperW > 0.0f && orbitDraw_.upperH > 0.0f && orbitDraw_.lowerH > 0.0f; }
	// Validation Studio / tests only: replaces the RAW series (non-owning; nullptr = production path).
	void setOrbitRawSeriesOverride(const OrbitSeriesView* view) { orbitRawOverride_ = view; }
	// OC-004 test/inspection seam: the resolved widget input of a binding after the last update() (null = none).
	const HudWidgetInput* widgetInputFor(const std::string& bindingId) const {
		auto it = widgetInputs_.find(bindingId);
		return it == widgetInputs_.end() ? nullptr : &it->second;
	}
	const HudVitalsDrawData* vitalsDrawFor(const std::string& bindingId) const {
		auto it = vitalsDraw_.find(bindingId);
		return it == vitalsDraw_.end() ? nullptr : &it->second;
	}

	// Validation Studio inspection surface (this task's §12).
	const HudCompiledProfile& compiledProfile() const { return compiled_; }
	const HudRegionCatalog& regionCatalog() const { return regions_; }
	const HudVocabularyResolver& vocabulary() const { return vocabulary_; }
	ofRectangle pixelBoundsFor(const std::string& regionId) const;

	// Production (MinimalSlice-scope) membership: the original §13 slice plus
	// the Living HUD v0.1 activations. regionInOriginalMinimalSlice() is the
	// first part alone — tooling provenance only, never a draw-time gate.
	static bool regionInMinimalSlice(const std::string& regionId);
	static bool regionInOriginalMinimalSlice(const std::string& regionId);
	// Which production allowlist part a binding region belongs to, for
	// tooling (HudArtReferenceExporter): "original" (historical §13 slice),
	// "living_hud", "clock", "navigation", "system_telemetry"; nullptr =
	// FullProfile only.
	// media_viewport is not a binding region (always drawn) -> nullptr.
	static const char* productionProvenance(const std::string& regionId);

private:
	void resolveBinding(const HudCompiledBinding& compiled, const FakeHudFrameData& frame);
	void resolveBindingReal(const HudCompiledBinding& compiled, const HudFrameData& frame);
	std::string primaryRoleFor(HudWidgetType type) const;
	// Bindings that will attempt to draw per region this frame (scope/studio
	// filtered) — the slot count draw() slices multi-binding regions by, and
	// the same number update() sizes vitals traces with.
	std::unordered_map<std::string, int> computeRegionSlotCounts() const;
	bool bindingOccupiesSlot(const HudCompiledBinding& compiled) const;
	void resolveVitalsTrace(const HudCompiledBinding& compiled, const HudFrameData& frame, HudWidgetInput& input);
	void updateOrbit(const HudFrameData& frame); // once per update(), never per binding or in draw()

	HudRegionCatalog regions_;
	HudPresentationProfileRegistry profiles_;
	HudVocabularyResolver vocabulary_;
	HudProfileCompiler compiler_;
	HudSourceResolver resolver_;
	HudRealFrameResolver realResolver_;
	HudMissingDataController missingData_;
	HudHistoryStore history_;
	HudWidgetRegistry widgets_;
	// mutable: MediaViewportMesh::updateGeometry() is a cache-rebuild-if-
	// changed call (see that method's own "no unnecessary per-frame
	// rebuild" comment) invoked from draw() (const, by this class's own
	// design — see the class header comment on why drawing never mutates
	// resolved data), exactly the same rationale as `stats_` below.
	mutable MediaViewportMesh mediaViewportMesh_;

	std::string currentSceneId_;
	HudCompiledProfile compiled_;
	bool studioMode_ = false;
	RenderScope renderScope_ = RenderScope::MinimalSlice;

	float canvasWidth_ = 1280.0f;
	float canvasHeight_ = 720.0f;
	float elapsedSeconds_ = 0.0f;
	uint64_t lastGeneration_ = 0;
	bool haveLastGeneration_ = false;

	// Production-path-only state (populated by update(HudFrameData) —
	// stays at defaults for the fake/Studio path).
	const ofTexture* currentSceneTexture_ = nullptr; // non-owning, valid only for the frame that supplied it — see SceneFrame's own lifetime rule
	const ofTexture* shellTexture_ = nullptr; // non-owning — see setShellTexture()
	ofColor viewportBacking_ = ofColor(20, 26, 24, 255);
	ofColor viewportBorder_ = ofColor(255, 255, 255, 255);
	bool viewportBorderOn_ = false;
	const ofTexture* pressedTexture_ = nullptr; // non-owning — see setPressedArtwork()
	const ofTexture* buttonIdleTexture_ = nullptr;    // non-owning — see setButtonFaces()
	const ofTexture* buttonFocusedTexture_ = nullptr;
	const ofTexture* buttonActiveTexture_ = nullptr;
	const IHudLabelTypography* labelTypography_ = nullptr; // non-owning, dev-only
	HudWidgetColors colors_;
	bool daytime_ = true;
	int buttonLabelMode_ = 0;
	std::string pressedRegionId_;
	std::optional<std::string> focusedControl_;
	float activationStart_ = -1.0f;
	bool activationPending_ = false; // set by triggerActivationFeedback(), started by the next update()
	bool activationAccepted_ = false;
	glm::ivec2 currentSceneNativeSize_{0, 0};
	uint64_t lastObservedFrameNumber_ = 0;
	bool haveObservedFrameNumber_ = false;
	bool lastFrameWasFresh_ = false;
	bool frameLoading_ = false;                 // real HudFrameData health == Loading this frame
	mutable int loadingCaptures_ = 0;           // diagnostic only (EXPERIENCE_RUNTIME_HUD_LOADING_CAPTURE_DIR)

	std::unordered_map<std::string, HudResolvedValue> lastValid_; // key: bindingId + "\x1f" + role
	std::unordered_map<std::string, HudWidgetInput> widgetInputs_; // key: bindingId
	// System Telemetry v1: per-binding cached trace geometry (key: bindingId).
	// unordered_map nodes are address-stable, so widgetInputs_ can point at them.
	std::unordered_map<std::string, HudVitalsDrawData> vitalsDraw_;
	OrbitDrawData orbitDraw_;
	HudVisualActivityBallistics visualActivity_; // OC-004: HUD-owned smoothing of scene.visual.activity
	const OrbitSeriesView* orbitOverride_ = nullptr;
	const OrbitSeriesView* orbitRawOverride_ = nullptr;
	// Adapter buffer: the ratified scenecolor::SceneColorSnapshot mapped to the Orbit presentation sample
	// (display-encoded RGB for colour, linear relative luminance). Refilled only on a new snapshot revision.
	std::array<OrbitSample, kOrbitMaxSamples> orbitBuf_{};
	uint64_t orbitBufRevision_ = 0;
	uint32_t orbitBufCount_ = 0;
	// Same adapter for the RAW VIDEO series (HudFrameData::sceneColorRaw), refilled only on a new RAW revision.
	std::array<OrbitSample, kOrbitMaxSamples> orbitRawBuf_{};
	uint64_t orbitRawBufRevision_ = 0;
	uint32_t orbitRawBufCount_ = 0;
	std::unordered_map<std::string, int> slotCounts_; // computeRegionSlotCounts() as of the last update()

	mutable FrameStats stats_;
};

} // namespace hudpresent
