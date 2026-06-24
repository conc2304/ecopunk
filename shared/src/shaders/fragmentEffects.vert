#version 120

// Pi (GLES): precision qualifier required; on desktop GLSL 1.20 it is ignored.
#ifdef PLATFORM_PI
precision mediump float;
#endif

varying vec2 texCoordVarying;

void main(){
	texCoordVarying = gl_MultiTexCoord0.xy;
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
