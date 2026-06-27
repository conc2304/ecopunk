#version 120

uniform sampler2D tex0;
uniform vec2 resolution;
uniform float time;
uniform float amount; // 1.0 to 12.0 px
uniform float scale;  // 0.01 to 0.06
uniform float speed;  // 0.1 to 2.0

varying vec2 vTexCoord;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main() {
    vec2 uv = vTexCoord;
    // multiply by resolution so scale drives pixel-frequency noise
    vec2 p = uv * resolution * scale;

    float n1 = noise(p + vec2(time * speed, 0.0));
    float n2 = noise(p * 1.7 + vec2(0.0, -time * speed * 0.6));

    // amount in pixels → normalize to [0,1] texture space
    vec2 offset = vec2(n1 - 0.5, n2 - 0.5) * amount / resolution;
    vec3 color = texture2D(tex0, uv + offset).rgb;

    gl_FragColor = vec4(color, 1.0);
}
