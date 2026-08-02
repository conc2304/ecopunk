#pragma once
#include "ofMain.h"
#include <map>
#include <string>

// This local fork is intentional (see config.make's comments — name
// collisions with shared/src's top-level classes). New effects added here
// are NOT visible to the canonical shared/src/video-effects/ catalog.
// Before adding a new effect, check shared/src/video-effects/README.md and
// shared/src/video-effects/catalog/DefaultVideoEffectCatalog.cpp first —
// Quadrant::drawWithEffect() already sources its fixed-default effects
// (bioluminescence, chromatic_aberration, edge_glow, ink_outlines,
// pixel_drift, water_refraction, ascii_solarpunk) from that catalog instead
// of hand-duplicating their uniforms; follow that pattern rather than
// adding a new hardcoded branch.
class ShaderLibrary {
public:
    void      setup();
    ofShader& get(const std::string& name);
    bool      has(const std::string& name) const;

private:
    std::map<std::string, ofShader> shaders;
    void load(const std::string& name, const std::string& fragPath);
};
