#include "ofApp.h"
#include "ContourPresets.h"

namespace {
	const int kCanvasWidth = 1280;
	const int kCanvasHeight = 720;
}

void ofApp::setup() {
	ofSetFrameRate(30);
	ofBackground(0);
	// All shaders here use sampler2D/texture2D against normalized [0,1]
	// texcoords, same reasoning as the other sketches in this repo.
	ofDisableArbTex();

	source.setup();
	effect.setup(kCanvasWidth, kCanvasHeight);

	// Optional demo mask: if bin/data/mask.png exists, wire it in via
	// setMask() so maskEnabled/Mask-driven displacement and breakup gating
	// have something real to sample. Entirely optional — the effect must
	// (and does) behave stably with activeMask left null.
	haveMaskImage = maskImage.load("mask.png");
	if (haveMaskImage) {
		effect.setMask(&maskImage.getTexture());
	}

	// v2: polygon mask pipeline runs at a small, bounded working resolution
	// independent of the main effect's own preprocessing resolution.
	maskPolySource.setup(160, 90);

	panel.setup(effect.parameters(), "settings.xml", 10, 10);

	presetCleanBtn.setup("Preset: Clean Portrait");
	presetTopographicBtn.setup("Preset: Topographic Figure");
	presetSideDissolveBtn.setup("Preset: Side Dissolve");
	presetAnalogScanBtn.setup("Preset: Analog Scan");
	presetGhostBtn.setup("Preset: Ghost Contour");
	presetSilhouetteBtn.setup("Preset: Silhouette Emergence");
	presetConvergingBtn.setup("Preset: Converging Contours");
	saveCustomBtn.setup("Save Custom Preset");
	loadCustomBtn.setup("Load Custom Preset");

	presetCleanBtn.addListener(this, &ofApp::onPresetClean);
	presetTopographicBtn.addListener(this, &ofApp::onPresetTopographic);
	presetSideDissolveBtn.addListener(this, &ofApp::onPresetSideDissolve);
	presetAnalogScanBtn.addListener(this, &ofApp::onPresetAnalogScan);
	presetGhostBtn.addListener(this, &ofApp::onPresetGhost);
	presetSilhouetteBtn.addListener(this, &ofApp::onPresetSilhouette);
	presetConvergingBtn.addListener(this, &ofApp::onPresetConverging);
	saveCustomBtn.addListener(this, &ofApp::onSaveCustom);
	loadCustomBtn.addListener(this, &ofApp::onLoadCustom);

	panel.add(&presetCleanBtn);
	panel.add(&presetTopographicBtn);
	panel.add(&presetSideDissolveBtn);
	panel.add(&presetAnalogScanBtn);
	panel.add(&presetGhostBtn);
	panel.add(&presetSilhouetteBtn);
	panel.add(&presetConvergingBtn);
	panel.add(&saveCustomBtn);
	panel.add(&loadCustomBtn);

	ContourPresets::apply(effect, "Clean Portrait");

	// Defaults to video, not the camera: this sketch (like its siblings)
	// treats a live capture device as something that may not exist on the
	// dev/deploy machine, and prefers the shared clip pool that's already
	// wired into every other sketch in this repo (see applyInputMode()).
	effect.parameters().getGroup("Input").getInt("Input Mode (0=Img 1=Vid 2=Cam)") = ContourSource::INPUT_VIDEO;
	applyInputMode(ContourSource::INPUT_VIDEO);
}

void ofApp::applyInputMode(int mode) {
	lastAppliedInputMode = mode;
	if (mode == ContourSource::INPUT_IMAGE) {
		source.applyMode(ContourSource::INPUT_IMAGE, ofToDataPath("media/sample.jpg", true), "", 0);
	} else if (mode == ContourSource::INPUT_VIDEO) {
		// Sketch-local bin/data/media/ wins if the user drops a clip there
		// (e.g. a real portrait/silhouette test clip); otherwise falls back
		// to the shared clip pool every other sketch in this repo already
		// symlinks in (bin/data/sharedMedia -> blueprint_emergence's media
		// folder) so this sketch has something to play out of the box.
		ofDirectory localMedia(ofToDataPath("media", true));
		bool localHasClips = localMedia.exists() && localMedia.listDir() > 0;
		std::string dir = localHasClips ? ofToDataPath("media", true) : ofToDataPath("sharedMedia", true);
		source.applyMode(ContourSource::INPUT_VIDEO, "", dir, 0);
	} else {
		source.applyMode(ContourSource::INPUT_CAMERA, "", "", 0);
	}
}

