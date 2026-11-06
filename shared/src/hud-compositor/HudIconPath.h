#pragma once

// ============================================================================
// HudIconPath.h — tiny SVG path-data reader for the HUD's flat single-colour
// icons (24 x 24 viewBox, fill only). OF-free; covered by
// hud-compositor-test/hud_icon_path_tests.cpp. The widget turns the contours
// into an ofPath and fills it with a theme colour, so an icon follows the
// palette exactly like text does.
//
// Supported commands: M m L l H h V v C c S s Z z (all the HUD's icons need;
// arcs are not supported and make parse() fail rather than draw a wrong
// shape). The icon sources live in assets/hud/icons/*.svg; the same path data
// is embedded below so the HUD has no file dependency at draw time.
// ============================================================================

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace hudicon {

struct Pt { float x = 0.0f, y = 0.0f; };
struct Contour {
	std::vector<Pt> pts;
	bool closed = false;
};

// assets/hud/icons/light_mode.svg and dark_mode.svg (Material Symbols, Apache 2.0).
inline const char* lightModePath() {
	return "M12 7c-2.76 0-5 2.24-5 5s2.24 5 5 5 5-2.24 5-5-2.24-5-5-5M2 13h2c.55 0 1-.45 1-1s-.45-1-1-1H2c-.55 0-1 .45-1 1s.45 1 1 1m18 0h2c.55 0 1-.45 1-1s-.45-1-1-1h-2c-.55 0-1 .45-1 1s.45 1 1 1M11 2v2c0 .55.45 1 1 1s1-.45 1-1V2c0-.55-.45-1-1-1s-1 .45-1 1m0 18v2c0 .55.45 1 1 1s1-.45 1-1v-2c0-.55-.45-1-1-1s-1 .45-1 1M5.99 4.58c-.39-.39-1.03-.39-1.41 0-.39.39-.39 1.03 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0s.39-1.03 0-1.41zm12.37 12.37c-.39-.39-1.03-.39-1.41 0-.39.39-.39 1.03 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0 .39-.39.39-1.03 0-1.41zm1.06-10.96c.39-.39.39-1.03 0-1.41-.39-.39-1.03-.39-1.41 0l-1.06 1.06c-.39.39-.39 1.03 0 1.41s1.03.39 1.41 0zM7.05 18.36c.39-.39.39-1.03 0-1.41-.39-.39-1.03-.39-1.41 0l-1.06 1.06c-.39.39-.39 1.03 0 1.41s1.03.39 1.41 0z";
}
inline const char* darkModePath() {
	return "M12 3c-4.97 0-9 4.03-9 9s4.03 9 9 9 9-4.03 9-9c0-.46-.04-.92-.1-1.36-.98 1.37-2.58 2.26-4.4 2.26-2.98 0-5.4-2.42-5.4-5.4 0-1.81.89-3.42 2.26-4.4-.44-.06-.9-.1-1.36-.1";
}

// Navigation-button icons (assets/hud/icons/nav_*.svg, Material Symbols, Apache 2.0), keyed by
// the HUD control region id. scene_next is the scene_previous double chevron mirrored
// horizontally (`mirrored` = true); every other icon is drawn as-is. nullptr = no icon.
inline const char* buttonIconPath(const std::string& regionId, bool& mirrored) {
	mirrored = false;
	if (regionId == "controls.scene_previous") return "M17.59 18 19 16.59 14.42 12 19 7.41 17.59 6l-6 6zM11 18l1.41-1.41L7.83 12l4.58-4.59L11 6l-6 6z";
	if (regionId == "controls.scene_next") { mirrored = true; return "M17.59 18 19 16.59 14.42 12 19 7.41 17.59 6l-6 6zM11 18l1.41-1.41L7.83 12l4.58-4.59L11 6l-6 6z"; }
	if (regionId == "controls.media_previous") return "M18.41 16.59 13.82 12l4.59-4.59L17 6l-6 6 6 6zM6 6h2v12H6z";
	if (regionId == "controls.media_next") return "M5.59 7.41 10.18 12l-4.59 4.59L7 18l6-6-6-6zM16 6h2v12h-2z";
	if (regionId == "controls.fullscreen") return "M21 11V3h-8l3.29 3.29-10 10L3 13v8h8l-3.29-3.29 10-10z";
	if (regionId == "controls.menu") return "M3 18h18v-2H3zm0-5h18v-2H3zm0-7v2h18V6z";
	if (regionId == "controls.mode") return "M12 4V1L8 5l4 4V6c3.31 0 6 2.69 6 6 0 1.01-.25 1.97-.7 2.8l1.46 1.46C19.54 15.03 20 13.57 20 12c0-4.42-3.58-8-8-8m0 14c-3.31 0-6-2.69-6-6 0-1.01.25-1.97.7-2.8L5.24 7.74C4.46 8.97 4 10.43 4 12c0 4.42 3.58 8 8 8v3l4-4-4-4z";
	return nullptr;
}

namespace detail {
inline bool readNumber(const std::string& s, size_t& i, float& out) {
	while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == ',')) ++i;
	const size_t start = i;
	if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
	bool digits = false;
	while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; digits = true; }
	if (i < s.size() && s[i] == '.') {
		++i;
		while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; digits = true; }
	}
	if (!digits) { i = start; return false; }
	if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
		size_t j = i + 1;
		if (j < s.size() && (s[j] == '-' || s[j] == '+')) ++j;
		if (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) {
			while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) ++j;
			i = j;
		}
	}
	out = static_cast<float>(std::atof(s.substr(start, i - start).c_str()));
	return true;
}
inline void cubic(std::vector<Pt>& pts, Pt p0, Pt p1, Pt p2, Pt p3) {
	constexpr int kSteps = 12;
	for (int k = 1; k <= kSteps; ++k) {
		const float t = static_cast<float>(k) / kSteps, u = 1.0f - t;
		pts.push_back({u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x,
			u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y});
	}
}
} // namespace detail

