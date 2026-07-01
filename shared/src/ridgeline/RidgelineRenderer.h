#pragma once

#include "ofMain.h"

// RidgelineRenderer
//
// Brightness-driven horizontal ridgeline effect (Joy Division / pulsar style).
// Samples luminance from a source ofPixels along N horizontal rows, builds
// CPU-side polylines, and draws them GPU-side as transparent stroked lines
// over a solid ground or source image.
//
// Designed for Raspberry Pi 3B / GLSL ES 1.0 constraints: no per-pixel
// fragment shader, no FBO ping-pong. All expensive work (pixel reads, polyline
// construction) happens in update(); draw() is GL calls only.
//
// Usage:
//   RidgelineRenderer ridge;
//   ridge.setup(1280, 720);
//   ridge.setParams(...);
//   // each frame, after the source pixels are available:
//   ridge.update(sourcePixels);
//   ridge.draw(); // call inside ofApp::draw()

class RidgelineRenderer {
public:
	struct Params {
		int numLines        = 42;     // number of horizontal ridge lines
		int samplesPerLine  = 64;     // luminance samples per line (keep low on Pi: 40-80)
		float amplitude     = 80.0f;  // px, brighter pixels push the line up by up to this much
		float spacingPct    = 0.10f;  // vertical gap between line rest-positions, as % of canvas height
		bool overlayMode    = false;  // false = replace (black ground), true = draw under the source image first
		float marginXPct    = 0.08f;  // horizontal margin as % of canvas width
		float centerYPct    = 0.55f;  // vertical center of the stack as % of canvas height
		bool flipX          = false;  // mirror the entire output horizontally (left becomes right)
		bool flipY          = false;  // mirror the entire output vertically (bottom becomes top)
		ofColor lineColor   = ofColor(255, 255, 255);
		ofColor groundColor = ofColor(13, 13, 13); // GROUND_DARK from design doc
	};

	void setup(int canvasWidth, int canvasHeight);
	void setParams(const Params& p) { params = p; rebuildNeeded = true; }
	const Params& getParams() const { return params; }

	// Call once per frame. sourcePixels must be RGB or RGBA.
	// Does the CPU-side sampling + polyline build. Throttle from ofApp
	// (e.g. every other frame) if profiling shows this is too expensive.
	void update(const ofPixels& sourcePixels);

	// GL calls only. Optionally pass the source texture to draw beneath the
	// ridgelines when params.overlayMode is true. opacity (0–1) scales all
	// drawn colors so the effect can fade in/out via the slot lifecycle.
	void draw(ofTexture* sourceTexForOverlay = nullptr, float opacity = 1.0f);

private:
	Params params;
	bool rebuildNeeded = true;

	int canvasW = 1280;
	int canvasH = 720;

	struct Line {
		ofPolyline stroke;
	};
	std::vector<Line> lines;

	float sampleLuminance(const ofPixels& px, float u, float v) const;
	void rebuildLines(const ofPixels& sourcePixels);
};
