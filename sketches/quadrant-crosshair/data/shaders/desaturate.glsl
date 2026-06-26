#version 120
uniform sampler2D tex;
uniform float     alpha;
varying vec2 vTexCoord;
void main() {
    vec3  original = texture2D(tex, vTexCoord).rgb;
    float luma     = dot(original, vec3(0.299, 0.587, 0.114));
    vec3  effect   = vec3(luma);
    gl_FragColor   = vec4(mix(original, effect, alpha), alpha);
}
