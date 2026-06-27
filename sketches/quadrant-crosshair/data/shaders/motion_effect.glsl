#version 120
// Per-quadrant effect: composites pre-computed motion extraction output.
// motionTex        (unit 4) = accumulation-based motion  — bound by Quadrant::drawWithEffect
// motionDelayedTex (unit 5) = delayed-frame motion       — bound by Quadrant::drawWithEffect
uniform sampler2D tex;
uniform sampler2D motionTex;
uniform sampler2D motionDelayedTex;
uniform float     alpha;
uniform float     motionGamma;      // contrast curve on motion texture (default 1.0)
uniform int       blendMode;        // 0=mix  1=additive  2=screen
uniform int       motionSourceMode; // 0=accum(default)  1=delayed  2=blend accum+delayed
varying vec2 vTexCoord;

void main() {
    vec3 original = texture2D(tex,            vTexCoord).rgb;
    vec3 motionA  = texture2D(motionTex,      vTexCoord).rgb;
    vec3 motionD  = texture2D(motionDelayedTex, vTexCoord).rgb;

    // Select motion source
    vec3 motion;
    if (motionSourceMode == 1) {
        motion = motionD;
    } else if (motionSourceMode == 2) {
        motion = mix(motionA, motionD, 0.5);
    } else {
        motion = motionA;
    }

    // Optional gamma on the motion layer before composite
    motion = pow(clamp(motion, 0.001, 1.0), vec3(motionGamma));

    vec3 result;
    if (blendMode == 1) {
        result = clamp(original + motion * alpha, 0.0, 1.0);
    } else if (blendMode == 2) {
        vec3 screened = 1.0 - (1.0 - original) * (1.0 - motion);
        result = mix(original, screened, alpha);
    } else {
        result = mix(original, motion, alpha);
    }

    gl_FragColor = vec4(result, alpha);
}
