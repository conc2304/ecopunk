#version 120
// Per-quadrant effect: composites pre-computed motion extraction output.
// motionTex is bound at unit 4 from Quadrant::drawWithEffect.
uniform sampler2D tex;
uniform sampler2D motionTex;
uniform float     alpha;
uniform float     motionGamma;  // contrast curve on motion texture (default 1.0)
uniform int       blendMode;    // 0=mix  1=additive  2=screen
varying vec2 vTexCoord;

void main() {
    vec3 original = texture2D(tex,       vTexCoord).rgb;
    vec3 motion   = texture2D(motionTex, vTexCoord).rgb;

    // Optional gamma on the motion layer before composite
    motion = pow(clamp(motion, 0.001, 1.0), vec3(motionGamma));

    vec3 result;
    if (blendMode == 1) {
        // Additive: motion brightens the original
        result = clamp(original + motion * alpha, 0.0, 1.0);
    } else if (blendMode == 2) {
        // Screen: 1 - (1-a)(1-b)
        vec3 screened = 1.0 - (1.0 - original) * (1.0 - motion);
        result = mix(original, screened, alpha);
    } else {
        // Mix (default)
        result = mix(original, motion, alpha);
    }

    gl_FragColor = vec4(result, alpha);
}
