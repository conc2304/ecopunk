#pragma once
#include "ofMain.h"
#include <map>
#include <string>

// Copied from quadrant-crosshair/src/ShaderLibrary.h (class shape unchanged),
// with the load() list trimmed to the effects Section 4 of the brief keeps
// for Modes B/C. EDGE_GLOW and DITHER_ERROR are excluded per the brief —
// both were already flagged as Pi cost/fidelity risks in the Quadrant
// Crosshair handoff. PASSTHROUGH isn't a shader at all here — FTFragment
// just skips binding one and draws the raw texture.
//
// This local fork is intentional and currently unavoidable — see
// config.make's comments (name collisions with shared/src's top-level
// classes) — but it means new effects added here are NOT visible to the
// canonical shared/src/video-effects/ catalog. Before adding a new effect
// to this file, check shared/src/video-effects/README.md and
// shared/src/video-effects/catalog/DefaultVideoEffectCatalog.cpp first —
// if the effect already exists there, prefer copying its exact uniform
// values (as FTFragment.cpp's dither/threshold/recolor/channelshift/
// heatmap_recolor branches already do) rather than inventing new ones.
class ShaderLibrary {
public:
	void      setup();
	ofShader& get(const std::string& name);
	bool      has(const std::string& name) const;

private:
	std::map<std::string, ofShader> shaders;
	void load(const std::string& name, const std::string& fragPath);
};
