#pragma once
// Embedded GLSL for the crt-phosphor sketch. Shaders live in source (not only
// in the gitignored bin/data/) so the sketch is reproducible from a clean
// checkout. Target: GLSL 1.20 (desktop GL 2.1 compat, same convention as every
// other sketch here) and the same text under GLES2 via the precision header.
// Only the desktop path has been run; the Pi / GLES2 path is UNTESTED.
//
// UV convention: every pass draws the same full-screen quad whose texcoords
// are (x/w, 1 - y/h), so all intermediate render targets are plain
// GL_TEXTURE_2D with normalized UVs and orientation is preserved pass to pass.

namespace crt {

static const char* kHeader =
	"#version 120\n"
	"#ifdef GL_ES\n"
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n"
	"#endif\n";

static const char* kVert = R"GLSL(
varying vec2 vUV;
void main() {
	vUV = gl_MultiTexCoord0.xy;
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)GLSL";

// ============================================================ DISPLAY STAGE
// Shared GLSL for the base shader and every display structure. Order inside a
// structure program:  source (analog shift, optional interlace field offsets)
//   -> prep (black/gain/gamma, per channel)  -> color mapper (palette axis)
//   -> structure mask (emitter geometry in OUTPUT pixel space)  -> RGB out.
// Palette colors are display-referred sRGB (no gamma conversion anywhere).
static const char* kCommon = R"GLSL(
uniform sampler2D tex0;
uniform vec2  uRes;
uniform float uTime;
uniform float uBlack, uGain, uGamma;
uniform float uScanPitch, uScanDepth;
uniform float uDrift, uBand, uTear;
// theme / mono palette + mode accent
uniform vec3  uPrimary, uPeak, uSecondary;
uniform float uSecInf;
uniform float uAccentMode;
// color mapping axis
uniform int   uPalMode;       // 0 mono/theme, 1 source RGB, 2 ramp, 3 EGA, 4 dual phosphor
uniform vec3  uRamp[8];
uniform float uRampN;
uniform float uStepped;       // 1 = hard bands (four-tone)
uniform vec3  uEga[16];
uniform vec3  uDual[5];       // baseLo, baseHi, energizedLo, energizedHi, deep
uniform int   uDither;        // 0 off, 1 low (2x2), 2 high (4x4)
// behavior axis
uniform int   uBehavior;      // 0 none, 1 adaptive mosaic, 2 floating phosphors, 3 interlaced colour fields
uniform float uBhA;           // floating energy
uniform float uBhB;           // interlace field separation (px)
uniform float uFieldHz;
uniform float uT1, uT2;       // mosaic detail thresholds
uniform float uSeed;
varying vec2 vUV;

const vec3 W709 = vec3(0.2126, 0.7152, 0.0722);
float gRow = 0.0;             // current scan/cell row, used by interlace

float hash(vec2 p) {
	p = fract(p * vec2(0.1031, 0.1030));
	p += dot(p, p.yx + 33.33);
	return fract((p.x + p.y) * p.x);
}

// Per-line drift + rolling sync band + rare tear, as horizontal uv shift.
float analogDx(vec2 uv, float row) {
	float dx = (hash(vec2(row, floor(uTime * 24.0))) - 0.5) * uDrift;
	float bandY = 1.0 - fract(uTime * 0.11);
	float d = (uv.y - bandY) / 0.07;
	dx += uBand * exp(-d * d) * sin(uv.y * 90.0 + uTime * 9.0) * 0.006;
	float slot = floor(uTime * 3.0);
	if (hash(vec2(slot, 7.0)) < uTear) {
		float y0 = hash(vec2(slot, 11.0));
		float hgt = 0.02 + 0.08 * hash(vec2(slot, 13.0));
		if (uv.y > y0 && uv.y < y0 + hgt) dx += (hash(vec2(slot, 17.0)) - 0.5) * 0.18;
	}
	return dx;
}

// Source fetch. Interlaced colour fields: honest channel/field PHASE offsets
// (R and B displaced horizontally in opposite directions, G alternates a
// scan line per field row). No historical frame is used.
vec3 srcAt(vec2 uv) {
	vec2 u = clamp(uv, vec2(0.0), vec2(1.0));
	if (uBehavior == 3) {
		float o = uBhB / uRes.x;
		float field = mod(floor(uTime * uFieldHz) + gRow, 2.0);
		float oy = (field - 0.5) * 2.0 / uRes.y * uBhB;
		return vec3(texture2D(tex0, clamp(u + vec2(o, 0.0), 0.0, 1.0)).r,
		            texture2D(tex0, clamp(u + vec2(0.0, oy), 0.0, 1.0)).g,
		            texture2D(tex0, clamp(u - vec2(o, 0.0), 0.0, 1.0)).b);
	}
	return texture2D(tex0, u).rgb;
}

vec3 prepSrc(vec3 c) { return pow(clamp((c - uBlack) * uGain, 0.0, 1.0), vec3(uGamma)); }
float lumaOf(vec3 c) { return dot(c, W709); }

// Cheap forward-difference edge strength on prepped luma (0..1).
float edgeAt(vec2 uv) {
	float l0 = lumaOf(prepSrc(srcAt(uv)));
	float lx = lumaOf(prepSrc(srcAt(uv + vec2(2.0 / uRes.x, 0.0))));
	float ly = lumaOf(prepSrc(srcAt(uv + vec2(0.0, 2.0 / uRes.y))));
	return clamp((abs(lx - l0) + abs(ly - l0)) * 2.5, 0.0, 1.0);
}

// Ordered dither threshold in [0,1) anchored to LOGICAL cell coordinates.
float bayer(vec2 cell) {
	vec2 p = mod(floor(cell), 4.0);
	float x0 = mod(p.x, 2.0), y0 = mod(p.y, 2.0);
	if (uDither == 1) return (2.0 * abs(x0 - y0) + y0) / 4.0;
	float x1 = floor(p.x / 2.0), y1 = floor(p.y / 2.0);
	return (8.0 * abs(x0 - y0) + 4.0 * y0 + 2.0 * abs(x1 - y1) + y1) / 16.0;
}

// Linear ramp through up to 8 stops using hat weights (constant-index loop).
vec3 rampColor(float t) {
	float x = clamp(t, 0.0, 1.0) * (uRampN - 1.0);
	vec3 c = vec3(0.0);
	for (int i = 0; i < 8; i++) c += uRamp[i] * max(1.0 - abs(x - float(i)), 0.0);
	return c;
}

// Nearest EGA entry by weighted RGB distance (real RGB quantisation).
vec3 egaNearest(vec3 c) {
	vec3 best = uEga[0];
	float bd = 1e9;
	for (int i = 0; i < 16; i++) {
		vec3 d = c - uEga[i];
		float dist = dot(d * d, vec3(0.30, 0.55, 0.15));
		if (dist < bd) { bd = dist; best = uEga[i]; }
	}
	return best;
}

// Mono / theme emission: tonal ramp from PRIMARY, SECONDARY only on high
// energy highlights and mode accents (same formula as the original base).
vec3 emitMono(float E, float accent) {
	vec3 col = mix(uPrimary, uPeak, smoothstep(0.62, 1.0, E)) * E;
	float sec = uSecInf * (0.5 * smoothstep(0.82, 1.0, E) + accent);
	return mix(col, uSecondary * max(E, 0.35 * accent), clamp(sec, 0.0, 1.0));
}

// Colour-mapping axis. c = prepped source RGB for the emitting cell.
vec3 mapColor(vec3 c, float edge, vec2 cell, float accent) {
	float L = clamp(lumaOf(c), 0.0, 1.0);
	float amp = uDither == 1 ? 0.5 : 1.0;
	float b = uDither > 0 ? bayer(cell) - 0.5 : 0.0;
	if (uPalMode == 1) return c;
	if (uPalMode == 2) {
		float t = L;
		if (uStepped > 0.5) {
			float q = floor(clamp(t + b * amp / uRampN, 0.0, 0.9999) * uRampN) / (uRampN - 1.0);
			return rampColor(q);
		}
		return rampColor(t + b * amp / (uRampN - 1.0));
	}
	if (uPalMode == 3) return egaNearest(clamp(c + b * amp * 0.25, 0.0, 1.0));
	if (uPalMode == 4) {
		float tb = clamp(L + b * amp * 0.1, 0.0, 1.0);
		vec3 base = mix(uDual[4], mix(uDual[0], uDual[1], tb), smoothstep(0.02, 0.30, L));
		float e = clamp(smoothstep(0.10, 0.35, edge) + smoothstep(0.82, 1.0, L), 0.0, 1.0);
		vec3 en = mix(uDual[2], uDual[3], tb);
		return mix(base, en, e * 0.9);
	}
	return emitMono(L, accent);
}
)GLSL";

