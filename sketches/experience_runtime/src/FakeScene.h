#pragma once

#include "SceneContract.h"
#include "FakeSceneLifecycleState.h"

#include "EffectActivityStatus.h" // shared/src/video-effects/knowledge/ — the real,
                                   // shared-effects-domain-owned type; see FakeScene.cpp's
                                   // buildActiveEffectsLabels() and this increment's
                                   // implementation report ("Task 5/6") for why this feeds
                                   // SceneHudStatus::activeEffects directly rather than a
                                   // new HudFrameData field — EffectActivityStatus.h's own
                                   // header comment is explicit that it is "NOT a
                                   // shared-contract type" and "does not change
                                   // SceneHudStatus's shape."

#include "ofFbo.h"
#include "ofShader.h"
#include "ofTexture.h"

#include <cstdint>

// FakeScene — a complete IEcopunkScene implementation used only to prove
// the ExperienceRuntime scaffold. Deliberately lives in the runtime
// application, not under shared/src/scene/, so it is never mistaken for a
// template real scenes should copy (per the discovery report's
// recommendation, "Fake-Scene Design").
//
// Deliberately reports a native render size (kNativeRenderSize below) that
// differs from the runtime window's 1280x720, so tests catch any code that
// silently assumes a scene's render size equals the window/SceneFbo size —
// see the discovery report §7/§13 on the ofGetWidth()/ofGetHeight() risk
// found in fragment-trail/quadrant-crosshair.
//
// All lifecycle/counter bookkeeping is delegated to FakeSceneLifecycleState
// (see that header) rather than duplicated here — that's what makes the
// bare-compiler test suite under test/ possible without pulling in OF/GL.
class FakeScene : public IEcopunkScene {
public:
	// -- IEcopunkScene -------------------------------------------------
	void setup(const SceneServices& services) override;
	void activate() override;
	void deactivate() override;

	void update(float dt) override;
	void drawToCurrentTarget() override;

	glm::ivec2 nativeRenderSize() const override;

	std::string sceneId() const override;
	std::string displayName() const override;

	SceneHudStatus hudStatus() const override;
	SceneCapabilities capabilities() const override;
	bool executeCommand(SceneCommand command) override;

	void reset() override;
	void shutdown() override;

	// -- Test-support surface, NOT part of IEcopunkScene ----------------
	// Everything below exists only to make FakeScene's internal state
	// observable/controllable from tests and from experience_runtime's
	// development-only key bindings (InputRouter). Real scenes will not
	// have an equivalent public surface.

	using LifecycleCounters = FakeSceneLifecycleState::Counters;

	const LifecycleCounters& counters() const { return lifecycle_.counters; }

	bool isPermanentlyShutDown() const { return lifecycle_.shutDown; }
	bool isActive() const { return lifecycle_.active; }
	bool isSetUp() const { return lifecycle_.didSetup; }

	// Drives the SceneHealth reported by hudStatus() for Ready/Loading/
	// Degraded/Failed test coverage. FakeScene starts at Loading until
	// setup() completes, then Ready, matching the general shape scenes
	// are expected to follow.
	void setTestHealth(SceneHealth health) { testHealth_ = health; }
	SceneHealth testHealth() const { return testHealth_; }

	// Drives what hudStatus().semantic contains — covers every scenario
	// Development Stream 1's FakeScene requirements list: absence,
	// completeness, individually-missing signals (vs. present-but-zero),
	// multiple SceneMetric value types, zero metrics, and the approved
	// 16-metric boundary. Deterministic given a fixed resetEpoch/
	// activeSeconds — no randomness anywhere in this scene.
	enum class SemanticVariant {
		Absent,        // hudStatus().semantic == std::nullopt
		Complete,      // all 5 activity signals, full timing, 5 metrics
		               // (one of each HudMetricValueType)
		MissingSignals,// only SceneActivity::overall populated; the rest
		               // are std::nullopt (missing, not zero)
		ZeroSignals,   // all 5 activity signals populated AND == 0.0f
		               // (present-but-inactive, per the contract's own
		               // "zero means present but inactive" rule)
		ZeroMetrics,   // semantic present, metrics vector empty
		ManyMetrics,   // exactly kMaxMetrics (16) metrics
	};
	void setSemanticVariant(SemanticVariant variant) { semanticVariant_ = variant; }
	SemanticVariant semanticVariant() const { return semanticVariant_; }

	static constexpr size_t kMaxMetrics = 16;

