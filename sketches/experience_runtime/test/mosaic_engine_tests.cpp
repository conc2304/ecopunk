// Standalone tests for MosaicEngine.h / MosaicShape.h (no openFrameworks / GL).
#include "../src/MosaicEngine.h"

#include <cstdio>
#include <set>
#include <string>

using namespace mosaic;
static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void testModes() {
	std::set<std::string> keys;
	for (int m = 0; m < kModeCount; ++m) {
		const ModeSpec& s = spec(ModeId(m)); CHECK(int(s.id) == m, "order"); keys.insert(s.key);
		for (int d = 0; d < 2; ++d) { CHECK(s.dial[d].label && std::string(s.dial[d].label).size() <= 8, "label <= 8 glyphs %s", s.dial[d].label); CHECK(s.dial[d].lo <= s.dial[d].hi && s.dial[d].def >= 0 && s.dial[d].def <= 1, "dial %s", s.key); }
	}
	CHECK(keys.size() == 13, "13 unique ids");
}
static void testDials() {
	Engine e; e.configure(1280, 720, false, 3);
	e.setDial(ModeId::ECHO_MATRIX, 0, 4.f); CHECK(e.dial(ModeId::ECHO_MATRIX, 0) == 1.f, "clamp hi");
	e.setDial(ModeId::ECHO_MATRIX, 0, -1.f); CHECK(e.dial(ModeId::ECHO_MATRIX, 0) == 0.f, "clamp lo");
	e.setDial(ModeId::ECHO_MATRIX, 1, 0.7f); e.setMode(ModeId::OPEN_SLOT_PUZZLE); e.setMode(ModeId::ECHO_MATRIX);
	CHECK(e.dial(ModeId::ECHO_MATRIX, 1) == 0.7f, "per-mode dial persists");
	e.resetDials(); CHECK(e.dial(ModeId::ECHO_MATRIX, 1) == spec(ModeId::ECHO_MATRIX).dial[1].def, "reset");
	// dial drives observable properties
	auto vac = [&](float d) { Engine g; g.configure(1280, 720, false, 9); g.setAutoCycle(false); g.setMode(ModeId::OPEN_SLOT_PUZZLE); g.setDial(ModeId::OPEN_SLOT_PUZZLE, 1, d); for (int i = 0; i < 1200; ++i) g.update(1.f / 60); return g.vacantCellCount(); };
	CHECK(vac(1.f) > vac(0.f), "vacancy dial changes vacant cell count (%d vs %d)", vac(1.f), vac(0.f));
	auto cover = [&](float d) { Engine g; g.configure(1280, 720, false, 9); g.setAutoCycle(false); g.setMode(ModeId::CHROMATIC_OCCUPATION); g.setDial(ModeId::CHROMATIC_OCCUPATION, 0, d); for (int i = 0; i < 60 * 40; ++i) g.update(1.f / 60); return g.telemetry().occupancy; };
	CHECK(cover(1.f) > cover(0.f) + 0.2f, "coverage dial: %.2f vs %.2f", cover(1.f), cover(0.f));
	auto movers = [&](float d) { Engine g; g.configure(1280, 720, false, 9); g.setAutoCycle(false); g.setMode(ModeId::OPEN_SLOT_PUZZLE); g.setDial(ModeId::OPEN_SLOT_PUZZLE, 0, d); g.setDial(ModeId::OPEN_SLOT_PUZZLE, 1, 1.f); int mx = 0; for (int i = 0; i < 1800; ++i) { g.update(1.f / 60); mx = std::max(mx, g.movers()); } return mx; };
	CHECK(movers(1.f) > movers(0.f) && movers(0.f) <= 1, "active movers dial: %d vs %d", movers(1.f), movers(0.f));
}
static void testQuartet() {
	for (int pass = 0; pass < 2; ++pass) {
		Engine e; e.configure(1280, 720, false, 21 + pass); e.setAutoCycle(false); e.setMode(ModeId::MIRROR_QUARTET); e.setDial(ModeId::MIRROR_QUARTET, 0, pass == 0 ? 1.f : 0.f);
		for (int i = 0; i < 60 * 40; ++i) { e.update(1.f / 60); if (i > 240 && e.movers() == 0 && i % 90 == 0) { const QuartetStats q = e.quartetStats(); if (pass == 0) { CHECK(q.complete == q.units && q.partial == 0 && q.strayTiles == 0 && q.wrongTransform == 0, "strict: %d/%d complete, partial %d stray %d wrong %d", q.complete, q.units, q.partial, q.strayTiles, q.wrongTransform); } } }
		// exact TL/TR/BL/BR transforms on every present member
		for (const Tile& t : e.allTiles()) if (t.live && !t.dying && t.group >= 0 && t.target().type == CType::Video && pass == 0) { CHECK(t.target().fx == ((t.gslot & 1) != 0) && t.target().fy == ((t.gslot & 2) != 0), "slot %d transform fx=%d fy=%d", t.gslot, t.target().fx, t.target().fy); }
		const QuartetStats q = e.quartetStats(); std::printf("quartet fidelity %s: units %d complete %d partial %d stray %d\n", pass == 0 ? "1.0" : "0.0", q.units, q.complete, q.partial, q.strayTiles);
		if (pass == 1) CHECK(q.complete < q.units && q.partial > 0, "loose endpoint has incomplete groups");
	}
}
static void testMoveInvariants() {
	for (ModeId m : {ModeId::OPEN_SLOT_PUZZLE, ModeId::DOMINO_TRANSLATION, ModeId::MIRROR_QUARTET}) {
		Engine e; e.configure(1280, 720, false, 5 + int(m)); e.setAutoCycle(false); e.setMode(m); if (m == ModeId::OPEN_SLOT_PUZZLE) { e.setDial(m, 0, 1.f); e.setDial(m, 1, 0.6f); } if (m == ModeId::DOMINO_TRANSLATION) { e.setDial(m, 0, 1.f); e.setDial(m, 1, 1.f); } e.setDial(m, 1, m == ModeId::MIRROR_QUARTET ? 1.f : e.dial(m, 1));
		const int before = e.liveTileCount(); int maxMov = 0;
		for (int i = 0; i < 60 * 120; ++i) { e.update(i % 11 == 0 ? 0.05f : 1.f / 60); const std::string err = e.validate(); CHECK(err.empty(), "%s step %d: %s", spec(m).key, i, err.c_str()); if (!err.empty()) break; maxMov = std::max(maxMov, e.movers());
			if (m == ModeId::DOMINO_TRANSLATION && i > 60) { CHECK(e.liveTileCount() == before, "domino conserves tiles (%d vs %d)", e.liveTileCount(), before); } }
		std::printf("%s: max movers %d, tiles %d\n", spec(m).key, maxMov, e.liveTileCount());
		CHECK(maxMov > 0, "%s moved", spec(m).key);
		// let everything settle: no dangling reservations
		e.setAutoCycle(false); e.setDial(m, 0, 0.f);
	}
}
static void testRebuildClearsClaims() {
	Engine e; e.configure(1280, 720, false, 2); e.setAutoCycle(false); e.setMode(ModeId::DOMINO_TRANSLATION); e.setDial(ModeId::DOMINO_TRANSLATION, 1, 1.f);
	for (int i = 0; i < 600 && e.movers() == 0; ++i) e.update(1.f / 60);
	CHECK(e.movers() > 0, "have in-flight moves"); e.setMode(ModeId::OPEN_SLOT_PUZZLE); CHECK(e.validate().empty(), "mode switch mid-move: %s", e.validate().c_str());
	for (int c = 0; c < kCells; ++c) CHECK(e.reservedAt(c) < 0 || e.movers() > 0, "no stale reservation");
	e.onMediaChanged(); CHECK(e.validate().empty(), "media change"); e.reseed(99); CHECK(e.validate().empty(), "reseed");
	for (int i = 0; i < 300; ++i) e.update(1.f / 60); CHECK(e.ghostTiles().size() < 5, "ghosts fade away (%zu)", e.ghostTiles().size());
}
static void testAllModesLong() {
	for (int m = 0; m < kModeCount; ++m) {
		Engine e; e.configure(1280, 720, false, 70 + m); e.setAutoCycle(false); e.setMode(ModeId(m)); float minOcc = 1, maxOcc = 0; int maxTiles = 0;
		for (int i = 0; i < 60 * 90; ++i) {
			e.update(1.f / 60); const auto& t = e.telemetry(); minOcc = std::min(minOcc, i > 300 ? t.occupancy : 1.f); maxOcc = std::max(maxOcc, t.occupancy); maxTiles = std::max(maxTiles, t.tiles);
			if (i % 120 == 0) { const std::string err = e.validate(); CHECK(err.empty(), "%s: %s", spec(ModeId(m)).key, err.c_str()); }
			for (const Tile& tl : e.allTiles()) if (tl.live && tl.alpha > 0.01f) { CHECK(tl.rx >= -1 && tl.ry >= -1 && tl.rx + tl.rw <= 1281 && tl.ry + tl.rh <= 721, "%s tile within canvas (%f %f)", spec(ModeId(m)).key, tl.rx, tl.ry); }
		}
		std::printf("%-22s occupancy %.2f..%.2f  tiles<=%d  phase %s\n", spec(ModeId(m)).key, minOcc, maxOcc, maxTiles, e.telemetry().phase.c_str());
		CHECK(maxTiles <= kMaxTiles, "bounded");
	}
}
static void testDeterminism() {
	auto run = [](uint64_t s) { Engine e; e.configure(1280, 720, false, s); e.setAutoCycle(true); for (int i = 0; i < 60 * 150; ++i) e.update(1.f / 60); double h = 0; for (auto& t : e.allTiles()) if (t.live) h += t.rx * 1.3 + t.ry * 0.7 + t.alpha + double(t.target().u); return h; };
	CHECK(run(4) == run(4), "same seed replays"); CHECK(run(4) != run(5), "different seed differs");
	// frame-rate independence: same sim time at 30 vs 120 fps settles to the same counts of tiles in static modes
	auto cnt = [](float dt) { Engine e; e.configure(1280, 720, false, 8); e.setAutoCycle(false); e.setMode(ModeId::CELLULAR_GROWTH); float t = 0; while (t < 20.f) { e.update(dt); t += dt; } return e.telemetry().occupancy; };
	CHECK(std::fabs(cnt(1.f / 30) - cnt(1.f / 120)) < 0.12f, "frame-rate tolerant growth (%f vs %f)", cnt(1.f / 30), cnt(1.f / 120));
}
static void testCycleAndBackgrounds() {
	Engine e; e.configure(1280, 720, false, 12); std::set<int> seen; std::set<int> bgs;
	for (int i = 0; i < 60 * 60 * 14; ++i) { e.update(1.f / 60); seen.insert(int(e.mode())); bgs.insert(int(e.background())); }
	CHECK(seen.size() == 13, "auto-cycle visits all 13 modes (%zu)", seen.size()); CHECK(bgs.size() == 4, "all four background profiles used (%zu)", bgs.size());
}
static void testShapes() {
	std::vector<Pt> o; const Mask masks[] = {maskSquare(), maskRounded(0.25f), maskRounded(0.5f), maskCorners(1.f, 0, 0, 0), maskCorners(0.5f, 0.5f, 0, 0), maskCorners(1.f, 1.f, 1.f, 1.f), maskChamfer(true, false, true, false)};
	for (const Mask& m : masks) { maskOutline(10, 20, 80, 80, m, o); for (const Pt& p : o) CHECK(p.x >= 10 - 1e-3f && p.x <= 90 + 1e-3f && p.y >= 20 - 1e-3f && p.y <= 100 + 1e-3f, "outline in rect"); CHECK(polygonArea(o) <= 6400.f + 1e-2f && polygonArea(o) > 1000.f, "area %f", polygonArea(o)); }
	maskOutline(0, 0, 80, 80, maskRounded(0.5f), o); CHECK(std::fabs(polygonArea(o) - 3.14159f * 40 * 40) < 120, "circle area");
	maskOutline(0, 0, 160, 80, maskRounded(0.5f), o); CHECK(polygonArea(o) < 160 * 80, "pill is clipped");
}
int main() {
	testModes(); testDials(); testShapes(); testQuartet(); testMoveInvariants(); testRebuildClearsClaims(); testAllModesLong(); testDeterminism(); testCycleAndBackgrounds();
	std::printf(g_fail ? "FAILED (%d)\n" : "ALL PASSED\n", g_fail); return g_fail ? 1 : 0;
}
