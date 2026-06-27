#version 120

uniform sampler2D currentFrame;
uniform sampler2D delayedFrame;
uniform sampler2D accumFrame;

uniform float sensitivity;
uniform float neutralGrey;
uniform float boost;
uniform float gamma;

uniform int outputMode;
uniform int referenceMode;

varying vec2 vTexCoord;

void main() {
    vec3 current = texture2D(currentFrame,  vTexCoord).rgb;
    vec3 delayed = texture2D(delayedFrame,  vTexCoord).rgb;
    vec3 accum   = texture2D(accumFrame,    vTexCoord).rgb;

    // referenceMode:
    // 0 = delayed frame difference: current - N frames ago
    // 1 = accumulation difference:  current - memory buffer
    vec3 reference = delayed;

    if (referenceMode == 1) {
        reference = accum;
    }

    vec3 diff = current - reference;

    float mag = length(diff) / 1.7320508;
    mag *= sensitivity;
    mag *= boost;
    mag = clamp(mag, 0.0, 1.0);

    float safeGamma = max(gamma, 0.0001);

    vec3 outColor;

    if (outputMode == 0) {
        // LUMA_GLOW: grey field, brightness scales with motion magnitude
        // (matches original mode 0 — used by the energy state machine in ofApp)
        float m = pow(mag, safeGamma);
        float luma = clamp(neutralGrey + m, 0.0, 1.0);
        outColor = vec3(luma);

    } else if (outputMode == 1) {
        // CHROMA_PRESERVE: original hue bleeds in where motion occurs
        // (matches original mode 1)
        float m = pow(mag, safeGamma);
        vec3 grey = vec3(neutralGrey);
        outColor = mix(grey, current, clamp(m * 2.0, 0.0, 1.0));

    } else if (outputMode == 2) {
        // SIGNED_FIELD: positive diff = warm, negative diff = cool
        // (matches original mode 2)
        float signedMag = dot(diff, vec3(0.333));
        vec3 warm = vec3(0.78, 0.49, 0.16);
        vec3 cool = vec3(0.16, 0.38, 0.62);
        vec3 mid  = vec3(neutralGrey);

        if (signedMag > 0.0) {
            float m = clamp(signedMag * sensitivity * boost, 0.0, 1.0);
            m = pow(m, safeGamma);
            outColor = mix(mid, warm, m);
        } else {
            float m = clamp(-signedMag * sensitivity * boost, 0.0, 1.0);
            m = pow(m, safeGamma);
            outColor = mix(mid, cool, m);
        }

    } else if (outputMode == 3) {
        // RAW_MASK: black = no motion, white = motion (useful for quadrant debug)
        float m = pow(mag, safeGamma);
        outColor = vec3(m);

    } else if (outputMode == 4) {
        // DEBUG_REFERENCE: shows the selected comparison frame
        outColor = reference;

    } else {
        float m = pow(mag, safeGamma);
        outColor = vec3(m);
    }

    gl_FragColor = vec4(clamp(outColor, 0.0, 1.0), 1.0);
}
