#pragma once

#include "EffectActivityStatus.h"
#include "ShaderLibrary.h"
#include "TFAmbientTextureLayer.h"
#include "TFBackgroundLayer.h"
#include "TFComposition.h"
#include "TFPatternBSP.h"
#include "TFPatternBlobGrid.h"
#include "TFPatternBands.h"
#include "TFPatternColumnGrid.h"
#include "TFPatternTelescopingFrames.h"
#include "TFPatternParticleField.h"
#include "TFPatternEcologicalSuccession.h"
#include "TFPatternNetworkGrowth.h"
#include "TFPatternTemporalTides.h"
#include "TFPatternType.h"
#include "TimeOffsetVideoBuffer.h"

#include "ofRectangle.h"

// TemporalSceneCore — Temporal Production Scene #2 Migration: the reusable,
// host-agnostic orchestration a production IEcopunkScene adapter drives
// (see TemporalProductionScene). Owns exactly the real pipeline the
// standalone temporal-fields ofApp owns for its own artwork:
//
//     ShaderLibrary -> TFComposition -> (9x TFPattern*) -> TFFragmentTransition
//                    -> TFBackgroundLayer (-> TFEffectPicker -> TFImageCycler)
//                    -> TFAmbientTextureLayer
//
// and nothing else.
//
// Deliberately does NOT own: a VideoPlaybackService or
// TimeOffsetPlaybackAdapter instance (the caller owns the ONE appropriate
// instance for its context and hands this class the resulting
// TimeOffsetVideoBuffer* — see setup() below; this class never selects or
// loads media itself, matching every existing TFPattern*/TFBackgroundLayer
// call site's existing contract), window/global state (frame rate,
// fullscreen, vsync, ofExit, cursor, ofDisableArbTex — none of that is
// touched here), ofxGui/ofParameter state, TFParameterPanel, TFHudLayer, the
// HUD glitch overlay, MotionExtraction, or any debug/overlay drawing.
//
// Scope decision (documented, not silent — see this session's completion
// report "Deviations from prompt"): the standalone temporal-fields ofApp is
// NOT refactored in this session to construct one of these and drive it
// through the same instance-wiring code the way blob-region-prototype's
// ofApp was refactored onto BlobSceneCore. This class drives the exact same
// real TFComposition/TFPattern*/TFBackgroundLayer/TFAmbientTextureLayer
// production classes the standalone sketch does — not a copy, fork, or
// simplified "runtime version" of any of them — it is simply a second real
// instance of the same real pipeline, the same relationship BlobSceneCore
// itself had to ofApp's original inline ownership before that refactor.
// Unifying the instance-wiring itself was judged a materially larger,
// higher-regression-risk change (temporal-fields' ofApp is entangled with a
// much larger live-tunable ofxGui surface — TFParameterPanel pushes 9
// patterns' + background's + composition-transition params every frame —
// and three startup self-tests) than this session's "smallest complete
// migration" scope warrants; flagged as a newly discovered risk/future-work
// item rather than attempted here.
//
// Production defaults: every pattern/background/ambient Params value used
// by setup() below is either that struct's own in-class default member
// initializer, or (for TFBackgroundLayer::Params::effectWeights, whose
// in-class default is an EMPTY map — see TFEffectPicker::Weights) the exact
// same named-effect list and TFSettings.h constants
// (BACKGROUND_EFFECT_CYCLE_INTERVAL/BACKGROUND_EFFECT_RAW_WEIGHT/
// BACKGROUND_EFFECT_DEFAULT_WEIGHT) TFParameterPanel::getBackgroundParams()
// seeds those same ofParameter<float>s with at startup — see
// defaultBackgroundParams()'s own comment. No value here is invented; every
// one is either a real struct default already used in production, or a
// real named constant copied unchanged from TFSettings.h.
class TemporalSceneCore {
public:
	// buffer: the caller's TimeOffsetVideoBuffer (reached through its own
	// TimeOffsetPlaybackAdapter — see shared/src/video-playback/adapters/
	// TimeOffsetPlaybackAdapter.h) — not owned, must outlive this object.
	// Never constructed here (see class comment).
	void setup(TimeOffsetVideoBuffer* buffer, int canvasW, int canvasH);

