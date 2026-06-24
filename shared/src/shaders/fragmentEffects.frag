#version 120

#ifdef PLATFORM_PI
precision mediump float;
#endif

uniform sampler2D tex0;
uniform float desaturateAmount;
uniform float circleMask;    // > 0.5: apply radial discard
uniform vec2  maskCenterPx;  // circle center in OpenGL window coords (y increases upward)
uniform float maskRadiusPx;  // circle radius in pixels

varying vec2 texCoordVarying;

void main(){
	if(circleMask > 0.5 && distance(gl_FragCoord.xy, maskCenterPx) > maskRadiusPx) discard;

	vec4 color = texture2D(tex0, texCoordVarying);
	float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));
	vec3 desaturated = mix(color.rgb, vec3(luma), desaturateAmount);
	gl_FragColor = vec4(desaturated, color.a);
}
