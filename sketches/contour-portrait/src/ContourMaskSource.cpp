#include "ContourMaskSource.h"
#include <algorithm>

void ContourMaskSource::setup(int workingWidth, int workingHeight) {
	width = std::max(workingWidth, 8);
	height = std::max(workingHeight, 8);
	colorImg.allocate(width, height);
	grayImg.allocate(width, height);
	bgImg.allocate(width, height);
	diffImg.allocate(width, height);
	bgCaptured = false;
}

bool ContourMaskSource::loadResizedGray(ofTexture & tex, bool isColorSource) {
	if (!tex.isAllocated()) return false;
	tex.readToPixels(tmpPixels);
	if (!tmpPixels.isAllocated()) return false;
	tmpPixels.resize(width, height);

	// ofxCvImage::setFromPixels(const ofPixels&) handles channel
	// conversion internally, so this works whether the source texture is
	// RGB/RGBA (video/camera) or single-channel (an already-grayscale mask).
	if (isColorSource) {
		colorImg.setFromPixels(tmpPixels);
		grayImg.setFromColorImage(colorImg);
	} else {
		grayImg.setFromPixels(tmpPixels);
	}
	return true;
}

void ContourMaskSource::captureBackground(ofTexture & source) {
	if (!loadResizedGray(source, true)) return;
	bgImg = grayImg;
	bgCaptured = true;
}

void ContourMaskSource::updateFromLiveDiff(ofTexture & source, int thresholdValue, int minAreaPct, int maxAreaPct) {
	if (!bgCaptured) {
		polygon.clear();
		return;
	}
	if (!loadResizedGray(source, true)) {
		polygon.clear();
		return;
	}
	diffImg.absDiff(bgImg, grayImg);
	diffImg.threshold(thresholdValue);
	extractLargestBlob(diffImg, minAreaPct, maxAreaPct);
}

void ContourMaskSource::updateFromMaskTexture(ofTexture & mask, int thresholdValue) {
	if (!loadResizedGray(mask, true)) {
		polygon.clear();
		return;
	}
	grayImg.threshold(thresholdValue);
	extractLargestBlob(grayImg, 0, 100);
}

void ContourMaskSource::extractLargestBlob(ofxCvGrayscaleImage & binaryImg, int minAreaPct, int maxAreaPct) {
	int totalPx = width * height;
	int minArea = std::max(1, totalPx * std::max(minAreaPct, 0) / 100);
	int maxArea = std::max(minArea + 1, totalPx * std::min(std::max(maxAreaPct, 1), 100) / 100);

	contourFinder.findContours(binaryImg, minArea, maxArea, 1, false);
	if (contourFinder.blobs.empty()) {
		polygon.clear();
		return;
	}

	const auto & blob = contourFinder.blobs[0];
	polygon.clear();
	polygon.reserve(blob.pts.size());
	for (auto & p : blob.pts) {
		polygon.push_back(glm::vec2(p.x / (float)width, p.y / (float)height));
	}
}
