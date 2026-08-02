#include "ContourSource.h"
#include <cstdlib>

#ifdef __APPLE__
	#import <Foundation/Foundation.h>
#endif

void ContourSource::setup() {
	ofPixels px;
	px.allocate(4, 4, OF_PIXELS_RGB);
	px.setColor(ofColor::black);
	blankTex.loadData(px);
}

ContourSource::~ContourSource() {
	// Best-effort: if a prior camera setup() attempt threw (see applyMode()
	// below), the underlying AVCaptureSession can be left mid
	// beginConfiguration/commitConfiguration, and closing it then throws
	// too -- including from ofVideoGrabber's own destructor at process
	// exit, outside any of this class's try/catch blocks. Guarding the
	// explicit close() here can't reach into that destructor, but it does
	// stop *this* class from adding a second throw on top during teardown.
#ifdef __APPLE__
	@try {
		closeAll();
	} @catch (NSException * ex) {
		ofLogError("ContourSource") << "camera teardown threw during shutdown (see README known limitations): "
									 << [[ex reason] UTF8String];
	}
#else
	closeAll();
#endif
}

void ContourSource::closeAll() {
	if (video.isLoaded()) video.close();
	if (grabber.isInitialized()) grabber.close();
	available = false;
}

void ContourSource::applyMode(int mode, const std::string & imagePath, const std::string & videoDir, int cameraDeviceId) {
	bool modeChanged = (mode != currentMode);
	bool imageChanged = (mode == INPUT_IMAGE) && (imagePath != currentImagePath || modeChanged);
	bool videoChanged = (mode == INPUT_VIDEO) && (videoDir != currentVideoPath || modeChanged);
	bool cameraChanged = (mode == INPUT_CAMERA) && (cameraDeviceId != currentCameraId || modeChanged);

	if (!imageChanged && !videoChanged && !cameraChanged) return;

	closeAll();
	currentMode = mode;
	currentImagePath = imagePath;
	currentVideoPath = videoDir;
	currentCameraId = cameraDeviceId;

	if (mode == INPUT_IMAGE) {
		if (imagePath.empty()) {
			available = false;
			return;
		}
		available = image.load(imagePath);
		if (!available) {
			ofLogError("ContourSource") << "failed to load image: " << imagePath;
		}
	} else if (mode == INPUT_VIDEO) {
		// videoDir is a folder to scan for a clip, same convention as
		// VideoSystem/RPVideoSampler elsewhere in this repo (one shared
		// media pool symlinked into each sketch's bin/data, rather than a
		// single hardcoded filename).
		if (videoDir.empty()) {
			available = false;
			return;
		}
		ofDirectory dir(videoDir);
		if (!dir.exists()) {
			ofLogError("ContourSource") << "media directory not found: " << videoDir;
			available = false;
			return;
		}
		dir.allowExt("mp4");
		dir.listDir();
		if (dir.size() == 0) {
			ofLogError("ContourSource") << "no .mp4 files found in " << videoDir;
			available = false;
			return;
		}
		int pick = (int)ofRandom((float)dir.size());
		std::string file = dir.getPath(pick);
		available = video.load(file);
		if (available) {
			video.setLoopState(OF_LOOP_NORMAL);
			video.setVolume(0);
			video.play();
		} else {
			ofLogError("ContourSource") << "failed to load video: " << file;
		}
	} else if (mode == INPUT_CAMERA) {
		// ofVideoGrabber::setup() can throw an uncaught AVFoundation/ObjC
		// exception on macOS -- not just when zero devices are listed, but
		// also when the OS can't hand the process a real capture input for
		// a listed device (e.g. camera permission not granted to this
		// process). That's a genuine crash-the-whole-app risk the rest of
		// oF's video API doesn't have, so it's guarded on both sides: a
		// device-count check before calling setup() (cheap, avoids the
		// common case), and an Objective-C exception handler around the
		// call itself (this file compiles as Objective-C++, like every
		// .cpp in an oF macOS project) so a permission/driver failure
		// degrades to "no source" instead of taking the process down.
		if (cameraKnownBroken) {
			ofLogError("ContourSource") << "camera setup previously failed on this run -- not retrying "
										 << "(see README known limitations)";
			available = false;
			return;
		}
		auto devices = grabber.listDevices();
		if (devices.empty()) {
			ofLogError("ContourSource") << "no camera devices available";
			available = false;
			return;
		}
		grabber.setDeviceID(cameraDeviceId);
		grabber.setDesiredFrameRate(30);
#ifdef __APPLE__
		@try {
			grabber.setup(1280, 720);
			available = grabber.isInitialized();
		} @catch (NSException * ex) {
			ofLogError("ContourSource") << "camera setup threw an exception (likely a permissions/driver "
										 << "issue, not a code bug): " << [[ex reason] UTF8String];
			available = false;
			cameraKnownBroken = true;
		}
#else
		grabber.setup(1280, 720);
		available = grabber.isInitialized();
#endif
		if (!available) {
			ofLogError("ContourSource") << "failed to open camera device " << cameraDeviceId;
		}
	}
}

void ContourSource::update() {
	if (currentMode == INPUT_VIDEO && video.isLoaded()) {
		video.update();
	} else if (currentMode == INPUT_CAMERA && grabber.isInitialized()) {
		grabber.update();
	}
}

ofTexture & ContourSource::getTexture() {
	if (currentMode == INPUT_IMAGE && image.isAllocated()) return image.getTexture();
	if (currentMode == INPUT_VIDEO && video.isLoaded()) return video.getTexture();
	if (currentMode == INPUT_CAMERA && grabber.isInitialized()) return grabber.getTexture();
	return blankTex;
}