// ---------------------------------------------------------------- CRT base
// "original_phosphor": luma -> grille (vertical, primary) + scanlines
// (secondary). With uPalMode == 0 this is BIT-FOR-BIT the original reference
// path; other colour mappings reuse the same mask geometry.
static const char* kBaseFrag = R"GLSL(
uniform float uGrillePitch, uGrilleDepth, uGrilleSharp, uBleed;

void main() {
	vec2 uv = vUV;
	vec2 px = uv * uRes;
	float row = floor(px.y / max(uScanPitch, 1.0));
	gRow = row;
	uv.x += analogDx(uv, row);

	// 3-tap horizontal beam softness (~0.8 px) keeps grille from aliasing.
	vec2 o = vec2(0.8 / uRes.x, 0.0);
	float v0 = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
	vec3 c = (srcAt(uv) * 0.5 + srcAt(uv - o) * 0.25 + srcAt(uv + o) * 0.25) * v0;

	float L = dot(c, W709);
	L = clamp((L - uBlack) * uGain, 0.0, 1.0);
	L = pow(L, uGamma);

	float ph = fract(px.x / uGrillePitch);
	float stripe = pow(0.5 + 0.5 * cos(6.2831853 * ph), uGrilleSharp);
	float bleed = smoothstep(0.55, 1.0, L) * uBleed;
	float gMask = mix(1.0 - uGrilleDepth, 1.0, stripe);
	gMask = mix(gMask, 1.0, bleed);

	float sy = 0.5 + 0.5 * cos(6.2831853 * px.y / uScanPitch);
	float sMask = mix(1.0 - uScanDepth, 1.0, sy);
	sMask = mix(sMask, 1.0, bleed * 0.6);

	float E = L * gMask * sMask;
	float accent = texture2D(tex0, clamp(uv, vec2(0.0), vec2(1.0))).a * uAccentMode;
	vec3 col;
	if (uPalMode == 0) {
		col = emitMono(E, accent);
	} else {
		vec2 cell = floor(px / vec2(uGrillePitch, max(uScanPitch, 1.0)));
		col = mapColor(prepSrc(c), edgeAt(uv), cell, accent) * gMask * sMask;
	}
	col += uPrimary * 0.012 * gMask;
	gl_FragColor = vec4(col, 1.0);
}
)GLSL";

