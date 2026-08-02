#include "ofMain.h"
#include "ofApp.h"

int main() {
	ofGLWindowSettings settings;
	settings.setSize(1280, 720);
	settings.windowMode = OF_WINDOW;
	ofCreateWindow(settings);

	ofSetFrameRate(24); // Pi 3B render target — see brief Section 7

	return ofRunApp(new ofApp());
}
