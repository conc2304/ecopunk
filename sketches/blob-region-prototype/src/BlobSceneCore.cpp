#include "BlobSceneCore.h"
#include "VideoRegionMathOf.h"

#include "ofGraphics.h"
#include "ofUtils.h"

const std::array<std::string, 18> & BlobSceneCore::effectChoices() {
	// Unchanged from ofApp.cpp's former file-local kEffectChoices — see
	// that array's original comment (still in ofApp.cpp's history) for why
	// "desaturate" is first and why "ridgeline"/"contour" are excluded.
	static const std::array<std::string, 18> kChoices = {
		"desaturate", "invert", "recolor", "threshold", "dither", "solarize", "scanlines",
		"channelshift", "hue_rotate", "ascii_solarpunk", "bioluminescence", "chromatic_aberration",
		"edge_glow", "ink_outlines", "pixel_drift", "pixel_sorting", "water_refraction",
		"heatmap_recolor"
	};
	return kChoices;
}

void BlobSceneCore::setup(VideoPlaybackService & video) {
	video_ = &video;

	shaderLib_.setup();
	detector_.setup(BlobDetector::Config {});
	tracker_.setup(BlobTracker::Config {});
	regionController_.setup(&shaderLib_);

	didSetup_ = true;
}

void BlobSceneCore::activate() {
	active_ = true;
	// See header comment: reactivation must never carry over track
	// identities/fragments from a previous activation.
	reset();
}

void BlobSceneCore::deactivate() {
	active_ = false;
}

void BlobSceneCore::reset() {
	detector_.reset();
	tracker_.reset();
	regionController_.reset();
}

void BlobSceneCore::shutdown() {
	active_ = false;
	reset();
}

bool BlobSceneCore::hasVideoFrame() const {
	return video_ != nullptr && video_->currentTexture() != nullptr;
}

float BlobSceneCore::occupiedAreaFraction() const {
	float total = 0.0f;
	for (const VideoRegion & r : tracker_.getActiveRegions()) {
		total += r.smoothedNormalizedBounds.width * r.smoothedNormalizedBounds.height;
	}
	return ofClamp(total, 0.0f, 1.0f);
}

BlobSceneCore::UpdateTimings BlobSceneCore::update(float dt, const ofRectangle & destRect) {
	UpdateTimings timings;
	if (!active_ || video_ == nullptr) {
		return timings;
	}

	// Mirrors the pre-migration ofApp::update() gate exactly:
	// VideoPlaybackService::currentTexture() returns nullptr unless health
	// is Ready or Degraded — don't touch detection/tracking/region code
	// until real media is actually playing.
	const ofTexture * srcTex = video_->currentTexture();
	if (srcTex == nullptr) {
		return timings;
	}

	const ofPixels * srcPixels = video_->currentPixels();
	if (srcPixels != nullptr && srcPixels->isAllocated()) {
		detector_.update(*srcPixels);
	}
	timings.detectorMs = detector_.getLastProcessingTimeMs();

	uint64_t t0 = ofGetElapsedTimeMicros();
	tracker_.update(detector_.getDetections(), dt);
	timings.trackerMs = static_cast<float>(ofGetElapsedTimeMicros() - t0) / 1000.0f;

	if (srcTex->isAllocated()) {
		int srcW = static_cast<int>(srcTex->getWidth());
		int srcH = static_cast<int>(srcTex->getHeight());

		uint64_t t1 = ofGetElapsedTimeMicros();
		regionController_.update(tracker_.getActiveRegions(), *srcTex, srcW, srcH, destRect, dt);
		timings.regionUpdateMs = static_cast<float>(ofGetElapsedTimeMicros() - t1) / 1000.0f;
	}

	return timings;
}

void BlobSceneCore::draw(const ofRectangle & destRect) {
	if (!active_ || video_ == nullptr) {
		return;
	}

	const ofTexture * srcTex = video_->currentTexture();
	if (srcTex == nullptr || !srcTex->isAllocated()) {
		return;
	}

	// Background draw mode (off/normal/effect) — the region fragments
	// drawn below are additive on top of whatever this produces (or on
	// top of whatever the caller already cleared the target to, in "off"
	// mode), never a replacement for it.
	drawBackground(*srcTex, destRect);
	regionController_.draw();
}

void BlobSceneCore::drawBackground(const ofTexture & srcTex, const ofRectangle & destRect) {
	int mode = backgroundConfig_.mode;

	if (mode == 0) {
		// Off — leave the target as whatever the caller already cleared
		// it to. Detection/tracking/fragments are unaffected: they still
		// read the live texture directly, not this draw.
		return;
	}

	VideoRegionMath::Rect cropSrc = VideoRegionMath::computeCropFillSourceRect(
		srcTex.getWidth(), srcTex.getHeight(), VideoRegionMath::toRect(destRect));

	if (mode == 1) {
		// Normal — unshaded cover-fit draw, the original/default behavior.
		ofSetColor(255);
		srcTex.drawSubsection(destRect.x, destRect.y, destRect.width, destRect.height,
			cropSrc.x, cropSrc.y, cropSrc.width, cropSrc.height);
		return;
	}

	// Effect — run the whole background through one ShaderLibrary shader,
	// via the same VideoRegionEffectRenderer used per-fragment (just a
	// separate instance/scratch FBO pair — see backgroundEffectRenderer_'s
	// declaration in the header for why).
	VideoRegionEffectRenderer::RenderRequest req;
	req.sourceTexture = &srcTex;
	req.sourceCropPixels = VideoRegionMath::toOf(cropSrc);
	req.destinationBounds = destRect;
	req.alpha = 1.0f;
	req.effectName = backgroundConfig_.effectName;
	req.effectAmount = backgroundConfig_.effectAmount;

	backgroundEffectRenderer_.render(req, shaderLib_);
}
