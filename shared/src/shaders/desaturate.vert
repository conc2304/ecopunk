#ifdef PLATFORM_PI
	#version 100
	precision mediump float;
#else
	#version 120
#endif

varying vec2 texCoordVarying;

void main(){
	texCoordVarying = gl_MultiTexCoord0.xy;
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
