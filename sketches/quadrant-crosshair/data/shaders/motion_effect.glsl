#version 120
// Per-quadrant effect shader: composites pre-computed motion extraction output.
// motionTex is bound at unit 4 from Quadrant::drawWithEffect.
uniform sampler2D tex;
uniform sampler2D motionTex;
uniform float     alpha;
varying vec2 vTexCoord;

void main() {
    vec3 original = texture2D(tex, vTexCoord).rgb;
    vec3 motion   = texture2D(motionTex, vTexCoord).rgb;
    gl_FragColor  = vec4(mix(original, motion, alpha), alpha);
}
