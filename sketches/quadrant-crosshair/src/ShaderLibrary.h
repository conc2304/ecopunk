#pragma once
#include "ofMain.h"
#include <map>
#include <string>

namespace quadrantcrosshair {

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
    // Where the shader files come from.
    //   Standalone    — this sketch's own bin/data/shaders/ +
    //                   of_nature_shader_pack_glsl/ (unchanged).
    //   CanonicalSync — ExperienceRuntime: bin/data/shared-video-effects/,
    //                   delivered from shared/assets/video-effects/ by
    //                   scripts/sync-video-effect-assets.py per the runtime's
    //                   effect-manifest.json (byte-identical files). Only the
    //                   shaders production actually draws are loaded there:
    //                   rd_step (unused ReactionDiffusion) and temporal_trails
    //                   (not in QuadrantManager's pool) are skipped.
    enum class AssetLayout { Standalone, CanonicalSync };

    void      setup(AssetLayout layout = AssetLayout::Standalone);
    ofShader& get(const std::string& name);
    bool      has(const std::string& name) const;

private:
    std::map<std::string, ofShader> shaders;
    std::string vertPath = "shaders/vert.glsl";
    void load(const std::string& name, const std::string& fragPath);
};

} // namespace quadrantcrosshair
