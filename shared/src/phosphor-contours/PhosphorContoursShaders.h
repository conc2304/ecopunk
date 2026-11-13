#pragma once
// Embedded GLSL for the Phosphor Contours render core. Same conventions as shared/src/crt-phosphor:
// GLSL 1.20 (desktop GL 2.1), the same text under GLES2 via the precision header (UNTESTED on Pi),
// every target is a plain GL_TEXTURE_2D with NORMALIZED uv. Orientation: see Pipeline::fill() (luma and
// contour flip once each, every later pass preserves).
//
// Passes:  luma -> contour -> raster -> accumulate(history) -> bright/blur -> final
//   luma     : anti-aliased 1/4 (Desktop) or 1/8 (PiSafe) luminance downsample
//   blur     : 2 separable gaussian pairs on that small target -> the STRUCTURAL luma (kBlurFrag, shared with glow)
//   structure: Isophote / RasterRelief / Hybrid line signal from the structural luma; fine detail only modulates
//              emission.  r = line signal, g = structural luma, b = line confidence
//   raster   : fixed screen-space dot/dash grid, rolling per-row analogue slip of the SAMPLE, seeded
//              event-driven dropout (luma dependent).  r = emission
//   accumulate: phosphor persistence  H = max(cur*tint, H_prev * exp(-dt/tau))
//   bright/blur: reduced-resolution glow of the history
//   final    : dark olive background + emission + glow + hot-core shift + grain, soft clamp

namespace phc {

static const char* kHeader =
	"#version 120\n"
	"#ifdef GL_ES\n"
	"#extension GL_OES_standard_derivatives : enable\n"
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n"
	"#endif\n";

static const char* kVert = R"GLSL(
varying vec2 vUV;
void main() {
	vUV = gl_MultiTexCoord0.xy;
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)GLSL";

// Deterministic, cheap hash + value noise (defined here; nothing external).
static const char* kNoise = R"GLSL(
float hash21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}
float vnoise(vec2 p) {
	vec2 i = floor(p), f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
	           mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x), f.y);
}
)GLSL";

// Pass 1: structural-luma downsample. The target is 1/lumaDiv of the source; four bilinear taps (each a 2x2 box)
// cover the footprint of one destination texel, so the downsample is anti-aliased, not point-sampled.
// Output r=g=b=luma so the shared separable blur (kBlurFrag) can run on it unchanged.
static const char* kLumaFrag = R"GLSL(
uniform sampler2D tex0;
uniform vec2  uTexel;     // 1 / source size
uniform float uOff;       // tap offset in source texels (= lumaDiv / 4)
varying vec2 vUV;
float L(vec2 uv) { return dot(texture2D(tex0, uv).rgb, vec3(0.2126, 0.7152, 0.0722)); }
void main() {
	vec2 o = uTexel * uOff;
	float b = 0.25 * (L(vUV + vec2(-o.x, -o.y)) + L(vUV + vec2(o.x, -o.y)) + L(vUV + vec2(-o.x, o.y)) + L(vUV + vec2(o.x, o.y)));
	gl_FragColor = vec4(b, b, b, 1.0);
}
)GLSL";

// Pass 3: STRUCTURE. Writes the line signal into the contour target: r = line (0..1), g = structural luma,
// b = line confidence (1 = a real line, <1 = bands too dense to resolve; the raster substitutes tone stipple).
// All geometry comes from the blurred structural luma (tex0). The raw source (tex1) only modulates emission.
// Isophote slope is taken from texture differences, NOT fwidth(): no derivative extension is needed, so there is
// no GLES2 OES_standard_derivatives dependency in this path.
static const char* kStructureFrag = R"GLSL(
uniform sampler2D tex0;   // structural luma (blurred, linear)
uniform sampler2D tex1;   // source colour (detail only)
uniform vec2  uRes;       // contour target size, px
uniform vec2  uTexel;     // 1 / structural target size
uniform float uTime, uSeed;
uniform float uWIso, uWRel;
uniform float uLo, uHi, uContrast, uGamma, uDetail;
uniform float uLevels, uIsoHalf, uAA, uGradEmph, uLumaFloor;
uniform float uWarpX, uWarpY, uWarpRate, uWarpScale;
uniform float uSpacing, uAmp, uPivot, uRelHalf, uDrift, uVert;
varying vec2 vUV;

