// Geometry/UV tests for MediaViewportMesh — compiled against real
// openFrameworks headers (ofMesh/ofRectangle/ofTexture) but never calls
// draw()/bind() (no GL context created or required here, same convention
// as hud_real_frame_tests.cpp — see that file's own header comment).
//
// Built via `make -f Makefile.tests test-viewport`.

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

#include "../hud-compositor/MediaViewportMesh.h"

using namespace hudpresent;

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const char* file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define HUD_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

#define HUD_CHECK_NEAR(a, b, eps) \
	do { \
		g_total++; \
		double _a = static_cast<double>(a); \
		double _b = static_cast<double>(b); \
		if (std::abs(_a - _b) > (eps)) { \
			std::ostringstream _oss; \
			_oss << #a << " (" << _a << ") != " << #b << " (" << _b << ") within " << (eps); \
			reportFailure(__FILE__, __LINE__, _oss.str()); \
		} \
	} while (0)

namespace {

void test_vertex_and_triangle_counts_are_deterministic() {
	MediaViewportMesh a, b;
	MediaViewportGeometryParams params;
	params.bounds = ofRectangle(0, 0, 300, 200);
	params.cornerRadius = 16.0f;
	params.bevelSize = 40.0f;

	a.updateGeometry(params, glm::ivec2(1280, 720));
	b.updateGeometry(params, glm::ivec2(1280, 720));

	HUD_CHECK(a.vertexCount() == b.vertexCount());
	HUD_CHECK(a.vertexCount() > 4); // center + at least a handful of outline points
	HUD_CHECK(a.triangleCount() == a.vertexCount() - 2);

	// Same params/source -> byte-identical vertex positions (deterministic
	// triangulation, this task's own "deterministic geometry" requirement).
	for (size_t i = 0; i < a.mesh().getNumVertices(); ++i) {
		HUD_CHECK_NEAR(a.mesh().getVertex(i).x, b.mesh().getVertex(i).x, 1e-6);
		HUD_CHECK_NEAR(a.mesh().getVertex(i).y, b.mesh().getVertex(i).y, 1e-6);
	}
}

void test_no_unnecessary_rebuild() {
	MediaViewportMesh mesh;
	MediaViewportGeometryParams params;
	params.bounds = ofRectangle(0, 0, 300, 200);

	mesh.updateGeometry(params, glm::ivec2(1280, 720));
	HUD_CHECK(mesh.lastUpdateRebuilt() == true); // first call always rebuilds

	mesh.updateGeometry(params, glm::ivec2(1280, 720));
	HUD_CHECK(mesh.lastUpdateRebuilt() == false); // identical params -> no rebuild

	params.cornerRadius += 1.0f;
	mesh.updateGeometry(params, glm::ivec2(1280, 720));
	HUD_CHECK(mesh.lastUpdateRebuilt() == true); // geometry change -> rebuild

	mesh.updateGeometry(params, glm::ivec2(1280, 720));
	HUD_CHECK(mesh.lastUpdateRebuilt() == false);

	mesh.updateGeometry(params, glm::ivec2(640, 360)); // source-size-only change -> rebuild (UVs depend on it)
	HUD_CHECK(mesh.lastUpdateRebuilt() == true);
}

void test_geometry_stays_within_bounds() {
	MediaViewportMesh mesh;
	MediaViewportGeometryParams params;
	params.bounds = ofRectangle(50, 60, 400, 250);
	params.cornerRadius = 24.0f;
	params.bevelSize = 60.0f;
	mesh.updateGeometry(params, glm::ivec2(1920, 1080));

	for (size_t i = 0; i < mesh.mesh().getNumVertices(); ++i) {
		auto v = mesh.mesh().getVertex(i);
		HUD_CHECK(v.x >= params.bounds.x - 1e-3f);
		HUD_CHECK(v.x <= params.bounds.x + params.bounds.width + 1e-3f);
		HUD_CHECK(v.y >= params.bounds.y - 1e-3f);
		HUD_CHECK(v.y <= params.bounds.y + params.bounds.height + 1e-3f);
	}
}

void test_bevel_cuts_lower_right_corner() {
	MediaViewportMesh mesh;
	MediaViewportGeometryParams params;
	params.bounds = ofRectangle(0, 0, 300, 200);
	params.cornerRadius = 10.0f;
	params.bevelSize = 50.0f;
	mesh.updateGeometry(params, glm::ivec2(300, 200));

	// No vertex should land exactly at the un-cut lower-right corner
	// (300,200) -- the bevel must have removed it.
	bool foundRawCorner = false;
	for (size_t i = 0; i < mesh.mesh().getNumVertices(); ++i) {
		auto v = mesh.mesh().getVertex(i);
		if (std::abs(v.x - 300.0f) < 1e-3f && std::abs(v.y - 200.0f) < 1e-3f) foundRawCorner = true;
	}
	HUD_CHECK(!foundRawCorner);

	// The two bevel endpoints (300, 200-bevel) and (300-bevel, 200) MUST
	// both be present -- the vertex-cut technique this task specifies.
	bool foundRightEndpoint = false, foundBottomEndpoint = false;
	for (size_t i = 0; i < mesh.mesh().getNumVertices(); ++i) {
		auto v = mesh.mesh().getVertex(i);
		if (std::abs(v.x - 300.0f) < 1e-3f && std::abs(v.y - 150.0f) < 1e-3f) foundRightEndpoint = true;
		if (std::abs(v.x - 250.0f) < 1e-3f && std::abs(v.y - 200.0f) < 1e-3f) foundBottomEndpoint = true;
	}
	HUD_CHECK(foundRightEndpoint);
	HUD_CHECK(foundBottomEndpoint);
}

void test_cover_uv_matching_aspect_is_identity() {
	MediaViewportMesh mesh;
	MediaViewportGeometryParams params;
	params.bounds = ofRectangle(0, 0, 400, 200); // 2:1
	params.cornerRadius = 0.0f;
	params.bevelSize = 0.0f;
	mesh.updateGeometry(params, glm::ivec2(800, 400)); // also 2:1 -> no cropping, UV == normalized position

	// Center vertex (index 0) should map to UV (0.5, 0.5) regardless.
	auto centerUV = mesh.mesh().getTexCoord(0);
	HUD_CHECK_NEAR(centerUV.x, 0.5f, 1e-4);
	HUD_CHECK_NEAR(centerUV.y, 0.5f, 1e-4);

	for (size_t i = 0; i < mesh.mesh().getNumVertices(); ++i) {
		auto v = mesh.mesh().getVertex(i);
		auto uv = mesh.mesh().getTexCoord(i);
		float expectedU = v.x / params.bounds.width;
		float expectedV = v.y / params.bounds.height;
		HUD_CHECK_NEAR(uv.x, expectedU, 1e-3);
		HUD_CHECK_NEAR(uv.y, expectedV, 1e-3);
	}
}

void test_cover_uv_crops_mismatched_aspect() {
	MediaViewportMesh mesh;
	MediaViewportGeometryParams params;
	params.bounds = ofRectangle(0, 0, 200, 200); // 1:1 bounds
	params.cornerRadius = 0.0f;
	params.bevelSize = 0.0f;
	mesh.updateGeometry(params, glm::ivec2(1280, 720)); // 16:9 source, much wider than bounds

	// A square viewport showing a 16:9 source must crop left/right ->
	// the left/right edge UVs must NOT be exactly 0/1 (they're pulled
	// inward toward 0.5).
	// outline order after the center vertex (index 0): first real
	// boundary vertex is the top-left arc's first point, at local x=0 for
	// a zero-radius rect.
	auto leftEdgeUV = mesh.mesh().getTexCoord(1); // top-left corner, radius=0 -> exactly bounds' top-left
	HUD_CHECK(leftEdgeUV.x > 0.001f); // cropped inward from raw 0.0
}

void test_supports_different_source_sizes_without_distortion_direction_change() {
	// Same bounds, two very different source aspect ratios -> UV scale
	// factors must differ (proves sourceSize genuinely participates in
	// geometry/UV construction, not just accepted and ignored).
	MediaViewportMesh meshWide, meshTall;
	MediaViewportGeometryParams params;
	params.bounds = ofRectangle(0, 0, 200, 200);
	params.cornerRadius = 0.0f;
	params.bevelSize = 0.0f;

	meshWide.updateGeometry(params, glm::ivec2(1920, 1080)); // very wide source
	meshTall.updateGeometry(params, glm::ivec2(1080, 1920)); // very tall source

	auto uvWide = meshWide.mesh().getTexCoord(1);
	auto uvTall = meshTall.mesh().getTexCoord(1);
	HUD_CHECK(std::abs(uvWide.x - uvTall.x) > 1e-3f || std::abs(uvWide.y - uvTall.y) > 1e-3f);
}

} // namespace

int main() {
	test_vertex_and_triangle_counts_are_deterministic();
	test_no_unnecessary_rebuild();
	test_geometry_stays_within_bounds();
	test_bevel_cuts_lower_right_corner();
	test_cover_uv_matching_aspect_is_identity();
	test_cover_uv_crops_mismatched_aspect();
	test_supports_different_source_sizes_without_distortion_direction_change();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed.\n";
	if (g_failures > 0) {
		std::cerr << g_failures << " FAILURE(S)\n";
		return 1;
	}
	return 0;
}
