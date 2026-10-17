#include "ofApp.h"
#include "BlobDetectionProfileApply.h"
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

	loadDetectionProfiles();
	if (const char * demo = std::getenv("BLOB_PROFILE_DEMO")) {
		profileDemoMode = demo;
		ofLogNotice("BlobProfileDemo") << "scripted profile demo: " << profileDemoMode << " (file " << detectionProfilesPath << ")";
	}
}

// -- Per-video detection presets ---------------------------------------------

std::optional<std::string> ofApp::currentMediaId() const {
	return videoPlayback.status().mediaId;
}

void ofApp::loadDetectionProfiles() {
	detectionProfilesPath = blobdetectionprofile::profilesPath();
	BlobDetectionProfileStore::LoadStatus st = detectionProfiles.load(detectionProfilesPath);
	detectionProfileFileStatus = st == BlobDetectionProfileStore::LoadStatus::Loaded
		? "loaded " + ofToString(detectionProfiles.size()) + " profile(s)"
		: st == BlobDetectionProfileStore::LoadStatus::Missing ? "no file yet (first save creates it)" : "REJECTED (saving disabled)";
	ofLogNotice("BlobDetectionProfile") << "profiles: " << detectionProfileFileStatus << " — " << detectionProfilesPath;
	for (const std::string & m : detectionProfiles.messages()) {
		ofLogNotice("BlobDetectionProfile") << m;
	}
	detectionProfileTracker.invalidate(); // reapply for the current media
}

void ofApp::applyDetectionProfileToGui(const BlobDetectionProfile & effective) {
	// The GUI is the source of truth for this dev app (syncParamsToSystems()
	// pushes it into the detector every frame), so presets land in the GUI.
	BlobDetectionProfile p = resolveProfile(blobdetectionprofile::canonicalDefaults(), &effective);
	pThreshold = *p.threshold;
	pMinBlobArea = static_cast<float>(*p.minBlobAreaNormalized);
	pMaxBlobArea = static_cast<float>(*p.maxBlobAreaNormalized);
	pErodeIterations = *p.erodeIterations;
	pDilateIterations = *p.dilateIterations;
}

void ofApp::saveDetectionProfileForCurrentMedia() {
	syncParamsToSystems();
	std::optional<std::string> mediaId = currentMediaId();
	if (!mediaId || mediaId->empty()) {
		ofLogError("BlobDetectionProfile") << "NOT SAVED: no canonical media id is active";
		return;
	}
	BlobDetectionProfile p = blobdetectionprofile::fromConfig(blobCore.detectionConfig());
	std::string error;
	if (!detectionProfiles.set(*mediaId, p, error) || !detectionProfiles.save(detectionProfilesPath, error)) {
		ofLogError("BlobDetectionProfile") << "NOT SAVED: " << error;
		return;
	}
	ofLogNotice("BlobDetectionProfile") << "Saved Blob detection profile -> " << detectionProfilesPath;
	std::istringstream lines(describeProfile(*mediaId, "per_media", p));
	std::string line;
	while (std::getline(lines, line)) {
		ofLogNotice("BlobDetectionProfile") << line;
	}
	detectionProfileFileStatus = "loaded " + ofToString(detectionProfiles.size()) + " profile(s)";
	detectionProfileSource = "per_media";
	detectionProfileTracker.invalidate();
}

void ofApp::restoreDetectionDefaults() {
	applyDetectionProfileToGui(blobdetectionprofile::canonicalDefaults());
	detectionProfileSource = "defaults (manual; file unchanged)";
	ofLogNotice("BlobDetectionProfile") << "detector restored to defaults for " << currentMediaId().value_or("-") << " (not saved)";
}

