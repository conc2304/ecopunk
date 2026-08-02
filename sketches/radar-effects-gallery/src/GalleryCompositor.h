#pragma once
#include "ofMain.h"
#include <array>
#include <map>
#include <string>
#include <vector>

// Decoupled from hud::PulseInfo on purpose — the compositor only needs these
// five fields and shouldn't have to include the widget header just for a
// struct shape.
struct GalleryPulseInfo {
	float x = 0.0f, y = 0.0f;
	float radius = 0.0f;
	float ageMs = 0.0f;
	int emitterId = -1;
};

// Runs the nine candidate decay/reveal shaders (+ Mode 0 baseline) from the
// Effects Gallery engineering handoff, one small ofShader per mode rather
// than a single uber-shader with a mode uniform — the doc's own
// recommendation (Section 2), to avoid GLSL ES dynamic-branch instruction-
// count risk on VideoCore IV-class GPUs. Whether that risk is real on this
// specific GPU/driver is an open question the doc explicitly asks to
// confirm on-device (Section 6) — not something this desktop build can
// settle either way.
class GalleryCompositor {
public:
	static constexpr int NUM_MODES = 11;

	void setup(int canvasW, int canvasH);
	void update(float dt, const std::vector<GalleryPulseInfo> & pulses);
	void draw(ofTexture & videoTex, ofTexture & maskTex, const std::vector<GalleryPulseInfo> & pulses);

	void nextMode() { modeIndex = (modeIndex + 1) % NUM_MODES; }
	void prevMode() { modeIndex = (modeIndex - 1 + NUM_MODES) % NUM_MODES; }
	int getModeIndex() const { return modeIndex; }
	const std::string & getModeName() const { return modeNames[modeIndex]; }

	// Mode 2 (per-emitter hue tint): assigns a color the first time an
	// emitterId is seen, then returns the same one on every later call.
	// ofApp reads this when building each frame's RPPulseStamp::tint.
	ofColor getEmitterTint(int emitterId);

private:
	int canvasW = 0, canvasH = 0;
	int modeIndex = 0;

	std::array<ofShader, NUM_MODES> shaders;
	std::array<std::string, NUM_MODES> modeNames;

	std::map<int, ofColor> emitterTints;
	static const std::array<ofColor, 6> tintPalette;

	// Mode 7 — telemetry ghosting. See gallery_mode7_telemetry.frag's header
	// comment for why this is a local, minimal reimplementation rather than
	// shared/src/AnnotationRenderer.
	ofTrueTypeFont codeFont;
	std::vector<std::string> codeFragments;
	struct GhostText {
		glm::vec2 pos;
		std::string text;
		float opacity = 1.0f;
	};
	std::vector<GhostText> ghostTexts;
	float mode7SpawnTimer = 0.0f;

	// Mode 9 — boot-up static burst. "Arriving" is derived per-frame from
	// PulseInfo::ageMs, not stored — see gallery_mode9_bootstatic.frag.
	static constexpr float arrivalWindowMs = 250.0f;
	static constexpr int maxArriving = 8;

	void applyModeUniforms(ofShader & shader, const std::vector<GalleryPulseInfo> & pulses);
	void drawMode7Text();
};