	// Resumes active behavior. Restarts the composition cycle from its
	// first registered pattern with a fresh seed (TFComposition::
	// startCycle()) — a reactivated Temporal scene must never carry over
	// mid-transition/mid-cycle state from whatever was on screen the last
	// time it was active, matching the same reactivation discipline
	// BlobSceneCore::activate() applies to its own transient state.
	void activate();

	// Stops active behavior. Reversible — releases no resources; update()/
	// draw() become no-ops until activate() is called again.
	void deactivate();

	// Clears transient composition/pattern cycle state (restarts the cycle
	// from its first registered pattern, fresh seed) without touching
	// active_ — safe to call whether or not this object is currently
	// active. Mirrors BlobSceneCore::reset()'s "clears transient state
	// without changing configuration or activation" contract.
	void reset();

	// Same effect as reset() today — no additional GL/FBO/decoder
	// resources are separately allocated by this class beyond what
	// ShaderLibrary/TFComposition/TFPattern*/TFBackgroundLayer/
	// TFAmbientTextureLayer already own and reuse across calls. Leaves the
	// object valid for destruction.
	void shutdown();

	void resizeCanvas(int canvasW, int canvasH);

	// No-op when inactive. Advances composition/pattern/background/ambient
	// state only — does not touch the caller's TimeOffsetVideoBuffer/
	// TimeOffsetPlaybackAdapter/VideoPlaybackService (the caller drives
	// those, in the exact order documented in TemporalProductionScene::
	// update()'s own comment, before calling this).
	void update(float dt);

	// No-op when inactive. Draws the real artwork only — background,
	// composition (active pattern + any in-flight transition), ambient
	// underlay/overlay textures — into whatever is currently bound. No
	// GUI, no debug overlay, no local HUD (TFHudLayer/hud_overlay are not
	// constructed by this class at all).
	void draw();

	bool isActive() const { return active_; }
	bool didSetup() const { return didSetup_; }

	// -- Real, already-computed status/semantic sources --
	TFComposition::CyclePhase compositionPhase() const { return composition_.getPhase(); }
	float compositionPhaseElapsed() const { return composition_.getPhaseElapsed(); }
	TFPatternType activePatternType() const { return composition_.getActivePatternType(); }
	bool hasMedia() const { return buffer_ != nullptr && buffer_->hasMedia(); }
	int historyFrameCount() const { return buffer_ != nullptr ? buffer_->getHistoryFrameCount() : 0; }
	int historyCapacityFrames() const { return buffer_ != nullptr ? buffer_->getHistoryCapacityFrames() : 0; }
	int numPlayheads() const { return buffer_ != nullptr ? buffer_->getNumPlayheads() : 0; }

	// The real, currently-owned effect owner's canonical activity — see
	// TFBackgroundLayer::effectActivityStatus() -> TFEffectPicker::
	// activityStatus() (DEC-015). Forwarded unchanged; no field is
	// reconstructed here.
	videoeffects::EffectActivityStatus effectActivityStatus() const { return backgroundLayer_.effectActivityStatus(); }

	// -- Debug/dev-only access, for future standalone-sharing work; the
	// production scene adapter must never call these. --
	TFComposition& composition() { return composition_; }
	TFBackgroundLayer& backgroundLayer() { return backgroundLayer_; }
	TFAmbientTextureLayer& ambientTextures() { return ambientTextures_; }

private:
	// See class header comment on why this seeds the real named-effect
	// list + TFSettings.h constants rather than leaving effectWeights at
	// its empty in-class default (which would silently disable the 16
	// shader effects' entire weighted-cycling behavior in production).
	static TFBackgroundLayer::Params defaultBackgroundParams();

	TimeOffsetVideoBuffer* buffer_ = nullptr; // not owned, see setup()

	ShaderLibrary shaderLib_;
	TFAmbientTextureLayer ambientTextures_;
	TFBackgroundLayer backgroundLayer_;

	TFPatternBSP bspPattern_;
	TFPatternBlobGrid blobGridPattern_;
	TFPatternBands bandsPattern_;
	TFPatternColumnGrid columnGridPattern_;
	TFPatternTelescopingFrames telescopingFramesPattern_;
	TFPatternParticleField particleFieldPattern_;
	TFPatternEcologicalSuccession ecologicalSuccessionPattern_;
	TFPatternNetworkGrowth networkGrowthPattern_;
	TFPatternTemporalTides temporalTidesPattern_;

	TFComposition composition_;

	int canvasW_ = 0;
	int canvasH_ = 0;

	bool didSetup_ = false;
	bool active_ = false;
};
