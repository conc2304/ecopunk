#pragma once
#include "ofMain.h"

// Thin wrapper around the three input kinds the effect needs to be able to
// take (Primary Goal: "video, prerecorded video input" + validation list's
// static image / camera stream). Owns exactly one live decoder at a time —
// switching inputMode tears down whichever of ofImage/ofVideoPlayer/
// ofVideoGrabber was active and (re)loads the new one, same one-decoder-at-
// a-time discipline as shared/src/VideoSampler.
class ContourSource {
public:
	enum Mode { INPUT_IMAGE = 0, INPUT_VIDEO = 1, INPUT_CAMERA = 2 };

	~ContourSource();

	void setup();
	void update();

	// Applies mode/path changes queued via the GUI. Cheap to call every
	// frame; only does work when something actually changed. videoDir is a
	// folder scanned for a random .mp4 to play (see ContourSource.cpp).
	void applyMode(int mode, const std::string & imagePath, const std::string & videoDir, int cameraDeviceId);

	bool isAvailable() const { return available; }
	ofTexture & getTexture();

private:
	int currentMode = -1;
	std::string currentImagePath;
	std::string currentVideoPath;
	int currentCameraId = -1;

	ofImage image;
	ofVideoPlayer video;
	ofVideoGrabber grabber;

	bool available = false;
	ofTexture blankTex;

	// Latched true the first time ofVideoGrabber::setup() throws (see
	// ContourSource.cpp) so a flaky camera/permissions setup on this
	// process doesn't get retried every time the mode flips back to camera.
	bool cameraKnownBroken = false;

	void closeAll();
};
