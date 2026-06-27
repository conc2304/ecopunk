#version 120

uniform sampler2D tex0;
uniform vec2 resolution;
uniform float time;
uniform float threshold; // 0.15 to 0.6
uniform float intensity; // 0.0 to 2.0
uniform vec3 glowColor; // e.g. vec3(0.1, 1.0, 0.75)

varying vec2 vTexCoord;

float luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 uv = vTexCoord;
    vec2 px = 1.0 / resolution;
    vec3 src = texture2D(tex0, uv).rgb;

    float c  = luma(src);
    float cx = luma(texture2D(tex0, uv + vec2(px.x, 0.0)).rgb);
    float cy = luma(texture2D(tex0, uv + vec2(0.0, px.y)).rgb);
    float edge = abs(c - cx) + abs(c - cy);

    float pulse = 0.65 + 0.35 * sin(time * 2.0 + c * 8.0);
    float mask = smoothstep(threshold, threshold * 2.5, edge + c * 0.15);

    vec3 color = src + glowColor * mask * pulse * intensity;
    gl_FragColor = vec4(color, 1.0);
}
