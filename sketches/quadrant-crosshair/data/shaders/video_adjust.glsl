uniform sampler2D tex;
uniform float brightness;  // additive offset, e.g. -0.10
uniform float contrast;    // multiplier pivoted at 0.5, e.g. 1.10
uniform float saturation;  // 1.0 = original, 1.15 = +15%

varying vec2 vTexCoord;

void main() {
    vec4 c = texture2D(tex, vTexCoord);

    // Brightness
    c.rgb += brightness;

    // Contrast (pivot at mid-grey so blacks/whites stay balanced)
    c.rgb = (c.rgb - 0.5) * contrast + 0.5;

    // Saturation
    float luma = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    c.rgb = mix(vec3(luma), c.rgb, saturation);

    gl_FragColor = clamp(c, 0.0, 1.0);
}
