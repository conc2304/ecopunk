#include "ofApp.h"
#include "BESettings.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofSetFrameRate(TARGET_FPS);

	grid.setup(GRID_COLS, GRID_ROWS, CANVAS_W, CANVAS_H);
	annotations.setup(&grid, DIVIDER_COL, CANVAS_W, CANVAS_H);
	annotations.loadCodeFont(CODE_FONT_PATH, SIZE_CODE);
	annotations.setMeasurementLineTiming(MLINE_DRAW_SPEED, MLINE_FADE_DELAY, MLINE_FADE_OPACITY);
	annotations.setCodeTextTiming(CODE_TEXT_INTERVAL_MIN, CODE_TEXT_INTERVAL_MAX, CODE_TEXT_OPACITY_MIN, CODE_TEXT_OPACITY_MAX);
	annotations.setCodeFragments(loadCodeFragments());

	Fragment::loadFragmentShader("shaders/fragmentEffects.vert", "shaders/fragmentEffects.frag");
	videoSampler.setup(MEDIA_PATH);

	composition.setupBE(&grid, &videoSampler, CANVAS_W, CANVAS_H, DIVIDER_COL);
	composition.setOnFragmentPlaced([this](Fragment* newFrag, Fragment* nearest){
		annotations.onFragmentPlaced(newFrag, nearest);
	});
	composition.setOnCycleStart([this](){
		videoSampler.cancelPending(); // drop in-flight captures before fragments.clear() destroys their targets
		videoSampler.selectVideoForCycle();
		annotations.reset();
	});
	composition.startCycle();
}

//--------------------------------------------------------------
std::vector<std::string> ofApp::loadCodeFragments() const{
	std::vector<std::string> lines;
	ofBuffer buffer = ofBufferFromFile("codefragments.txt");
	for(const auto& line : buffer.getLines()){
		if(!line.empty()){
			lines.push_back(line);
		}
	}
	return lines;
}

//--------------------------------------------------------------
void ofApp::update(){
	float dt = ofGetLastFrameTime();
	videoSampler.update();
	composition.update(dt);
	annotations.update(dt);
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

	annotations.drawMeasurementLines();
	annotations.drawCodeText();

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
