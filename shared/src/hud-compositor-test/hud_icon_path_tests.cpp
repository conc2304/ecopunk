// HudIconPath — the HUD's SVG path-data reader (OF-free).
#include "../hud-compositor/HudIconPath.h"

#include <iostream>
#include <string>

namespace {
int g_total = 0, g_failures = 0;
void check(bool c, const std::string& what) {
	g_total++;
	if (!c) { g_failures++; std::cerr << "FAIL: " << what << "\n"; }
}
void bounds(const std::vector<hudicon::Contour>& cs, float& x0, float& y0, float& x1, float& y1) {
	x0 = y0 = 1e9f; x1 = y1 = -1e9f;
	for (const auto& c : cs) for (const auto& p : c.pts) { x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y); }
}
} // namespace

int main() {
	using namespace hudicon;
	std::vector<Contour> sun, moon, t;
	check(parse(lightModePath(), sun), "light_mode path parses");
	check(sun.size() == 9, "sun = disc + 4 axis rays + 4 diagonal rays (9 contours), got " + std::to_string(sun.size()));
	float x0, y0, x1, y1;
	bounds(sun, x0, y0, x1, y1);
	check(x0 >= 0.9f && y0 >= 0.9f && x1 <= 23.1f && y1 <= 23.1f, "sun stays inside the 24 viewBox");
	check(x0 < 1.5f && x1 > 22.5f, "sun spans the viewBox width (rays reach the edges)");

	check(parse(darkModePath(), moon), "dark_mode path parses");
	check(moon.size() == 1, "moon is one contour");
	bounds(moon, x0, y0, x1, y1);
	check(x0 >= 2.9f && x1 <= 21.1f && y0 >= 2.9f && y1 <= 21.1f, "moon stays inside its 3..21 box");

	check(parse("M0 0h10v10H0z", t) && t.size() == 1 && t[0].pts.size() == 4 && t[0].closed, "h/v/H/z square");
	check(parse("m1 1 2 0 0 2z", t) && t[0].pts.size() == 3 && t[0].pts[1].x == 3.0f && t[0].pts[2].y == 3.0f, "relative m with implicit lineto pairs");
	check(!parse("M0 0a5 5 0 0 1 10 0", t) && t.empty(), "arcs are rejected, not drawn wrong");
	check(!parse("", t) && !parse("L5 5", t) && !parse("M0", t), "empty / no-moveto / truncated numbers fail");
	check(parse("M0 0c.5.5 1.5.5 2 0", t) && t[0].pts.back().x > 1.99f, "numbers packed like '.5.5' split correctly");

	// Navigation icons: every control has one; scene_next is the mirrored scene_previous.
	for (const char* id : {"controls.scene_previous", "controls.scene_next", "controls.media_previous", "controls.media_next", "controls.fullscreen", "controls.menu", "controls.mode"}) {
		bool mirrored = false;
		const char* d = buttonIconPath(id, mirrored);
		check(d != nullptr && parse(d, t) && !t.empty(), std::string("nav icon parses: ") + id);
		bounds(t, x0, y0, x1, y1);
		check(x0 >= 0.0f && y0 >= 0.0f && x1 <= 24.0f && y1 <= 24.0f, std::string("nav icon inside the viewBox: ") + id);
	}
	bool mp = false, mn = false;
	std::vector<Contour> prev, next;
	parse(buttonIconPath("controls.scene_previous", mp), prev);
	parse(buttonIconPath("controls.scene_next", mn), next);
	mirrorX(next);
	check(!mp && mn, "only scene_next is flagged mirrored");
	float px0, py0, px1, py1, nx0, ny0, nx1, ny1;
	bounds(prev, px0, py0, px1, py1);
	bounds(next, nx0, ny0, nx1, ny1);
	check(nx0 > 24.0f - px1 - 0.01f && px0 > 0.0f, "mirrored scene_next sits on the opposite side (chevrons point right)");
	check(buttonIconPath("controls.unknown", mp) == nullptr, "unknown region has no icon");

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
