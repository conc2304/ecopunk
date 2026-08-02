#version 120

uniform sampler2D tex0;
uniform vec2 resolution;
uniform float time;
uniform float amplitude; // 2.0 to 12.0 px
uniform float frequency; // 0.005 to 0.04
uniform float speed;     // 0.2 to 2.0

varying vec2 vTexCoord;

void main() {
    vec2 uv = vTexCoord;

    // multiply by resolution so frequency stays in pixel-frequency units
    float px = uv.x * resolution.x;
    float py = uv.y * resolution.y;

    float waveA = sin((py * frequency) + time * speed);
    float waveB = cos((px * frequency * 1.7) - time * speed * 0.73);
    float waveC = sin(((px + py) * frequency * 0.8) + time * speed * 1.31);

    // amplitude in pixels → normalize to texture space
    vec2 offset = vec2(waveA + waveC * 0.5, waveB) * amplitude / resolution;
    vec3 color = texture2D(tex0, uv + offset).rgb;

    gl_FragColor = vec4(color, 1.0);
}