float remap(float l) {
	float x = clamp((l - uLo) / max(uHi - uLo, 1e-3), 0.0, 1.0);
	x = clamp((x - 0.5) * uContrast + 0.5, 0.0, 1.0);
	return pow(max(x, 1e-4), uGamma);
}
float S(vec2 uv) {
	vec2 h = uTexel * 0.5;
	return remap(texture2D(tex0, clamp(uv, h, 1.0 - h)).r);
}
vec2 warp(vec2 uv) {
	float t = uTime * uWarpRate;
	float a = vnoise(vec2(uv.y * uWarpScale * 5.0 + uSeed, t)) - 0.5;
	float b = vnoise(vec2(uv.y * uWarpScale * 1.6 + 9.0 + uSeed, t * 0.55 + 3.0)) - 0.5;
	float dx = (a * 0.6 + b * 1.0) * 2.0 * uWarpX / uRes.x;
	float dy = (vnoise(vec2(uv.x * uWarpScale * 2.0 + 4.0, uv.y * uWarpScale * 2.0 + t * 0.7 + uSeed)) - 0.5) * 2.0 * uWarpY / uRes.y;
	return uv + vec2(dx, dy);
}

// Isophotes of the structural luma. Width is constant in output px (distance to the threshold divided by the
// local band slope), so thin lines stay continuous instead of aliasing into dots.
float isophote(out float conf) {
	vec2 w = clamp(warp(vUV), 0.0, 1.0);
	float Ls = S(w);
	float sx = (S(w + vec2(uTexel.x, 0.0)) - S(w - vec2(uTexel.x, 0.0))) / (2.0 * uTexel.x);
	float sy = (S(w + vec2(0.0, uTexel.y)) - S(w - vec2(0.0, uTexel.y))) / (2.0 * uTexel.y);
	float fw = uLevels * length(vec2(sx / uRes.x, sy / uRes.y));   // band phase per output px
	float fwc = max(fw, 0.002);
	float v = Ls * uLevels;
	float fr = fract(v);
	float d = min(fr, 1.0 - fr);                                   // phase distance to nearest threshold
	float line = 1.0 - smoothstep(uIsoHalf - uAA, uIsoHalf + uAA, d / fwc);
	// flat plateaus near a threshold would otherwise fill in as blobs: hard-gate tiny slopes, then optional emphasis
	float gradVis = smoothstep(0.004, 0.02, fw) * mix(1.0, smoothstep(0.02, 0.08, fw), uGradEmph * 0.5);
	conf = smoothstep(5.0 * uIsoHalf, 10.0 * uIsoHalf, 1.0 / fwc); // band period (px) too small for the line width -> fade
	return line * gradVis * conf;
}

float structAt(float along, float across) {
	return S(uVert > 0.5 ? vec2(across / uRes.x, along / uRes.y) : vec2(along / uRes.x, across / uRes.y));
}
// Displaced scanlines as an implicit curve family: each base line yb is pushed by amp*(S(x,yb)-pivot). For a pixel,
// recover the base coordinate by fixed-point iteration (a contraction while |dD/dy| < 1, which the amplitude cap
// and smooth structural luma guarantee), then measure distance to the nearest base line scaled by the local slope.
// No loop over lines, no geometry; cost is ~7 taps.
float relief() {
	vec2 px = vUV * uRes;
	float along = uVert > 0.5 ? px.y : px.x;
	float across = uVert > 0.5 ? px.x : px.y;
	float yb = across;
	for (int i = 0; i < 4; i++) yb = across - uAmp * (structAt(along, yb) - uPivot);
	float dD = uAmp * (structAt(along, yb + 2.0) - structAt(along, yb - 2.0)) * 0.25;   // dD/dyb
	float slope = clamp(1.0 + dD, 0.25, 4.0);
	float u = (yb - uDrift * uTime) / uSpacing;
	float fr = fract(u);
	float dpx = min(fr, 1.0 - fr) * uSpacing * slope;
	return 1.0 - smoothstep(uRelHalf - uAA, uRelHalf + uAA, dpx);
}

void main() {
	float conf = 1.0, sig = 0.0;
	if (uWIso > 0.001) { float ci; sig += uWIso * isophote(ci); conf = mix(conf, ci, uWIso); }
	if (uWRel > 0.001) sig += uWRel * relief();
	float Lm = S(vUV);
	float lumaVis = smoothstep(0.04, 0.12, Lm) * mix(uLumaFloor, 1.0, smoothstep(0.12, 0.55, Lm));
	// fine detail (raw - structural) modulates emission only; the source texture is un-flipped relative to vUV
	float raw = dot(texture2D(tex1, vec2(vUV.x, 1.0 - vUV.y)).rgb, vec3(0.2126, 0.7152, 0.0722));
	float det = raw - texture2D(tex0, vUV).r;
	float mod_ = 1.0 + uDetail * clamp(det * 6.0, -1.0, 1.0);
	gl_FragColor = vec4(clamp(sig * lumaVis * mod_, 0.0, 1.0), Lm, conf, 1.0);
}
)GLSL";

