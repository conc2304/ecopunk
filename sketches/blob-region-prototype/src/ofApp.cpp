#include "ofApp.h"
#include "VideoRegionMathOf.h"
#include <sstream>

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
	//
	// (Blob First Complete Production Migration: the production
	// ExperienceRuntime path establishes this same call once, globally,
	// in ExperienceRuntime::establishGlobalRenderingBaseline() — this
	// standalone sketch still needs its own call since it never runs
	// under that runtime.)
	ofDisableArbTex();

	ofSetWindowTitle("blob-region-prototype");
	ofBackground(0);
	ofSetVerticalSync(true);

	// automaticAdvance is deliberately false: this scene has never
	// advanced media on its own (no next/prev command, no hold — see
	// docs/video-playback-ownership-probe-report.md's per-scene
	// compatibility matrix) and preserving that exact "play the same clip
	// forever" behavior in the standalone dev sketch is not a place to
	// introduce new automatic-cycling behavior it never had. (The
	// production scene adapter shares the runtime's own
	// VideoPlaybackService, configured with automaticAdvance=true — see
	// BlobProductionScene's own notes on that deliberate difference.)
	//
	// mediaRoot points at the canonical physical root (Shared Video
	// Playback Engineering Session 2, Task C) — this sketch's own
	// bin/data/media/ now holds only compatibility symlinks, no longer
	// scanned directly.
	VideoPlaybackService::Config videoConfig;
	videoConfig.mediaRoot = ofToDataPath("../../../../assets/shared/media", true);
	videoConfig.automaticAdvance = false;
	videoPlayback.setup(videoConfig);

	blobCore.setup(videoPlayback);
	blobCore.activate();

	{
		std::stringstream choicesLog;
		choicesLog << "effect index choices:";
		const auto & choices = BlobSceneCore::effectChoices();
		for (size_t i = 0; i < choices.size(); i++) {
			choicesLog << " " << i << "=" << choices[i];
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
	BlobDetector::Config & dc = blobCore.detectionConfig();
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

	BlobTracker::Config & tc = blobCore.trackingConfig();
	tc.maxAssociationDistanceNormalized = pAssociationDistance;
	tc.minIoU = pMinIoU;
	tc.positionSmoothing = pPositionSmoothing;
	tc.sizeSmoothing = pSizeSmoothing;
	tc.maxMissingSeconds = pMaxMissingSeconds;

	VideoRegionController::Params & rp = blobCore.regionParams();
	rp.fragmentsEnabled = pFragmentsEnabled;
	const auto & choices = BlobSceneCore::effectChoices();
	int idx = ofClamp(pEffectIndex.get(), 0, static_cast<int>(choices.size()) - 1);
	rp.effectName = choices[idx];
	rp.effectAmount = pEffectAmount;
	rp.fragmentScale = pFragmentScale;
	rp.cropPadding = pCropPadding;
	rp.maxActiveFragments = pMaxActiveFragments;
	rp.drawMode = pDrawMode;

	BlobSceneCore::BackgroundConfig & bc = blobCore.backgroundConfig();
	bc.mode = pBackgroundMode;
	int bgIdx = ofClamp(pBackgroundEffectIndex.get(), 0, static_cast<int>(choices.size()) - 1);
	bc.effectName = choices[bgIdx];
	bc.effectAmount = pBackgroundEffectAmount;
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

	ofRectangle destRect(0, 0, static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight()));
	BlobSceneCore::UpdateTimings timings = blobCore.update(dt, destRect);

	detectorMsAvg.push(timings.detectorMs);
	trackerMsAvg.push(timings.trackerMs);
	regionUpdateMsAvg.push(timings.regionUpdateMs);

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
			<< " trackedRegions=" << blobCore.trackCount()
			<< " activeFragments=" << blobCore.activeFragmentCount()
			<< " scratchFbo=" << blobCore.regionController().getScratchFboWidth() << "x" << blobCore.regionController().getScratchFboHeight();
	}
}

