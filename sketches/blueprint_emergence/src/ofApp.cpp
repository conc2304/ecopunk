#include "ofApp.h"
#include "BESettings.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofSetFrameRate(TARGET_FPS);

	grid.setup(GRID_COLS, GRID_ROWS, CANVAS_W, CANVAS_H);
	annotations.setup(&grid, DIVIDER_COL, CANVAS_W, CANVAS_H);

	composition.setupBE(&grid, CANVAS_W, CANVAS_H, DIVIDER_COL);
	composition.startCycle();
}

//--------------------------------------------------------------
void ofApp::update(){
	composition.update(ofGetLastFrameTime());
}

//--------------------------------------------------------------
void ofApp::draw(){
	using Phase = CompositionBase::CyclePhase;
	Phase phase = composition.getPhase();

	if(phase == Phase::RESET_HOLD){
		ofBackground(ofColor::black);
		ofSetColor(TEXT_DIM);
		ofDrawBitmapString("fps " + ofToString(ofGetFrameRate(), 1) + "  (reset hold)", 12, 18);
		return;
	}

	float dividerProgress = 1.0f;
	float gridAlpha = 1.0f;
	bool labelShown = true;

	if(phase == Phase::BLANK){
		float elapsed = composition.getPhaseElapsed();
		dividerProgress = ofClamp(elapsed / DIVIDER_DRAW_DURATION, 0.0f, 1.0f);
		float gridElapsed = elapsed - DIVIDER_DRAW_DURATION;
		gridAlpha = ofClamp(gridElapsed / GRID_FADEIN_DURATION, 0.0f, 1.0f);
		labelShown = elapsed >= (DIVIDER_DRAW_DURATION + GRID_FADEIN_DURATION);
	}

	ofBackground(GROUND_DARK);

	float dividerX = DIVIDER_COL * grid.getCellWidth();

	ofSetColor(composition.isZoneALight() ? GROUND_LIGHT : GROUND_DARK);
	ofDrawRectangle(0, 0, dividerX, CANVAS_H);

	ofSetColor(GROUND_DARK);
	ofDrawRectangle(dividerX, 0, CANVAS_W - dividerX, CANVAS_H);

	for(const auto& fragment : composition.getFragments()){
		fragment->draw();
	}

	annotations.drawGrid(gridAlpha);
	annotations.drawDivider(dividerProgress);

	if(labelShown){
		annotations.drawCornerLabel("[0,0]");
	}

	if(showOccupancyDebug){
		drawOccupancyDebug();
	}

	ofSetColor(TEXT_DIM);
	ofDrawBitmapString("fps " + ofToString(ofGetFrameRate(), 1)
		+ "  zone a: " + (composition.isZoneALight() ? "light" : "dark")
		+ "  fragments: " + ofToString(composition.getFragments().size())
		+ "  seed: " + ofToString(composition.getCycleSeed())
		+ "  ('g' grid debug, 'r' restart cycle)", 12, 18);
}

//--------------------------------------------------------------
void ofApp::drawOccupancyDebug() const{
	ofSetColor(255, 0, 0, 90);
	for(int row = 0; row < grid.getRows(); row++){
		for(int col = 0; col < grid.getCols(); col++){
			if(grid.isOccupied(col, row)){
				ofRectangle cell = grid.cellRect(col, row);
				ofDrawRectangle(cell);
			}
		}
	}
}

//--------------------------------------------------------------
void ofApp::exit(){

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	if(key == 'g'){
		showOccupancyDebug = !showOccupancyDebug;
	} else if(key == 'r'){
		composition.startCycle();
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
