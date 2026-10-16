#version 120
uniform sampler2D tex;
uniform float     saturation; // 0 = fully grayscale, 1 = original color
uniform float     opacity;    // final output alpha
varying vec2 vTexCoord;
void main() {
    vec3  original = texture2D(tex, vTexCoord).rgb;
    float luma     = dot(original, vec3(0.299, 0.587, 0.114));
    vec3  desat    = mix(vec3(luma), original, saturation);
    gl_FragColor   = vec4(desat, opacity);
}
