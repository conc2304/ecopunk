// RT-007: PresentationTransform (canonical 1280x720 -> framebuffer, contain)
// and the EXPERIENCE_RUNTIME_FULLSCREEN override parser. Pure; no OF/GL.
#include "PresentationTransform.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int g_total = 0;
int g_failures = 0;
void check(bool ok, const std::string& what, int line) {
	++g_total;
	if (!ok) {
		++g_failures;
		std::cout << "FAIL line " << line << ": " << what << "\n";
	}
}
#define CHECK(expr) check((expr), #expr, __LINE__)
bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }
} // namespace

int main() {
	// 1280x720: identity -> the direct draw path.
	{
		const auto t = PresentationTransform::contain(1280, 720);
		CHECK(t.isIdentity());
		CHECK(near(t.scale, 1.0f) && near(t.offsetX, 0.0f) && near(t.offsetY, 0.0f));
	}
	// Rock Pi target 1600x900: 1.25, no letterbox.
	{
		const auto t = PresentationTransform::contain(1600, 900);
		CHECK(!t.isIdentity());
		CHECK(near(t.scale, 1.25f));
		CHECK(near(t.offsetX, 0.0f) && near(t.offsetY, 0.0f));
		CHECK(near(t.drawWidth(), 1600.0f) && near(t.drawHeight(), 900.0f));
		// Round trip and corners.
		CHECK(near(t.toCanvasX(1600.0f), 1280.0f) && near(t.toCanvasY(900.0f), 720.0f));
		CHECK(near(t.toFramebufferX(t.toCanvasX(777.0f)), 777.0f));
		CHECK(near(t.toCanvasX(800.0f), 640.0f) && near(t.toCanvasY(450.0f), 360.0f));
	}
	// 1920x1080 (desktop fullscreen): 1.5, no letterbox.
	{
		const auto t = PresentationTransform::contain(1920, 1080);
		CHECK(near(t.scale, 1.5f) && near(t.offsetX, 0.0f) && near(t.offsetY, 0.0f));
	}
	// 16:10 (1920x1200): width-limited, 60 px bars top and bottom.
	{
		const auto t = PresentationTransform::contain(1920, 1200);
		CHECK(near(t.scale, 1.5f) && near(t.offsetX, 0.0f) && near(t.offsetY, 60.0f));
		CHECK(!PresentationTransform::insideCanvas(t.toCanvasX(10.0f), t.toCanvasY(30.0f))); // in the top bar
		CHECK(PresentationTransform::insideCanvas(t.toCanvasX(10.0f), t.toCanvasY(70.0f)));
	}
	// Ultra-wide 2560x1080: height-limited, pillarbox.
	{
		const auto t = PresentationTransform::contain(2560, 1080);
		CHECK(near(t.scale, 1.5f) && near(t.offsetX, 320.0f) && near(t.offsetY, 0.0f));
	}
	// Retina-style 2560x1440 framebuffer for a 1280x720 logical window: 2.0.
	{
		const auto t = PresentationTransform::contain(2560, 1440);
		CHECK(near(t.scale, 2.0f) && !t.isIdentity());
	}
	// Smaller than canonical (e.g. a clamped 1080x608 window): scales down.
	{
		const auto t = PresentationTransform::contain(1080, 608);
		CHECK(near(t.scale, 1080.0f / 1280.0f) && t.offsetY >= 0.0f);
	}
	// Degenerate (minimized): identity values, no division by zero.
	{
		const auto t = PresentationTransform::contain(0, 0);
		CHECK(near(t.scale, 1.0f));
	}
	// Change detection is by framebuffer size.
	CHECK(PresentationTransform::contain(1600, 900) == PresentationTransform::contain(1600, 900));
	CHECK(PresentationTransform::contain(1600, 900) != PresentationTransform::contain(1280, 720));

	// EXPERIENCE_RUNTIME_FULLSCREEN parsing.
	unsetenv("EXPERIENCE_RUNTIME_FULLSCREEN");
	CHECK(!fullscreenOverrideFromEnv().has_value());
	setenv("EXPERIENCE_RUNTIME_FULLSCREEN", "1", 1);
	CHECK(fullscreenOverrideFromEnv() == std::optional<bool>(true));
	setenv("EXPERIENCE_RUNTIME_FULLSCREEN", "0", 1);
	CHECK(fullscreenOverrideFromEnv() == std::optional<bool>(false));
	setenv("EXPERIENCE_RUNTIME_FULLSCREEN", "", 1);
	CHECK(!fullscreenOverrideFromEnv().has_value());
	setenv("EXPERIENCE_RUNTIME_FULLSCREEN", "yes", 1);
	CHECK(!fullscreenOverrideFromEnv().has_value()); // only 1/0 are recognised
	unsetenv("EXPERIENCE_RUNTIME_FULLSCREEN");

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
