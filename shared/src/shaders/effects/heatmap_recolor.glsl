#version 120
uniform sampler2D tex;
uniform float alpha;          // heatmap.mix: 0 = original, 1 = full heatmap
uniform float intensity;      // heatmap.intensity, 0..2
uniform float gamma;          // heatmap.gamma, 0.25..3.0
uniform float minLuminance;   // heatmap.minLuminance, 0..1
uniform float maxLuminance;   // heatmap.maxLuminance, 0..1
uniform int   palette;        // heatmap.palette: 0=thermal 1=bioluminescent 2=solarpunk 3=infrared
uniform int   reverse;        // heatmap.reverse: 0/1
varying vec2 vTexCoord;

vec3 ramp5(float t, vec3 c0, vec3 c1, vec3 c2, vec3 c3, vec3 c4) {
    float seg = clamp(t, 0.0, 1.0) * 4.0;
    float f = fract(seg);
    if (seg < 1.0) return mix(c0, c1, f);
    if (seg < 2.0) return mix(c1, c2, f);
    if (seg < 3.0) return mix(c2, c3, f);
    return mix(c3, c4, f);
}

vec3 paletteColor(float t, int pal) {
    if (pal == 1) { // Bioluminescent
        return ramp5(t, vec3(0.01, 0.02, 0.03), vec3(0.00, 0.20, 0.22),
                        vec3(0.00, 0.65, 0.75), vec3(0.55, 0.95, 0.35), vec3(0.85, 1.00, 0.90));
    } else if (pal == 2) { // Solarpunk Botanical
        return ramp5(t, vec3(0.06, 0.07, 0.06), vec3(0.06, 0.30, 0.14),
                        vec3(0.45, 0.65, 0.15), vec3(0.85, 0.60, 0.20), vec3(0.97, 0.94, 0.82));
    } else if (pal == 3) { // Infrared Vegetation
        return ramp5(t, vec3(0.02, 0.02, 0.08), vec3(0.55, 0.05, 0.55),
                        vec3(0.85, 0.20, 0.10), vec3(0.98, 0.75, 0.10), vec3(1.00, 1.00, 1.00));
    }
    return ramp5(t, vec3(0.00, 0.00, 0.05), vec3(0.00, 0.05, 0.55),   // Thermal Classic (default)
                    vec3(0.10, 0.80, 0.55), vec3(0.95, 0.55, 0.05), vec3(1.00, 1.00, 1.00));
}

void main() {
    vec4 source = texture2D(tex, vTexCoord);
    float luminance = dot(source.rgb, vec3(0.2126, 0.7152, 0.0722));

    // GLSL zero-inits uniforms that C++ never explicitly binds. This effect's
    // documented defaults are non-zero (maxLuminance=1.0, gamma=1.0,
    // intensity=1.0), so treat an exact 0.0 as "not set" and fall back —
    // otherwise sketches that only bind tex/alpha (no per-effect dispatch)
    // would render washed-out or black instead of a sane default heatmap.
    float lo = clamp(minLuminance, 0.0, 1.0);
    float hi = maxLuminance > 0.0 ? clamp(maxLuminance, 0.0, 1.0) : 1.0;
    float range = max(hi - lo, 0.0001);
    float mapped = clamp((luminance - lo) / range, 0.0, 1.0);

    float safeGamma = gamma > 0.0 ? gamma : 1.0;
    mapped = pow(mapped, max(safeGamma, 0.0001));
    if (reverse != 0) mapped = 1.0 - mapped;

    // paletteColor()'s if/else-if chain already falls through to Thermal
    // Classic for any value outside 1..3 (including negative), so no
    // separate clamp is needed here — and GLSL 1.20's clamp()/min()/max()
    // built-ins only have float-genType overloads, not int ones, so
    // clamp(palette, 0, 3) with an int uniform fails to compile at all.
    vec3 heatColor = paletteColor(mapped, palette);
    float safeIntensity = intensity > 0.0 ? intensity : 1.0;
    heatColor = clamp(heatColor * safeIntensity, 0.0, 1.0);

    vec3 finalColor = mix(source.rgb, heatColor, clamp(alpha, 0.0, 1.0));
    gl_FragColor = vec4(clamp(finalColor, 0.0, 1.0), source.a);
}
