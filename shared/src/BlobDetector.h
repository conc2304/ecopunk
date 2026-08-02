#pragma once

#include "ofPixels.h"
#include "ofRectangle.h"
#include "ofxCvContourFinder.h"
#include "ofxCvGrayscaleImage.h"
#include "glm/vec2.hpp"
#include <vector>

// One detection, already in normalized [0,1] source-frame coordinates —
// analysis-resolution pixels never leak past this class (see
// docs/blob-region-architecture.md's coordinate-space notes).
struct BlobDetection {
	ofRectangle normalizedBounds;
	glm::vec2 normalizedCentroid { 0.0f, 0.0f };
	float normalizedArea = 0.0f;
	float confidence = 1.0f;
};

// Frame-difference blob detector. Does NOT own a video source — callers
// (VideoRegionController's host ofApp) hand it whatever full-resolution
// ofPixels the shared video source already decoded (see
// TimeOffsetVideoBuffer::getRawVideoPixels()); this class never opens an
// ofVideoPlayer, never grabs a screen/texture readback, and never sees GPU
// state at all.
//
// Pipeline (see update()): downsample+grayscale -> frame difference against
// the previous analysis frame -> threshold -> optional erode/dilate ->
// ofxCvContourFinder -> area filter -> normalized detections. All working
// buffers are allocated once (in setup(), or lazily the first time the
// configured analysis size changes) and reused every call — no
// steady-state per-frame allocation.
class BlobDetector {
public:
	struct Config {
		bool enabled = true;
		int analysisWidth = 320;
		int analysisHeight = 180;
		int processEveryNFrames = 2;

		int threshold = 40; // 0-255, applied to the abs-diff grayscale image
		float minBlobAreaNormalized = 0.0008f; // fraction of analysisWidth*analysisHeight
		float maxBlobAreaNormalized = 0.35f;
		int maxBlobs = 12;
		int erodeIterations = 1;
		int dilateIterations = 2;
	};

	void setup(const Config & initialConfig);

	// sourceFramePixels: the full-resolution decoded frame (any pixel
	// format ofPixels supports — this class reads it only through
	// ofPixels::getColor(), so RGB/RGBA/BGRA source formats are all handled
	// uniformly without a format-specific fast path; see the downsample
	// step in the .cpp for why that's the safe choice here).
	//
	// Internally rate-limited by config.processEveryNFrames: returns false
	// (detections left unchanged from the previous successful call) on
	// frames it skips, true when it actually ran the pipeline this call.
	bool update(const ofPixels & sourceFramePixels);

	const std::vector<BlobDetection> & getDetections() const { return detections; }

	Config & getConfig() { return config; }
	const Config & getConfig() const { return config; }

	float getLastProcessingTimeMs() const { return lastProcessingTimeMs; }
	int getAnalysisWidth() const { return config.analysisWidth; }
	int getAnalysisHeight() const { return config.analysisHeight; }

	// Debug visualization — the analysis-resolution grayscale frame and the
	// thresholded diff image actually fed to the contour finder.
	const ofxCvGrayscaleImage & getDebugCurrentGray() const { return currentGray; }
	const ofxCvGrayscaleImage & getDebugDiffImage() const { return diffGray; }
	const ofxCvContourFinder & getDebugContourFinder() const { return contourFinder; }

private:
	void ensureAnalysisBuffersAllocated();
	void downsampleToGray(const ofPixels & src, ofPixels & dstGray) const;

	Config config;

	ofPixels grayAnalysisPixels; // scratch: downsampled+grayscale current frame (1 channel, unsigned char)
	ofxCvGrayscaleImage currentGray;
	ofxCvGrayscaleImage prevGray;
	ofxCvGrayscaleImage diffGray;
	ofxCvContourFinder contourFinder;

	bool buffersAllocated = false;
	bool havePrevFrame = false;
	int allocatedW = -1;
	int allocatedH = -1;

	int frameCounter = 0;
	float lastProcessingTimeMs = 0.0f;

	std::vector<BlobDetection> detections;
};
