#pragma once

#include "BlobDetector.h"
#include "BlobTracker.h"
#include "ShaderLibrary.h"
#include "VideoPlaybackService.h"
#include "VideoRegionController.h"
#include "VideoRegionEffectRenderer.h"

#include <array>
#include <string>

// BlobSceneCore — the reusable, host-agnostic orchestration extracted from
// ofApp (Blob First Complete Production Migration). Owns exactly the same
// pipeline ofApp previously owned directly — ShaderLibrary -> BlobDetector ->
// BlobTracker -> VideoRegionController, plus a second, separate
// VideoRegionEffectRenderer for the full-frame background (see
// VideoRegionController.h's own header comment for why background/fragment
// effects intentionally stay two independent execution paths, not one
// coordinator, in this increment) — and nothing else.
//
// Deliberately does NOT own: a VideoPlaybackService instance (the caller —
// the standalone sketch's ofApp, or the production IEcopunkScene adapter —
// owns/references the ONE canonical instance appropriate to its context and
// hands it to setup() below; this class never constructs its own decoder),
// window/global state (frame rate, fullscreen, vsync, ofExit, cursor —
// none of that existed in ofApp's ownership either), ofxGui/ofParameter
// state, or any debug/overlay drawing. Both the standalone dev sketch and
// the production scene adapter construct one of these and drive it through
// the same setup/activate/deactivate/update/draw/reset/shutdown surface.
class BlobSceneCore {
public:
	// The 18 ShaderLibrary effect names selectable for the background and
	// per-fragment effect passes — moved here (unchanged) from ofApp.cpp's
	// former file-local kEffectChoices array so both the standalone dev
	// GUI (which still needs an index -> name mapping for its plain
	// ofParameter<int> sliders) and any future caller share one list
	// instead of a second hand-copied duplicate. See the original comment
	// this was moved from for why "ridgeline" and "contour" are excluded.
	static const std::array<std::string, 18> & effectChoices();

	// Background draw configuration — mirrors ofApp's former
	// pBackgroundMode/pBackgroundEffectIndex/pBackgroundEffectAmount
	// ofParameters, decoupled from ofxGui/ofParameter so a non-GUI caller
	// (the production scene adapter) can drive it directly.
	struct BackgroundConfig {
		int mode = 1; // 0 = off (canvas left as caller cleared it), 1 = normal (unshaded cover-fit), 2 = effect
		std::string effectName = "desaturate";
		float effectAmount = 1.0f;
	};

	// Per-update-call timing breakdown, preserved for the standalone
	// sketch's existing perf overlay/console stats — not consumed by the
	// production scene adapter.
	struct UpdateTimings {
		float detectorMs = 0.0f;
		float trackerMs = 0.0f;
		float regionUpdateMs = 0.0f;
	};

	// video: the caller's single canonical VideoPlaybackService instance —
	// not owned, must outlive this object. Never constructed here (see
	// class comment).
	void setup(VideoPlaybackService & video);

	// Resumes active behavior. Always performs the same transient-state
	// clear as reset() (see that method) — a reactivated Blob scene must
	// never carry over track identities/fragments from whatever was on
	// screen the last time it was active (the shared VideoPlaybackService
	// may well have auto-advanced to different media in the meantime).
	void activate();

	// Stops transient behavior. Reversible — does not release resources
	// or clear tracker/fragment state (the next activate() does that);
	// update()/draw() become no-ops until activate() is called again.
	void deactivate();

	// No-op when inactive or when the shared video source has no current
	// texture yet (mirrors ofApp's original "don't touch detection/
	// tracking/region code until real media is actually playing" gate).
	// destRect: the destination the background/fragments should be
	// composited into — the caller's own canvas/FBO size, NOT necessarily
	// ofGetWidth()/ofGetHeight() (the production adapter draws into a
	// runtime-owned FBO sized to nativeRenderSize(), not the app window).
	UpdateTimings update(float dt, const ofRectangle & destRect);