// Structure uniforms shared by the structure programs.
static const char* kStructCommon = R"GLSL(
uniform float uCell;          // logical cell width (px, output space)
uniform float uCellAspect;    // cell height / width
uniform float uGap;           // dark mask gap / edge softness fraction
uniform float uRadius;        // emitter radius fraction
uniform float uNorm;          // brightness normalisation for sparse masks
uniform float uScan;          // secondary scanline modulation (0..1 of look depth)
float scanMask(float py) {
	return mix(1.0 - uScanDepth * uScan, 1.0, 0.5 + 0.5 * cos(6.2831853 * py / max(uScanPitch, 1.0)));
}
)GLSL";

// rgb_triad: delta-arranged R/G/B round emitters per cell, staggered rows.
static const char* kStructTriadFrag = R"GLSL(
void main() {
	vec2 px = vUV * uRes;
	float Cw = uCell, Ch = uCell * uCellAspect;
	float row = floor(px.y / Ch);
	gRow = row;
	float stag = mod(row, 2.0) * 0.5 * Cw;
	float xs = px.x + stag;
	float col = floor(xs / Cw);
	vec2 local = vec2(fract(xs / Cw) * Cw, fract(px.y / Ch) * Ch);
	vec2 centre = vec2((col + 0.5) * Cw - stag, (row + 0.5) * Ch);
	vec2 uv = centre / uRes;
	uv.x += analogDx(uv, row);
	vec3 s = prepSrc(srcAt(uv));
	vec3 mc = mapColor(s, edgeAt(uv), vec2(col, row), 0.0);
	float rad = Cw * uRadius;
	float soft = max(rad * uGap, 0.5);
	vec2 pr = vec2(0.25 * Cw, 0.28 * Ch), pg = vec2(0.75 * Cw, 0.28 * Ch), pb = vec2(0.5 * Cw, 0.74 * Ch);
	vec3 M = vec3(1.0 - smoothstep(rad - soft, rad + soft, length(local - pr)),
	              1.0 - smoothstep(rad - soft, rad + soft, length(local - pg)),
	              1.0 - smoothstep(rad - soft, rad + soft, length(local - pb)));
	gl_FragColor = vec4(mc * M * uNorm * scanMask(px.y), 1.0);
}
)GLSL";