// Flattens `d` into polylines. Returns false (and clears `out`) on an
// unsupported command or malformed numbers.
inline bool parse(const std::string& d, std::vector<Contour>& out) {
	out.clear();
	size_t i = 0;
	char cmd = 0;
	Pt cur, start, lastCtrl;
	bool lastWasCubic = false;
	auto fail = [&]() { out.clear(); return false; };
	auto skipSpace = [&]() { while (i < d.size() && (std::isspace(static_cast<unsigned char>(d[i])) || d[i] == ',')) ++i; };
	auto nums = [&](float* v, int n) {
		for (int k = 0; k < n; ++k) if (!detail::readNumber(d, i, v[k])) return false;
		return true;
	};
	while (true) {
		skipSpace();
		if (i >= d.size()) break;
		if (std::isalpha(static_cast<unsigned char>(d[i]))) cmd = d[i++];
		else if (cmd == 0) return fail();
		else if (cmd == 'M') cmd = 'L'; // implicit repeats: extra pairs after M/m are lineto
		else if (cmd == 'm') cmd = 'l';
		const bool rel = std::islower(static_cast<unsigned char>(cmd)) != 0;
		const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));
		float v[6];
		bool cubicNow = false;
		switch (up) {
			case 'M':
				if (!nums(v, 2)) return fail();
				cur = rel ? Pt{cur.x + v[0], cur.y + v[1]} : Pt{v[0], v[1]};
				start = cur;
				out.push_back({});
				out.back().pts.push_back(cur);
				break;
			case 'L': case 'H': case 'V': {
				Pt p = cur;
				if (up == 'L') { if (!nums(v, 2)) return fail(); p = rel ? Pt{cur.x + v[0], cur.y + v[1]} : Pt{v[0], v[1]}; }
				else if (up == 'H') { if (!nums(v, 1)) return fail(); p.x = rel ? cur.x + v[0] : v[0]; }
				else { if (!nums(v, 1)) return fail(); p.y = rel ? cur.y + v[0] : v[0]; }
				if (out.empty()) return fail();
				cur = p;
				out.back().pts.push_back(cur);
				break;
			}
			case 'C': case 'S': {
				Pt c1;
				if (up == 'C') {
					if (!nums(v, 6)) return fail();
					c1 = rel ? Pt{cur.x + v[0], cur.y + v[1]} : Pt{v[0], v[1]};
					const Pt c2 = rel ? Pt{cur.x + v[2], cur.y + v[3]} : Pt{v[2], v[3]};
					const Pt e = rel ? Pt{cur.x + v[4], cur.y + v[5]} : Pt{v[4], v[5]};
					if (out.empty()) return fail();
					detail::cubic(out.back().pts, cur, c1, c2, e);
					lastCtrl = c2; cur = e;
				} else {
					if (!nums(v, 4)) return fail();
					c1 = lastWasCubic ? Pt{2 * cur.x - lastCtrl.x, 2 * cur.y - lastCtrl.y} : cur;
					const Pt c2 = rel ? Pt{cur.x + v[0], cur.y + v[1]} : Pt{v[0], v[1]};
					const Pt e = rel ? Pt{cur.x + v[2], cur.y + v[3]} : Pt{v[2], v[3]};
					if (out.empty()) return fail();
					detail::cubic(out.back().pts, cur, c1, c2, e);
					lastCtrl = c2; cur = e;
				}
				cubicNow = true;
				break;
			}
			case 'Z':
				if (out.empty()) return fail();
				out.back().closed = true;
				cur = start;
				// A command after Z with no new M starts a subpath at the start point.
				skipSpace();
				if (i < d.size() && !std::isalpha(static_cast<unsigned char>(d[i]))) return fail();
				break;
			default: return fail(); // arcs and anything else: not supported
		}
		// After a closing Z, a following relative/absolute draw command continues from `start`;
		// if the next token is a draw command (not M/m), open a fresh contour there.
		if (up == 'Z') {
			size_t j = i;
			while (j < d.size() && (std::isspace(static_cast<unsigned char>(d[j])) || d[j] == ',')) ++j;
			if (j < d.size() && std::toupper(static_cast<unsigned char>(d[j])) != 'M') {
				out.push_back({});
				out.back().pts.push_back(cur);
			}
		}
		lastWasCubic = cubicNow;
	}
	// A path that never closed an explicit shape still fills as closed.
	for (auto& c : out) if (c.pts.size() >= 3) c.closed = true;
	return !out.empty();
}

// Mirrors contours across the 24-unit viewBox's vertical centre line (x -> 24 - x).
inline void mirrorX(std::vector<Contour>& contours) {
	for (auto& c : contours) {
		for (auto& p : c.pts) p.x = 24.0f - p.x;
		std::reverse(c.pts.begin(), c.pts.end()); // keep the winding direction consistent
	}
}

} // namespace hudicon
