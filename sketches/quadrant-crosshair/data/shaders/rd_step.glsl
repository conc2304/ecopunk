uniform sampler2D rdState;
uniform vec2      resolution;
uniform float     feedRate;  // nominal 0.055, drifts ±0.008 via LFO
uniform float     killRate;  // nominal 0.062, drifts ±0.006 via LFO

varying vec2 vTexCoord;

void main() {
    vec2 texel = 1.0 / resolution;

    vec4  center = texture2D(rdState, vTexCoord);
    float U = center.r;
    float V = center.g;

    float lapU =
        texture2D(rdState, vTexCoord + vec2( texel.x, 0.0)).r +
        texture2D(rdState, vTexCoord + vec2(-texel.x, 0.0)).r +
        texture2D(rdState, vTexCoord + vec2(0.0,  texel.y)).r +
        texture2D(rdState, vTexCoord + vec2(0.0, -texel.y)).r -
        4.0 * U;
    float lapV =
        texture2D(rdState, vTexCoord + vec2( texel.x, 0.0)).g +
        texture2D(rdState, vTexCoord + vec2(-texel.x, 0.0)).g +
        texture2D(rdState, vTexCoord + vec2(0.0,  texel.y)).g +
        texture2D(rdState, vTexCoord + vec2(0.0, -texel.y)).g -
        4.0 * V;

    float Du       = 1.0;
    float Dv       = 0.5;
    float reaction = U * V * V;

    float newU = U + (Du * lapU - reaction + feedRate * (1.0 - U)) * 0.5;
    float newV = V + (Dv * lapV + reaction - (killRate + feedRate) * V) * 0.5;

    newU = clamp(newU, 0.0, 1.0);
    newV = clamp(newV, 0.0, 1.0);

    gl_FragColor = vec4(newU, newV, 0.0, 1.0);
}
