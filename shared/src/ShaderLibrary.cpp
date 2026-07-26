#include "ShaderLibrary.h"
#include "ofLog.h"

void ShaderLibrary::setup() {
	load("desaturate", "shaders/effects/desaturate.glsl");
	load("invert", "shaders/effects/invert.glsl");
	load("recolor", "shaders/effects/recolor.glsl");
	load("threshold", "shaders/effects/threshold.glsl");
	load("dither", "shaders/effects/dither.glsl");
	load("solarize", "shaders/effects/solarize.glsl");
	load("scanlines", "shaders/effects/scanlines.glsl");
	load("channelshift", "shaders/effects/channelshift.glsl");
	load("hue_rotate", "shaders/effects/hue_rotate.glsl");
	load("ascii_solarpunk", "shaders/effects/ascii_threshold_solarpunk.glsl");
	// nature pack
	load("bioluminescence", "of_nature_shader_pack_glsl/bioluminescence.glsl");
	load("chromatic_aberration", "of_nature_shader_pack_glsl/chromatic_aberration.glsl");
	load("edge_glow", "of_nature_shader_pack_glsl/edge_glow.glsl");
	load("ink_outlines", "of_nature_shader_pack_glsl/ink_outlines.glsl");
	load("pixel_drift", "of_nature_shader_pack_glsl/pixel_drift.glsl");
	load("pixel_sorting", "of_nature_shader_pack_glsl/pixel_sorting.glsl");
	load("water_refraction", "of_nature_shader_pack_glsl/water_refraction.glsl");
}

void ShaderLibrary::load(const std::string & name, const std::string & fragPath) {
	bool ok = shaders[name].load("shaders/effects/vert.glsl", fragPath);
	if (!ok) {
		ofLogError("ShaderLibrary") << "failed to load: " << name << " (" << fragPath << ")";
	} else {
		ofLogNotice("ShaderLibrary") << "loaded: " << name;
	}
}

ofShader & ShaderLibrary::get(const std::string & name) { return shaders.at(name); }
bool ShaderLibrary::has(const std::string & name) const { return shaders.count(name) > 0; }
