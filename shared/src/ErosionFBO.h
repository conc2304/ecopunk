#pragma once

#include "ofFbo.h"
#include "ofShader.h"

// Ping-pong decay-and-blend FBO: capture the current frame's content, blend
// it against the running history (mix(current, history, decayRate) — a
// normalized blend, so the result can't run away toward the clamp ceiling),
// and present the composited result. Gives the canvas a few seconds of
// visual memory — recent draws leave a fading trace instead of disappearing
// outright the instant they're no longer drawn.
//
// Adapted from quadrant-crosshair/src/Quadrant.cpp's embedded fbo_read/
// fbo_write pattern (per-quadrant erosion accumulation), generalized into a
// standalone reusable class that wraps a whole scene's draw() call instead
// of one region.
class ErosionFBO {
	public:
		void setup(int width, int height, float decayRate = 0.92f);

		// Bracket the scene draw calls that should be captured this frame.
		void beginCapture();
		void endCapture();

		// Runs the ping-pong blend pass against the just-captured frame.
		void update();

		// Draws the composited result to the currently bound target.
		void draw(float x = 0, float y = 0);
		void draw(float x, float y, float w, float h);

		void clear();

		bool isReady() const { return ready; }

		void setDecayRate(float rate) { decayRate = rate; }
		void setDesatAmount(float amount) { desatAmount = amount; }

	private:
		ofFbo captureFbo;
		ofFbo fboA, fboB;
		bool pingPong = false; // false: read=A write=B, true: read=B write=A
		bool ready = false;
		bool primed = false; // false until the first real frame has been written into history

		ofShader erosionShader;
		float decayRate = 0.92f;
		float desatAmount = 0.0f;
		int width = 0;
		int height = 0;

		ofFbo & readFbo() { return pingPong ? fboB : fboA; }
		ofFbo & writeFbo() { return pingPong ? fboA : fboB; }
};
