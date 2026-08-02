#include "FTImageCycler.h"
#include "FTTextureCropFill.h"
#include "ofFileUtils.h"
#include "ofLog.h"
#include "ofMath.h"
#include <algorithm>
#include <utility>

void FTImageCycler::setup(const std::string& folderPath) {
	ofDirectory dir;
	dir.allowExt("jpg");
	dir.allowExt("jpeg");
	dir.allowExt("png");
	dir.listDir(folderPath);
	dir.sort();

	imagePaths.clear();
	for (const auto& file : dir.getFiles()) {
		imagePaths.push_back(file.getAbsolutePath());
	}

	currentIndex = -1;
	nextIndex = -1;
	holdTimer = 0.0f;

	if (imagePaths.empty()) {
		ofLogNotice("FTImageCycler") << "no images found in " << folderPath;
		return;
	}

	currentIndex = 0;
	currentImg.load(imagePaths[currentIndex]);

	if (imagePaths.size() > 1) {
		nextIndex = 1;
		nextImg.load(imagePaths[nextIndex]);
	}

	ofLogNotice("FTImageCycler") << "found " << imagePaths.size() << " image(s) in " << folderPath;
}

void FTImageCycler::update(float dt) {
	if (imagePaths.empty()) {
		return;
	}
	holdTimer += dt;
	if (holdTimer >= params.cycleInterval) {
		advance();
	}
}

void FTImageCycler::advance() {
	if (imagePaths.size() < 2) {
		holdTimer = 0.0f;
		return;
	}

	currentIndex = nextIndex;
	currentImg = std::move(nextImg);

	nextIndex = (currentIndex + 1) % static_cast<int>(imagePaths.size());
	nextImg.load(imagePaths[nextIndex]);

	holdTimer = 0.0f;
}

void FTImageCycler::draw(const ofRectangle& destRect, float alpha) {
	if (currentIndex < 0 || !currentImg.isAllocated()) {
		return;
	}

	float fadeStart = std::max(0.0f, params.cycleInterval - params.fadeDuration);
	float fadeProgress = params.fadeDuration > 0.0f
		? ofClamp((holdTimer - fadeStart) / params.fadeDuration, 0.0f, 1.0f)
		: 0.0f;

	if (fadeProgress > 0.0f && nextImg.isAllocated()) {
		ftDrawTextureCroppedToFill(nextImg.getTexture(), destRect, alpha);
	}
	ftDrawTextureCroppedToFill(currentImg.getTexture(), destRect, alpha * (1.0f - fadeProgress));
}
