#version 120

uniform sampler2D tex0;
uniform vec2 resolution;
uniform float threshold; // 0.08 to 0.35
uniform float inkStrength; // 0.0 to 1.0
uniform float posterizeLevels; // 4.0 to 12.0

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
    float ink = smoothstep(threshold, threshold * 2.0, edge);

    vec3 poster = floor(src * posterizeLevels) / posterizeLevels;
    vec3 color = mix(poster, vec3(0.0), ink * inkStrength);

    gl_FragColor = vec4(color, 1.0);
}
