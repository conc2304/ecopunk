#pragma once

#include "BlueprintSceneCore.h"
#include "VideoPlaybackService.h"
#include "HudOverlayLayer.h"
#include "HudOverlayDialPanel.h"
#include "HudOverlayDialState.h"
#include "ofMain.h"

class ofApp : public ofBaseApp {

public:
	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;

	void keyPressed(int key) override;
	void keyReleased(int key) override;
	void mouseMoved(int x, int y) override;
	void mouseDragged(int x, int y, int button) override;
	void mousePressed(int x, int y, int button) override;
	void mouseReleased(int x, int y, int button) override;
	void mouseScrolled(int x, int y, float scrollX, float scrollY) override;
	void mouseEntered(int x, int y) override;
	void mouseExited(int x, int y) override;
	void windowResized(int w, int h) override;
	void dragEvent(ofDragInfo dragInfo) override;
	void gotMessage(ofMessage msg) override;

private:
	void drawOccupancyDebug() const;

	// Migrated from VideoSampler to the shared VideoPlaybackService
	// (Shared Video Playback — Engineering Session 2, Task G). This
	// standalone sketch owns its own instance; ExperienceRuntime's
	// production scene uses the runtime's canonical one instead.
	VideoPlaybackService videoPlayback;
	BlueprintSceneCore scene;

	bool showOccupancyDebug = false;

	// HUD Glitch Overlay System — standalone, isolated module (see
	// shared/src/hud_overlay/README.md). Toggled with 'o'; while active it
	// fully replaces this sketch's draw with the overlay alone over a
	// solid near-black canvas, no composition wiring.
	hudoverlay::HudOverlayLayer hudOverlay;
	hudoverlay::HudOverlayDialPanel hudOverlayPanel;
	hudoverlay::HudOverlayDialState hudOverlayDials;
	bool hudOverlayActive = false;
};
