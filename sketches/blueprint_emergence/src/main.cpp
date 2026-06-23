#include "ofMain.h"
#include "ofApp.h"
#include "BESettings.h"

//========================================================================
int main( ){

	ofGLWindowSettings settings;
	settings.setSize(CANVAS_W, CANVAS_H);
	settings.windowMode = OF_WINDOW; //can also be OF_FULLSCREEN

	auto window = ofCreateWindow(settings);

	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();

}
