#include "ShaderLibrary.h"
#include "ofLog.h"

void ShaderLibrary::setup() {
	load("desaturate",   "shaders/desaturate.glsl");
	load("invert",       "shaders/invert.glsl");
	load("recolor",      "shaders/recolor.glsl");
	load("threshold",    "shaders/threshold.glsl");
	load("dither",       "shaders/dither.glsl");
	load("solarize",     "shaders/solarize.glsl");
	load("scanlines",    "shaders/scanlines.glsl");
	load("channelshift", "shaders/channelshift.glsl");
	load("heatmap_recolor", "shaders/heatmap_recolor.glsl");
}

void ShaderLibrary::load(const std::string& name, const std::string& fragPath) {
	bool ok = shaders[name].load("shaders/vert.glsl", fragPath);
	if (ok) {
		ofLogNotice("ShaderLibrary") << "Loaded: " << name;
	} else {
		ofLogError("ShaderLibrary") << "Failed to load: " << name << " from " << fragPath;
		shaders.erase(name);
	}
}

ofShader& ShaderLibrary::get(const std::string& name) { return shaders.at(name); }
bool ShaderLibrary::has(const std::string& name) const { return shaders.count(name) > 0; }
