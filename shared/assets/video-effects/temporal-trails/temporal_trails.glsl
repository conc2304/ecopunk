#version 120

uniform sampler2D currentTex;
uniform sampler2D previousTex;
uniform float decay; // 0.85 to 0.98
uniform float currentWeight; // 0.1 to 0.4
uniform float brighten; // 1.0 to 1.2

varying vec2 vTexCoord;

void main() {
    vec2 uv = vTexCoord;
    vec3 current = texture2D(currentTex, uv).rgb;
    vec3 previous = texture2D(previousTex, uv).rgb * decay;

    vec3 color = max(previous, current * currentWeight) * brighten;
    color = mix(color, current, 1.0 - decay);

    gl_FragColor = vec4(color, 1.0);
}