	// Drives BOTH hudStatus().activeEffects (compatibility label strings,
	// via buildActiveEffectsLabels()) AND currentEffectActivityStatus()
	// (the canonical, Architecture-Closure-Session-authorized
	// HudFrameData.effects source) — two independent consumers reading
	// the same underlying demo slots, neither derived from the other's
	// OUTPUT (activeEffects is never built from an EffectActivityStatus
	// value, and vice versa — see this class's .cpp for both build
	// functions and this increment's report on why that distinction
	// matters).
	enum class EffectActivityTestState {
		NoSnapshot,     // currentEffectActivityStatus() == std::nullopt —
		                // "no authoritative Shared Effects snapshot available"
		PresentEmpty,   // a real EffectActivityStatus, health=Ready, zero
		                // slots — "snapshot exists, zero active effects"
		PresentActive,  // a real EffectActivityStatus, health=Ready, the
		                // same deterministic two-slot demo as before
	};
	void setEffectActivityTestState(EffectActivityTestState state) { effectActivityTestState_ = state; }
	EffectActivityTestState effectActivityTestState() const { return effectActivityTestState_; }

	// Convenience wrappers preserving the prior boolean dev-key/harness
	// call sites (Engineering Session 2) — true maps to PresentActive,
	// false to NoSnapshot (NOT PresentEmpty; PresentEmpty is only reachable
	// via setEffectActivityTestState() directly, since Session 2 had no
	// concept of it).
	void setEffectActivityDemoEnabled(bool enabled) {
		effectActivityTestState_ = enabled ? EffectActivityTestState::PresentActive
			: EffectActivityTestState::NoSnapshot;
	}
	bool effectActivityDemoEnabled() const {
		return effectActivityTestState_ == EffectActivityTestState::PresentActive;
	}

	// The canonical, Architecture-Closure-Session source —
	// SceneManager::captureEffectActivityStatus() polls this exactly once
	// per runtime frame, feeding ExperienceRuntime's HudFrameData.effects.
	// Health is derived via the REAL videoeffects::deriveEffectHealth()
	// against a real (honestly empty — this scene requests zero shader
	// effects) VideoEffectLoadReport, never synthesized directly — see
	// the .cpp for why a full VideoEffectService was NOT instantiated
	// here (this increment's report, "authoritative Shared Effects
	// owner/source").
	std::optional<videoeffects::EffectActivityStatus> currentEffectActivityStatus() const;

	// When enabled, drawToCurrentTarget() deliberately leaves scissor,
	// stencil, blend mode, shader, texture, matrix, style, and a nested
	// FBO bind dirty on exit — the payload for proving SceneRenderGuard
	// restores the approved baseline (Scene-HUD-Contract-v1.md §5)
	// regardless of what the scene left behind.
	void setContaminateGLStateOnDraw(bool enabled) { contaminateOnDraw_ = enabled; }
	bool contaminateGLStateOnDraw() const { return contaminateOnDraw_; }

	static constexpr const char* kSceneId = "fake-scene";
	static constexpr const char* kDisplayName = "Fake Scene";

private:
	void applyGLContamination() const;
	SceneSemanticData buildSemanticData() const;
	std::vector<std::string> buildActiveEffectsLabels() const;

	// mutable: hudStatus()/capabilities() are const per IEcopunkScene, but
	// still need to record their own statusPollCount/capabilityPollCount
	// ground-truth counters (see FakeSceneLifecycleState's header comment).
	// Every OTHER use of lifecycle_ happens from non-const methods.
	mutable FakeSceneLifecycleState lifecycle_;

	SceneHealth testHealth_ = SceneHealth::Loading;
	SemanticVariant semanticVariant_ = SemanticVariant::Absent;
	EffectActivityTestState effectActivityTestState_ = EffectActivityTestState::NoSnapshot;
	bool contaminateOnDraw_ = false;

	SceneServices services_{};
	float activeSeconds_ = 0.0f;

	// Allocated lazily in setup(), released in shutdown() — a real (if
	// tiny) owned GPU resource so allocCount/releaseCount mean something,
	// and reused as the deliberately-left-bound texture during GL
	// contamination.
	mutable ofTexture markerTexture_;

	// Built lazily in setup() from inline GLSL (no bin/data dependency).
	// Used only to contaminate "no shader bound" during deliberate GL
	// contamination — see applyGLContamination(). Left un-ended on
	// purpose in that path.
	mutable ofShader markerShader_;
	bool markerShaderLoaded_ = false;

	// Tiny FBO used only to contaminate the raw GL framebuffer binding
	// (via a direct glBindFramebuffer() call in applyGLContamination(),
	// NOT via ofFbo::begin(), which would also corrupt OF's own global
	// matrix/view stack in a way no SceneRenderGuard could generically
	// repair — see applyGLContamination()'s comment for why that
	// distinction matters).
	mutable ofFbo nestedContaminationFbo_;
};
