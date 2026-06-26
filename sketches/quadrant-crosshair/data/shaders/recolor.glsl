#version 120
uniform sampler2D tex;
uniform vec3      tint;
uniform float     alpha;
varying vec2 vTexCoord;
void main() {
    vec3  original = texture2D(tex, vTexCoord).rgb;
    float luma     = dot(original, vec3(0.299, 0.587, 0.114));
    vec3  effect   = luma * tint;
    gl_FragColor   = vec4(mix(original, effect, alpha), alpha);
}
