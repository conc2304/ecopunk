#pragma once
#include "ofMain.h"
#include <vector>

// Bounds-local pixel-space stamp for one pulse, ready to composite into the
// mask. `freshness` (0=oldest/about to retire, 1=just spawned) only matters
// to the sorted-draw fallback path — the max-blend path ignores draw order.
struct RPPulseStamp {
	float x = 0.0f, y = 0.0f;
	float radius = 0.0f;
	float bandWidth = 60.0f;
	float freshness = 1.0f;
};

// System H — Reveal Mask (Path A: low-res ping-pong FBO with decay).
//
// Structured like shared/src/ErosionFBO.h/.cpp's proven ping-pong lifecycle
// (capture -> ping-pong blend -> present) but deliberately NOT that class:
// ErosionFBO's shader does a normalized mix(current, history, decayRate),
// which would dilute an overlapping bright pulse toward the current frame
// every tick instead of letting the brightest recent stamp persist. This
// class's shader (reveal_mask.frag) does decay-then-max instead:
// result = max(history * trailPersistence, stampedCurrent). Kept local to
// this sketch per the engineering handoff — this risk (ping-pong FBO cost,
// blend-equation availability) shouldn't leak into shared/src/hud and
// silently affect sketches that never touch this feature.
//
// Unlike ErosionFBO, there's no first-frame "primed" special case: history
// starts fully transparent (0,0,0,0), and max(0 * persistence, curr) == curr
// on frame one, so the correct result falls out without extra bookkeeping.
class RPRevealMask {
public:
	void setup(int width, int height, float trailPersistence = 0.965f);

	// Bracket this frame's pulse-stamp draws.
	void beginStamp();
	void stampPulses(const std::vector<RPPulseStamp> & stamps);
	void endStamp();

	// Runs the decay/max-blend ping-pong pass against this frame's stamps.
	void update();

	ofTexture & getMaskTexture();
	void clear();

	bool isReady() const { return ready; }
	// True if stamps were composited via real GL_MAX blend-equation; false
	// means the sorted-draw alpha-over fallback was used instead (see
	// detectMaxBlendSupport()) — surfaced so the compositor/HUD can report
	// which path is actually active on a given machine.
	bool isUsingMaxBlend() const { return supportsMaxBlend; }

	void setTrailPersistence(float rate) { trailPersistence = rate; }
	void setMaxOpacity(float amount) { maxOpacity = amount; }

private:
	ofFbo captureFbo;
	ofFbo fboA, fboB;
	bool pingPong = false; // false: read=A write=B, true: read=B write=A
	bool ready = false;
	bool supportsMaxBlend = false;

	ofShader maskShader;
	float trailPersistence = 0.965f;
	float maxOpacity = 1.0f;
	int width = 0;
	int height = 0;

	ofFbo & readFbo() { return pingPong ? fboB : fboA; }
	ofFbo & writeFbo() { return pingPong ? fboA : fboB; }

	void detectMaxBlendSupport();
	void drawStampMesh(const RPPulseStamp & stamp) const;
};
