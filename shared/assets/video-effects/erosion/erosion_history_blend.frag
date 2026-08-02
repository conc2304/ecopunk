#version 120

#ifdef PLATFORM_PI
precision mediump float;
#endif

uniform sampler2D history;     // previous composited frame
uniform sampler2D current;     // this frame's captured scene
uniform float decayRate;       // 0.0-1.0, how much of history survives per frame
uniform float desatAmount;     // 0.0-1.0, global desaturation applied to the result

varying vec2 texCoordVarying;

void main(){
	vec4 hist = texture2D(history, texCoordVarying);
	vec4 curr = texture2D(current, texCoordVarying);

	// Normalized blend (weights sum to 1) so the result can never run away
	// toward the clamp ceiling — decayRate is how much of the old frame
	// survives; the rest is freshly replaced by the current frame.
	vec4 result = mix(curr, hist, decayRate);

	float luma = dot(result.rgb, vec3(0.299, 0.587, 0.114));
	result.rgb = mix(result.rgb, vec3(luma), desatAmount);

	gl_FragColor = result;
}
