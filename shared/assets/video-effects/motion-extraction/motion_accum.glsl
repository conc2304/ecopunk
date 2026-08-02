#version 120
uniform sampler2D currentFrame;
uniform sampler2D previousAccum;
uniform float     decayWeight;
varying vec2 vTexCoord;

void main() {
    vec3 current = texture2D(currentFrame,  vTexCoord).rgb;
    vec3 history = texture2D(previousAccum, vTexCoord).rgb;
    vec3 accum   = mix(current, history, decayWeight);
    gl_FragColor = vec4(accum, 1.0);
}
