#include "ShaderLibrary.h"

void ShaderLibrary::setup() {
    load("desaturate",   "shaders/desaturate.glsl");
    load("invert",       "shaders/invert.glsl");
    load("recolor",      "shaders/recolor.glsl");
    load("threshold",    "shaders/threshold.glsl");
    load("dither",       "shaders/dither.glsl");
    load("solarize",     "shaders/solarize.glsl");
    load("scanlines",    "shaders/scanlines.glsl");
    load("channelshift", "shaders/channelshift.glsl");
    load("rd_step",      "shaders/rd_step.glsl");
    load("erosion",      "shaders/erosion.glsl");
    load("motion_effect","shaders/motion_effect.glsl");
}

void ShaderLibrary::load(const std::string& name, const std::string& fragPath) {
    shaders[name].load("shaders/vert.glsl", fragPath);
    ofLogNotice("ShaderLibrary") << "Loaded: " << name;
}

ofShader& ShaderLibrary::get(const std::string& name) { return shaders.at(name); }
bool ShaderLibrary::has(const std::string& name) const { return shaders.count(name) > 0; }
