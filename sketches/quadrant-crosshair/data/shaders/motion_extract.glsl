#version 120
uniform sampler2D currentFrame;
uniform sampler2D accumFrame;
uniform float     sensitivity;
uniform float     neutralGrey;
uniform int       outputMode;
varying vec2 vTexCoord;

void main() {
    vec3 current = texture2D(currentFrame, vTexCoord).rgb;
    vec3 accum   = texture2D(accumFrame,   vTexCoord).rgb;

    vec3  diff = current - accum;
    float mag  = length(diff) * sensitivity;

    vec3 outColor;

    if (outputMode == 0) {
        // LUMA_GLOW: grey field, brightness scales with motion magnitude
        float luma = clamp(neutralGrey + mag, 0.0, 1.0);
        outColor = vec3(luma);

    } else if (outputMode == 1) {
        // CHROMA_PRESERVE: original hue bleeds in where motion occurs
        vec3 grey = vec3(neutralGrey);
        outColor = mix(grey, current, clamp(mag * 2.0, 0.0, 1.0));

    } else {
        // SIGNED_FIELD: positive diff = warm (orange), negative = cool (blue)
        float signed_mag = dot(diff, vec3(0.333));
        vec3  warm = vec3(0.78, 0.49, 0.16);
        vec3  cool = vec3(0.16, 0.38, 0.62);
        vec3  mid  = vec3(neutralGrey);
        if (signed_mag > 0.0) {
            outColor = mix(mid, warm, clamp(signed_mag * sensitivity, 0.0, 1.0));
        } else {
            outColor = mix(mid, cool, clamp(-signed_mag * sensitivity, 0.0, 1.0));
        }
    }

    gl_FragColor = vec4(outColor, 1.0);
}