std::string ofApp::presetsDir() const {
	return ofToDataPath("presets/", true);
}

void ofApp::onPresetClean() { ContourPresets::apply(effect, "Clean Portrait"); }
void ofApp::onPresetTopographic() { ContourPresets::apply(effect, "Topographic Figure"); }
void ofApp::onPresetSideDissolve() { ContourPresets::apply(effect, "Side Dissolve"); }
void ofApp::onPresetAnalogScan() { ContourPresets::apply(effect, "Analog Scan"); }
void ofApp::onPresetGhost() { ContourPresets::apply(effect, "Ghost Contour"); }
void ofApp::onPresetSilhouette() { ContourPresets::apply(effect, "Silhouette Emergence"); }
void ofApp::onPresetConverging() { ContourPresets::apply(effect, "Converging Contours"); }

void ofApp::onSaveCustom() {
	ofDirectory::createDirectory(presetsDir(), true, true);
	std::string filename = presetsDir() + "custom_" + ofGetTimestampString("%Y%m%d_%H%M%S") + ".xml";
	panel.saveToFile(filename);
	ofLogNotice("ofApp") << "saved preset: " << filename;
}

void ofApp::onLoadCustom() {
	ofDirectory dir(presetsDir());
	if (!dir.exists()) {
		ofLogWarning("ofApp") << "no presets/ directory yet -- save one first";
		return;
	}
	dir.allowExt("xml");
	dir.sort();
	if (dir.size() == 0) {
		ofLogWarning("ofApp") << "no custom presets found in " << presetsDir();
		return;
	}
	// Loads the most recently saved custom preset. A production build would
	// expose a list; this sketch keeps the control surface to what the
	// brief asks for (save/load, not a preset browser).
	std::string filename = dir.getFile(dir.size() - 1).getAbsolutePath();
	panel.loadFromFile(filename);
	ofLogNotice("ofApp") << "loaded preset: " << filename;
}

void ofApp::update() {
	int wantedMode = effect.parameters().getGroup("Input").getInt("Input Mode (0=Img 1=Vid 2=Cam)").get();
	if (wantedMode != lastAppliedInputMode) {
		applyInputMode(wantedMode);
	}

	source.update();
	updateMaskPolygon();
	effect.update(source.getTexture(), ofGetLastFrameTime());
}

void ofApp::updateMaskPolygon() {
	auto & maskG = effect.parameters().getGroup("Mask");
	int wantedSource = maskG.getInt("Mask Source (0=None 1=BgSubtract 2=ExtTexture)").get();

	// Background subtraction needs a captured reference frame; re-arm it
	// automatically on the frame the mode is first selected or the input
	// source changes, rather than requiring 'b' every time (still available
	// for manually re-capturing after the scene changes).
	if (wantedSource == ContourDisplacementEffect::MASK_SOURCE_BACKGROUND_SUBTRACT
		&& (wantedSource != lastAppliedMaskSource || !maskPolySource.hasBackground())
		&& source.isAvailable()) {
		maskPolySource.captureBackground(source.getTexture());
	}
	lastAppliedMaskSource = wantedSource;

	const int thresholdValue = 40; // fixed working values -- not exposed as
	const int minAreaPct = 1;      // GUI params to keep the Mask group to
	const int maxAreaPct = 90;     // what the v2 brief actually lists

	if (wantedSource == ContourDisplacementEffect::MASK_SOURCE_BACKGROUND_SUBTRACT && source.isAvailable()) {
		maskPolySource.updateFromLiveDiff(source.getTexture(), thresholdValue, minAreaPct, maxAreaPct);
		effect.setMaskPolygon(maskPolySource.getPolygon());
	} else if (wantedSource == ContourDisplacementEffect::MASK_SOURCE_EXTERNAL_TEXTURE && haveMaskImage) {
		maskPolySource.updateFromMaskTexture(maskImage.getTexture(), thresholdValue);
		effect.setMaskPolygon(maskPolySource.getPolygon());
	} else if (wantedSource == ContourDisplacementEffect::MASK_SOURCE_NONE) {
		effect.setMaskPolygon({});
	}
	// else: a source mode is selected but its prerequisite isn't ready yet
	// (no video source available yet, or no mask.png loaded) -- leave
	// whatever polygon is already set alone rather than clearing it every
	// single frame while waiting.
}

