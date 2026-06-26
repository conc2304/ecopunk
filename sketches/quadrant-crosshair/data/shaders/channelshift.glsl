#version 120
uniform sampler2D tex;
uniform float     shift;
uniform float     alpha;
varying vec2 vTexCoord;
void main() {
    vec3  original = texture2D(tex, vTexCoord).rgb;
    float r        = texture2D(tex, vTexCoord + vec2( shift, 0.0)).r;
    float g        = original.g;
    float b        = texture2D(tex, vTexCoord - vec2( shift, 0.0)).b;
    vec3  effect   = vec3(r, g, b);
    gl_FragColor   = vec4(mix(original, effect, alpha), alpha);
}