static const char* kRasterFrag = R"GLSL(
uniform sampler2D tex0;   // contour
uniform vec2  uRes;
uniform float uTime, uSeed;
uniform vec2  uCell;      // phosphor cell size in px (w = dash length, h = row pitch)
uniform float uDotRadius; // 0..~0.6 of the cell
uniform float uDropout, uFlickHz, uJitter, uStagger;
uniform float uRowAmp, uRowRate, uSlip, uFill, uTone, uSnap;
varying vec2 vUV;
void main() {
	vec2 px = vUV * uRes;
	float row = floor(px.y / uCell.y);
	float stag = mod(row, 2.0) * uStagger * uCell.x * 0.5;
	float cx = floor((px.x - stag) / uCell.x);
	vec2 cell = vec2(cx, row);
	vec2 local = vec2(fract((px.x - stag) / uCell.x), fract(px.y / uCell.y)) - 0.5;
	// static per-cell bead irregularity (hash, not time): no popping
	local -= (vec2(hash21(cell + uSeed), hash21(cell + 17.3 + uSeed)) - 0.5) * uJitter;
	// rolling analogue slip: the SAMPLE position slides along each row (grid itself stays fixed)
	float rn = vnoise(vec2(row * 0.09 + uSeed, uTime * uRowRate)) - 0.5;
	float band = vnoise(vec2(row * 0.017 + 3.0, uTime * 0.12 + uSeed * 2.0));
	float slip = smoothstep(0.72, 0.95, band) * (hash21(vec2(row, floor(uTime * 0.5))) - 0.5) * 2.0;
	float shift = rn * 2.0 * uRowAmp + slip * uSlip;
	vec2 cpx = vec2((cx + 0.5) * uCell.x + stag + shift, (row + 0.5) * uCell.y);
	vec2 cuv = clamp(cpx / uRes, 0.0, 1.0);
	vec4 c = texture2D(tex0, cuv);                         // per-cell luma (stable dropout / fill decisions)
	// each phosphor is a WINDOW onto the continuous contour field (not a point sample), so dashes follow the
	// band and bands narrower than a cell do not alias into speckle
	float sig = texture2D(tex0, clamp((px + vec2(shift, 0.0)) / uRes, 0.0, 1.0)).r;
	// cell snap: a phosphor lights fully if the line crosses ANYWHERE in its cell, so lines thinner than a cell
	// are never lost between dot centres (the dot mask still gives the dotted CRT look)
	vec2 q = uCell * 0.33 / uRes;
	float cov = max(max(c.r, sig), max(max(texture2D(tex0, clamp(cuv + vec2(q.x, 0.0), 0.0, 1.0)).r, texture2D(tex0, clamp(cuv - vec2(q.x, 0.0), 0.0, 1.0)).r),
	                                   max(texture2D(tex0, clamp(cuv + vec2(0.0, q.y), 0.0, 1.0)).r, texture2D(tex0, clamp(cuv - vec2(0.0, q.y), 0.0, 1.0)).r)));
	sig = mix(sig, cov, uSnap);
	float Lm = c.g;
	float fill = uFill * smoothstep(0.80, 1.0, Lm);  // bright masses merge
	float rr = length(local / max(uDotRadius, 0.05));
	float mask = 1.0 - smoothstep(0.78, 1.0, rr);
	// seeded dropout; each cell re-rolls on its own slow event clock, so there is never a global new mask
	float key = floor(uTime * uFlickHz + hash21(cell + 5.1 + uSeed) * 7.0);
	float h = hash21(cell * 1.31 + vec2(key * 1.7, key * 3.1) + uSeed);
	float thr = uDropout * (1.0 - 0.8 * smoothstep(0.35, 0.95, Lm));  // highlights keep coverage
	float alive = smoothstep(thr, thr + 0.08, h);
	float shimmer = 0.88 + 0.12 * hash21(cell + key);
	// tonal stipple where the contour is not resolvable: density follows luma (dark = sparse, bright = merged)
	float dens = (1.0 - c.b) * uTone * smoothstep(0.14, 0.80, Lm);
	float stip = step(hash21(cell * 0.77 + 7.0 + uSeed), dens) * step(0.002, dens);  // dens==0 must never light (hash can be exactly 0)
	float e = clamp(max(sig, stip * 0.55) + fill, 0.0, 1.0) * mask * alive * shimmer;
	gl_FragColor = vec4(e, e, e, 1.0);
}
)GLSL";

