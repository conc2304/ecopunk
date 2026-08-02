#pragma once

#include <string>
#include <vector>
#include "ofImage.h"
#include "ofRectangle.h"

// Crossfades between static images from a folder on its own timer. Lazily
// keeps only current+next ofImage resident, matching TimeOffsetVideoBuffer's
// "only what's actually shown lives on the GPU" discipline. Copied from
// temporal-fields/src/TFImageCycler.h — trimmed the _tint/_mask filename
// exclusion, since fragment-trail's own bin/data/backgrounds/ folder isn't
// shared with any ambient-texture system (temporal-fields' backgrounds/
// folder doubles as TFAmbientTextureLayer's asset source; nothing here does).
class FTImageCycler {
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