// aperture_grille: vertical R/G/B stripes continuous in Y (long columns).
static const char* kStructGrilleFrag = R"GLSL(
void main() {
	vec2 px = vUV * uRes;
	float P = uCell;
	float lx = floor(px.x / P);
	float row = floor(px.y / max(uScanPitch, 1.0));
	gRow = row;
	vec2 uv = vec2((lx + 0.5) * P, floor(px.y) + 0.5) / uRes;
	uv.x += analogDx(uv, row);
	vec3 s = prepSrc(srcAt(uv));
	vec3 mc = mapColor(s, edgeAt(uv), vec2(lx, floor(px.y / 2.0)), 0.0);
	float f = fract(px.x / P * 3.0);
	float sub = floor(mod(px.x, P) / (P / 3.0));
	float prof = smoothstep(0.0, uGap, f) * smoothstep(1.0, 1.0 - uGap, f);
	vec3 M = vec3(sub < 0.5 ? prof : 0.0, (sub > 0.5 && sub < 1.5) ? prof : 0.0, sub > 1.5 ? prof : 0.0);
	gl_FragColor = vec4(mc * M * uNorm * scanMask(px.y), 1.0);
}
)GLSL";

// round_matrix: one round emitter per cell (sampled once per cell).
//   adaptive_mosaic : quadtree block size tiers (4C/2C/C) from local detail.
//   floating_phosphors: 3x3 neighbour emitters drifting analytically.
static const char* kStructRoundFrag = R"GLSL(
float detailAt(vec2 centre, float B) {
	vec2 q = vec2(B * 0.28);
	float a = lumaOf(prepSrc(srcAt((centre + vec2(-q.x, -q.y)) / uRes)));
	float b = lumaOf(prepSrc(srcAt((centre + vec2( q.x, -q.y)) / uRes)));
	float c = lumaOf(prepSrc(srcAt((centre + vec2(-q.x,  q.y)) / uRes)));
	float d = lumaOf(prepSrc(srcAt((centre + vec2( q.x,  q.y)) / uRes)));
	return max(max(a, b), max(c, d)) - min(min(a, b), min(c, d));
}
float dotMask(vec2 d, float r) {
	float soft = max(r * uGap, 0.5);
	return 1.0 - smoothstep(r - soft, r + soft, length(d));
}
void main() {
	vec2 px = vUV * uRes;
	float C = uCell;
	float row = floor(px.y / C);
	gRow = row;
	vec3 acc = vec3(0.0);
	if (uBehavior == 2) {
		vec2 base = floor(px / C);
		for (int j = -1; j <= 1; j++) {
			for (int i = -1; i <= 1; i++) {
				vec2 cn = base + vec2(float(i), float(j));
				vec2 c0 = (cn + 0.5) * C;
				vec2 uv0 = c0 / uRes;
				vec3 s = prepSrc(srcAt(uv0));
				float L = lumaOf(s);
				float act = clamp(L + edgeAt(uv0) * 2.0, 0.0, 1.0);        // luminance + image activity drive the drift
				float h = hash(cn + uSeed * 17.0);
				float ph = 6.2831853 * (fract(uTime / 6.0) + h);            // periodic, loop-friendly
				vec2 dir = vec2(cos(ph), sin(ph * 2.0 + h * 6.2831853) * 0.7);
				vec2 off = dir * uBhA * C * (0.12 + 0.88 * act);
				float r = C * uRadius * (0.85 + 0.45 * act * uBhA);
				float m = dotMask(px - (c0 + off), r);
				acc += mapColor(s, 0.0, cn, 0.0) * m;
			}
		}
	} else {
		float B = C;
		if (uBehavior == 1) {
			vec2 c4 = (floor(px / (4.0 * C)) + 0.5) * 4.0 * C;
			if (detailAt(c4, 4.0 * C) < uT2) B = 4.0 * C;
			else {
				vec2 c2 = (floor(px / (2.0 * C)) + 0.5) * 2.0 * C;
				B = detailAt(c2, 2.0 * C) < uT1 ? 2.0 * C : C;
			}
		}
		vec2 cell = floor(px / B);
		vec2 centre = (cell + 0.5) * B;
		vec2 uv = centre / uRes;
		uv.x += analogDx(uv, floor(centre.y / C));
		vec3 s = prepSrc(srcAt(uv));
		float L = lumaOf(s);
		float r = B * uRadius * (0.7 + 0.3 * sqrt(L));
		acc = mapColor(s, edgeAt(uv), cell * (B / C), 0.0) * dotMask(px - centre, r);
	}
	gl_FragColor = vec4(acc * uNorm * scanMask(px.y), 1.0);
}
)GLSL";

