#version 120
uniform sampler2D tex;
uniform vec2      resolution;
uniform float     alpha;
varying vec2 vTexCoord;
void main() {
    vec3  original = texture2D(tex, vTexCoord).rgb;
    float line     = mod(floor(vTexCoord.y * resolution.y), 2.0);
    vec3  effect   = original * (line < 1.0 ? 0.35 : 1.0);
    gl_FragColor   = vec4(mix(original, effect, alpha), alpha);
}
