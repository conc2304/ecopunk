#include "ShaderLibrary.h"

void ShaderLibrary::setup() {
    load("desaturate",          "shaders/desaturate.glsl");
    load("invert",              "shaders/invert.glsl");
    load("recolor",             "shaders/recolor.glsl");
    load("threshold",           "shaders/threshold.glsl");
    load("dither",              "shaders/dither.glsl");
    load("solarize",            "shaders/solarize.glsl");
    load("scanlines",           "shaders/scanlines.glsl");
    load("channelshift",        "shaders/channelshift.glsl");
    load("rd_step",             "shaders/rd_step.glsl");
    load("erosion",             "shaders/erosion.glsl");
    load("motion_effect",       "shaders/motion_effect.glsl");
    load("ascii_solarpunk",     "shaders/ascii_threshold_solarpunk.glsl");
    // nature pack
    load("bioluminescence",     "of_nature_shader_pack_glsl/bioluminescence.glsl");
    load("chromatic_aberration","of_nature_shader_pack_glsl/chromatic_aberration.glsl");
    load("edge_glow",           "of_nature_shader_pack_glsl/edge_glow.glsl");
    load("ink_outlines",        "of_nature_shader_pack_glsl/ink_outlines.glsl");
    load("pixel_drift",         "of_nature_shader_pack_glsl/pixel_drift.glsl");
    load("pixel_sorting",       "of_nature_shader_pack_glsl/pixel_sorting.glsl");
    load("temporal_trails",     "of_nature_shader_pack_glsl/temporal_trails.glsl");
    load("water_refraction",    "of_nature_shader_pack_glsl/water_refraction.glsl");
}

void ShaderLibrary::load(const std::string& name, const std::string& fragPath) {
    shaders[name].load("shaders/vert.glsl", fragPath);
    ofLogNotice("ShaderLibrary") << "Loaded: " << name;
}

ofShader& ShaderLibrary::get(const std::string& name) { return shaders.at(name); }
bool ShaderLibrary::has(const std::string& name) const { return shaders.count(name) > 0; }
