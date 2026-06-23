#include "ofApp.h"
#include "BESettings.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofSetFrameRate(TARGET_FPS);

	grid.setup(GRID_COLS, GRID_ROWS, CANVAS_W, CANVAS_H);
	annotations.setup(&grid, DIVIDER_COL, CANVAS_W, CANVAS_H);

	rollZoneVariant();
}

//--------------------------------------------------------------
void ofApp::rollZoneVariant(){
	zoneALight = ofRandom(1.0f) < 0.3f; // §06: Zone A uses GROUND_LIGHT in 30% of cycles
	blankPhaseStart = ofGetElapsedTimef();
}

//--------------------------------------------------------------
void ofApp::update(){
	float elapsed = ofGetElapsedTimef() - blankPhaseStart;

	dividerProgress = ofClamp(elapsed / DIVIDER_DRAW_DURATION, 0.0f, 1.0f);

	float gridElapsed = elapsed - DIVIDER_DRAW_DURATION;
	gridAlpha = ofClamp(gridElapsed / GRID_FADEIN_DURATION, 0.0f, 1.0f);

	labelShown = elapsed >= (DIVIDER_DRAW_DURATION + GRID_FADEIN_DURATION);
}

//--------------------------------------------------------------
void ofApp::draw(){
	ofBackground(GROUND_DARK);

	float dividerX = DIVIDER_COL * grid.getCellWidth();

	ofSetColor(zoneALight ? GROUND_LIGHT : GROUND_DARK);
	ofDrawRectangle(0, 0, dividerX, CANVAS_H);

	ofSetColor(GROUND_DARK);
	ofDrawRectangle(dividerX, 0, CANVAS_W - dividerX, CANVAS_H);

	annotations.drawGrid(gridAlpha);
	annotations.drawDivider(dividerProgress);

	if(labelShown){
		annotations.drawCornerLabel("[0,0]");
	}

	ofSetColor(TEXT_DIM);
	ofDrawBitmapString("fps " + ofToString(ofGetFrameRate(), 1) + "  zone a: " +
		(zoneALight ? "light" : "dark") + "  ('z' to re-roll)", 12, 18);
}

//--------------------------------------------------------------
void ofApp::exit(){

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	if(key == 'z'){
		rollZoneVariant();
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
