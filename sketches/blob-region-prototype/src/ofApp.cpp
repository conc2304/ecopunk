#include "ofApp.h"
#include <array>
#include <sstream>

namespace {
	// Index into ShaderLibrary by name, chosen for the GUI's plain
	// ofParameter<int> slider (see ofApp.h's comment on pEffectIndex).
	// "desaturate" first because the handoff explicitly calls for starting
	// with one known-safe lightweight shader before trusting the rest of
	// the pool against this new drawSubsection+shader path. All 17 are now
	// wired with correct per-shader uniforms (see
	// VideoRegionEffectRenderer::setEffectUniforms) — earlier this list only
	// had 4 entries because most of ShaderLibrary's shaders need uniforms
	// beyond tex/tex0/resolution/alpha that weren't being set, which is what
	// produced solid black ("hue_rotate": unset valueMult defaults to 0) or
	// solid white ("threshold": unset threshold defaults to 0) fragments.
	const std::array<std::string, 18> kEffectChoices = {
		"desaturate", "invert", "recolor", "threshold", "dither", "solarize", "scanlines",
		"channelshift", "hue_rotate", "ascii_solarpunk", "bioluminescence", "chromatic_aberration",
		"edge_glow", "ink_outlines", "pixel_drift", "pixel_sorting", "water_refraction",
		"heatmap_recolor"
	};
}

void ofApp::setup() {
	// Every other sketch that uses ShaderLibrary calls this first
	// (blueprint_emergence/ofApp.cpp:21, temporal-fields/ofApp.cpp:13,
	// quadrant-crosshair/ofApp.cpp:8) — without it, oF defaults to
	// GL_TEXTURE_RECTANGLE textures, which every ShaderLibrary shader's
	// "uniform sampler2D tex" + normalized-[0,1] texture2D() sampling is
	// incompatible with. Missing this call is what made every shaded
	// fragment sample as solid black regardless of real crop content —
	// confirmed via a CPU pixel readback that showed real varied color
	// going INTO the shader pass and solid (0,0,0,255) coming out of it.
	ofDisableArbTex();

	ofSetWindowTitle("blob-region-prototype");
	ofBackground(0);
	ofSetVerticalSync(true);

	shaderLib.setup();

	// automaticAdvance is deliberately false: this scene has never
	// advanced media on its own (no next/prev command, no hold — see
	// docs/video-playback-ownership-probe-report.md's per-scene
	// compatibility matrix) and preserving that exact "play the same clip
	// forever" behavior is this migration's whole point, not a place to
	// introduce new automatic-cycling behavior the scene never had.
	//
	// mediaRoot updated to the canonical physical root (Shared Video
	// Playback Engineering Session 2, Task C) — this sketch's own
	// bin/data/media/ now holds only compatibility symlinks (see
	// docs/shared-video-playback-engineering-session-2-report.md), no
	// longer scanned directly.
	VideoPlaybackService::Config videoConfig;
	videoConfig.mediaRoot = ofToDataPath("../../../../assets/shared/media", true);
	videoConfig.automaticAdvance = false;
	videoPlayback.setup(videoConfig);

	blobDetector.setup(BlobDetector::Config {});
	blobTracker.setup(BlobTracker::Config {});
	regionController.setup(&shaderLib);

	{
		std::stringstream choicesLog;
		choicesLog << "effect index choices:";
		for (size_t i = 0; i < kEffectChoices.size(); i++) {
			choicesLog << " " << i << "=" << kEffectChoices[i];
		}
		ofLogNotice("ofApp") << choicesLog.str();
	}

	gui.setup("Blob Region Prototype");

	detectionGroup.setName("Detection");
	detectionGroup.add(pDetectionEnabled);
	detectionGroup.add(pAnalysisWidth);
	detectionGroup.add(pAnalysisHeight);
	detectionGroup.add(pProcessEveryNFrames);
	detectionGroup.add(pThreshold);
	detectionGroup.add(pMinBlobArea);
	detectionGroup.add(pMaxBlobArea);
	detectionGroup.add(pMaxBlobs);
	detectionGroup.add(pErodeIterations);
	detectionGroup.add(pDilateIterations);

	trackingGroup.setName("Tracking");
	trackingGroup.add(pAssociationDistance);
	trackingGroup.add(pMinIoU);
	trackingGroup.add(pPositionSmoothing);
	trackingGroup.add(pSizeSmoothing);
	trackingGroup.add(pMaxMissingSeconds);

	backgroundGroup.setName("Background");
	backgroundGroup.add(pBackgroundMode);
	backgroundGroup.add(pBackgroundEffectIndex);
	backgroundGroup.add(pBackgroundEffectAmount);

	renderingGroup.setName("Rendering");
	renderingGroup.add(pFragmentsEnabled);
	renderingGroup.add(pEffectIndex);
	renderingGroup.add(pEffectAmount);
	renderingGroup.add(pFragmentScale);
	renderingGroup.add(pCropPadding);
	renderingGroup.add(pMaxActiveFragments);
	renderingGroup.add(pDrawMode);

	perfGroup.setName("Performance / Debug");
	perfGroup.add(pDebugVisualization);
	perfGroup.add(pDebugTiming);

	gui.add(detectionGroup);
	gui.add(trackingGroup);
	gui.add(backgroundGroup);
	gui.add(renderingGroup);
	gui.add(perfGroup);
}

