#pragma once

#include <string>
#include <vector>
#include "ofImage.h"
#include "ofRectangle.h"

// Crossfades between static images from a folder on its own timer. Lazily
// keeps only current+next ofImage resident — never the whole folder —
// matching VideoSampler's "only one thing decoding at a time" discipline,
// for Pi 3B memory safety. No existing static-image-folder precedent
// exists anywhere in this codebase to match (confirmed via grep — zero
// ofImage/ofLoadImage hits in shared/src or any sketch); this mirrors
// TFParameterPanel::rescanPresets()'s ofDirectory scan idiom instead.
class TFImageCycler {
	public:
		struct Params {
			float cycleInterval = 10.0f; // total time each image is "current", including its fade-out
			float fadeDuration = 2.0f; // crossfade window at the end of each interval
		};

		void setup(const std::string& folderPath);
		void setParams(const Params& p) { params = p; }
		void update(float dt);
		bool hasImages() const { return !imagePaths.empty(); }

		void draw(const ofRectangle& destRect, float alpha = 1.0f);

	private:
		void advance();

		std::vector<std::string> imagePaths;
		Params params;
		int currentIndex = -1;
		int nextIndex = -1;
		ofImage currentImg;
		ofImage nextImg;
		float holdTimer = 0.0f;
};
