#include "BlobDetector.h"
#include "ofMath.h"
#include "ofUtils.h"
#include <algorithm>

void BlobDetector::setup(const Config & initialConfig) {
	config = initialConfig;
	ensureAnalysisBuffersAllocated();
}

void BlobDetector::ensureAnalysisBuffersAllocated() {
	int w = std::max(1, config.analysisWidth);
	int h = std::max(1, config.analysisHeight);

	if (buffersAllocated && w == allocatedW && h == allocatedH) {
		return;
	}

	grayAnalysisPixels.allocate(w, h, OF_PIXELS_GRAY);
	currentGray.allocate(w, h);
	prevGray.allocate(w, h);
	diffGray.allocate(w, h);

	allocatedW = w;
	allocatedH = h;
	buffersAllocated = true;
	havePrevFrame = false; // stale reference frame at the old resolution — must re-seed
}

void BlobDetector::downsampleToGray(const ofPixels & src, ofPixels & dstGray) const {
	// Deliberately a hand-rolled nearest-neighbor downsample+luma pass
	// instead of ofPixels::resizeTo() + ofxCvColorImage::setFromPixels():
	// resizeTo() requires the destination to already match the source's
	// bytes-per-pixel, and ofxCvColorImage::setFromPixels()/
	// ofxCvGrayscaleImage::setFromPixels() both memcpy raw bytes assuming a
	// FIXED channel count (3 and 1 respectively) with no regard for the
	// source ofPixels' actual ofPixelFormat — feeding either of those an
	// RGBA source (a real possibility depending on video backend/platform)
	// would silently misread the buffer. Reading through
	// ofPixels::getColor(x,y), which is format-aware internally, sidesteps
	// that entirely and costs exactly analysisWidth*analysisHeight color
	// reads regardless of source resolution — the same 320x180 = 57,600
	// iteration bound this class is sized around either way.
	int srcW = static_cast<int>(src.getWidth());
	int srcH = static_cast<int>(src.getHeight());
	int dstW = static_cast<int>(dstGray.getWidth());
	int dstH = static_cast<int>(dstGray.getHeight());
	if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) {
		return;
	}

	unsigned char * dst = dstGray.getData();

	for (int y = 0; y < dstH; y++) {
		int sy = std::min(srcH - 1, (y * srcH) / dstH);
		for (int x = 0; x < dstW; x++) {
			int sx = std::min(srcW - 1, (x * srcW) / dstW);
			ofColor c = src.getColor(sx, sy);
			float luma = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
			dst[y * dstW + x] = static_cast<unsigned char>(ofClamp(luma, 0.0f, 255.0f));
		}
	}
}

bool BlobDetector::update(const ofPixels & sourceFramePixels) {
	if (!config.enabled || !sourceFramePixels.isAllocated()) {
		return false;
	}

	ensureAnalysisBuffersAllocated();

	frameCounter++;
	int everyN = std::max(1, config.processEveryNFrames);
	if (frameCounter % everyN != 0) {
		return false;
	}

	uint64_t startMicros = ofGetElapsedTimeMicros();

	downsampleToGray(sourceFramePixels, grayAnalysisPixels);
	currentGray.setFromPixels(grayAnalysisPixels.getData(), allocatedW, allocatedH);

	if (!havePrevFrame) {
		prevGray = currentGray;
		havePrevFrame = true;
		lastProcessingTimeMs = static_cast<float>(ofGetElapsedTimeMicros() - startMicros) / 1000.0f;
		return false; // need a second frame before a difference means anything
	}

	diffGray.absDiff(prevGray, currentGray);
	diffGray.threshold(ofClamp(config.threshold, 0, 255));

	for (int i = 0; i < std::max(0, config.erodeIterations); i++) {
		diffGray.erode();
	}
	for (int i = 0; i < std::max(0, config.dilateIterations); i++) {
		diffGray.dilate();
	}

	float analysisArea = static_cast<float>(allocatedW) * static_cast<float>(allocatedH);
	int minAreaPx = std::max(1, static_cast<int>(config.minBlobAreaNormalized * analysisArea));
	int maxAreaPx = std::max(minAreaPx + 1, static_cast<int>(config.maxBlobAreaNormalized * analysisArea));
	int maxBlobs = std::max(1, config.maxBlobs);

	// nConsidered (maxBlobs) caps how many blobs findContours keeps
	// (largest-area first) — this is where "Limit maximum detections" is
	// actually enforced, not a separate post-filter.
	contourFinder.findContours(diffGray, minAreaPx, maxAreaPx, maxBlobs, false /*bFindHoles*/, true);

	detections.clear();
	detections.reserve(contourFinder.blobs.size());
	for (const ofxCvBlob & blob : contourFinder.blobs) {
		BlobDetection d;
		d.normalizedBounds = ofRectangle(
			blob.boundingRect.x / static_cast<float>(allocatedW),
			blob.boundingRect.y / static_cast<float>(allocatedH),
			blob.boundingRect.width / static_cast<float>(allocatedW),
			blob.boundingRect.height / static_cast<float>(allocatedH));
		d.normalizedCentroid = glm::vec2(
			blob.centroid.x / static_cast<float>(allocatedW),
			blob.centroid.y / static_cast<float>(allocatedH));
		d.normalizedArea = blob.area / analysisArea;
		d.confidence = 1.0f;
		detections.push_back(d);
	}

	prevGray = currentGray;

	lastProcessingTimeMs = static_cast<float>(ofGetElapsedTimeMicros() - startMicros) / 1000.0f;
	return true;
}