// ascii_matrix: luminance -> glyph index (never RGB-quantised); GPU atlas.
static const char* kStructAsciiFrag = R"GLSL(
uniform sampler2D uAtlas;
uniform float uGlyphN;
uniform vec2  uCellSize;
void main() {
	vec2 px = vUV * uRes;
	vec2 cell = floor(px / uCellSize);
	vec2 centre = (cell + 0.5) * uCellSize;
	vec2 loc = fract(px / uCellSize);
	gRow = cell.y;
	vec2 q = uCellSize * 0.25;
	vec3 s = (srcAt((centre + vec2(-q.x, -q.y)) / uRes) + srcAt((centre + vec2(q.x, -q.y)) / uRes)
	        + srcAt((centre + vec2(-q.x,  q.y)) / uRes) + srcAt((centre + vec2(q.x,  q.y)) / uRes)) * 0.25;
	s = prepSrc(s);
	float L = clamp(lumaOf(s), 0.0, 1.0);
	float idx = floor(L * (uGlyphN - 1.0) + 0.5);
	// crop to the glyph ink area of the atlas cell so neighbouring glyphs sit tight
	vec2 auv = vec2((idx + mix(0.12, 0.88, loc.x)) / uGlyphN, mix(0.20, 0.84, loc.y));
	float a = texture2D(uAtlas, auv).r;
	vec3 mc = mapColor(s, edgeAt(centre / uRes), cell, 0.0);
	gl_FragColor = vec4(mc * (a * uNorm + 0.04) * scanMask(px.y), 1.0);
}
)GLSL";

