uniform sampler2D accumulated;  // previous frame FBO
uniform sampler2D videoFrame;   // current video texture
uniform float     decayRate;    // 0.92–0.98, driven by LFO_GRID_DECAY
uniform float     videoAlpha;   // 0.10–0.20

varying vec2 vTexCoord;

void main() {
    vec4 history = texture2D(accumulated, vTexCoord);
    vec4 current = texture2D(videoFrame,  vTexCoord);

    vec4 result = history * decayRate + current * videoAlpha;
    gl_FragColor = clamp(result, 0.0, 1.0);
}
