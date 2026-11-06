// OC-004 core tests (bare compiler): c++ -std=c++17 -Isrc test/scene_activity_core_tests.cpp
#include "SceneActivityCore.h"
#include <cstdio>
using namespace sceneactivity;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
int main() {
	float a[4] = {0, 0.5f, 1, 0.25f}, b[4] = {0, 0.25f, 0.5f, 0.25f};
	CHECK(std::fabs(meanAbsDiff(a, b, 4) - 0.1875) < 1e-9);
	CHECK(std::isnan(meanAbsDiff(nullptr, b, 4)));
	CHECK(segmentCount(0.0, 12) == 0 && segmentCount(1e-6, 12) == 1 && segmentCount(1.0, 12) == 12 && segmentCount(0.5, 12) == 6);
	Normalizer n;
	CHECK(n.push(0.0, 0.0).fixed == 0.0);
	CHECK(n.push(0.1, 0.04).fixed == 1.0);
	Normalizer r; // rolling: a quiet series must not self-amplify below the floor
	auto q = r.push(0.0, 0.0005);
	CHECK(q.rollingRef == r.params().floorRaw && q.rolling < 0.6);
	for (int i = 0; i < 700; ++i) r.push(1.0 + i * 0.1, 0.01); // 70 s later the 0.0005 max has aged out
	CHECK(std::fabs(r.push(80.0, 0.01).rollingRef - 0.01) < 1e-12);
	Normalizer bd; bd.push(0, 5.0); // an outlier flash cannot push the bounded ref past 2x fixed
	CHECK(std::fabs(bd.push(0.1, 0.01).boundedRef - 0.08) < 1e-12);
	Smoother s; s.push(1.0, 0); double up = s.push(0.0, 0.1);
	CHECK(up > 0.85); // slow decay
	Smoother s2; s2.push(0.0, 0); CHECK(s2.push(1.0, 0.1) > 0.7); // fast attack
	std::printf(fails ? "FAILED\n" : "scene_activity_core_tests: all passed\n");
	return fails;
}
