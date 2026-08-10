#version 120

#ifdef PLATFORM_PI
precision mediump float;
#endif

// Particle Field's spawn/death existence-fade (Erosion style) — a single-
// texture counterpart to fragmentDissolve.frag's two-texture value
// crossfade. `progress` ramps 0->1 at spawn and 1->0 at death using the
// SAME per-pixel hash mask both times, which is what makes this "one shared
// random blob layout" per fragment (see TFPatternParticleField.cpp).
uniform sampler2D tex;
uniform float progress; // 0 = fully hidden, 1 = fully revealed

varying vec2 texCoordVarying;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main(){
	float threshold = hash(floor(texCoordVarying * 48.0));
	if (threshold >= progress) {
		discard;
	}
	gl_FragColor = texture2D(tex, texCoordVarying);
}
