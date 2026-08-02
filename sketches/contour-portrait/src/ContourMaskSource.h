#pragma once
#include "ofMain.h"
#include "ofxOpenCv.h"

// Produces the normalized-canvas-space silhouette polygon that
// ContourDisplacementEffect::setMaskPolygon() consumes for v2 Capability 1
// (mask-clipped line existence). Two independent paths, matching the
// brief's `maskSource` options:
//
//   - updateFromLiveDiff(): background subtraction (ofxCvGrayscaleImage::
//     absDiff + threshold) against a captured reference frame, for a
//     static-camera live setup. Call captureBackground() once against an
//     empty-scene frame before the first updateFromLiveDiff().
//   - updateFromMaskTexture(): threshold + contour trace directly on an
//     already-supplied raster mask/silhouette texture (e.g. a static
//     image's own thresholded silhouette, or a foreground mask computed
//     elsewhere) -- no background reference needed, safe to call once and
//     hold static for still-image mode.
//
// Both funnel through ofxCvContourFinder, taking blobs[0] (largest blob)
// as the boundary -- same as the brief's own algorithm description.
// Genuinely disjoint regions (e.g. a hand separated from the torso by open
// space) are a separate, smaller blob and are not merged in; only a
// concave/connected shape (e.g. a raised arm still joined to the body) is
// represented by the one polygon.
//
// Runs its own small, bounded working-resolution pipeline (readback +
// OpenCV ops), independent of ContourDisplacementEffect's own
// preprocessing -- same "one small CPU pass per frame" discipline as the
// rest of this sketch, not a second unbounded cost source.
class ContourMaskSource {
public:
	void setup(int workingWidth, int workingHeight);

	void captureBackground(ofTexture & source);
	bool hasBackground() const { return bgCaptured; }

	void updateFromLiveDiff(ofTexture & source, int thresholdValue, int minAreaPct, int maxAreaPct);
	void updateFromMaskTexture(ofTexture & mask, int thresholdValue);

	bool hasPolygon() const { return !polygon.empty(); }
	const std::vector<glm::vec2> & getPolygon() const { return polygon; }

private:
	void extractLargestBlob(ofxCvGrayscaleImage & binaryImg, int minAreaPct, int maxAreaPct);
	bool loadResizedGray(ofTexture & tex, bool isColorSource);

	int width = 0, height = 0;
	ofxCvColorImage colorImg;
	ofxCvGrayscaleImage grayImg, bgImg, diffImg;
	ofxCvContourFinder contourFinder;
	std::vector<glm::vec2> polygon; // normalized [0,1], canvas space
	bool bgCaptured = false;
	ofPixels tmpPixels;
};