// Diagnostic only: shows one channel of the structure target as grey (chan 0 = line signal, 1 = structural luma).
static const char* kMonoFrag = R"GLSL(
uniform sampler2D tex0;
uniform float uChan;
varying vec2 vUV;
void main() {
	vec4 c = texture2D(tex0, vUV);
	float v = uChan > 0.5 ? c.g : c.r;
	gl_FragColor = vec4(v, v, v, 1.0);
}
)GLSL";

// Range statistics: mean-pools the structural luma onto a tiny grid (16 taps per texel) for the CPU percentile read.
static const char* kStatsFrag = R"GLSL(
uniform sampler2D tex0;
uniform vec2 uCellUV;     // size of one destination texel in source uv
varying vec2 vUV;
void main() {
	float a = 0.0;
	for (int j = 0; j < 4; j++)
		for (int i = 0; i < 4; i++)
			a += texture2D(tex0, vUV + (vec2(float(i), float(j)) * 0.25 - vec2(0.375)) * uCellUV).r;
	a *= 0.0625;
	gl_FragColor = vec4(a, a, a, 1.0);
}
)GLSL";

static const char* kAccumFrag = R"GLSL(
uniform sampler2D tex0;   // raster emission
uniform sampler2D tex1;   // previous history
uniform vec3  uTint;
uniform float uDecay;     // exp(-dt/tau); 0 = no persistence
varying vec2 vUV;
void main() {
	vec3 cur = texture2D(tex0, vUV).rrr * uTint;
	vec3 prev = texture2D(tex1, vUV).rgb * uDecay;
	gl_FragColor = vec4(max(cur, prev), 1.0);
}
)GLSL";

static const char* kBrightFrag = R"GLSL(
uniform sampler2D tex0;
uniform float uThresh;
varying vec2 vUV;
void main() {
	vec3 c = texture2D(tex0, vUV).rgb;
	float m = max(c.r, max(c.g, c.b));
	gl_FragColor = vec4(c * smoothstep(uThresh, uThresh + 0.25, m), 1.0);
}
)GLSL";

static const char* kBlurFrag = R"GLSL(
uniform sampler2D tex0;
uniform vec2 uDir;       // texel step (already scaled by radius)
varying vec2 vUV;
void main() {
	vec3 s = texture2D(tex0, vUV).rgb * 0.2270;
	s += (texture2D(tex0, vUV + uDir * 1.385).rgb + texture2D(tex0, vUV - uDir * 1.385).rgb) * 0.3162;
	s += (texture2D(tex0, vUV + uDir * 3.231).rgb + texture2D(tex0, vUV - uDir * 3.231).rgb) * 0.0703;
	gl_FragColor = vec4(s, 1.0);
}
)GLSL";

static const char* kFinalFrag = R"GLSL(
uniform sampler2D tex0;   // history (sharp emission)
uniform sampler2D tex1;   // glow
uniform vec2  uRes;
uniform float uTime;
uniform vec3  uTint, uBg;
uniform float uGain, uBloom, uGrain, uVignette;
varying vec2 vUV;
void main() {
	vec3 e = texture2D(tex0, vUV).rgb * uGain;
	vec3 g = texture2D(tex1, vUV).rgb * uBloom;
	vec3 col = uBg + e + g;
	// locally overexposed cores drift toward a hot pale citron
	float hot = smoothstep(0.85, 1.6, max(col.r, col.g));
	col = mix(col, vec3(1.0, 1.0, 0.62) * max(col.g, 0.0), 0.35 * hot);
	// grain on an ~18 Hz clock (not per-frame white noise)
	vec2 px = vUV * uRes;
	float n = hash21(px + floor(uTime * 18.0) * 7.31) - 0.5;
	col += n * uGrain * (0.35 + col.g);
	vec2 q = vUV - 0.5;
	col *= 1.0 - uVignette * dot(q, q) * 1.6;
	col = col / (1.0 + 0.18 * max(col - 0.85, 0.0));   // soft shoulder, safe for 8-bit
	gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
)GLSL";

} // namespace phc