// vector_beam: NOT a raster mask. Wide-Sobel edges + quantised iso-contours
// rendered as beam-width lines with selective emission and a scan pulse. This
// is a shader-derived approximation, not polyline extraction / beam traversal.
static const char* kStructVectorFrag = R"GLSL(
uniform float uBeamW;
uniform float uLevels;
uniform float uEdgeT;
float lumAt(vec2 p) { return lumaOf(prepSrc(srcAt(p / uRes))); }
void main() {
	vec2 px = vUV * uRes;
	gRow = floor(px.y / 4.0);
	float r = max(1.0, uBeamW * 0.5);
	float tl = lumAt(px + vec2(-r,  r)), t = lumAt(px + vec2(0.0,  r)), tr = lumAt(px + vec2(r,  r));
	float l  = lumAt(px + vec2(-r, 0.0)), L = lumAt(px),                 rr = lumAt(px + vec2(r, 0.0));
	float bl = lumAt(px + vec2(-r, -r)), b = lumAt(px + vec2(0.0, -r)), br = lumAt(px + vec2(r, -r));
	float gx = (tr + 2.0 * rr + br) - (tl + 2.0 * l + bl);
	float gy = (tl + 2.0 * t + tr) - (bl + 2.0 * b + br);
	float gmag = length(vec2(gx, gy)) / (8.0 * r);          // luma change per pixel
	float edgeLine = smoothstep(uEdgeT, uEdgeT * 2.5, gmag * r * 2.0);
	float f = L * uLevels;
	float dl = abs(f - floor(f + 0.5));
	float pixDist = dl / max(uLevels * gmag, 1e-4);          // px distance to the nearest iso-contour
	float contour = (1.0 - smoothstep(0.0, uBeamW, pixDist)) * smoothstep(0.004, 0.03, gmag);
	float beam = max(edgeLine * 0.95, contour * 0.65);
	float sweep = fract(uTime * 0.22);                       // scan pulse accent
	float pulse = exp(-pow((vUV.x - sweep) * 7.0, 2.0));
	beam *= 0.55 + 0.45 * smoothstep(0.1, 0.5, L + edgeLine) + 1.3 * pulse * edgeLine;
	vec3 s = prepSrc(srcAt(vUV));
	float mx = max(max(s.r, s.g), max(s.b, 0.06));
	vec3 ci = (s / mx) * (0.4 + 0.6 * L);
	vec3 mc = mapColor(ci, edgeLine, floor(px / 4.0), 0.0);
	float core = smoothstep(0.75, 1.0, beam);
	vec3 col = mc * beam * uNorm;
	col = mix(col, max(col, uPeak * core * 0.9), core * 0.5);
	gl_FragColor = vec4(col, 1.0);
}
)GLSL";

// -------------------------------------------------------------- bright pass
static const char* kBrightFrag = R"GLSL(
uniform sampler2D tex0;
uniform vec2  uTexel;     // texel size of the SOURCE
uniform float uThresh;
varying vec2 vUV;
void main() {
	vec3 s = texture2D(tex0, vUV + uTexel * vec2(-0.5, -0.5)).rgb
	       + texture2D(tex0, vUV + uTexel * vec2( 0.5, -0.5)).rgb
	       + texture2D(tex0, vUV + uTexel * vec2(-0.5,  0.5)).rgb
	       + texture2D(tex0, vUV + uTexel * vec2( 0.5,  0.5)).rgb;
	s *= 0.25;
	float l = max(s.g, max(s.r, s.b));
	float k = max(l - uThresh, 0.0) / max(l, 1e-4);
	gl_FragColor = vec4(s * k, 1.0);
}
)GLSL";

