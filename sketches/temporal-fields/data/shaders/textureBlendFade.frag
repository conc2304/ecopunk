#version 120

#ifdef PLATFORM_PI
precision mediump float;
#endif

// Pre-tints a texture's color toward a blend mode's identity color (black
// for OF_BLENDMODE_SCREEN, white for OF_BLENDMODE_MULTIPLY) proportional to
// opacity, before the GL blend stage runs. Needed because oF's built-in
// SCREEN/MULTIPLY glBlendFunc combinations don't reference source alpha the
// way ALPHA blending does -- SCREEN's (GL_ONE_MINUS_DST_COLOR, GL_ONE)
// ignores src alpha entirely, and MULTIPLY's (GL_DST_COLOR,
// GL_ONE_MINUS_SRC_ALPHA) saturates near 1.0 at low alpha instead of fading
// toward no visible effect -- so ofSetColor's alpha channel alone can't
// make either blend mode respond to the opacity slider. See
// TFAmbientTextureLayer::drawLayer().

uniform sampler2D tex;
uniform float opacity; // 0..TFAmbientTextureLayer::kMaxOpacity
uniform vec3 identityColor; // vec3(0.0) for screen, vec3(1.0) for multiply

varying vec2 texCoordVarying;

void main(){
	vec4 texColor = texture2D(tex, texCoordVarying);
	vec3 tinted = mix(identityColor, texColor.rgb, opacity);
	gl_FragColor = vec4(tinted, 1.0);
}