void ofApp::draw() {
	ofBackground(0);
	effect.draw(ofRectangle(0, 0, ofGetWidth(), ofGetHeight()));

	drawDebugViews();

	if (showOverlay) {
		std::stringstream ss;
		ss << "fps: " << ofGetFrameRate() << "\n";
		ss << "vertices: " << effect.getVertexCount() << "\n";
		ss << "source available: " << (source.isAvailable() ? "yes" : "no") << "\n";
		ss << "[i] cycle input mode   [m] toggle mask   [b] capture bg for mask   [g] toggle gui   [f] toggle fps overlay\n";
		ofSetColor(255);
		ofDrawBitmapStringHighlight(ss.str(), 20, ofGetHeight() - 90);
	}

	if (effect.parameters().getGroup("Debug").getBool("Show Gui")) {
		panel.draw();
	}
}

void ofApp::drawDebugViews() {
	auto & debug = effect.parameters().getGroup("Debug");
	int x = ofGetWidth() - 220;
	int y = 10;
	int w = 200, h = 112;

	ofPushStyle();
	if (debug.getBool("Show Source Debug")) {
		ofSetColor(255);
		effect.getSourceDebugTexture().draw(x, y, w, h);
		ofNoFill();
		ofSetColor(255, 255, 0);
		ofDrawRectangle(x, y, w, h);
		ofFill();
		y += h + 8;
	}
	if (debug.getBool("Show Processed Debug")) {
		ofSetColor(255);
		effect.getProcessedDebugTexture().draw(x, y, w, h);
		ofNoFill();
		ofSetColor(0, 255, 255);
		ofDrawRectangle(x, y, w, h);
		ofFill();
		y += h + 8;
	}
	if (debug.getBool("Show Mask Debug")) {
		ofSetColor(255);
		effect.getMaskDebugTexture().draw(x, y, w, h);
		ofNoFill();
		ofSetColor(255, 0, 255);
		ofDrawRectangle(x, y, w, h);
		ofFill();
		y += h + 8;
	}
	if (debug.getBool("Show Color Debug")) {
		ofSetColor(255);
		effect.getColorDebugTexture().draw(x, y, w, h);
		ofNoFill();
		ofSetColor(255, 128, 0);
		ofDrawRectangle(x, y, w, h);
		ofFill();
		y += h + 8;
	}
	ofPopStyle();

	if (debug.getBool("Show Fps")) {
		ofSetColor(255);
		ofDrawBitmapStringHighlight(ofToString(ofGetFrameRate(), 1) + " fps", 20, 20);
	}
}

void ofApp::keyPressed(int key) {
	if (key == 'i') {
		int mode = (lastAppliedInputMode + 1) % 3;
		effect.parameters().getGroup("Input").getInt("Input Mode (0=Img 1=Vid 2=Cam)") = mode;
	}
	if (key == 'm') {
		auto & p = effect.parameters().getGroup("Mask").getBool("Mask Enabled");
		p = !p.get();
	}
	if (key == 'b') {
		if (source.isAvailable()) {
			maskPolySource.captureBackground(source.getTexture());
			ofLogNotice("ofApp") << "captured background reference for mask background-subtraction";
		}
	}
	if (key == 'g') showOverlay = !showOverlay;
	if (key == 'f') {
		auto & p = effect.parameters().getGroup("Debug").getBool("Show Fps");
		p = !p.get();
	}
}

void ofApp::windowResized(int w, int h) {
	(void)w;
	(void)h;
	// Logical canvas stays fixed (kCanvasWidth x kCanvasHeight); draw()
	// scales it to fill whatever the window/projection surface is now, same
	// as this sketch's siblings.
}
