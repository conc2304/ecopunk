#version 120

uniform sampler2D tex0;
uniform vec2 resolution;
uniform float amount; // 0.5 to 3.0 px
uniform float radial; // 0.0 to 1.0

varying vec2 vTexCoord;

void main() {
    vec2 uv = vTexCoord;
    vec2 center = vec2(0.5, 0.5);
    vec2 dir = uv - center;
    vec2 pxScale = 1.0 / resolution;
    // linear: horizontal shift; radial: shift away from center
    vec2 offset = mix(vec2(amount * pxScale.x, 0.0),
                      normalize(dir + 0.0001) * amount * pxScale.x,
                      radial);

    float r = texture2D(tex0, uv + offset).r;
    float g = texture2D(tex0, uv).g;
    float b = texture2D(tex0, uv - offset).b;

    gl_FragColor = vec4(r, g, b, 1.0);
}