void ofApp::draw() {
	ofBackground(0);

	ofRectangle destRect(0, 0, static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight()));

	float drawMs = 0.0f;
	if (blobCore.hasVideoFrame()) {
		uint64_t t0 = ofGetElapsedTimeMicros();
		blobCore.draw(destRect);
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
		for (const BlobDetection & d : blobCore.detector().getDetections()) {
			VideoRegionMath::Rect r = VideoRegionMath::mapNormalizedSourceRectToScreen(
				VideoRegionMath::toRect(d.normalizedBounds), srcTex->getWidth(), srcTex->getHeight(), cropSrc, destR);
			ofDrawRectangle(r.x, r.y, r.width, r.height);
		}

		// Tracked/smoothed regions in cyan, labeled with their stable id.
		ofSetColor(0, 220, 255);
		for (const VideoRegion & region : blobCore.tracker().getActiveRegions()) {
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
		float thumbH = thumbW * (static_cast<float>(blobCore.detector().getAnalysisHeight()) / std::max(1, blobCore.detector().getAnalysisWidth()));
		ofSetColor(255);
		blobCore.detector().getDebugCurrentGray().draw(10.0f, ofGetHeight() - thumbH * 2 - 20.0f, thumbW, thumbH);
		blobCore.detector().getDebugDiffImage().draw(10.0f, ofGetHeight() - thumbH - 10.0f, thumbW, thumbH);
		ofDrawBitmapStringHighlight("gray", 12, ofGetHeight() - thumbH * 2 - 24.0f);
		ofDrawBitmapStringHighlight("diff (thresholded)", 12, ofGetHeight() - thumbH - 14.0f);
	}

	if (pDebugTiming) {
		const auto & choices = BlobSceneCore::effectChoices();
		std::stringstream ss;
		ss << "fps: " << ofToString(ofGetFrameRate(), 1) << "\n";
		ss << "detector avg: " << ofToString(detectorMsAvg.average(), 3) << " ms\n";
		ss << "tracker avg: " << ofToString(trackerMsAvg.average(), 3) << " ms\n";
		ss << "region update avg: " << ofToString(regionUpdateMsAvg.average(), 3) << " ms\n";
		ss << "region draw avg: " << ofToString(drawMsAvg.average(), 3) << " ms\n";
		ss << "total frame avg: " << ofToString(totalFrameMsAvg.average(), 3) << " ms\n";
		ss << "analysis res: " << blobCore.detector().getAnalysisWidth() << "x" << blobCore.detector().getAnalysisHeight() << "\n";
		ss << "tracked regions: " << blobCore.trackCount() << "\n";
		ss << "active fragments: " << blobCore.activeFragmentCount() << " / " << pMaxActiveFragments.get() << "\n";
		ss << "scratch FBO: " << blobCore.regionController().getScratchFboWidth() << "x" << blobCore.regionController().getScratchFboHeight() << "\n";
		{
			static const char * kModeNames[3] = { "off", "normal", "effect" };
			int mode = ofClamp(pBackgroundMode.get(), 0, 2);
			ss << "background: " << kModeNames[mode];
			if (mode == 2) {
				int bgIdx = ofClamp(pBackgroundEffectIndex.get(), 0, static_cast<int>(choices.size()) - 1);
				ss << " (" << choices[bgIdx] << ", scratch " << blobCore.backgroundEffectRenderer().getScratchWidth()
				   << "x" << blobCore.backgroundEffectRenderer().getScratchHeight() << ")";
			}
			ss << "\n";
		}
		ss << "CPU pixels path: VideoPlaybackService::currentPixels() (no GPU readback)\n";
		ss << "[g] toggle GUI  [d] toggle debug overlay  [n/p] next/previous media";
		ofDrawBitmapStringHighlight(ss.str(), 20, 30);
	}
}

void ofApp::keyPressed(int key) {
	if (key == 'g') {
		guiVisible = !guiVisible;
	} else if (key == 'd') {
		pDebugVisualization = !pDebugVisualization;
	} else if (key == 'n') {
		// Blob Post-Acceptance Hardening: dev-only Next/Previous media
		// navigation, mirroring the production InputRouter's own n/p
		// mapping (sketches/experience_runtime/src/InputRouter.cpp) —
		// added so the standalone sketch can be manually navigated to the
		// same canonical media item as the runtime for controlled visual-
		// parity captures, without any new VideoPlaybackService API:
		// next()/previous() already exist and already reject cleanly at
		// the catalog's ends, so repeating either call more times than the
		// catalog size deterministically lands on a stable end item in
		// both apps.
		videoPlayback.next();
	} else if (key == 'p') {
		videoPlayback.previous();
	}
}
