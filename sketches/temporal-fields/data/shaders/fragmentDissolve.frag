#version 120

#ifdef PLATFORM_PI
precision mediump float;
#endif

// fromTex: the frozen "before" snapshot, bound at texture unit 0 (the
// default sampler unit) via ofTexture::bind() in TFFragmentTransition —
// texCoordVarying already spans its full [0,1] extent correctly since it
// was captured pre-cropped to exactly this fragment's own region.
uniform sampler2D fromTex;

// toTex: the live "after" source (a whole playhead buffer, not pre-cropped)
// — toUVRect picks out this fragment's own proportional crop from it.
uniform sampler2D toTex;
uniform vec4 toUVRect; // xMin, yMin, xMax, yMax in toTex's normalized space

uniform float progress; // 0..1

varying vec2 texCoordVarying;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main(){
	vec4 fromColor = texture2D(fromTex, texCoordVarying);

	vec2 toUV = mix(toUVRect.xy, toUVRect.zw, texCoordVarying);
	vec4 toColor = texture2D(toTex, toUV);

	// Per-pixel threshold against progress gives a scattered, eroded
	// reveal front rather than a uniform wipe.
	float threshold = hash(floor(texCoordVarying * 48.0));
	gl_FragColor = threshold < progress ? toColor : fromColor;
}
