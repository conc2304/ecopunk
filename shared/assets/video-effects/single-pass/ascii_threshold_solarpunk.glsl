// ascii_threshold_solarpunk.frag
// GLSL 120 — Thresholded ASCII-style solarpunk glyph overlay.
//
// Uniforms:
//   tex                : source image / video
//   resolution         : screen / render size in pixels
//   alpha              : overall layer opacity (driven by slot lifecycle; 0=invisible, 1=full)
//   cellSize           : glyph cell size in pixels (4 – 32)
//   thresholdMin       : luminance lower bound for threshold test
//   thresholdMax       : luminance upper bound for threshold test
//   thresholdMode      : 0=none  1=aboveMin  2=belowMax  3=betweenRange
//   opacity            : within-cell glyph blend (0=source, 1=full glyph)
//   contrast           : luma contrast remap  (1.0 = neutral)
//   bias               : luma bias remap  (-0.5 .. 0.5)
//   softness           : glyph edge AA width  (0.01 – 0.15)
//   asciiColorMode     : 0 = sampled source color   1 = black and white
//   asciiInvertMono    : 0 = white glyphs (default)  1 = black glyphs
//                        (only relevant when asciiColorMode == 1)
//   asciiBackgroundMode: 0 = original image behind non-glyph pixels
//                        1 = transparent behind non-glyph pixels
//
// Glyph density ramp (16 levels, low → high luma):
//   0  space     1  .    2  ,    3  :    4  -    5  +
//   6  =         7  o    8  x    9  *   10  <>  11  sun
//  12  #        13  bloom  14  ▭  15  █

#version 120

uniform sampler2D tex;
uniform vec2      resolution;

uniform float alpha;
uniform float cellSize;
uniform float thresholdMin;
uniform float thresholdMax;
uniform int   thresholdMode;

uniform float opacity;
uniform float contrast;
uniform float bias;
uniform float softness;

uniform int asciiColorMode;
uniform int asciiInvertMono;
uniform int asciiBackgroundMode;

varying vec2 vTexCoord;

// ── SDF helpers ───────────────────────────────────────────────────────────────

float luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

float stroke(float d, float w, float aa) {
    return 1.0 - smoothstep(w, w + aa, d);
}

float fillCircle(vec2 p, float r, float aa) {
    return 1.0 - smoothstep(r, r + aa, length(p));
}

float ring(vec2 p, float r, float w, float aa) {
    return stroke(abs(length(p) - r), w, aa);
}

float plusGlyph(vec2 p, float w, float aa) {
    return max(stroke(abs(p.x), w, aa), stroke(abs(p.y), w, aa));
}

float crossGlyph(vec2 p, float w, float aa) {
    return max(
        stroke(abs(p.x - p.y) * 0.7071, w, aa),
        stroke(abs(p.x + p.y) * 0.7071, w, aa)
    );
}

float diamondGlyph(vec2 p, float r, float aa) {
    return 1.0 - smoothstep(r, r + aa, abs(p.x) + abs(p.y));
}

float boxGlyph(vec2 p, float r, float aa) {
    return 1.0 - smoothstep(r, r + aa, max(abs(p.x), abs(p.y)));
}

