#version 120
uniform sampler2D tex;
uniform float     hueOffset;      // degrees, initial rotation (0-360)
uniform float     hueSpeed;       // degrees/sec, signed — 0 holds at hueOffset
uniform float     time;           // seconds, drives the rotation over time
uniform float     saturationMult;
uniform float     valueMult;
uniform float     alpha;
varying vec2 vTexCoord;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec4  original = texture2D(tex, vTexCoord);
    vec3  hsv       = rgb2hsv(original.rgb);
    float degrees   = hueOffset + hueSpeed * time;
    hsv.x = fract(hsv.x + degrees / 360.0);
    hsv.y = clamp(hsv.y * saturationMult, 0.0, 1.0);
    hsv.z = clamp(hsv.z * valueMult, 0.0, 1.0);
    vec3  effect = hsv2rgb(hsv);
    gl_FragColor = vec4(mix(original.rgb, effect, alpha), original.a);
}
