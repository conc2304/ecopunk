#pragma once

// MosaicShape — OF-free tile silhouette geometry for the Mosaic Assembly scene.
//
// A Mask is four corner treatments (TL, TR, BR, BL). Each corner radius is a FRACTION of min(w,h) in 0..1
// (1.0 on a single corner = a quarter-circle motif; 0.5 on two adjacent corners = semicircle/pill end;
// 0.5 on all four of a square = circle), or a chamfer (clipped corner) when the flag is set. Radii are
// clamped so adjacent corners never overlap and the outline NEVER leaves its rect.
//
// The same outline clips video and solid colour identically (the scene tessellates one polygon for both).

#include <algorithm>
#include <cmath>
#include <vector>

namespace mosaic {

struct Pt { float x, y; };

struct Mask {
	float r[4] = {0, 0, 0, 0};          // TL, TR, BR, BL (fraction of min(w,h))
	bool ch[4] = {false, false, false, false};
	bool isSquare() const { for (int i = 0; i < 4; ++i) if (r[i] > 0.001f) return false; return true; }
};

inline Mask maskSquare() { return Mask{}; }
inline Mask maskRounded(float r) { Mask m; for (float& v : m.r) v = r; return m; }
inline Mask maskCorners(float tl, float tr, float br, float bl) { Mask m; m.r[0] = tl; m.r[1] = tr; m.r[2] = br; m.r[3] = bl; return m; }
inline Mask maskChamfer(bool tl, bool tr, bool br, bool bl, float size = 0.3f) {
	Mask m; bool f[4] = {tl, tr, br, bl};
	for (int i = 0; i < 4; ++i) if (f[i]) { m.r[i] = size; m.ch[i] = true; }
	return m;
}
inline Mask lerpMask(const Mask& a, const Mask& b, float t) {
	Mask m;
	for (int i = 0; i < 4; ++i) { m.r[i] = a.r[i] + (b.r[i] - a.r[i]) * t; m.ch[i] = t < 0.5f ? a.ch[i] : b.ch[i]; }
	return m;
}
inline bool sameMask(const Mask& a, const Mask& b) {
	for (int i = 0; i < 4; ++i) if (std::fabs(a.r[i] - b.r[i]) > 0.002f || (a.ch[i] != b.ch[i] && (a.r[i] > 0.001f || b.r[i] > 0.001f))) return false;
	return true;
}

// Clockwise outline of the masked rect (screen coordinates, y down). All points lie inside [x,x+w]x[y,y+h].
inline void maskOutline(float x, float y, float w, float h, const Mask& m, std::vector<Pt>& out, int seg = 6) {
	out.clear();
	const float s = std::min(w, h);
	float r[4]; for (int i = 0; i < 4; ++i) r[i] = std::max(0.f, m.r[i]) * s;
	float k = 1.f;
	auto lim = [&](float a, float b, float len) { if (a + b > len && a + b > 1e-4f) k = std::min(k, len / (a + b)); };
	lim(r[0], r[1], w); lim(r[3], r[2], w); lim(r[0], r[3], h); lim(r[1], r[2], h);
	for (float& v : r) v *= k;
	const float cx[4] = {x + r[0], x + w - r[1], x + w - r[2], x + r[3]};
	const float cy[4] = {y + r[0], y + r[1], y + h - r[2], y + h - r[3]};
	const float a0[4] = {3.14159265f, 4.71238898f, 0.f, 1.57079633f};
	for (int c = 0; c < 4; ++c) {
		if (r[c] < 0.4f) { const float px = (c == 0 || c == 3) ? x : x + w, py = (c == 0 || c == 1) ? y : y + h; out.push_back({px, py}); continue; }
		if (m.ch[c]) { // chamfer: two points
			const float a = a0[c], b = a0[c] + 1.57079633f;
			out.push_back({cx[c] + r[c] * std::cos(a), cy[c] + r[c] * std::sin(a)});
			out.push_back({cx[c] + r[c] * std::cos(b), cy[c] + r[c] * std::sin(b)});
			continue;
		}
		const int n = std::max(3, std::min(seg + 2, int(r[c] * 0.5f) + 3));
		for (int i = 0; i <= n; ++i) { const float a = a0[c] + 1.57079633f * float(i) / float(n); out.push_back({cx[c] + r[c] * std::cos(a), cy[c] + r[c] * std::sin(a)}); }
	}
	for (Pt& p : out) { p.x = std::clamp(p.x, x, x + w); p.y = std::clamp(p.y, y, y + h); }
}

inline float polygonArea(const std::vector<Pt>& p) {
	float a = 0; for (size_t i = 0; i < p.size(); ++i) { const Pt& u = p[i]; const Pt& v = p[(i + 1) % p.size()]; a += u.x * v.y - v.x * u.y; }
	return std::fabs(a) * 0.5f;
}

} // namespace mosaic