// ------------------------------------------- separable 9-tap blur (5 fetches)
static const char* kBlurFrag = R"GLSL(
uniform sampler2D tex0;
uniform vec2 uDir;        // (texel,0) or (0,texel)
varying vec2 vUV;
void main() {
	vec3 c = texture2D(tex0, vUV).rgb * 0.2270270270;
	c += (texture2D(tex0, vUV + uDir * 1.3846153846).rgb + texture2D(tex0, vUV - uDir * 1.3846153846).rgb) * 0.3162162162;
	c += (texture2D(tex0, vUV + uDir * 3.2307692308).rgb + texture2D(tex0, vUV - uDir * 3.2307692308).rgb) * 0.0702702703;
	gl_FragColor = vec4(c, 1.0);
}
)GLSL";

// ---------------------------------------------- composite + persistence + FX
// base + bloom -> analog layer (noise, streaks, flicker, vignette)
// -> persistence max(cur, prev*decay). prev is a DIFFERENT texture from the
// render target (ping-pong), so no read/write hazard.
static const char* kCompositeFrag = R"GLSL(
uniform sampler2D tex0;   // CRT base
uniform sampler2D tex1;   // bloom
uniform sampler2D tex2;   // history (previous frame's output)
uniform vec2  uRes;
uniform float uTime;
uniform float uBloom;
uniform float uDecay;     // exp(-dt/tau); 0 disables persistence
uniform float uNoise, uStreak, uFlicker, uVignette;
varying vec2 vUV;

float hash(vec2 p) {
	p = fract(p * vec2(0.1031, 0.1030));
	p += dot(p, p.yx + 33.33);
	return fract((p.x + p.y) * p.x);
}

void main() {
	vec2 px = vUV * uRes;
	vec3 col = texture2D(tex0, vUV).rgb + texture2D(tex1, vUV).rgb * uBloom;

	// Soft shoulder: keeps bloom from clipping into a flat white/yellow mass.
	col = col / (col * 0.35 + 0.65);

	// Sparse vertical streaks (2 px wide columns, re-rolled ~6 Hz).
	float cell = floor(px.x / 2.0);
	float h  = hash(vec2(cell, floor(uTime * 6.0)));
	float hy = hash(vec2(cell, floor(px.y / 90.0) + floor(uTime * 6.0)));
	float streak = step(0.995, h) * hy * uStreak;
	col += vec3(0.05, 0.55, 0.20) * streak;

	// Noise, weighted so the black field stays mostly black.
	float n = hash(px + fract(uTime) * 311.0) - 0.5;
	col.g += n * uNoise * (0.15 + col.g);
	col.r += n * uNoise * 0.25 * (0.15 + col.g);
	col = max(col, 0.0);

	// Flicker.
	col *= 1.0 + uFlicker * (sin(uTime * 47.0) * 0.5 + hash(vec2(floor(uTime * 60.0), 3.0)) - 0.5);

	// Edge vignette.
	vec2 q = vUV - 0.5;
	col *= 1.0 - uVignette * smoothstep(0.25, 0.85, dot(q, q) * 2.0);

	// Persistence: a dying phosphor never goes below its decayed past.
	// History memory is vertically flipped w.r.t. the quad's UVs (the final
	// present flips it back), so read it with a flipped V to stay in register.
	vec3 prev = texture2D(tex2, vec2(vUV.x, 1.0 - vUV.y)).rgb * uDecay - (0.6 / 255.0) * step(0.001, uDecay);
	col = max(col, prev);

	gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
)GLSL";

// ============================================================ MODE STAGE
// A mode is an image/signal construction step that runs BEFORE the shared
// phosphor conversion. Output goes to a "signal frame" (rgb = scene signal,
// a = optional accent mask or mode-private data). Mode passes are drawn with
// the orientation-PRESERVING quad, so history is read with plain UVs.

