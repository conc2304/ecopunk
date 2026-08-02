#include "FTBackgroundLayer.h"
#include "FTTextureCropFill.h"

void FTBackgroundLayer::setup(TimeOffsetVideoBuffer* videoBuffer_, const std::string& imagesFolder, int canvasW_, int canvasH_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;

	imageCycler.setup(imagesFolder);

	// Full-canvas ambient widget. background.a MUST stay 0 here: this
	// widget spans the whole screen, and HudFrameRenderer::draw() fills its
	// full bounds with theme.colors.background whenever alpha > 0 —
	// UNCONDITIONALLY, regardless of frame style (see that file's
	// `if (colors.background.a > 0)` block, which runs before the style
	// switch and was the actual cause of fragment-trail's earlier
	// "video renders as a flat fill" bug once alpha there was too high).
	// A full-canvas nonzero background here would blot out the video/image
	// sections drawn just before it. showFrame is also off — a corner-bracket
	// border around the entire screen isn't wanted, just the hex cells.
	hud::HudTheme bgTheme;
	bgTheme.colors.secondary = ofColor(103, 255, 142, 90);
	bgTheme.colors.muted = ofColor(124, 232, 230, 40);
	bgTheme.colors.background = ofColor(0, 0, 0, 0);
	bgTheme.frame.showFrame = false;
	bgTheme.frame.showTicks = false;
	bgTheme.additive = false;
	hexGrid.setTheme(bgTheme);
	hexGrid.setBounds(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH));
	hud::HexGridOptions hexOpts;
	hexOpts.cellSize = 34.0f; // larger than the widget's small-panel default — reads better at full-canvas scale
	hexOpts.activation = 0.10f; // sparse — stays an ambient texture, not a focal element
	hexGrid.setOptions(hexOpts);

	bgShader.load("shaders/vert.glsl", "shaders/bg_dim.glsl");

	pickNextAltSection();
}

void FTBackgroundLayer::drawVideoDimmed(const ofTexture& tex, const ofRectangle& destRect) {
	if (!tex.isAllocated()) {
		return;
	}
	ofRectangle src = ftComputeCropFillSrcRect(tex.getWidth(), tex.getHeight(), destRect);

	bgShader.begin();
	bgShader.setUniformTexture("tex", tex, 0);
	bgShader.setUniform1f("saturation", kVideoSaturation);
	bgShader.setUniform1f("opacity", kVideoAlpha);
	ofSetColor(255);
	tex.drawSubsection(destRect.x, destRect.y, destRect.width, destRect.height, src.x, src.y, src.width, src.height);
	bgShader.end();
}

void FTBackgroundLayer::resizeCanvas(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
	hexGrid.setBounds(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH));
}

void FTBackgroundLayer::pickNextAltSection() {
	altSectionShowsImage = imageCycler.hasImages() && ofRandom(1.0f) < 0.5f;
	splitIsVertical = ofRandom(1.0f) < 0.5f;
	splitRatio = ofRandom(0.35f, 0.65f);
}

void FTBackgroundLayer::update(float dt) {
	imageCycler.update(dt);
	hexGrid.update(dt);

	sectionTimer += dt;
	if (sectionTimer >= sectionInterval) {
		sectionTimer = 0.0f;
		pickNextAltSection();
	}
}

void FTBackgroundLayer::draw() {
	if (videoBuffer == nullptr || !videoBuffer->hasMedia()) {
		return;
	}

	// GL blend state persists across frames/draw calls; FTFragmentPool::draw()
	// disables it again at the end of every frame, so by the time this next
	// frame's draw() runs (first thing in ofApp::draw(), before pool.draw()
	// re-enables it), it would otherwise still be off — silently dropping the
	// alpha component below and drawing kVideoAlpha fully opaque instead of
	// dimmed. Same class of bug as the fragment-content flat-fill issue.
	ofEnableAlphaBlending();

	float w = static_cast<float>(canvasW);
	float h = static_cast<float>(canvasH);
	const ofTexture& videoTex = videoBuffer->getRawVideoTexture();

	// Only actually split the canvas when there's a real background image to
	// put in the second section — two independently crop-to-fill'd rects
	// pulling from the SAME video source produces a duplicated/mirrored-
	// looking result (each rect centers its own crop on the same source
	// frame), not a meaningful split. Until bin/data/backgrounds/ has real
	// images, this degrades to one full-canvas dimmed video draw.
	if (altSectionShowsImage && imageCycler.hasImages()) {
		ofRectangle rectA;
		ofRectangle rectB;
		if (splitIsVertical) {
			float splitX = w * splitRatio;
			rectA = ofRectangle(0, 0, splitX, h);
			rectB = ofRectangle(splitX, 0, w - splitX, h);
		} else {
			float splitY = h * splitRatio;
			rectA = ofRectangle(0, 0, w, splitY);
			rectB = ofRectangle(0, splitY, w, h - splitY);
		}
		drawVideoDimmed(videoTex, rectA);
		imageCycler.draw(rectB, kVideoAlpha);
	} else {
		drawVideoDimmed(videoTex, ofRectangle(0, 0, w, h));
	}

	hexGrid.draw();

	ofDisableAlphaBlending();
}
