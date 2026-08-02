#version 120

uniform sampler2D tex0;
uniform vec2 resolution;
uniform float edgeStrength; // 0.5 to 3.0
uniform float glowStrength; // 0.0 to 2.0
uniform vec3 glowColor; // e.g. vec3(0.3, 1.0, 0.55)

varying vec2 vTexCoord;

float luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 uv = vTexCoord;
    vec2 px = 1.0 / resolution;

    float tl = luma(texture2D(tex0, uv + vec2(-px.x, -px.y)).rgb);
    float  t = luma(texture2D(tex0, uv + vec2(  0.0, -px.y)).rgb);
    float tr = luma(texture2D(tex0, uv + vec2( px.x, -px.y)).rgb);
    float  l = luma(texture2D(tex0, uv + vec2(-px.x,   0.0)).rgb);
    float  r = luma(texture2D(tex0, uv + vec2( px.x,   0.0)).rgb);
    float bl = luma(texture2D(tex0, uv + vec2(-px.x,  px.y)).rgb);
    float  b = luma(texture2D(tex0, uv + vec2(  0.0,  px.y)).rgb);
    float br = luma(texture2D(tex0, uv + vec2( px.x,  px.y)).rgb);

    float gx = -tl - 2.0*l - bl + tr + 2.0*r + br;
    float gy = -tl - 2.0*t - tr + bl + 2.0*b + br;
    float edge = clamp(length(vec2(gx, gy)) * edgeStrength, 0.0, 1.0);

    vec3 src = texture2D(tex0, uv).rgb;
    vec3 glow = glowColor * edge * glowStrength;

    gl_FragColor = vec4(src + glow, 1.0);
}
