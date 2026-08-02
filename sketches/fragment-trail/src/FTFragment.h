#pragma once

#include "ofMain.h"
#include "TimeOffsetVideoBuffer.h"
#include "ShaderLibrary.h"
#include "HudElements.h"
#include <string>

// Section 4 of the brief.
enum class FTContentMode {
	TIME_SLICE,          // Mode A — reuses Temporal Fields' TimeOffsetVideoBuffer playhead pool
	EFFECT_VARIED,        // Mode B — one effect per fragment, live undelayed feed
	EFFECT_PARAM_VARIANT   // Mode C — one shared effect, jittered per-fragment params
};

// Everything a fragment needs at spawn time. Mode/effect/jitter ASSIGNMENT
// policy lives in FTFragmentPool/FTModeController — this struct is just the
// resulting values for one fragment to hold and draw.
struct FTFragmentSpawnParams {
	glm::vec2 position{ 0.f, 0.f }; // canvas pixel coords, center of window
	float size = 140.f;             // window edge length in pixels (Section 1: 90-220px)
	FTContentMode mode = FTContentMode::EFFECT_VARIED;

	// Mode A only
	int playheadIndex = -1;
	float driftRate = 0.3f; // normalized history-offset units/sec toward maxHistorySeconds

	// Mode B/C
	std::string effectName; // ShaderLibrary key, or "" / "passthrough" for no shader

	// Mode B: fixed defaults. Mode C: the jittered value for whichever of
	// these the shared effect actually reads (see brief Section 4 Mode C).
	glm::vec3 tint{ 0.6f, 0.75f, 1.0f };
	float thresholdVal = 0.5f;
	float shiftVal = 0.01f;
	float ditherArc = 0.5f;   // fixed "peak dither" position — see dither.glsl's alpha-arc comment
	float ditherPx = 4.0f;
	float heatmapGamma = 0.9f;
	float heatmapMinLuminance = 0.05f;
	float heatmapMaxLuminance = 0.95f;
	int heatmapPalette = 0;
	int heatmapReverse = 0;

	std::string hexLabel;
	int spawnIndex = -1; // untruncated, unique per process lifetime — see FTFragmentPool::spawnCounter
};

// One spawned window-chrome particle: a live peephole into the current
// media, framed by hud::WindowChrome. Owns its own age/decay and content-mode
// state; TimeOffsetVideoBuffer/ShaderLibrary are borrowed references supplied
// each frame, not owned here.
class FTFragment {
public:
	// sustainSeconds: how long the fragment stays at full opacity before
	// decay starts. decaySeconds: how long it then takes to fade from full
	// to zero opacity. Total lifetime is sustainSeconds + decaySeconds.
	void spawn(const FTFragmentSpawnParams& params, float sustainSeconds, float decaySeconds, const hud::HudTheme& theme);
	void update(float dt);
	void draw(TimeOffsetVideoBuffer& videoBuffer, ShaderLibrary& shaderLib);

	float getAlpha() const;
	bool isExpired() const { return age >= sustain + decay; }
	int getPlayheadIndex() const { return params.playheadIndex; }
	FTContentMode getMode() const { return params.mode; }
	glm::vec2 getPosition() const { return params.position; }
	int getSpawnIndex() const { return params.spawnIndex; }
	// Mirrors the destRect math in draw() — for external callers (the HUD
	// overlay director) that need to anchor an effect to this fragment's
	// window without duplicating that math themselves.
	ofRectangle getBounds() const {
		return ofRectangle(params.position.x - params.size * 0.5f, params.position.y - params.size * 0.5f,
			params.size, params.size);
	}

private:
	FTFragmentSpawnParams params;
	float age = 0.f;
	float sustain = 1.6f;
	float decay = 1.6f;
	hud::WindowChrome chrome;

	void drawContent(const ofTexture& tex, const ofRectangle& destRect, ShaderLibrary& shaderLib);
};
