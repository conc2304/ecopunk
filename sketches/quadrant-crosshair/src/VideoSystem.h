#pragma once
#include "ofMain.h"
#include <string>
#include <vector>

class VideoSystem {
public:
	void setup(const std::string & mediaPath);
	void update();
	ofTexture & getTexture();
	glm::vec2 getVideoSize() const;
	void nextFile();

	const ofPixels & getPixels() const { return player.getPixels(); }
	bool isFrameNew() const { return player.isFrameNew(); }
	bool fileChanged() {
		bool v = _fileChanged;
		_fileChanged = false;
		return v;
	}
	const std::string & getCurrentFilename() const { return currentFilename_; }

	// ── In-code dials ──────────────────────────────────────────────────────
	float adjSaturation = 1.18f; // +18% saturation
	float adjContrast = 1.20f; // +10% contrast
	float adjBrightness = -0.10f; // -10% brightness

	int loopMin = 3; // min play-throughs before advancing to next file
	int loopMax = 5; // max play-throughs

private:
	ofVideoPlayer player;
	std::vector<std::string> files;
	int fileIndex = 0;
	std::vector<int> playlist;
	int playlistPos = 0;
	bool transitioning = false;
	bool _fileChanged = false;
	std::string currentFilename_;
	ofFbo fboAdjusted;
	ofShader adjustShader;
	bool shaderReady = false;
	int loopCount = 0;
	int targetLoops = 3;
	void allocateAdjustFbo();
	void processAdjustment();
	void loadFile(int index);
	void buildPlaylist();
	void pickTargetLoops();
};
