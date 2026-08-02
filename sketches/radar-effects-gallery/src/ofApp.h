#pragma once

#include "ofMain.h"
#include "HudElements.h"
#include "RPVideoSampler.h"
#include "RPRevealMask.h"
#include "GalleryCompositor.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

private:
	RPVideoSampler videoSampler;
	RPRevealMask revealMask;
	GalleryCompositor compositor;
	hud::PulseEmitterWidget pulseEmitter;

	bool showOverlay = true;
};