void ofApp::stepProfileDemo() {
	if (profileDemoMode.empty()) {
		return;
	}
	auto check = [&](bool ok, const std::string & what) {
		profileDemoChecks++;
		profileDemoFailures += ok ? 0 : 1;
		ofLogNotice("BlobProfileDemo") << (ok ? "PASS — " : "FAIL — ") << what;
	};
	auto expectApplied = [&](const std::string & tag) {
		std::string id = currentMediaId().value_or("");
		const BlobDetectionProfile * stored = detectionProfiles.find(id);
		BlobDetectionProfile want = resolveProfile(blobdetectionprofile::canonicalDefaults(), stored);
		BlobDetectionProfile have = blobdetectionprofile::fromConfig(blobCore.detectionConfig());
		std::string wantSource = stored != nullptr ? "per_media" : "defaults";
		check(have == want && detectionProfileSource == wantSource,
			tag + " media=" + id + " source=" + detectionProfileSource + " (want " + wantSource + ") threshold="
				+ ofToString(*have.threshold) + " erode=" + ofToString(*have.erodeIterations) + " dilate=" + ofToString(*have.dilateIterations)
				+ " minArea=" + ofToString(*have.minBlobAreaNormalized) + " maxArea=" + ofToString(*have.maxBlobAreaNormalized));
	};
	auto navigate = [&](bool forward) {
		profileDemoMediaBefore = currentMediaId();
		profileDemoFrames = 0;
		profileDemoStep++;
		if (!(forward ? videoPlayback.next() : videoPlayback.previous())) {
			ofLogError("BlobProfileDemo") << "navigation rejected";
		}
	};
	auto finish = [&]() {
		ofLogNotice("BlobProfileDemo") << "RESULT (" << profileDemoMode << "): " << (profileDemoChecks - profileDemoFailures) << "/"
									  << profileDemoChecks << " checks passed";
		profileDemoMode.clear();
		ofExit();
	};

	if (profileDemoMode == "verify-all") {
		// Validation sweep of every authored preset (dev tool): rewind to
		// the first catalog item, then visit each clip once — preset check
		// plus ~4 s of tracking stats — then alternate the last two clips.
		if (profileDemoPhase == 0) {
			if (!videoPlayback.previous()) {
				profileDemoPhase = 1;
				profileDemoMediaBefore.reset();
				profileDemoFrames = 0;
			}
			return;
		}
		std::optional<std::string> id = currentMediaId();
		if (!id || id == profileDemoMediaBefore || !blobCore.hasVideoFrame() || ++profileDemoFrames < 45) {
			return;
		}
		if (profileDemoStatFrames == 0) {
			expectApplied(profileDemoPhase == 1 ? "sweep" : "alternate");
		}
		// Stats window: tracked regions, fragments, largest single region.
		profileDemoStatFrames++;
		profileDemoTrackSum += blobCore.trackCount();
		profileDemoTrackPeak = std::max(profileDemoTrackPeak, blobCore.trackCount());
		profileDemoFragmentPeak = std::max(profileDemoFragmentPeak, blobCore.activeFragmentCount());
		for (const VideoRegion & r : blobCore.tracker().getActiveRegions()) {
			profileDemoLargestArea = std::max(profileDemoLargestArea, r.smoothedNormalizedBounds.width * r.smoothedNormalizedBounds.height);
		}
		if (profileDemoStatFrames < 240) {
			return;
		}
		VideoPlaybackStatus st = videoPlayback.status();
		ofLogNotice("BlobProfileDemo") << "STATS media=" << id.value_or("") << " title=\"" << st.fallbackDisplayTitle.value_or("") << "\""
									  << " source=" << detectionProfileSource
									  << " tracksAvg=" << ofToString(profileDemoTrackSum / profileDemoStatFrames, 2)
									  << " tracksPeak=" << profileDemoTrackPeak << " fragmentsPeak=" << profileDemoFragmentPeak
									  << " largestRegionArea=" << ofToString(profileDemoLargestArea, 3);
		if (const char * capture = std::getenv("BLOB_PROFILE_DEMO_CAPTURE")) {
			std::string title = st.fallbackDisplayTitle.value_or("");
			if (std::string(capture).find(title) != std::string::npos && !title.empty()) {
				profileDemoCaptureLabel = "verifyall_" + id.value_or("") + (profileDemoPhase == 2 ? "_alt" + ofToString(profileDemoAlternations) : "");
			}
		}
		profileDemoStatFrames = 0;
		profileDemoTrackSum = 0.0;
		profileDemoTrackPeak = 0;
		profileDemoFragmentPeak = 0;
		profileDemoLargestArea = 0.0f;
		profileDemoMediaBefore = id;
		profileDemoFrames = 0;
		if (profileDemoPhase == 1) {
			if (!videoPlayback.next()) {
				profileDemoPhase = 2; // end of catalog: alternate the last two clips
				profileDemoMediaBefore.reset();
			}
			return;
		}
		if (++profileDemoAlternations > 4) {
			finish();
			return;
		}
		if (!(profileDemoAlternations % 2 == 1 ? videoPlayback.previous() : videoPlayback.next())) {
			ofLogError("BlobProfileDemo") << "alternation navigation rejected";
		}
		return;
	}

	if (profileDemoMode == "verify" || profileDemoMode == "update") {
		// Seek each target media (as a user would navigate back to it), let
		// its preset apply, then check: A, B, an unprofiled clip, B, A.
		std::vector<std::string> ids = detectionProfiles.mediaIds();
		if (profileDemoTargets.empty()) {
			if (ids.size() < 2) {
				check(false, "verify needs two authored profiles in the file");
				finish();
				return;
			}
			profileDemoTargets = profileDemoMode == "update" ? std::vector<std::string> { ids[0] }
															   : std::vector<std::string> { ids[0], ids[1], "*", ids[1], ids[0] };
		}
		const std::string & target = profileDemoTargets[profileDemoStep];
		std::string current = currentMediaId().value_or("");
		bool onTarget = target == "*" ? (!current.empty() && detectionProfiles.find(current) == nullptr) : current == target;
		if (!onTarget) {
			if (++profileDemoSeekMoves > 120) {
				check(false, "could not reach media " + target);
				finish();
				return;
			}
			profileDemoFrames = 0;
			if (!(profileDemoSeekForward ? videoPlayback.next() : videoPlayback.previous())) {
				profileDemoSeekForward = !profileDemoSeekForward;
			}
			return;
		}
		if (!blobCore.hasVideoFrame() || ++profileDemoFrames < 45) {
			return;
		}
		if (profileDemoMode == "update") { // Test D: change A only
			expectApplied("A before update");
			pThreshold = 31;
			saveDetectionProfileForCurrentMedia();
			finish();
			return;
		}
		static const char * kTags[] = { "A after restart", "B after restart", "unprofiled media", "B again", "A again" };
		expectApplied(kTags[profileDemoStep]);
		if (profileDemoStep == 0) profileDemoCaptureLabel = "A";
		if (profileDemoStep == 1) profileDemoCaptureLabel = "B";
		if (profileDemoStep == 2) profileDemoCaptureLabel = "unprofiled";
		profileDemoSeekMoves = 0;
		profileDemoFrames = 0;
		if (++profileDemoStep >= static_cast<int>(profileDemoTargets.size())) {
			finish();
		}
		return;
	}

	// Wait until the new media is active, decoding, and its preset applied.
	std::optional<std::string> id = currentMediaId();
	if (!id || id == profileDemoMediaBefore || !blobCore.hasVideoFrame()) {
		return;
	}
	if (++profileDemoFrames < 45) {
		return;
	}

	if (profileDemoMode == "author") {
		if (profileDemoStep == 0) { // A: tune and save
			expectApplied("A before tuning");
			pThreshold = 28; pErodeIterations = 0; pMinBlobArea = 0.0005f;
			saveDetectionProfileForCurrentMedia();
			navigate(true);
		} else if (profileDemoStep == 1) { // B: A must not leak; tune and save
			expectApplied("B before tuning (A's values must not leak)");
			pThreshold = 46; pDilateIterations = 1; pMaxBlobArea = 0.5f;
			saveDetectionProfileForCurrentMedia();
			navigate(true);
		} else { // C: no profile -> defaults
			expectApplied("C (no profile)");
			finish();
		}
	}
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

	// Per-video preset, once per media change (no profile -> defaults).
	if (auto applied = detectionProfileTracker.observe(currentMediaId(), detectionProfiles, blobdetectionprofile::canonicalDefaults())) {
		applyDetectionProfileToGui(applied->effective);
		detectionProfileSource = applied->source;
		std::istringstream lines(describeProfile(applied->mediaId, applied->source, applied->effective));
		std::string line;
		while (std::getline(lines, line)) {
			ofLogNotice("BlobDetectionProfile") << line;
		}
	}
	stepProfileDemo();

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

	if (!profileDemoCaptureLabel.empty()) {
		std::string path = "captures/profile_demo_" + profileDemoCaptureLabel + ".png";
		ofFilePath::createEnclosingDirectory(path);
		ofSaveScreen(path);
		ofLogNotice("BlobProfileDemo") << "captured " << path;
		profileDemoCaptureLabel.clear();
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
		ss << "media: " << currentMediaId().value_or("(none)") << "\n";
		ss << "detection profile: " << detectionProfileSource << "  [" << detectionProfileFileStatus << "]\n";
		ss << "  threshold=" << blobCore.detectionConfig().threshold << " minArea=" << blobCore.detectionConfig().minBlobAreaNormalized
		   << " maxArea=" << blobCore.detectionConfig().maxBlobAreaNormalized << " erode=" << blobCore.detectionConfig().erodeIterations
		   << " dilate=" << blobCore.detectionConfig().dilateIterations << "\n";
		ss << "[g] toggle GUI  [d] toggle debug overlay  [n/p] next/previous media\n";
		ss << "[s] save detection profile for this media  [r] reload profiles  [D] restore detector defaults";
		ofDrawBitmapStringHighlight(ss.str(), 20, 30);
	}
}

void ofApp::keyPressed(int key) {
	if (key == 'g') {
		guiVisible = !guiVisible;
	} else if (key == 'd') {
		pDebugVisualization = !pDebugVisualization;
	} else if (key == 's') {
		saveDetectionProfileForCurrentMedia();
	} else if (key == 'r') {
		loadDetectionProfiles();
	} else if (key == 'D') {
		restoreDetectionDefaults();
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