void ofApp::syncParamsToSystems() {
	BlobDetector::Config & dc = blobDetector.getConfig();
	dc.enabled = pDetectionEnabled;
	dc.analysisWidth = pAnalysisWidth;
	dc.analysisHeight = pAnalysisHeight;
	dc.processEveryNFrames = pProcessEveryNFrames;
	dc.threshold = pThreshold;
	dc.minBlobAreaNormalized = pMinBlobArea;
	dc.maxBlobAreaNormalized = pMaxBlobArea;
	dc.maxBlobs = pMaxBlobs;
	dc.erodeIterations = pErodeIterations;
	dc.dilateIterations = pDilateIterations;

	BlobTracker::Config & tc = blobTracker.getConfig();
	tc.maxAssociationDistanceNormalized = pAssociationDistance;
	tc.minIoU = pMinIoU;
	tc.positionSmoothing = pPositionSmoothing;
	tc.sizeSmoothing = pSizeSmoothing;
	tc.maxMissingSeconds = pMaxMissingSeconds;

	VideoRegionController::Params & rp = regionController.params;
	rp.fragmentsEnabled = pFragmentsEnabled;
	int idx = ofClamp(pEffectIndex.get(), 0, static_cast<int>(kEffectChoices.size()) - 1);
	rp.effectName = kEffectChoices[idx];
	rp.effectAmount = pEffectAmount;
	rp.fragmentScale = pFragmentScale;
	rp.cropPadding = pCropPadding;
	rp.maxActiveFragments = pMaxActiveFragments;
	rp.drawMode = pDrawMode;
}

void ofApp::update() {
	uint64_t frameStartMicros = ofGetElapsedTimeMicros();
	lastFrameStartMicros = frameStartMicros;

	float dt = ofGetLastFrameTime();
	if (dt <= 0.0f || dt > 0.5f) {
		dt = 1.0f / 60.0f;
	}

	videoPlayback.update(dt);
	syncParamsToSystems();

	float detectorMs = 0.0f;
	float trackerMs = 0.0f;
	float regionUpdateMs = 0.0f;

	// Mirrors the old `videoBuffer.hasMedia()` gate exactly:
	// VideoPlaybackService::currentTexture() returns nullptr unless health
	// is Ready or Degraded (see VideoPlaybackService.cpp) — the same
	// "don't touch detection/tracking/region code until real media is
	// actually playing" behavior as before, expressed as a null check
	// instead of a bool getter.
	if (const ofTexture * srcTex = videoPlayback.currentTexture()) {
		// CPU pixel path: reuse the decoder's own pixels via
		// VideoPlaybackService::currentPixels() — no GPU readback anywhere
		// in this pipeline.
		const ofPixels * srcPixels = videoPlayback.currentPixels();
		if (srcPixels && srcPixels->isAllocated()) {
			blobDetector.update(*srcPixels);
		}
		detectorMs = blobDetector.getLastProcessingTimeMs();

		uint64_t t0 = ofGetElapsedTimeMicros();
		blobTracker.update(blobDetector.getDetections(), dt);
		trackerMs = static_cast<float>(ofGetElapsedTimeMicros() - t0) / 1000.0f;

		if (srcTex->isAllocated()) {
			int srcW = static_cast<int>(srcTex->getWidth());
			int srcH = static_cast<int>(srcTex->getHeight());
			ofRectangle destRect(0, 0, static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight()));

			uint64_t t1 = ofGetElapsedTimeMicros();
			regionController.update(blobTracker.getActiveRegions(), *srcTex, srcW, srcH, destRect, dt);
			regionUpdateMs = static_cast<float>(ofGetElapsedTimeMicros() - t1) / 1000.0f;
		}
	}

	detectorMsAvg.push(detectorMs);
	trackerMsAvg.push(trackerMs);
	regionUpdateMsAvg.push(regionUpdateMs);

	statsLogAccumSeconds += dt;
	if (statsLogAccumSeconds >= 2.0f) {
		statsLogAccumSeconds = 0.0f;
		ofLogNotice("PerfStats")
			<< "fps=" << ofToString(ofGetFrameRate(), 1)
			<< " detectorMs=" << ofToString(detectorMsAvg.average(), 3)
			<< " trackerMs=" << ofToString(trackerMsAvg.average(), 3)
			<< " regionUpdateMs=" << ofToString(regionUpdateMsAvg.average(), 3)
			<< " drawMs=" << ofToString(drawMsAvg.average(), 3)
			<< " totalMs=" << ofToString(totalFrameMsAvg.average(), 3)
			<< " trackedRegions=" << blobTracker.getTrackCount()
			<< " activeFragments=" << regionController.getActiveFragmentCount()
			<< " scratchFbo=" << regionController.getScratchFboWidth() << "x" << regionController.getScratchFboHeight();
	}
}