	// No-op when inactive or no current video texture. Draws background +
	// region fragments only — no debug overlay, no GUI, no "Loading
	// media..." placeholder text (callers decide whether/how to indicate
	// that state themselves; see class comment).
	void draw(const ofRectangle & destRect);

	// Clears all transient detector/tracker/fragment state without
	// touching configuration (BlobDetector::Config, BlobTracker::Config,
	// VideoRegionController::Params, backgroundConfig_ below all survive).
	// Safe to call whether or not setup()/activate() has run yet.
	void reset();

	// Permanently releases transient state (same effect as reset() today
	// — no additional GL/FBO resources are separately allocated by this
	// class beyond what BlobDetector/BlobTracker/VideoRegionController
	// already own and reuse across calls). Leaves the object valid for
	// destruction; a further setup()/activate() cycle is not expected
	// after shutdown() in production use, but is not unsafe.
	void shutdown();

	bool isActive() const { return active_; }
	bool didSetup() const { return didSetup_; }

	// -- Config access (mirrors ofApp's former syncParamsToSystems() targets) --
	BlobDetector::Config & detectionConfig() { return detector_.getConfig(); }
	const BlobDetector::Config & detectionConfig() const { return detector_.getConfig(); }
	BlobTracker::Config & trackingConfig() { return tracker_.getConfig(); }
	const BlobTracker::Config & trackingConfig() const { return tracker_.getConfig(); }
	VideoRegionController::Params & regionParams() { return regionController_.params; }
	const VideoRegionController::Params & regionParams() const { return regionController_.params; }
	BackgroundConfig & backgroundConfig() { return backgroundConfig_; }
	const BackgroundConfig & backgroundConfig() const { return backgroundConfig_; }

	// -- Cheap, real, already-computed status/semantic sources --
	// (see docs/reviews & Decision Log: no second CV pass is added here —
	// every value below reads state BlobTracker/VideoRegionController
	// already compute for their own lifecycle purposes.)
	bool hasVideoFrame() const;
	int trackCount() const { return tracker_.getTrackCount(); }
	int activeFragmentCount() const { return regionController_.getActiveFragmentCount(); }
	int managedRegionCount() const { return regionController_.getManagedRegionCount(); }
	// Sum of active tracked regions' normalized (smoothed) bounding-box
	// area, clamped to [0,1]. An approximation (overlapping regions double
	// count), not a pixel-exact occupied-area measurement — cheap and
	// honest about what it is, not a new analysis pass.
	float occupiedAreaFraction() const;

	// -- Debug/dev-only accessors — used by ofApp's standalone debug
	// overlay only. The production scene adapter must never call these. --
	BlobDetector & detector() { return detector_; }
	const BlobDetector & detector() const { return detector_; }
	BlobTracker & tracker() { return tracker_; }
	const BlobTracker & tracker() const { return tracker_; }
	VideoRegionController & regionController() { return regionController_; }
	const VideoRegionController & regionController() const { return regionController_; }
	VideoRegionEffectRenderer & backgroundEffectRenderer() { return backgroundEffectRenderer_; }

private:
	void drawBackground(const ofTexture & srcTex, const ofRectangle & destRect);

	VideoPlaybackService * video_ = nullptr; // not owned, see setup()

	ShaderLibrary shaderLib_;
	BlobDetector detector_;
	BlobTracker tracker_;
	VideoRegionController regionController_;

	// Separate scratch FBO pair from VideoRegionController's own (see
	// ofApp.h's original comment on backgroundEffectRenderer for why: the
	// background renders at full destRect size, and sharing one
	// high-water-mark FBO between "usually small blob crops" and "always
	// full-frame background" would permanently grow the fragment scratch
	// FBO to full-frame size the first time the background effect is used).
	VideoRegionEffectRenderer backgroundEffectRenderer_;
	BackgroundConfig backgroundConfig_;

	bool didSetup_ = false;
	bool active_ = false;
};
