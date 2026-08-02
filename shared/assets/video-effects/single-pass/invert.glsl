#version 120
uniform sampler2D tex;
uniform float     alpha;
varying vec2 vTexCoord;
void main() {
    vec3 original = texture2D(tex, vTexCoord).rgb;
    vec3 effect   = 1.0 - original;
    gl_FragColor  = vec4(mix(original, effect, alpha), alpha);
}