void ofApp::draw() {
	ofBackground(0);

	ofRectangle destRect(0, 0, static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight()));
	const ofTexture * srcTex = videoPlayback.currentTexture();

	float drawMs = 0.0f;

	if (srcTex && srcTex->isAllocated()) {
		// Background draw mode (off/normal/effect) — the region fragments
		// drawn below are additive on top of whatever this produces (or
		// on top of the plain black-cleared canvas, in "off" mode), never
		// a replacement for it.
		drawBackground(*srcTex, destRect);

		uint64_t t0 = ofGetElapsedTimeMicros();
		regionController.draw();
		drawMs = static_cast<float>(ofGetElapsedTimeMicros() - t0) / 1000.0f;
	} else {
		ofDrawBitmapStringHighlight("Loading media...", 20, 20);
	}

	drawMsAvg.push(drawMs);

	if (pDebugVisualization) {
		drawDebugOverlay();
	}

	float totalMs = static_cast<float>(ofGetElapsedTimeMicros() - lastFrameStartMicros) / 1000.0f;
	totalFrameMsAvg.push(totalMs);

	if (guiVisible) {
		gui.draw();
	}
}

void ofApp::drawBackground(const ofTexture & srcTex, const ofRectangle & destRect) {
	int mode = pBackgroundMode.get();

	if (mode == 0) {
		// Off — leave the canvas as whatever ofBackground(0) already
		// cleared it to. Detection/tracking/fragments are unaffected: they
		// still read the live texture directly, not this draw.
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
	// separate instance/scratch FBO pair — see backgroundEffectRenderer's
	// declaration in ofApp.h for why). The "crop" here is the same
	// cover-fit source rect the normal-mode draw uses, and the
	// "destination" is the full window — so this is really just one more
	// RenderRequest, sized to the whole background instead of one blob.
	int idx = ofClamp(pBackgroundEffectIndex.get(), 0, static_cast<int>(kEffectChoices.size()) - 1);

	VideoRegionEffectRenderer::RenderRequest req;
	req.sourceTexture = &srcTex;
	req.sourceCropPixels = VideoRegionMath::toOf(cropSrc);
	req.destinationBounds = destRect;
	req.alpha = 1.0f;
	req.effectName = kEffectChoices[idx];
	req.effectAmount = pBackgroundEffectAmount;

	backgroundEffectRenderer.render(req, shaderLib);
}

void ofApp::drawDebugOverlay() {
	ofRectangle destRect(0, 0, static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight()));
	const ofTexture * srcTex = videoPlayback.currentTexture();

	if (srcTex && srcTex->isAllocated()) {
		VideoRegionMath::Rect cropSrc = VideoRegionMath::computeCropFillSourceRect(
			srcTex->getWidth(), srcTex->getHeight(), VideoRegionMath::toRect(destRect));
		VideoRegionMath::Rect destR = VideoRegionMath::toRect(destRect);

		// Raw detections (this analysis frame's un-tracked contour boxes) in orange.
		ofPushStyle();
		ofNoFill();
		ofSetColor(255, 140, 0);
		for (const BlobDetection & d : blobDetector.getDetections()) {
			VideoRegionMath::Rect r = VideoRegionMath::mapNormalizedSourceRectToScreen(
				VideoRegionMath::toRect(d.normalizedBounds), srcTex->getWidth(), srcTex->getHeight(), cropSrc, destR);
			ofDrawRectangle(r.x, r.y, r.width, r.height);
		}

		// Tracked/smoothed regions in cyan, labeled with their stable id.
		ofSetColor(0, 220, 255);
		for (const VideoRegion & region : blobTracker.getActiveRegions()) {
			VideoRegionMath::Rect r = VideoRegionMath::mapNormalizedSourceRectToScreen(
				VideoRegionMath::toRect(region.smoothedNormalizedBounds), srcTex->getWidth(), srcTex->getHeight(), cropSrc, destR);
			ofDrawRectangle(r.x, r.y, r.width, r.height);
			ofDrawBitmapStringHighlight(
				"id " + ofToString(region.id) + " miss " + ofToString(region.missingSeconds, 2),
				r.x + 2, r.y - 6);
		}
		ofPopStyle();

		// Analysis-resolution debug thumbnails: grayscale + thresholded diff.
		float thumbW = 160.0f;
		float thumbH = thumbW * (static_cast<float>(blobDetector.getAnalysisHeight()) / std::max(1, blobDetector.getAnalysisWidth()));
		ofSetColor(255);
		blobDetector.getDebugCurrentGray().draw(10.0f, ofGetHeight() - thumbH * 2 - 20.0f, thumbW, thumbH);
		blobDetector.getDebugDiffImage().draw(10.0f, ofGetHeight() - thumbH - 10.0f, thumbW, thumbH);
		ofDrawBitmapStringHighlight("gray", 12, ofGetHeight() - thumbH * 2 - 24.0f);
		ofDrawBitmapStringHighlight("diff (thresholded)", 12, ofGetHeight() - thumbH - 14.0f);
	}

	if (pDebugTiming) {
		std::stringstream ss;
		ss << "fps: " << ofToString(ofGetFrameRate(), 1) << "\n";
		ss << "detector avg: " << ofToString(detectorMsAvg.average(), 3) << " ms\n";
		ss << "tracker avg: " << ofToString(trackerMsAvg.average(), 3) << " ms\n";
		ss << "region update avg: " << ofToString(regionUpdateMsAvg.average(), 3) << " ms\n";
		ss << "region draw avg: " << ofToString(drawMsAvg.average(), 3) << " ms\n";
		ss << "total frame avg: " << ofToString(totalFrameMsAvg.average(), 3) << " ms\n";
		ss << "analysis res: " << blobDetector.getAnalysisWidth() << "x" << blobDetector.getAnalysisHeight() << "\n";
		ss << "tracked regions: " << blobTracker.getTrackCount() << "\n";
		ss << "active fragments: " << regionController.getActiveFragmentCount() << " / " << pMaxActiveFragments.get() << "\n";
		ss << "scratch FBO: " << regionController.getScratchFboWidth() << "x" << regionController.getScratchFboHeight() << "\n";
		{
			static const char * kModeNames[3] = { "off", "normal", "effect" };
			int mode = ofClamp(pBackgroundMode.get(), 0, 2);
			ss << "background: " << kModeNames[mode];
			if (mode == 2) {
				int bgIdx = ofClamp(pBackgroundEffectIndex.get(), 0, static_cast<int>(kEffectChoices.size()) - 1);
				ss << " (" << kEffectChoices[bgIdx] << ", scratch " << backgroundEffectRenderer.getScratchWidth()
				   << "x" << backgroundEffectRenderer.getScratchHeight() << ")";
			}
			ss << "\n";
		}
		ss << "CPU pixels path: VideoPlaybackService::currentPixels() (no GPU readback)\n";
		ss << "[g] toggle GUI  [d] toggle debug overlay";
		ofDrawBitmapStringHighlight(ss.str(), 20, 30);
	}
}

void ofApp::keyPressed(int key) {
	if (key == 'g') {
		guiVisible = !guiVisible;
	} else if (key == 'd') {
		pDebugVisualization = !pDebugVisualization;
	}
}