// ── 16-level glyph ramp ───────────────────────────────────────────────────────
// p is in [-1, 1] (local cell coords). t is remapped luma in [0, 1].
// Density increases with t.
float denseGlyphRamp(float t, vec2 p, float aa) {

    if (t < 0.0625) return 0.0;   // space

    if (t < 0.1250)                // .  tiny dot
        return fillCircle(p, 0.07, aa);

    if (t < 0.1875)                // ,  small dot
        return fillCircle(p, 0.13, aa);

    if (t < 0.2500)                // :  colon
        return max(fillCircle(p + vec2(0.0,  0.23), 0.10, aa),
                   fillCircle(p - vec2(0.0,  0.23), 0.10, aa));

    if (t < 0.3125)                // -  dash
        return stroke(abs(p.y), 0.08, aa)
             * (1.0 - smoothstep(0.40, 0.52, abs(p.x)));

    if (t < 0.3750)                // +  plus
        return plusGlyph(p, 0.08, aa);

    if (t < 0.4375)                // =  equals
        return max(stroke(abs(p.y - 0.20), 0.08, aa),
                   stroke(abs(p.y + 0.20), 0.08, aa))
             * (1.0 - smoothstep(0.40, 0.52, abs(p.x)));

    if (t < 0.5000)                // o  ring
        return ring(p, 0.37, 0.07, aa);

    if (t < 0.5625)                // x  cross
        return crossGlyph(p, 0.08, aa);

    if (t < 0.6250)                // *  asterisk (plus + cross)
        return max(plusGlyph(p, 0.07, aa), crossGlyph(p, 0.07, aa));

    if (t < 0.6875)                // <>  diamond outline
        return diamondGlyph(p, 0.48, aa);

    if (t < 0.7500)                // sun  (ring + plus)
        return max(ring(p, 0.30, 0.06, aa), plusGlyph(p, 0.07, aa));

    if (t < 0.8125)                // #  hash  (4-line grid)
        return max(
            max(stroke(abs(p.y - 0.28), 0.07, aa),
                stroke(abs(p.y + 0.28), 0.07, aa)),
            max(stroke(abs(p.x - 0.28), 0.07, aa),
                stroke(abs(p.x + 0.28), 0.07, aa))
        );

    if (t < 0.8750)                // bloom  (ring + asterisk)
        return max(max(ring(p, 0.32, 0.06, aa),
                       plusGlyph(p, 0.07, aa)),
                   crossGlyph(p, 0.06, aa));

    if (t < 0.9375)                // ▭  box frame (hollow box)
        return boxGlyph(p, 0.62, aa) * (1.0 - boxGlyph(p, 0.38, aa));

    return boxGlyph(p, 0.68, aa);  // █  solid block
}

// ── Threshold test ────────────────────────────────────────────────────────────

bool passesThreshold(float lum) {
    if (thresholdMode == 1) return lum >= thresholdMin;
    if (thresholdMode == 2) return lum <= thresholdMax;
    if (thresholdMode == 3) return lum >= thresholdMin && lum <= thresholdMax;
    return true;
}

// ── Main ──────────────────────────────────────────────────────────────────────

void main() {
    vec2 uv  = vTexCoord;
    vec3 src = texture2D(tex, uv).rgb;

    // Cell-center sampling
    vec2 px           = uv * resolution;
    vec2 cell         = floor(px / cellSize);
    vec2 cellCenterUv = (cell + 0.5) * cellSize / resolution;
    vec3 cellColor    = texture2D(tex, cellCenterUv).rgb;

    float lum = luma(cellColor);
    lum = clamp(((lum - 0.5) * contrast) + 0.5 + bias, 0.0, 1.0);

    // Non-ASCII cell: show background or transparent
    if (!passesThreshold(lum)) {
        if (asciiBackgroundMode == 1)
            gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        else
            gl_FragColor = vec4(src, alpha);
        return;
    }

    // Local cell coords in [-1, 1]
    vec2  local  = fract(px / cellSize) * 2.0 - 1.0;
    float aa     = max(softness, 1.5 / max(cellSize, 1.0));
    float glyphV = denseGlyphRamp(lum, local, aa);

    // Glyph color
    vec3 glyphColor;
    if (asciiColorMode == 1) {
        glyphColor = (asciiInvertMono == 1) ? vec3(0.0) : vec3(1.0);
    } else {
        glyphColor = cellColor;
    }

    // Composite
    if (asciiBackgroundMode == 1) {
        // Transparent mode: glyph pixels carry their own alpha
        gl_FragColor = vec4(glyphColor, glyphV * opacity * alpha);
    } else {
        // Original image mode: glyph overlays source; whole layer fades with alpha
        vec3 finalColor = mix(src, glyphColor, glyphV * opacity);
        gl_FragColor = vec4(finalColor, alpha);
    }
}
