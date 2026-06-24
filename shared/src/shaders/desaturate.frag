#ifdef PLATFORM_PI
	#version 100
	precision mediump float;
#else
	#version 120
#endif

uniform sampler2D tex0;
uniform float desaturateAmount;

varying vec2 texCoordVarying;

void main(){
	vec4 color = texture2D(tex0, texCoordVarying);
	float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));
	vec3 desaturated = mix(color.rgb, vec3(luma), desaturateAmount);
	gl_FragColor = vec4(desaturated, color.a);
}
