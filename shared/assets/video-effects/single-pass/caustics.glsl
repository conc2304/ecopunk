#version 120

uniform sampler2D tex0;
uniform vec2 resolution;
uniform float time;
uniform float scale; // 0.01 to 0.05
uniform float intensity; // 0.0 to 1.0
uniform vec3 causticColor; // e.g. vec3(0.65, 0.95, 1.0)

varying vec2 vTexCoord;

float caustic(vec2 p) {
    float a = sin(p.x * 1.7 + time * 1.3) + cos(p.y * 2.1 - time * 1.1);
    float b = sin((p.x + p.y) * 1.2 + time * 0.7);
    float c = cos(length(p) * 2.4 - time * 1.6);
    float v = a + b + c;
    return smoothstep(1.15, 2.65, v);
}

void main() {
    vec2 uv = vTexCoord;
    vec3 src = texture2D(tex0, uv).rgb;

    // multiply by resolution so scale stays in pixel-frequency units
    vec2 p = uv * resolution * scale;
    float c1 = caustic(p);
    float c2 = caustic(p * 1.37 + vec2(12.3, 4.7));
    float c = clamp(c1 * 0.7 + c2 * 0.4, 0.0, 1.0);

    vec3 color = src + causticColor * c * intensity;
    gl_FragColor = vec4(color, 1.0);
}
