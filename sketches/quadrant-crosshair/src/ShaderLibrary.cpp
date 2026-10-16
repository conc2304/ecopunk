#include "ShaderLibrary.h"
#include "ofLog.h"

namespace quadrantcrosshair {

void ShaderLibrary::setup(AssetLayout layout) {
    struct Entry { const char* name; const char* standalone; const char* canonical; };
    // canonical == nullptr: not loaded in the CanonicalSync layout (see header).
    static const Entry kEntries[] = {
        { "desaturate",           "shaders/desaturate.glsl",                          "single-pass/desaturate.glsl" },
        { "invert",               "shaders/invert.glsl",                              "single-pass/invert.glsl" },
        { "recolor",              "shaders/recolor.glsl",                             "single-pass/recolor.glsl" },
        { "threshold",            "shaders/threshold.glsl",                           "single-pass/threshold.glsl" },
        { "dither",               "shaders/dither.glsl",                              "single-pass/dither.glsl" },
        { "solarize",             "shaders/solarize.glsl",                            "single-pass/solarize.glsl" },
        { "scanlines",            "shaders/scanlines.glsl",                           "single-pass/scanlines.glsl" },
        { "channelshift",         "shaders/channelshift.glsl",                        "single-pass/channelshift.glsl" },
        { "hue_rotate",           "shaders/hue_rotate.glsl",                          "single-pass/hue_rotate.glsl" },
        { "rd_step",              "shaders/rd_step.glsl",                             nullptr },
        { "erosion",              "shaders/erosion.glsl",                             "erosion/erosion_accumulation.glsl" },
        { "motion_effect",        "shaders/motion_effect.glsl",                       "motion-extraction/motion_effect.glsl" },
        { "ascii_solarpunk",      "shaders/ascii_threshold_solarpunk.glsl",           "single-pass/ascii_threshold_solarpunk.glsl" },
        { "heatmap_recolor",      "shaders/heatmap_recolor.glsl",                     "single-pass/heatmap_recolor.glsl" },
        { "bioluminescence",      "of_nature_shader_pack_glsl/bioluminescence.glsl",  "single-pass/bioluminescence.glsl" },
        { "chromatic_aberration", "of_nature_shader_pack_glsl/chromatic_aberration.glsl", "single-pass/chromatic_aberration.glsl" },
        { "edge_glow",            "of_nature_shader_pack_glsl/edge_glow.glsl",        "single-pass/edge_glow.glsl" },
        { "ink_outlines",         "of_nature_shader_pack_glsl/ink_outlines.glsl",     "single-pass/ink_outlines.glsl" },
        { "pixel_drift",          "of_nature_shader_pack_glsl/pixel_drift.glsl",      "single-pass/pixel_drift.glsl" },
        { "pixel_sorting",        "of_nature_shader_pack_glsl/pixel_sorting.glsl",    "single-pass/pixel_sorting.glsl" },
        { "temporal_trails",      "of_nature_shader_pack_glsl/temporal_trails.glsl",  nullptr },
        { "water_refraction",     "of_nature_shader_pack_glsl/water_refraction.glsl", "single-pass/water_refraction.glsl" },
    };
    const bool canonical = layout == AssetLayout::CanonicalSync;
    const std::string root = "shared-video-effects/";
    vertPath = canonical ? root + "common/vert.glsl" : "shaders/vert.glsl";
    for (const Entry& e : kEntries) {
        if (!canonical) load(e.name, e.standalone);
        else if (e.canonical != nullptr) load(e.name, root + e.canonical);
    }
}

void ShaderLibrary::load(const std::string& name, const std::string& fragPath) {
    bool ok = shaders[name].load(vertPath, fragPath);
    if (ok) {
        ofLogNotice("ShaderLibrary") << "Loaded: " << name;
    } else {
        ofLogError("ShaderLibrary") << "Failed to load: " << name << " from " << fragPath;
        shaders.erase(name);
    }
}

ofShader& ShaderLibrary::get(const std::string& name) { return shaders.at(name); }
bool ShaderLibrary::has(const std::string& name) const { return shaders.count(name) > 0; }

} // namespace quadrantcrosshair
