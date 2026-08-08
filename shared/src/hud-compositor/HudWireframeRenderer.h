#pragma once

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
#include "HudRealFrameResolver.h" // production path — real HudFrameData (see below)
#include "HudRegionCatalog.h"
#include "HudSourceResolver.h"
#include "HudVocabularyResolver.h"
#include "HudWidgetInput.h"
#include "MediaViewportMesh.h"
#include "widgets/HudWidgetBase.h"
#include "widgets/HudWidgetRegistry.h"

#include "ofRectangle.h"

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

	// Validation Studio inspection surface (this task's §12).
	const HudCompiledProfile& compiledProfile() const { return compiled_; }
	const HudRegionCatalog& regionCatalog() const { return regions_; }
	const HudVocabularyResolver& vocabulary() const { return vocabulary_; }
	ofRectangle pixelBoundsFor(const std::string& regionId) const;

	static bool regionInMinimalSlice(const std::string& regionId);

private:
	void resolveBinding(const HudCompiledBinding& compiled, const FakeHudFrameData& frame);
	void resolveBindingReal(const HudCompiledBinding& compiled, const HudFrameData& frame);
	std::string primaryRoleFor(HudWidgetType type) const;

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
	glm::ivec2 currentSceneNativeSize_{0, 0};
	uint64_t lastObservedFrameNumber_ = 0;
	bool haveObservedFrameNumber_ = false;
	bool lastFrameWasFresh_ = false;

	std::unordered_map<std::string, HudResolvedValue> lastValid_; // key: bindingId + "\x1f" + role
	std::unordered_map<std::string, HudWidgetInput> widgetInputs_; // key: bindingId

	mutable FrameStats stats_;
};

} // namespace hudpresent