// raster_excavation: a bright, narrow band sweeps (dt-driven phase supplied by
// the host) and energizes the image; energized image decays exp(-dt/tau).
static const char* kModeExcavationFrag = R"GLSL(
uniform sampler2D tex0;   // source
uniform sampler2D tex1;   // previous signal frame (rgb energy, a accent)
uniform float uPhase;     // 0..1 sweep position, from accumulated dt
uniform float uBandW;     // band half-width (uv)
uniform float uDecay;     // exp(-dt/tau), host-computed
uniform float uVertical;  // 1 = sweeps top->bottom, 0 = left->right
uniform float uReverse;   // 1 = reverse direction
varying vec2 vUV;
void main() {
	vec3 s = texture2D(tex0, vUV).rgb;
	float pos = uVertical > 0.5 ? (1.0 - vUV.y) : vUV.x;
	if (uReverse > 0.5) pos = 1.0 - pos;
	float d = (pos - uPhase) / max(uBandW, 1e-4);
	float band = exp(-d * d);
	vec3 cur = s * (0.10 + 1.25 * band);          // leading band energizes the image
	vec3 prev = texture2D(tex1, vUV).rgb * uDecay - 0.6 / 255.0;
	gl_FragColor = vec4(clamp(max(cur, prev), 0.0, 1.0), band);
}
)GLSL";

// phosphor_memory: GPU luma difference vs the previous frame's luma (kept in
// the history alpha) weights both brightness and persistence of moving areas.
// This is a frame-difference proxy, not optical flow.
static const char* kModeMemoryFrag = R"GLSL(
uniform sampler2D tex0;
uniform sampler2D tex1;   // previous signal (rgb energy, a = previous source luma)
uniform float uDt;
uniform float uTau;       // base afterimage time constant (s)
uniform float uMotionGain;
uniform float uMotionTau; // extra tau multiplier on moving pixels
varying vec2 vUV;
void main() {
	vec3 s = texture2D(tex0, vUV).rgb;
	float l = dot(s, vec3(0.2126, 0.7152, 0.0722));
	vec4 prev = texture2D(tex1, vUV);
	float m = clamp(abs(l - prev.a) * uMotionGain, 0.0, 1.0);
	float tau = uTau * (1.0 + uMotionTau * m);
	float decay = exp(-uDt / max(tau, 1e-3));
	vec3 cur = s * (0.55 + 0.45 * m);
	vec3 hist = prev.rgb * decay - 0.6 / 255.0;
	gl_FragColor = vec4(clamp(max(cur, hist + s * m * 0.25), 0.0, 1.0), l);
}
)GLSL";

// signal_columns: groups of output-space columns (no single-pixel shimmer)
// sample with deterministic per-group offsets plus a tiny phase drift. Spatial
// / time-modulated displacement only: NO delayed sampling (no frame history).
static const char* kModeColumnsFrag = R"GLSL(
uniform sampler2D tex0;
uniform float uGroupPx;
uniform float uAmp;       // max vertical displacement (uv)
uniform float uDrift;     // phase drift rate
uniform float uTime;
varying vec2 vUV;
float hash(float n) { return fract(sin(n * 127.1 + 311.7) * 43758.5453); }
void main() {
	float g = floor(gl_FragCoord.x / max(uGroupPx, 1.0));
	float h1 = hash(g), h2 = hash(g + 71.0);
	float ph = sin(uTime * uDrift * (0.4 + h2) + h1 * 6.2831853);
	float dy = (h1 - 0.5) * 2.0 * uAmp * (0.65 + 0.35 * ph);
	float dx = (h2 - 0.5) * uAmp * 0.12 * ph;
	vec2 uv = clamp(vUV + vec2(dx, dy), vec2(0.0), vec2(1.0));
	vec3 c = texture2D(tex0, uv).rgb;
	float seam = step(1.0 / max(uGroupPx, 1.0), fract(gl_FragCoord.x / max(uGroupPx, 1.0)));
	c *= mix(0.55, 1.0, seam);                    // thin gap between column groups
	gl_FragColor = vec4(c, 0.0);
}
)GLSL";

} // namespace crt
