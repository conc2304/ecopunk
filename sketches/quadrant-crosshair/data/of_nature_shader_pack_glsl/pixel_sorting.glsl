#version 120

uniform sampler2D tex0;
uniform vec2 resolution;
uniform float threshold; // 0.3 to 0.8
uniform float rangePx;   // 2.0 to 24.0
uniform float direction; // 0.0 horizontal, 1.0 vertical
uniform float intensity; // 0.0 to 1.0

varying vec2 vTexCoord;

float luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 uv = vTexCoord;
    vec3 src = texture2D(tex0, uv).rgb;
    float lum = luma(src);

    vec2 dir = mix(vec2(1.0, 0.0), vec2(0.0, 1.0), step(0.5, direction));
    float gate = smoothstep(threshold, threshold + 0.15, lum);

    vec3 acc = vec3(0.0);
    float count = 0.0;

    for (int i = -8; i <= 8; ++i) {
        float fi = float(i) / 8.0;
        // rangePx in pixels → divide by resolution for normalized step
        vec2 suv = uv + dir * fi * rangePx / resolution;
        vec3 c = texture2D(tex0, suv).rgb;
        float w = smoothstep(threshold, threshold + 0.2, luma(c));
        acc += c * w;
        count += w;
    }

    vec3 sortedLike = acc / max(count, 0.001);
    vec3 color = mix(src, sortedLike, gate * intensity);

    gl_FragColor = vec4(color, 1.0);
}
