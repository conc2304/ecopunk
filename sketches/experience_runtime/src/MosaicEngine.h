#pragma once

// MosaicEngine — OF-free composition engine for the Mosaic Assembly scene.
//
//   CellGrid  : 16x9 logical cells (immutable coordinates). occupant[] = tile standing in a cell,
//               reserved[] = tile that has claimed the cell as a move destination.
//   Tile      : stable identity + content (VIDEO crop | SOLID colour | VACANT), crop/zoom/mirror, alpha,
//               corner Mask, optional crossfade target, move state. Tiles are separate from cells.
//   Modes     : 13 strategies over this engine (build() + step()); none owns rendering or video.
//
// Move correctness: a move first CLAIMS its destination (reserved[to] = tile). A claim may target a cell whose
// occupant has already scheduled its own departure (chains). The origin cell is released when the move
// STARTS; the destination becomes occupied when it ENDS. Vacancy in transit = cells with neither occupant
// nor reservation. Rebuilds (mode / media / reseed) move every tile into a fading ghost list and clear all
// occupancy + reservations, so no stale claim or ghost tile survives.
//
// Deterministic: one seeded RNG and a fixed 1/60 s internal step.

#include "MosaicShape.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace mosaic {

constexpr int kModeCount = 13;
constexpr int kCols = 16, kRows = 9;
constexpr int kCells = kCols * kRows;
constexpr int kMaxTiles = 640;
constexpr float kStep = 1.0f / 60.0f;

enum class ModeId : int { MIRROR_QUARTET, KALEIDOSCOPE_FIELD, OPEN_SLOT_PUZZLE, CHROMATIC_OCCUPATION, DISSOLVING_GRID, ECHO_MATRIX, CHECKERBOARD_EXCHANGE,
	CELLULAR_GROWTH, HIERARCHICAL_MOSAIC, CHROMATIC_EXCAVATION, DOMINO_TRANSLATION, SYMMETRY_COLLAPSE, TRAVELING_APERTURE };
enum class Bg : int { Video, Desat, Black, Solid };

struct DialSpec { const char* label; float def, lo, hi; const char* unit; };
struct ModeSpec { ModeId id; const char* key; const char* title; DialSpec dial[2]; Bg bg; bool tileDesat; };

inline const std::array<ModeSpec, kModeCount>& modeSpecs() {
	static const std::array<ModeSpec, kModeCount> s = {{
		{ModeId::MIRROR_QUARTET, "MIRROR_QUARTET", "Mirror Quartet", {{"FIDELITY", 0.8f, 0.f, 1.f, "fid"}, {"DRIFT", 0.4f, 0.1f, 1.8f, "ev/s"}}, Bg::Black, true},
		{ModeId::KALEIDOSCOPE_FIELD, "KALEIDOSCOPE_FIELD", "Kaleidoscope Field", {{"SIZE", 0.5f, 2.f, 6.f, "cells"}, {"ROTATE", 0.35f, 0.f, 0.6f, "st/s"}}, Bg::Solid, false},
		{ModeId::OPEN_SLOT_PUZZLE, "OPEN_SLOT_PUZZLE", "Open-Slot Puzzle", {{"MOVERS", 0.4f, 1.f, 6.f, "tiles"}, {"VACANCY", 0.35f, 1.f, 10.f, "cells"}}, Bg::Black, false},
		{ModeId::CHROMATIC_OCCUPATION, "CHROMATIC_OCCUPATION", "Chromatic Occupation", {{"COVERAGE", 0.5f, 0.10f, 0.85f, "frac"}, {"TURNOVER", 0.4f, 0.5f, 5.f, "ev/s"}}, Bg::Desat, false},
		{ModeId::DISSOLVING_GRID, "DISSOLVING_GRID", "Dissolving Grid", {{"EROSION", 0.4f, 0.02f, 0.18f, "occ/s"}, {"MIN OCC", 0.4f, 0.10f, 0.80f, "frac"}}, Bg::Black, true},
		{ModeId::ECHO_MATRIX, "ECHO_MATRIX", "Echo Matrix", {{"RADIUS", 0.5f, 2.f, 9.f, "cells"}, {"SPEED", 0.45f, 1.5f, 8.f, "cell/s"}}, Bg::Desat, false},
		{ModeId::CHECKERBOARD_EXCHANGE, "CHECKERBOARD_EXCHANGE", "Checkerboard Exchange", {{"REGULAR", 0.7f, 0.f, 1.f, "reg"}, {"EXCHANGE", 0.4f, 0.15f, 1.2f, "ev/s"}}, Bg::Black, true},
		{ModeId::CELLULAR_GROWTH, "CELLULAR_GROWTH", "Cellular Growth", {{"GROWTH", 0.5f, 1.f, 10.f, "cell/s"}, {"BRANCH", 0.4f, 0.f, 1.f, "br"}}, Bg::Desat, false},
		{ModeId::HIERARCHICAL_MOSAIC, "HIERARCHICAL_MOSAIC", "Hierarchical Mosaic", {{"DEPTH", 0.5f, 1.f, 3.f, "lvl"}, {"RECOMB", 0.45f, 0.1f, 0.9f, "frac"}}, Bg::Solid, true},
		{ModeId::CHROMATIC_EXCAVATION, "CHROMATIC_EXCAVATION", "Chromatic Excavation", {{"PRESSURE", 0.5f, 0.f, 1.f, "pr"}, {"ISLAND", 0.45f, 1.5f, 5.f, "cells"}}, Bg::Video, false},
		{ModeId::DOMINO_TRANSLATION, "DOMINO_TRANSLATION", "Domino Translation", {{"CHAIN", 0.45f, 2.f, 8.f, "tiles"}, {"CADENCE", 0.45f, 0.3f, 3.f, "push/s"}}, Bg::Black, true},
		{ModeId::SYMMETRY_COLLAPSE, "SYMMETRY_COLLAPSE", "Symmetry Collapse", {{"HOLD", 0.5f, 4.f, 30.f, "s"}, {"SPREAD", 0.45f, 0.f, 1.f, "spr"}}, Bg::Solid, true},
		{ModeId::TRAVELING_APERTURE, "TRAVELING_APERTURE", "Traveling Aperture", {{"SIZE", 0.5f, 1.5f, 5.5f, "cells"}, {"TRAVEL", 0.4f, 0.5f, 6.f, "cell/s"}}, Bg::Solid, false},
	}};
	return s;
}
inline const ModeSpec& spec(ModeId m) { return modeSpecs()[size_t(m)]; }

inline float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline float smooth01(float t) { t = clamp01(t); return t * t * (3.f - 2.f * t); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

struct Rng {
	uint64_t s;
	explicit Rng(uint64_t seed = 1) : s(seed * 0x9E3779B97F4A7C15ULL + 0x2545F491ULL) {}
	uint64_t next() { s += 0x9E3779B97F4A7C15ULL; uint64_t z = s; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
	float f() { return float(next() >> 40) * (1.0f / 16777216.0f); }
	float range(float a, float b) { return a + (b - a) * f(); }
	int irange(int a, int b) { return b <= a ? a : a + int(next() % uint64_t(b - a + 1)); }
	bool chance(float p) { return f() < p; }
};

enum class CType : uint8_t { Vacant, Video, Solid };
struct Content {
	CType type = CType::Vacant;
	float u = 0.5f, v = 0.5f, zoom = 1.f; // crop centre (uv of the cover-fit source) and source magnification
	bool fx = false, fy = false;          // mirror X (flip over the Y axis) / mirror Y (flip over the X axis)
	float r = 0, g = 0, b = 0;            // solid colour
	bool sampled = false;                 // true: colour follows the video colour map at (u,v)
};
inline bool sameContent(const Content& a, const Content& b) {
	if (a.type != b.type) return false;
	if (a.type == CType::Vacant) return true;
	if (a.type == CType::Video) return std::fabs(a.u - b.u) < 1e-4f && std::fabs(a.v - b.v) < 1e-4f && std::fabs(a.zoom - b.zoom) < 1e-4f && a.fx == b.fx && a.fy == b.fy;
	if (a.sampled != b.sampled) return false;
	if (a.sampled) return std::fabs(a.u - b.u) < 1e-4f && std::fabs(a.v - b.v) < 1e-4f;
	return std::fabs(a.r - b.r) + std::fabs(a.g - b.g) + std::fabs(a.b - b.b) < 1e-3f;
}

struct Tile {
	bool live = false, dying = false, ghost = false;
	int cell = -1;                // current/home cell (settled)
	float span = 1.f;             // size in cells
	float gx = 0, gy = 0;         // top-left, grid units (animated)
	Content a, b; bool hasB = false; float mix = 0, mixRate = 3, mixDelay = 0;
	Mask mask, maskTo; float maskT = 1;
	float alpha = 0, aTarget = 1, aRate = 3, tdelay = 0;
	// move
	bool pending = false, moving = false;
	int fromCell = -1, toCell = -1; float mt = 0, mdur = 0.4f, mdelay = 0; int mcurve = 0;
	int group = -1, gslot = -1, tag = 0;
	// computed (pixels, inset by the tile gap)
	float rx = 0, ry = 0, rw = 0, rh = 0;
	Mask drawMask; // interpolated
	const Content& target() const { return hasB ? b : a; }
};

struct Telemetry {
	float occupancy = 0;    // fraction of grid area covered by tiles (video + solid), in-transit counted at destination
	float vacancy = 1;
	int movers = 0;         // tiles moving or about to move
	int tiles = 0;
	int ghosts = 0;
	float videoFrac = 0, solidFrac = 0;
	std::string phase;
	ModeId mode = ModeId::MIRROR_QUARTET;
	Bg bg = Bg::Black;
};

struct QuartetStats { int units = 0, complete = 0, partial = 0, strayTiles = 0, wrongTransform = 0; };

class Engine {
public:
	void configure(int canvasW, int canvasH, bool pi, uint64_t seed_) {
		W = float(canvasW); H = float(canvasH); cellPx = W / float(kCols); piProfile = pi; seed = seed_;
		resetDials(); colorGrid.assign(size_t(kCells) * 3, 0.35f);
		hardReset(ModeId::MIRROR_QUARTET);
	}
	void hardReset(ModeId m) {
		tiles.clear(); ghosts.clear(); rng = Rng(seed); clock = 0; acc = 0; gen = 0; bag.clear(); cycleTimer = rng.range(38.f, 62.f);
		rebuild(m, false);
	}
	void reseed(uint64_t s) { seed = s; rng = Rng(seed ^ (uint64_t(gen) * 0x51ED27ULL)); rebuild(mode_); }
	void onMediaChanged() { rebuild(mode_); }
	void setMode(ModeId m) { if (m == mode_ && !tiles.empty()) return; rebuild(m); cycleTimer = rng.range(38.f, 62.f); }
	ModeId mode() const { return mode_; }
	uint64_t currentSeed() const { return seed; }
	void setAutoCycle(bool on) { autoCycle = on; }
	bool autoCycleEnabled() const { return autoCycle; }

	void resetDials() { for (int m = 0; m < kModeCount; ++m) for (int d = 0; d < 2; ++d) dials[m][d] = spec(ModeId(m)).dial[d].def; }
	void setDial(ModeId m, int d, float n) { if (d >= 0 && d < 2) dials[int(m)][d] = clamp01(n); }
	float dial(ModeId m, int d) const { return (d >= 0 && d < 2) ? dials[int(m)][d] : 0.f; }
	float dialPhys(ModeId m, int d) const { const DialSpec& s = spec(m).dial[d]; return lerpf(s.lo, s.hi, dials[int(m)][d]); }

	// 16x9 representative colour map of the CURRENT frame (row-major, rgb 0..1), supplied by the scene (cheap GPU downscale).
	void setColorGrid(const std::vector<float>& rgb) { if (rgb.size() == size_t(kCells) * 3) colorGrid = rgb; }
	void setBackgroundOverride(int bgOrMinus1) { bgOverride = bgOrMinus1; }

	void update(float dt) {
		dt = std::min(std::max(dt, 0.f), 0.25f); acc += dt; int g = 0;
		while (acc >= kStep && g++ < 12) { acc -= kStep; stepOnce(kStep); }
		if (acc > kStep * 12) acc = 0;
	}

	const std::vector<Tile>& allTiles() const { return tiles; }
	const std::vector<Tile>& ghostTiles() const { return ghosts; }
	const Telemetry& telemetry() const { return tel; }
	Bg background() const { return bgOverride >= 0 ? Bg(bgOverride) : bg_; }
	Bg previousBackground() const { return bgPrev; }
	float backgroundFade() const { return bgFade; } // 0..1: previous -> current
	bool tileDesat() const { return spec(mode_).tileDesat; }
	void solidColor(float& r, float& g, float& b) const { r = pal()[0][0]; g = pal()[0][1]; b = pal()[0][2]; }
	std::array<float, 3> tileRgb(const Content& c) const { // resolved solid colour (sampled colours follow the live colour map, saturation-boosted)
		if (!c.sampled) return {c.r, c.g, c.b};
		const float gx = clamp01(c.u) * float(kCols) - 0.5f, gy = clamp01(c.v) * float(kRows) - 0.5f;
		const int x0 = std::clamp(int(std::floor(gx)), 0, kCols - 1), y0 = std::clamp(int(std::floor(gy)), 0, kRows - 1);
		const int x1 = std::min(x0 + 1, kCols - 1), y1 = std::min(y0 + 1, kRows - 1);
		const float fx = std::clamp(gx - float(x0), 0.f, 1.f), fy = std::clamp(gy - float(y0), 0.f, 1.f);
		std::array<float, 3> o{};
		for (int k = 0; k < 3; ++k) {
			auto at = [&](int x, int y) { return colorGrid[size_t(y * kCols + x) * 3 + size_t(k)]; };
			o[size_t(k)] = lerpf(lerpf(at(x0, y0), at(x1, y0), fx), lerpf(at(x0, y1), at(x1, y1), fx), fy);
		}
		const float l = 0.299f * o[0] + 0.587f * o[1] + 0.114f * o[2];
		for (float& v : o) v = clamp01(l + (v - l) * 1.45f);
		return o;
	}
	float cellPixels() const { return cellPx; }
	int movers() const { int n = 0; for (const auto& t : tiles) if (t.live && !t.dying && (t.pending || t.moving)) ++n; return n; }
	int occupantAt(int cell) const { return occupant[size_t(cell)]; }
	int reservedAt(int cell) const { return reserved[size_t(cell)]; }
	int liveTileCount() const { int n = 0; for (const auto& t : tiles) if (t.live && !t.dying) ++n; return n; }
	int vacantCellCount() const { int n = 0; for (int c = 0; c < kCells; ++c) if (occupant[size_t(c)] < 0 && reserved[size_t(c)] < 0) ++n; return n; }
	bool movesInFlight() const { return movers() > 0; }

	// Grid-state validator (seeded unit tests). Empty string = valid. Checks that every claim has exactly one owner,
	// no cell is double-claimed, and no tile is lost: settled tiles stand in their cell; in-flight tiles own a reservation.
	std::string validate() const {
		char b[160];
		int reservations = 0;
		for (int c = 0; c < kCells; ++c) {
			const int r = reserved[size_t(c)]; if (r < 0) continue; ++reservations;
			if (r >= int(tiles.size()) || !tiles[size_t(r)].live || (!tiles[size_t(r)].pending && !tiles[size_t(r)].moving) || tiles[size_t(r)].toCell != c) { std::snprintf(b, sizeof(b), "dangling reservation at cell %d", c); return b; }
		}
		int inflight = 0;
		for (size_t i = 0; i < tiles.size(); ++i) {
			const Tile& t = tiles[i]; if (!t.live || t.dying) continue;
			if (t.pending || t.moving) {
				++inflight;
				if (reserved[size_t(t.toCell)] != int(i)) { std::snprintf(b, sizeof(b), "tile %zu in flight without reservation", i); return b; }
				if (t.pending && occupant[size_t(t.fromCell)] != int(i)) { std::snprintf(b, sizeof(b), "pending tile %zu lost its origin", i); return b; }
				if (t.moving && occupant[size_t(t.fromCell)] == int(i)) { std::snprintf(b, sizeof(b), "moving tile %zu still occupies origin", i); return b; }
			} else if (occCells && t.cell >= 0 && occupant[size_t(t.cell)] != int(i)) { std::snprintf(b, sizeof(b), "settled tile %zu not in its cell %d", i, t.cell); return b; }
		}
		if (reservations != inflight) return "reservation count != tiles in flight";
		for (int c = 0; c < kCells; ++c) { const int o = occupant[size_t(c)]; if (o >= 0 && (o >= int(tiles.size()) || !tiles[size_t(o)].live || tiles[size_t(o)].dying)) { std::snprintf(b, sizeof(b), "stale occupant at cell %d", c); return b; } }
		return "";
	}

	QuartetStats quartetStats() const {
		QuartetStats s; if (mode_ != ModeId::MIRROR_QUARTET) return s;
		for (size_t u = 0; u < units.size(); ++u) {
			++s.units; const Unit& un = units[u]; int have = 0; bool ok = true; Content ref; bool refSet = false;
			for (int slot = 0; slot < 4; ++slot) {
				const int cell = (2 * un.uy + slot / 2) * kCols + 2 * un.ux + (slot % 2); const int t = occupant[size_t(cell)];
				if (t < 0) { ok = false; continue; }
				const Tile& T = tiles[size_t(t)]; ++have; const Content& c = T.target();
				if (c.type != CType::Video || T.group != int(u) || T.gslot != slot) { ok = false; ++s.wrongTransform; continue; }
				if (c.fx != ((slot & 1) != 0) || c.fy != ((slot & 2) != 0)) { ok = false; ++s.wrongTransform; }
				if (!refSet) { ref = c; refSet = true; } else if (std::fabs(c.u - ref.u) > 1e-4f || std::fabs(c.v - ref.v) > 1e-4f || std::fabs(c.zoom - ref.zoom) > 1e-4f) ok = false;
			}
			if (have == 4 && ok) ++s.complete; else if (have > 0) ++s.partial;
		}
		for (size_t i = 0; i < tiles.size(); ++i) if (tiles[i].live && !tiles[i].dying && tiles[i].group < 0) ++s.strayTiles;
		return s;
	}

private:
	// ---------------------------------------------------------------------------------------------
	float W = 1280, H = 720, cellPx = 80; bool piProfile = false;
	uint64_t seed = 1; Rng rng{1};
	float clock = 0, acc = 0, modeClock = 0;
	int gen = 0, palIdx = 0;
	ModeId mode_ = ModeId::MIRROR_QUARTET;
	Bg bg_ = Bg::Black, bgPrev = Bg::Black; float bgFade = 1.f; int bgOverride = -1;
	std::vector<Tile> tiles, ghosts; std::vector<int> freeList;
	std::array<int, kCells> occupant{}, reserved{};
	bool occCells = true; // validate(): settled tiles must stand in their cell (off for the span-based hierarchical mode)
	float dials[kModeCount][2];
	bool autoCycle = true; std::vector<int> bag; float cycleTimer = 50.f;
	std::vector<float> colorGrid;
	Telemetry tel;
	float ms[12] = {0}; int mi[12] = {0}; std::string phaseName;

	const float (*pal() const)[3] {
		static const float P[4][5][3] = {
			{{0.08f, 0.12f, 0.10f}, {0.20f, 0.45f, 0.35f}, {0.85f, 0.65f, 0.25f}, {0.90f, 0.88f, 0.80f}, {0.95f, 0.35f, 0.20f}},
			{{0.07f, 0.09f, 0.16f}, {0.20f, 0.35f, 0.60f}, {0.85f, 0.80f, 0.70f}, {0.55f, 0.75f, 0.70f}, {0.98f, 0.45f, 0.35f}},
			{{0.12f, 0.07f, 0.14f}, {0.45f, 0.25f, 0.55f}, {0.80f, 0.85f, 0.40f}, {0.90f, 0.88f, 0.92f}, {0.20f, 0.85f, 0.75f}},
			{{0.05f, 0.14f, 0.15f}, {0.10f, 0.45f, 0.50f}, {0.90f, 0.80f, 0.60f}, {0.70f, 0.30f, 0.25f}, {0.97f, 0.85f, 0.20f}}};
		return P[palIdx];
	}
	static int cidx(int cx, int cy) { return cy * kCols + cx; }
	static bool inGrid(int cx, int cy) { return cx >= 0 && cy >= 0 && cx < kCols && cy < kRows; }
	static float hash01(int a, int b, int c = 0) { uint32_t h = uint32_t(a) * 374761393u + uint32_t(b) * 668265263u + uint32_t(c) * 2147483647u; h = (h ^ (h >> 13)) * 1274126177u; return float((h ^ (h >> 16)) & 0xFFFFFF) / 16777216.f; }

	// ---- content / mask helpers ----------------------------------------------------------------------
	Content videoHome(int cell, float zoom = 1.f) const { Content c; c.type = CType::Video; c.u = (float(cell % kCols) + 0.5f) / float(kCols); c.v = (float(cell / kCols) + 0.5f) / float(kRows); c.zoom = zoom; return c; }
	Content videoAt(float u, float v, float zoom, bool fx, bool fy) const { Content c; c.type = CType::Video; c.u = u; c.v = v; c.zoom = zoom; c.fx = fx; c.fy = fy; return c; }
	Content solidAuthored(int i) const { Content c; c.type = CType::Solid; const float* p = pal()[1 + (i % 4)]; c.r = p[0]; c.g = p[1]; c.b = p[2]; return c; }
	Content solidAccent() const { Content c; c.type = CType::Solid; const float* p = pal()[4]; c.r = p[0]; c.g = p[1]; c.b = p[2]; return c; }
	Content solidSampled(int cell) const { Content c; c.type = CType::Solid; c.sampled = true; c.u = (float(cell % kCols) + 0.5f) / float(kCols); c.v = (float(cell / kCols) + 0.5f) / float(kRows); return c; }
	Content vacant() const { return Content{}; }
	static Mask cornerToward(int corner, float r) { Mask m; m.r[corner] = r; return m; } // TL0 TR1 BR2 BL3

	// ---- tile lifecycle ---------------------------------------------------------------------------------
	int allocTile() {
		if (!freeList.empty()) { int i = freeList.back(); freeList.pop_back(); tiles[size_t(i)] = Tile{}; return i; }
		if (int(tiles.size()) >= kMaxTiles) return -1;
		tiles.push_back(Tile{}); return int(tiles.size()) - 1;
	}
	int addTile(int cell, const Content& c, const Mask& m, float delay = 0.f, float fadeSec = 0.4f, float span = 1.f, float gx = -1, float gy = -1) {
		const int i = allocTile(); if (i < 0) return -1;
		Tile& t = tiles[size_t(i)];
		t.live = true; t.cell = cell; t.span = span; t.a = c; t.mask = t.maskTo = t.drawMask = m; t.maskT = 1; t.alpha = 0; t.aTarget = 1; t.aRate = 1.f / std::max(0.05f, fadeSec); t.tdelay = delay;
		t.gx = gx >= 0 ? gx : float(cell % kCols); t.gy = gy >= 0 ? gy : float(cell / kCols);
		if (cell >= 0 && span >= 1.f - 1e-4f && occCellsMode) occupant[size_t(cell)] = i;
		return i;
	}
	bool occCellsMode = true; // span-based hierarchical mode does not maintain occupant[]
	void dieTile(int i, float delay = 0.f) {
		Tile& t = tiles[size_t(i)]; if (!t.live || t.dying) return;
		if (t.cell >= 0 && occupant[size_t(t.cell)] == i) occupant[size_t(t.cell)] = -1;
		if (t.pending || t.moving) { if (t.toCell >= 0 && reserved[size_t(t.toCell)] == i) reserved[size_t(t.toCell)] = -1; if (t.fromCell >= 0 && occupant[size_t(t.fromCell)] == i) occupant[size_t(t.fromCell)] = -1; t.pending = t.moving = false; }
		t.dying = true; t.aTarget = 0; t.aRate = 3.f; t.tdelay = delay; t.group = -1;
	}
	Mask currentMask(const Tile& t) const { return lerpMask(t.mask, t.maskTo, smooth01(t.maskT)); }
	void setContent(int i, const Content& c, const Mask& m, float delay = 0.f, float fade = 0.4f) {
		Tile& t = tiles[size_t(i)];
		const bool sameC = sameContent(t.target(), c);
		if (!sameMask(t.maskTo, m)) { t.mask = currentMask(t); t.maskTo = m; t.maskT = 0; }
		if (sameC) return;
		if (t.hasB) t.a = t.mix > 0.5f ? t.b : t.a;
		t.b = c; t.hasB = true; t.mix = 0; t.mixDelay = delay; t.mixRate = 1.f / std::max(0.05f, fade);
	}
	// Per-cell state strategy: ensure the cell shows `c` (vacant = fade the tile out).
	void applyCell(int cell, const Content& c, const Mask& m, float delay = 0.f, float fade = 0.35f) {
		const int t = occupant[size_t(cell)];
		if (c.type == CType::Vacant) { if (t >= 0) dieTile(t, delay); return; }
		if (t < 0) { if (reserved[size_t(cell)] < 0) addTile(cell, c, m, delay, fade); return; }
		setContent(t, c, m, delay, fade);
	}

	// ---- moves ----------------------------------------------------------------------------------------------
	bool claimable(int to) const {
		if (reserved[size_t(to)] >= 0) return false;
		const int o = occupant[size_t(to)];
		return o < 0 || tiles[size_t(o)].pending; // the occupant has already scheduled its own departure (chain)
	}
	bool tileFree(int i) const { return i >= 0 && tiles[size_t(i)].live && !tiles[size_t(i)].dying && !tiles[size_t(i)].pending && !tiles[size_t(i)].moving; }
	bool moveTile(int i, int to, float dur, float delay, int curve) {
		if (!tileFree(i) || !claimable(to) || tiles[size_t(i)].cell == to) return false;
		Tile& t = tiles[size_t(i)]; t.pending = true; t.fromCell = t.cell; t.toCell = to; t.mt = 0; t.mdur = std::max(0.08f, dur); t.mdelay = delay; t.mcurve = curve;
		reserved[size_t(to)] = i; return true;
	}
	// Mutual exchange of two settled tiles (each claims the other's cell; both departures are scheduled together).
	bool swapTiles(int ta, int tb, float dur, int curve) {
		if (!tileFree(ta) || !tileFree(tb)) return false;
		Tile& A = tiles[size_t(ta)]; Tile& B = tiles[size_t(tb)]; const int ca = A.cell, cb = B.cell;
		if (reserved[size_t(ca)] >= 0 || reserved[size_t(cb)] >= 0 || ca == cb) return false;
		A.pending = true; A.fromCell = ca; A.toCell = cb; A.mt = 0; A.mdur = dur; A.mdelay = 0; A.mcurve = curve; reserved[size_t(cb)] = ta;
		B.pending = true; B.fromCell = cb; B.toCell = ca; B.mt = 0; B.mdur = dur; B.mdelay = 0; B.mcurve = curve; reserved[size_t(ca)] = tb;
		return true;
	}
	static float moveEase(int curve, float t) {
		t = clamp01(t);
		if (curve == 1) { const float c1 = 1.70158f * 1.1f, c3 = c1 + 1.f, u = t - 1.f; return 1.f + c3 * u * u * u + c1 * u * u; }
		if (curve == 2) return t < 0.5f ? 4 * t * t * t : 1.f - std::pow(-2.f * t + 2.f, 3.f) / 2.f;
		if (curve == 3) return t;
		return smooth01(t);
	}
	void stepTile(Tile& t, int idx, float dt) {
		if (t.tdelay > 0.f) t.tdelay -= dt; else t.alpha += (t.aTarget > t.alpha ? 1.f : -1.f) * std::min(std::fabs(t.aTarget - t.alpha), t.aRate * dt);
		if (t.hasB) { if (t.mixDelay > 0.f) t.mixDelay -= dt; else { t.mix += t.mixRate * dt; if (t.mix >= 1.f) { t.a = t.b; t.hasB = false; t.mix = 0; } } }
		if (t.maskT < 1.f) t.maskT = std::min(1.f, t.maskT + dt * 2.5f);
		if (t.pending) {
			t.mdelay -= dt;
			if (t.mdelay <= 0.f) { t.pending = false; t.moving = true; if (occupant[size_t(t.fromCell)] == idx) occupant[size_t(t.fromCell)] = -1; t.mt = 0; }
		}
		if (t.moving) {
			t.mt += dt / t.mdur; const float e = moveEase(t.mcurve, t.mt);
			const float fx = float(t.fromCell % kCols), fy = float(t.fromCell / kCols), tx = float(t.toCell % kCols), ty = float(t.toCell / kCols);
			t.gx = std::clamp(fx + (tx - fx) * e, 0.f, float(kCols) - t.span); t.gy = std::clamp(fy + (ty - fy) * e, 0.f, float(kRows) - t.span);
			if (t.mt >= 1.f) { t.gx = tx; t.gy = ty; t.moving = false; t.cell = t.toCell; occupant[size_t(t.toCell)] = idx; if (reserved[size_t(t.toCell)] == idx) reserved[size_t(t.toCell)] = -1; }
		}
	}

	// ---------------------------------------------------------------------------------------------------
	void rebuild(ModeId m, bool ghostOld = true) {
		if (ghostOld) {
			for (auto& t : tiles) if (t.live && !t.dying && t.alpha > 0.01f) { Tile g = t; g.ghost = true; g.pending = g.moving = false; g.aTarget = 0; g.aRate = 2.6f; g.tdelay = 0; ghosts.push_back(g); }
			if (ghosts.size() > 700) ghosts.erase(ghosts.begin(), ghosts.begin() + long(ghosts.size() - 700));
		}
		tiles.clear(); freeList.clear(); occupant.fill(-1); reserved.fill(-1);
		mode_ = m; ++gen; modeClock = 0; for (float& v : ms) v = 0; for (int& v : mi) v = 0; phaseName.clear();
		palIdx = rng.irange(0, 3); occCells = true; occCellsMode = true;
		bgPrev = bg_; bg_ = spec(m).bg; bgFade = (bgPrev == bg_) ? 1.f : 0.f;
		tiles.reserve(256);
		units.clear(); clusters.clear(); colonies.clear(); islands.clear(); patches.clear(); waves.clear(); nodes.clear(); freeNodes.clear();
		switch (m) {
			case ModeId::MIRROR_QUARTET: buildQuartet(); break;
			case ModeId::KALEIDOSCOPE_FIELD: buildKaleido(); break;
			case ModeId::OPEN_SLOT_PUZZLE: buildPuzzle(); break;
			case ModeId::CHROMATIC_OCCUPATION: buildOccupation(); break;
			case ModeId::DISSOLVING_GRID: buildDissolve(); break;
			case ModeId::ECHO_MATRIX: buildEcho(); break;
			case ModeId::CHECKERBOARD_EXCHANGE: buildChecker(); break;
			case ModeId::CELLULAR_GROWTH: buildGrowth(); break;
			case ModeId::HIERARCHICAL_MOSAIC: buildHier(); break;
			case ModeId::CHROMATIC_EXCAVATION: buildExcav(); break;
			case ModeId::DOMINO_TRANSLATION: buildDomino(); break;
			case ModeId::SYMMETRY_COLLAPSE: buildSymmetry(); break;
			case ModeId::TRAVELING_APERTURE: buildAperture(); break;
		}
		computeRects(); computeTelemetry();
	}

	void stepOnce(float dt) {
		clock += dt; modeClock += dt;
		switch (mode_) {
			case ModeId::MIRROR_QUARTET: stepQuartet(dt); break;
			case ModeId::KALEIDOSCOPE_FIELD: stepKaleido(dt); break;
			case ModeId::OPEN_SLOT_PUZZLE: stepPuzzle(dt); break;
			case ModeId::CHROMATIC_OCCUPATION: stepOccupation(dt); break;
			case ModeId::DISSOLVING_GRID: stepDissolve(dt); break;
			case ModeId::ECHO_MATRIX: stepEcho(dt); break;
			case ModeId::CHECKERBOARD_EXCHANGE: stepChecker(dt); break;
			case ModeId::CELLULAR_GROWTH: stepGrowth(dt); break;
			case ModeId::HIERARCHICAL_MOSAIC: stepHier(dt); break;
			case ModeId::CHROMATIC_EXCAVATION: stepExcav(dt); break;
			case ModeId::DOMINO_TRANSLATION: stepDomino(dt); break;
			case ModeId::SYMMETRY_COLLAPSE: stepSymmetry(dt); break;
			case ModeId::TRAVELING_APERTURE: stepAperture(dt); break;
		}
		for (size_t i = 0; i < tiles.size(); ++i) {
			Tile& t = tiles[i]; if (!t.live) continue;
			stepTile(t, int(i), dt);
			if (t.dying && t.alpha <= 0.002f && t.tdelay <= 0.f) { t.live = false; freeList.push_back(int(i)); }
		}
		for (auto& g : ghosts) g.alpha = std::max(0.f, g.alpha - g.aRate * dt);
		ghosts.erase(std::remove_if(ghosts.begin(), ghosts.end(), [](const Tile& g) { return g.alpha <= 0.002f; }), ghosts.end());
		if (bgFade < 1.f) bgFade = std::min(1.f, bgFade + dt / 0.9f);
		computeRects(); computeTelemetry(); autoCycleStep(dt);
	}

	void computeRects() {
		const float gap = 1.5f;
		auto rect = [&](Tile& t) {
			const float s = t.span * cellPx; t.rx = t.gx * cellPx + gap; t.ry = t.gy * cellPx + gap; t.rw = std::max(1.f, s - 2 * gap); t.rh = t.rw;
			t.drawMask = lerpMask(t.mask, t.maskTo, smooth01(t.maskT));
		};
		for (auto& t : tiles) if (t.live) rect(t);
		for (auto& t : ghosts) rect(t);
	}

	void computeTelemetry() {
		float area = 0, vid = 0, sol = 0; int n = 0;
		for (const auto& t : tiles) {
			if (!t.live || t.dying) continue; ++n;
			const float a = t.span * t.span / float(kCells); area += a;
			if (t.target().type == CType::Video) vid += a; else sol += a;
		}
		tel.occupancy = std::min(1.f, area); tel.vacancy = 1.f - tel.occupancy; tel.tiles = n; tel.ghosts = int(ghosts.size());
		tel.videoFrac = vid; tel.solidFrac = sol; tel.movers = movers(); tel.phase = phaseName; tel.mode = mode_; tel.bg = background();
	}

	void autoCycleStep(float dt) {
		if (!autoCycle) return;
		cycleTimer -= dt; if (cycleTimer > 0) return;
		if (bag.empty()) { for (int i = 0; i < kModeCount; ++i) bag.push_back(i); for (int i = kModeCount - 1; i > 0; --i) std::swap(bag[size_t(i)], bag[size_t(rng.irange(0, i))]); if (bag.back() == int(mode_)) std::swap(bag.front(), bag.back()); }
		const int next = bag.back(); bag.pop_back();
		if (next == int(mode_)) { cycleTimer = 1.f; return; }
		rebuild(ModeId(next)); cycleTimer = rng.range(38.f, 62.f);
	}

	// ---- shared neighbourhood helpers ----------------------------------------------------------------------
	template <class F> Mask adjMask(int cell, F same, float r) const { // round the convex corners that face open space (organic outlines)
		const int cx = cell % kCols, cy = cell / kCols;
		auto has = [&](int dx, int dy) { const int x = cx + dx, y = cy + dy; return inGrid(x, y) && same(cidx(x, y)); };
		const bool up = has(0, -1), dn = has(0, 1), lf = has(-1, 0), rt = has(1, 0);
		return maskCorners((!up && !lf) ? r : 0.f, (!up && !rt) ? r : 0.f, (!dn && !rt) ? r : 0.f, (!dn && !lf) ? r : 0.f);
	}
	std::vector<int> shuffled(std::vector<int> v) { for (int i = int(v.size()) - 1; i > 0; --i) std::swap(v[size_t(i)], v[size_t(rng.irange(0, i))]); return v; }
	std::vector<int> allCells() { std::vector<int> v(kCells); for (int i = 0; i < kCells; ++i) v[size_t(i)] = i; return v; }

	// ================================ 01 MIRROR QUARTET ===========================================================
	struct Unit { int ux = 0, uy = 0; float u = 0.5f, v = 0.5f, zoom = 1.f, thr = 0.5f; int variant = 0, slotA = 0, slotB = 1; bool strict = true; };
	std::vector<Unit> units;
	static constexpr int kUnitCols = kCols / 2, kUnitRows = (kRows - 1) / 2; // 8 x 4 (bottom row of cells left for loose strays)
	Mask quartetMask(int slot, bool strict) const { return strict ? cornerToward(slot == 0 ? 0 : slot == 1 ? 1 : slot == 3 ? 2 : 3, 0.5f) : (slot == 0 || slot == 3) ? maskRounded(0.12f) : maskSquare(); }
	Content quartetContent(const Unit& un, int slot) const { return videoAt(un.u, un.v, un.zoom, (slot & 1) != 0, (slot & 2) != 0); }
	void buildQuartet() {
		for (int uy = 0; uy < kUnitRows; ++uy) for (int ux = 0; ux < kUnitCols; ++ux) {
			Unit un; un.ux = ux; un.uy = uy; un.u = (2.f * float(ux) + 1.f) / float(kCols) + rng.range(-0.03f, 0.03f); un.v = (2.f * float(uy) + 1.f) / float(kRows) + rng.range(-0.04f, 0.04f);
			un.zoom = rng.range(1.0f, 2.4f); un.thr = rng.f(); un.variant = rng.irange(0, 2); un.slotA = rng.irange(0, 3); un.slotB = (un.slotA + rng.irange(1, 3)) % 4; units.push_back(un);
		}
		mi[0] = -1; ms[0] = 1.5f; phaseName = "QUARTET"; applyQuartet(true);
	}
	bool unitStrict(const Unit& un, float f) const { return f >= 0.999f || un.thr <= 0.18f + 0.82f * f * 0.9f; }
	void applyQuartet(bool initial) {
		const float f = dial(ModeId::MIRROR_QUARTET, 0); mi[0] = int(std::lround(f * 100)); const bool strictField = f >= 0.999f;
		for (size_t ui = 0; ui < units.size(); ++ui) {
			Unit& un = units[ui]; un.strict = unitStrict(un, f);
			for (int slot = 0; slot < 4; ++slot) {
				const int cell = (2 * un.uy + slot / 2) * kCols + 2 * un.ux + (slot % 2); const float delay = initial ? rng.range(0.f, 0.9f) : rng.range(0.f, 0.3f);
				Content c = quartetContent(un, slot); Mask m = quartetMask(slot, un.strict);
				if (!un.strict) { // loose: some members missing / recoloured / replaced by non-matching crops — never reported as a complete quartet
					if (slot == un.slotA) { if (un.variant == 0) c = vacant(); else if (un.variant == 1) c = solidAuthored(int(ui) + slot); else c = videoAt(un.u + 0.12f, un.v - 0.1f, un.zoom * 1.3f, (slot & 1) == 0, (slot & 2) == 0); }
					if (slot == un.slotB && un.variant != 0 && rng.chance(0.6f)) c = (un.variant == 2) ? vacant() : solidAccent();
				}
				int t = occupant[size_t(cell)];
				if (c.type == CType::Vacant) { if (t >= 0) dieTile(t, delay); continue; }
				if (t < 0) { t = addTile(cell, c, m, delay, 0.5f); } else setContent(t, c, m, delay, 0.4f);
				if (t >= 0) { tiles[size_t(t)].group = un.strict || c.type == CType::Video ? int(ui) : int(ui); tiles[size_t(t)].gslot = slot; }
			}
		}
		// the bottom cell row: stray asymmetric fragments only in the loose field (strict field = no non-quartet tiles)
		for (int cx = 0; cx < kCols; ++cx) {
			const int cell = (kRows - 1) * kCols + cx; const int t = occupant[size_t(cell)];
			const bool want = !strictField && hash01(cx, gen, int(f * 10)) < 0.55f * (1.f - f);
			if (!want) { if (t >= 0 && tiles[size_t(t)].group < 0) dieTile(t, rng.range(0.f, 0.3f)); continue; }
			Content c = hash01(cx, gen, 7) < 0.5f ? solidAuthored(cx) : videoAt(hash01(cx, 3, gen), 0.9f, 1.5f, hash01(cx, 5, gen) < 0.5f, false);
			if (t < 0) addTile(cell, c, maskRounded(0.3f), rng.range(0.f, 0.4f), 0.5f);
		}
	}
	void stepQuartet(float dt) {
		const int fid = int(std::lround(dial(ModeId::MIRROR_QUARTET, 0) * 100)); if (fid != mi[0] && modeClock > 0.2f) applyQuartet(false);
		const float rate = dialPhys(ModeId::MIRROR_QUARTET, 1);
		ms[0] -= dt; if (ms[0] > 0) return;
		ms[0] = rng.range(0.6f, 1.4f) / rate;
		const int ui = rng.irange(0, int(units.size()) - 1); Unit& un = units[size_t(ui)];
		auto busy = [&](const Unit& u) { for (int s = 0; s < 4; ++s) { const int c = (2 * u.uy + s / 2) * kCols + 2 * u.ux + s % 2; const int t = occupant[size_t(c)]; if (t >= 0 && !tileFree(t)) return true; if (reserved[size_t(c)] >= 0) return true; } return false; };
		if (busy(un)) return;
		if (rng.chance(0.5f)) { // mutate the unit's source crop (all four members change together)
			un.u = std::clamp(un.u + rng.range(-0.12f, 0.12f), 0.05f, 0.95f); un.v = std::clamp(un.v + rng.range(-0.15f, 0.15f), 0.08f, 0.92f); un.zoom = rng.range(1.0f, 2.6f);
			for (int slot = 0; slot < 4; ++slot) { const int cell = (2 * un.uy + slot / 2) * kCols + 2 * un.ux + slot % 2; const int t = occupant[size_t(cell)]; if (t < 0 || tiles[size_t(t)].target().type != CType::Video || tiles[size_t(t)].gslot != slot) continue; if (!un.strict && (slot == un.slotA)) continue; setContent(t, quartetContent(un, slot), currentMaskOf(t), slot * 0.05f, 0.5f); }
		} else { // swap with an adjacent unit: the whole 2x2 group moves as a unit
			const int dx = rng.chance(0.5f) ? 1 : 0, dy = dx ? 0 : 1; const int vx = un.ux + dx, vy = un.uy + dy; if (vx >= kUnitCols || vy >= kUnitRows) return;
			int other = -1; for (size_t k = 0; k < units.size(); ++k) if (units[k].ux == vx && units[k].uy == vy) other = int(k);
			if (other < 0 || busy(units[size_t(other)])) return;
			Unit& ov = units[size_t(other)]; const float dur = rng.range(0.7f, 1.1f);
			// tiles keep their slot; they move to the same slot of the other block
			for (int slot = 0; slot < 4; ++slot) {
				const int ca = (2 * un.uy + slot / 2) * kCols + 2 * un.ux + slot % 2, cb = (2 * ov.uy + slot / 2) * kCols + 2 * ov.ux + slot % 2;
				const int ta = occupant[size_t(ca)], tb = occupant[size_t(cb)];
				if (ta >= 0 && tb >= 0) swapTiles(ta, tb, dur, 0);
				else if (ta >= 0) moveTile(ta, cb, dur, 0.0f, 0);
				else if (tb >= 0) moveTile(tb, ca, dur, 0.0f, 0);
			}
			std::swap(un.ux, ov.ux); std::swap(un.uy, ov.uy);
			for (int slot = 0; slot < 4; ++slot) { for (Unit* U : {&un, &ov}) { const int cell = (2 * U->uy + slot / 2) * kCols + 2 * U->ux + slot % 2; const int rsv = reserved[size_t(cell)]; if (rsv >= 0) tiles[size_t(rsv)].group = int(U == &un ? ui : other); } }
		}
	}
	Mask currentMaskOf(int t) const { return tiles[size_t(t)].maskTo; }

	// ================================ 02 KALEIDOSCOPE FIELD ========================================================
	struct Cluster { int x = 0, y = 0, S = 2; float bx = 0, by = 0; int orient = 0; float timer = 0; };
	std::vector<Cluster> clusters; std::array<int, kCells> own{};
	void clusterCells(const Cluster& c, std::vector<int>& out) const { out.clear(); for (int j = 0; j < c.S; ++j) for (int i = 0; i < c.S; ++i) out.push_back(cidx(c.x + i, c.y + j)); }
	bool clusterFits(int x, int y, int S, int ignore) const { if (x < 0 || y < 0 || x + S > kCols || y + S > kRows) return false; for (int j = 0; j < S; ++j) for (int i = 0; i < S; ++i) { const int o = own[size_t(cidx(x + i, y + j))]; if (o >= 0 && o != ignore) return false; } return true; }
	void renderCluster(int ci, float delay) {
		Cluster& c = clusters[size_t(ci)]; const int S = c.S, half = S / 2;
		for (int j = 0; j < S; ++j) for (int i = 0; i < S; ++i) {
			const int qx = i < half ? i : S - 1 - i, qy = j < half ? j : S - 1 - j; const bool fx = i >= half, fy = j >= half;
			int sx = qx, sy = qy; if (c.orient & 1) std::swap(sx, sy);
			if (c.orient & 2) { sx = half - 1 - sx; if (sx < 0) sx = 0; }
			const float u = (c.bx + float(sx) + 0.5f) / float(kCols), v = (c.by + float(sy) + 0.5f) / float(kRows);
			const int toward = (i < half) ? (j < half ? 2 : 1) : (j < half ? 3 : 0);
			const int ring = std::min(std::min(i, S - 1 - i), std::min(j, S - 1 - j));
			Mask m;
			if (ring == 0 && ((i == 0 || i == S - 1) && (j == 0 || j == S - 1))) m = cornerToward((toward + 2) % 4, 0.55f);
			else if (ring % 2 == 1) m = cornerToward(toward, 1.0f);   // quarter-circle motif facing the centre -> flower
			else m = maskRounded(0.12f);
			applyCell(cidx(c.x + i, c.y + j), videoAt(u, v, 1.f, fx, fy), m, delay + 0.04f * float(ring + i + j), 0.5f);
			own[size_t(cidx(c.x + i, c.y + j))] = ci;
		}
	}
	void clearCluster(int ci, float delay) { Cluster& c = clusters[size_t(ci)]; for (int j = 0; j < c.S; ++j) for (int i = 0; i < c.S; ++i) { const int cell = cidx(c.x + i, c.y + j); applyCell(cell, vacant(), maskSquare(), delay + 0.03f * float(i + j)); own[size_t(cell)] = -1; } }
	void buildKaleido() {
		own.fill(-1); phaseName = "MOTIFS";
		const int maxS = std::max(2, int(std::lround(dialPhys(ModeId::KALEIDOSCOPE_FIELD, 0) / 2.f)) * 2);
		for (int tries = 0; tries < 60 && int(clusters.size()) < 9; ++tries) {
			const int S = std::max(2, std::min(maxS, 2 * rng.irange(1, maxS / 2))); const int x = rng.irange(0, kCols - S), y = rng.irange(0, kRows - S);
			if (!clusterFits(x, y, S, -1)) continue;
			Cluster c; c.x = x; c.y = y; c.S = S; c.bx = std::clamp(rng.range(0.f, float(kCols - S / 2)), 0.f, float(kCols - S / 2)); c.by = std::clamp(rng.range(0.f, float(kRows - S / 2)), 0.f, float(kRows - S / 2)); c.orient = rng.irange(0, 3); c.timer = rng.range(2.f, 6.f);
			clusters.push_back(c); renderCluster(int(clusters.size()) - 1, rng.range(0.f, 0.8f));
		}
		ms[0] = 2.f; ms[1] = 1.f;
	}
	void stepKaleido(float dt) {
		const float maxSf = dialPhys(ModeId::KALEIDOSCOPE_FIELD, 0), rot = dialPhys(ModeId::KALEIDOSCOPE_FIELD, 1);
		const int maxS = std::max(2, int(std::lround(maxSf / 2.f)) * 2);
		for (size_t ci = 0; ci < clusters.size(); ++ci) {
			Cluster& c = clusters[ci];
			if (c.S > maxS) { clearCluster(int(ci), 0.f); c.S = maxS; if (!clusterFits(c.x, c.y, c.S, int(ci))) { c.x = std::min(c.x, kCols - c.S); c.y = std::min(c.y, kRows - c.S); } renderCluster(int(ci), 0.1f); }
		}
		ms[0] -= dt; // rotation: the motif orientation steps (source quadrant order / transposition)
		if (rot > 0.005f && ms[0] <= 0) { ms[0] = rng.range(0.7f, 1.3f) / rot; if (!clusters.empty()) { const int ci = rng.irange(0, int(clusters.size()) - 1); clusters[size_t(ci)].orient = (clusters[size_t(ci)].orient + 1) % 4; renderCluster(ci, 0.f); } }
		ms[1] -= dt;
		if (ms[1] <= 0 && !clusters.empty()) {
			ms[1] = rng.range(2.5f, 6.f); const int ci = rng.irange(0, int(clusters.size()) - 1); Cluster& c = clusters[size_t(ci)]; const int act = rng.irange(0, 3);
			if (act == 0 && c.S + 2 <= maxS) { const int ns = c.S + 2, nx = std::clamp(c.x - 1, 0, kCols - ns), ny = std::clamp(c.y - 1, 0, kRows - ns); if (clusterFits(nx, ny, ns, ci)) { clearCluster(ci, 0.f); c.x = nx; c.y = ny; c.S = ns; renderCluster(ci, 0.2f); } }
			else if (act == 1 && c.S > 2) { clearCluster(ci, 0.f); c.S -= 2; c.x += 1; c.y += 1; renderCluster(ci, 0.2f); }
			else if (act == 2) { clearCluster(ci, 0.f); for (int tries = 0; tries < 12; ++tries) { const int nx = rng.irange(0, kCols - c.S), ny = rng.irange(0, kRows - c.S); if (clusterFits(nx, ny, c.S, ci)) { c.x = nx; c.y = ny; break; } } c.bx = rng.range(0.f, float(kCols - c.S / 2)); c.by = rng.range(0.f, float(kRows - c.S / 2)); renderCluster(ci, 0.7f); }
			else { c.bx = std::clamp(c.bx + rng.range(-3.f, 3.f), 0.f, float(kCols - c.S / 2)); c.by = std::clamp(c.by + rng.range(-2.f, 2.f), 0.f, float(kRows - c.S / 2)); renderCluster(ci, 0.f); }
		}
		if (clusters.size() < 6 && rng.chance(0.004f)) { const int S = std::max(2, std::min(maxS, 2 * rng.irange(1, maxS / 2))); const int x = rng.irange(0, kCols - S), y = rng.irange(0, kRows - S); if (clusterFits(x, y, S, -1)) { Cluster c; c.x = x; c.y = y; c.S = S; c.bx = rng.range(0.f, float(kCols - S / 2)); c.by = rng.range(0.f, float(kRows - S / 2)); c.orient = rng.irange(0, 3); clusters.push_back(c); renderCluster(int(clusters.size()) - 1, 0.f); } }
	}

	// ================================ 03 OPEN-SLOT PUZZLE ===========================================================
	Mask puzzleMask() const { return maskRounded(0.14f); }
	void buildPuzzle() {
		phaseName = "SLIDING"; const int V = std::lround(dialPhys(ModeId::OPEN_SLOT_PUZZLE, 1)); std::vector<int> cells = shuffled(allCells()); std::vector<char> hole(kCells, 0);
		for (int i = 0; i < V; ++i) hole[size_t(cells[size_t(i)])] = 1;
		for (int c = 0; c < kCells; ++c) if (!hole[size_t(c)]) addTile(c, videoHome(c), puzzleMask(), rng.range(0.f, 0.9f), 0.6f);
		ms[0] = 1.f; ms[1] = rng.range(8.f, 14.f);
	}
	bool tryPushChain(int vacantCell, int dx, int dy, int maxLen, float stagger, int curve) {
		const int vx = vacantCell % kCols, vy = vacantCell / kCols; std::vector<int> run; int x = vx - dx, y = vy - dy;
		while ((int)run.size() < maxLen && inGrid(x, y)) { const int t = occupant[size_t(cidx(x, y))]; if (t < 0 || !tileFree(t)) break; run.push_back(t); x -= dx; y -= dy; }
		if (run.empty()) return false;
		const int len = rng.irange(1, int(run.size())); int dest = vacantCell; int moved = 0;
		for (int k = 0; k < len; ++k) { const int from = tiles[size_t(run[size_t(k)])].cell; if (!moveTile(run[size_t(k)], dest, rng.range(0.38f, 0.55f), stagger * float(k), curve)) break; dest = from; ++moved; }
		return moved > 0;
	}
	void stepPuzzle(float dt) {
		const int maxMovers = int(std::lround(dialPhys(ModeId::OPEN_SLOT_PUZZLE, 0))), V = int(std::lround(dialPhys(ModeId::OPEN_SLOT_PUZZLE, 1)));
		ms[2] -= dt;
		if (ms[2] <= 0) { // vacancy dial changed live: remove / restore tiles one at a time
			ms[2] = 0.35f; const int cur = vacantCellCount();
			if (cur < V) { std::vector<int> c = shuffled(allCells()); for (int cell : c) if (occupant[size_t(cell)] >= 0 && reserved[size_t(cell)] < 0 && tileFree(occupant[size_t(cell)])) { dieTile(occupant[size_t(cell)]); break; } }
			else if (cur > V && movers() == 0) { std::vector<int> c = shuffled(allCells()); for (int cell : c) if (occupant[size_t(cell)] < 0 && reserved[size_t(cell)] < 0) { addTile(cell, videoHome(cell), puzzleMask(), 0.f, 0.5f); break; } }
		}
		ms[1] -= dt; if (ms[1] <= 0 && ms[3] <= 0) { ms[3] = rng.range(1.6f, 3.4f); ms[1] = ms[3] + rng.range(7.f, 13.f); } // restful pause
		if (ms[3] > 0) { ms[3] -= dt; if (movers() == 0) return; }
		ms[0] -= dt; if (ms[0] > 0 || movers() >= maxMovers || ms[3] > 0) return;
		ms[0] = rng.range(0.12f, 0.45f) * (1.f + 0.15f * float(6 - maxMovers));
		std::vector<int> holes; for (int c = 0; c < kCells; ++c) if (occupant[size_t(c)] < 0 && reserved[size_t(c)] < 0) holes.push_back(c);
		if (holes.empty()) return; holes = shuffled(holes);
		for (int h : holes) {
			static const int dd[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; const int start = rng.irange(0, 3);
			for (int k = 0; k < 4; ++k) { const int* d = dd[(start + k) % 4]; const int len = std::min(maxMovers - movers(), rng.chance(0.25f) ? rng.irange(2, 3) : 1); if (len < 1) return; if (tryPushChain(h, d[0], d[1], len, 0.14f, rng.chance(0.3f) ? 1 : 0)) return; }
		}
	}

	// ================================ 04 CHROMATIC OCCUPATION =======================================================
	std::array<uint8_t, kCells> cs{}; // 0 empty, 1 authored, 2 sampled, 3 video lens
	Mask occMask(int cell) const { const int k = int(hash01(cell % kCols / 2, cell / kCols / 2, gen) * 3.f); return k == 0 ? maskRounded(0.22f) : k == 1 ? maskRounded(0.5f) : cornerToward((cell % 2 == 0) ? 0 : 2, 1.0f); }
	Content occContent(int cell, int kind) {
		if (kind == 1) { return hash01(cell, gen, 9) < 0.14f ? solidAccent() : solidAuthored(int(hash01(cell % kCols / 3, cell / kCols / 3, gen) * 4.f)); }
		if (kind == 2) return solidSampled(cell);
		return videoAt(hash01(cell, gen, 1) * 0.8f + 0.1f, hash01(cell, gen, 2) * 0.8f + 0.1f, 1.8f, hash01(cell, gen, 3) < 0.5f, false);
	}
	void setOcc(int cell, int kind, float delay) { cs[size_t(cell)] = uint8_t(kind); applyCell(cell, kind ? occContent(cell, kind) : vacant(), occMask(cell), delay, 0.45f); }
	void buildOccupation() {
		cs.fill(0); phaseName = "COLONIZING"; const int seeds = 3; for (int i = 0; i < seeds; ++i) setOcc(rng.irange(0, kCells - 1), rng.irange(1, 3), rng.range(0.f, 0.6f)); ms[0] = 0.5f;
	}
	void stepOccupation(float dt) {
		ms[0] -= dt; if (ms[0] > 0) return;
		const float cover = dialPhys(ModeId::CHROMATIC_OCCUPATION, 0), turn = dialPhys(ModeId::CHROMATIC_OCCUPATION, 1);
		ms[0] = rng.range(0.7f, 1.3f) / turn; int occ = 0; for (uint8_t v : cs) if (v) ++occ; const float frac = float(occ) / float(kCells);
		auto frontier = [&](bool wantOcc) { std::vector<int> v; for (int c = 0; c < kCells; ++c) { if ((cs[size_t(c)] != 0) == wantOcc) { const int x = c % kCols, y = c / kCols; bool adj = false; static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; for (auto& q : d) if (inGrid(x + q[0], y + q[1]) && ((cs[size_t(cidx(x + q[0], y + q[1]))] != 0) != wantOcc)) adj = true; if (adj) v.push_back(c); } } return v; };
		if (frac < cover - 0.03f) { std::vector<int> f = frontier(false); if (f.empty()) { f = shuffled(allCells()); } for (int k = 0; k < rng.irange(1, 3) && !f.empty(); ++k) { const int c = f[size_t(rng.irange(0, int(f.size()) - 1))]; if (cs[size_t(c)] == 0) { const float r = rng.f(); setOcc(c, r < 0.4f ? 2 : r < 0.8f ? 1 : 3, 0.f); } } }
		else if (frac > cover + 0.03f) { std::vector<int> f = frontier(true); if (!f.empty()) setOcc(f[size_t(rng.irange(0, int(f.size()) - 1))], 0, 0.f); }
		else { std::vector<int> o; for (int c = 0; c < kCells; ++c) if (cs[size_t(c)]) o.push_back(c); if (!o.empty()) { const int c = o[size_t(rng.irange(0, int(o.size()) - 1))]; const int old = cs[size_t(c)]; setOcc(c, old == 3 ? rng.irange(1, 2) : (rng.chance(0.4f) ? 3 : 3 - old), 0.f); } }
	}

	// ================================ 05 DISSOLVING GRID ============================================================
	std::array<float, kCells> noiseV{}; std::array<int, kCells> rankOf{};
	void buildDissolve() {
		phaseName = "WHOLE"; for (int c = 0; c < kCells; ++c) noiseV[size_t(c)] = rng.f();
		ms[0] = 1.f; ms[1] = 0; ms[2] = rng.range(3.f, 6.f); mi[0] = 0; dissolveApply(true);
	}
	void dissolveApply(bool initial) {
		std::vector<int> order = allCells(); std::sort(order.begin(), order.end(), [&](int a, int b) { return noiseV[size_t(a)] < noiseV[size_t(b)]; });
		for (int i = 0; i < kCells; ++i) rankOf[size_t(order[size_t(i)])] = i;
		const float occ = ms[0]; const int keep = int(std::lround(occ * float(kCells))), band = int(std::lround(0.10f * float(kCells)));
		auto isVideo = [&](int c) { return rankOf[size_t(c)] < keep; };
		for (int c = 0; c < kCells; ++c) {
			const float delay = initial ? rng.range(0.f, 0.9f) : noiseV[size_t(c)] * 0.35f;
			if (isVideo(c)) applyCell(c, videoHome(c), adjMask(c, isVideo, 0.45f), delay, 0.4f);
			else if (rankOf[size_t(c)] < keep + band && occ < 0.97f) applyCell(c, solidSampled(c), adjMask(c, [&](int q) { return rankOf[size_t(q)] >= keep && rankOf[size_t(q)] < keep + band; }, 0.4f), delay, 0.5f);
			else applyCell(c, vacant(), maskSquare(), delay);
		}
	}
	void stepDissolve(float dt) {
		const float rate = dialPhys(ModeId::DISSOLVING_GRID, 0), minOcc = dialPhys(ModeId::DISSOLVING_GRID, 1);
		for (int c = 0; c < kCells; ++c) noiseV[size_t(c)] += dt * 0.004f * (hash01(c, 3, gen) - 0.5f); // slow drift keeps islands alive
		float& occ = ms[0]; ms[2] -= dt; const float old = occ;
		if (mi[0] == 0) { phaseName = "WHOLE"; if (ms[2] <= 0) { mi[0] = 1; } }
		else if (mi[0] == 1) { phaseName = "ERODING"; occ -= rate * dt; if (occ <= minOcc) { occ = minOcc; mi[0] = 2; ms[2] = rng.range(2.f, 5.f); } }
		else if (mi[0] == 2) { phaseName = "ISLANDS"; if (ms[2] <= 0) mi[0] = 3; }
		else { phaseName = "REGROWING"; occ += rate * 1.5f * dt; if (occ >= 1.f) { occ = 1.f; mi[0] = 0; ms[2] = rng.range(3.f, 7.f); } }
		if (occ > 1.f) occ = 1.f; if (occ < minOcc && mi[0] != 3) occ = std::max(occ, minOcc);
		ms[4] -= dt; if (std::fabs(occ - old) > 0.0001f || ms[4] <= 0.f) { if (ms[4] <= 0.f) ms[4] = 0.25f; dissolveApply(false); }
	}

	// ================================ 06 ECHO MATRIX ==================================================================
	struct Wave { int src; float t0, speed, maxR; int kind; };
	std::vector<Wave> waves;
	void buildEcho() {
		phaseName = "STILL"; for (int c = 0; c < kCells; ++c) addTile(c, videoHome(c), maskRounded(0.06f), (float(c % kCols) + float(c / kCols)) * 0.04f, 0.6f);
		ms[0] = 1.2f;
	}
	void stepEcho(float dt) {
		const float R = dialPhys(ModeId::ECHO_MATRIX, 0), sp = dialPhys(ModeId::ECHO_MATRIX, 1);
		ms[0] -= dt; if (ms[0] <= 0 && waves.size() < 2) { ms[0] = rng.range(1.8f, 4.5f); waves.push_back({rng.irange(0, kCells - 1), modeClock, sp * rng.range(0.85f, 1.2f), R, rng.irange(0, 2)}); }
		waves.erase(std::remove_if(waves.begin(), waves.end(), [&](const Wave& w) { return (modeClock - w.t0) * w.speed > w.maxR + 3.f; }), waves.end());
		phaseName = waves.empty() ? "STILL" : "PROPAGATING";
		ms[1] -= dt; if (ms[1] > 0) return; ms[1] = 0.05f;
		for (int c = 0; c < kCells; ++c) {
			Content want = videoHome(c); Mask m = maskRounded(0.06f);
			for (const Wave& w : waves) {
				const float dx = float(c % kCols - w.src % kCols), dy = float(c / kCols - w.src / kCols), d = std::sqrt(dx * dx + dy * dy), front = (modeClock - w.t0) * w.speed;
				if (d > w.maxR || d > front || d < front - 1.7f) continue;
				const int k = int(d); const Content srcC = videoHome(w.src, 1.f);
				want = videoAt(srcC.u, srcC.v, 2.0f + 0.5f * float(k % 3), (k & 1) != 0 || w.kind == 1, ((k / 2) & 1) != 0 || w.kind == 2);
				m = (k % 2) ? cornerToward((c + k) % 4, 1.0f) : maskRounded(0.5f); break;
			}
			const int t = occupant[size_t(c)]; if (t >= 0) setContent(t, want, m, 0.f, 0.22f);
		}
	}

	// ================================ 07 CHECKERBOARD EXCHANGE =========================================================
	std::array<uint8_t, kCells> cp{}; // population role index (0/1) per cell
	int popKinds[2] = {0, 1};          // 0 video, 1 zoom crop, 2 colour, 3 empty
	Content popContent(int cell, int kind) const {
		switch (kind) { case 0: return videoHome(cell); case 1: return videoAt(hash01(cell, gen, 4) * 0.7f + 0.15f, hash01(cell, gen, 5) * 0.7f + 0.15f, 2.2f, false, false);
			case 2: return hash01(cell, gen, 8) < 0.2f ? solidAccent() : (hash01(cell, gen, 6) < 0.5f ? solidAuthored(cell % kCols / 4) : solidSampled(cell)); default: return Content{}; }
	}
	Mask popMask(int cell, int kind) const { const int cx = cell % kCols, cy = cell / kCols; if (kind == 0) return maskSquare(); if (kind == 1) return maskRounded(0.25f); if (kind == 2) return cornerToward(((cx & 1) << 1 | (cy & 1)) == 0 ? 0 : ((cx & 1) << 1 | (cy & 1)) == 1 ? 3 : ((cx & 1) << 1 | (cy & 1)) == 2 ? 1 : 2, 1.0f); return maskSquare(); }
	void checkerRole(float delaySpread, bool initial) {
		const float reg = dial(ModeId::CHECKERBOARD_EXCHANGE, 0);
		for (int c = 0; c < kCells; ++c) {
			const int par = ((c % kCols) + (c / kCols)) & 1; const bool flip = hash01(c, 1, gen) < (1.f - reg) * 0.4f; const int role = (cp[size_t(c)] ^ par ^ (flip ? 1 : 0)) & 1; const int kind = popKinds[role];
			applyCell(c, popContent(c, kind), popMask(c, kind), initial ? rng.range(0.f, 0.8f) : delaySpread * hash01(c, 2, int(clock * 3.f)), 0.4f);
		}
	}
	void buildChecker() {
		static const int pairs[4][2] = {{0, 2}, {1, 3}, {0, 1}, {2, 3}}; const int* p = pairs[rng.irange(0, 3)]; popKinds[0] = p[0]; popKinds[1] = p[1]; cp.fill(0);
		phaseName = "PATTERN"; ms[0] = 2.f; mi[0] = int(dial(ModeId::CHECKERBOARD_EXCHANGE, 0) * 100); checkerRole(0, true);
	}
	void stepChecker(float dt) {
		const int reg = int(dial(ModeId::CHECKERBOARD_EXCHANGE, 0) * 100); if (reg != mi[0]) { mi[0] = reg; checkerRole(0.2f, false); }
		const float freq = dialPhys(ModeId::CHECKERBOARD_EXCHANGE, 1); ms[0] -= dt; if (ms[0] > 0) return; ms[0] = rng.range(0.8f, 1.4f) / freq;
		const bool local = rng.chance(0.4f); const int ox = rng.irange(0, kCols - 1), oy = rng.irange(0, kRows - 1), dir = rng.irange(0, 3); const float rad = rng.range(2.5f, 5.f);
		phaseName = local ? "LOCAL EXCHANGE" : "WAVE EXCHANGE";
		for (int c = 0; c < kCells; ++c) {
			const int x = c % kCols, y = c / kCols; if (local && std::hypot(float(x - ox), float(y - oy)) > rad) continue;
			cp[size_t(c)] ^= 1; const int par = (x + y) & 1; const bool flip = hash01(c, 1, gen) < (1.f - dial(ModeId::CHECKERBOARD_EXCHANGE, 0)) * 0.4f; const int kind = popKinds[(cp[size_t(c)] ^ par ^ (flip ? 1 : 0)) & 1];
			const float dist = local ? std::hypot(float(x - ox), float(y - oy)) * 0.07f : (dir == 0 ? x : dir == 1 ? kCols - 1 - x : dir == 2 ? y * 1.5f : (kRows - 1 - y) * 1.5f) * 0.05f;
			applyCell(c, popContent(c, kind), popMask(c, kind), dist, 0.35f);
		}
	}

	// ================================ 08 CELLULAR GROWTH ===============================================================
	struct Colony { int kind = 0; int colorIdx = 0; std::vector<int> cells; float zoom = 1.6f; bool fx = false; };
	std::vector<Colony> colonies; std::array<int, kCells> owner{};
	Content colonyContent(const Colony& c, int cell) const { if (c.kind == 0) return videoAt((float(cell % kCols) + 0.5f) / float(kCols), (float(cell / kCols) + 0.5f) / float(kRows), c.zoom, c.fx, false); if (c.kind == 1) return solidAuthored(c.colorIdx); return solidSampled(cell); }
	void refreshColonyCell(int cell, float delay) {
		const int o = owner[size_t(cell)]; if (o < 0) { applyCell(cell, vacant(), maskSquare(), delay); return; }
		applyCell(cell, colonyContent(colonies[size_t(o)], cell), adjMask(cell, [&](int q) { return owner[size_t(q)] == o; }, 0.9f), delay, 0.4f);
	}
	void touchNeighbors(int cell) { const int x = cell % kCols, y = cell / kCols; static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; for (auto& q : d) if (inGrid(x + q[0], y + q[1])) { const int n = cidx(x + q[0], y + q[1]); if (owner[size_t(n)] >= 0) refreshColonyCell(n, 0.f); } }
	int ownedCount() const { int n = 0; for (int o : owner) if (o >= 0) ++n; return n; }
	void seedColony() { for (int tries = 0; tries < 30; ++tries) { const int c = rng.irange(0, kCells - 1); if (owner[size_t(c)] >= 0) continue; Colony col; col.kind = rng.irange(0, 2); col.colorIdx = rng.irange(0, 3); col.zoom = rng.range(1.4f, 2.2f); col.fx = rng.chance(0.5f); col.cells.push_back(c); colonies.push_back(col); owner[size_t(c)] = int(colonies.size()) - 1; refreshColonyCell(c, 0.f); return; } }
	void buildGrowth() { owner.fill(-1); phaseName = "GROWING"; mi[0] = 0; for (int i = 0; i < 4; ++i) seedColony(); ms[0] = 0.3f; }
	void stepGrowth(float dt) {
		const float rate = dialPhys(ModeId::CELLULAR_GROWTH, 0), br = dialPhys(ModeId::CELLULAR_GROWTH, 1);
		ms[0] -= dt; if (ms[0] > 0) return; ms[0] = 1.f / rate * rng.range(0.7f, 1.3f); const int total = ownedCount();
		if (mi[0] == 0 && (total > int(0.62f * kCells) || (modeClock > 25.f && rng.chance(0.01f)))) { mi[0] = 1; phaseName = "CONTRACTING"; }
		if (mi[0] == 1 && total < int(0.18f * kCells)) { mi[0] = 0; phaseName = "GROWING"; for (int i = 0; i < 2; ++i) seedColony(); }
		if (colonies.empty()) return;
		if (mi[0] == 0) {
			const int ci = rng.irange(0, int(colonies.size()) - 1); Colony& col = colonies[size_t(ci)]; std::vector<int> fr;
			for (int c : col.cells) { const int x = c % kCols, y = c / kCols; static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; for (auto& q : d) if (inGrid(x + q[0], y + q[1]) && owner[size_t(cidx(x + q[0], y + q[1]))] < 0) fr.push_back(cidx(x + q[0], y + q[1])); }
			if (fr.empty()) { if (rng.chance(0.15f)) seedColony(); return; }
			const int maxSame = br > 0.66f ? 1 : br > 0.33f ? 2 : 3;
			for (int tries = 0; tries < 6; ++tries) { const int f = fr[size_t(rng.irange(0, int(fr.size()) - 1))]; int same = 0; const int x = f % kCols, y = f / kCols; static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; for (auto& q : d) if (inGrid(x + q[0], y + q[1]) && owner[size_t(cidx(x + q[0], y + q[1]))] == ci) ++same; if (same > maxSame) continue; owner[size_t(f)] = ci; col.cells.push_back(f); refreshColonyCell(f, 0.f); touchNeighbors(f); break; }
		} else {
			const int ci = rng.irange(0, int(colonies.size()) - 1); Colony& col = colonies[size_t(ci)]; if (col.cells.empty()) return;
			const int c = col.cells.back(); col.cells.pop_back(); owner[size_t(c)] = -1; refreshColonyCell(c, 0.f); touchNeighbors(c);
		}
	}

	// ================================ 09 HIERARCHICAL MOSAIC =============================================================
	struct Node { bool alive = false, leaf = true; float x = 0, y = 0, span = 4; int level = 0, parent = -1, child[4] = {-1, -1, -1, -1}, tile = -1, slot = 0; int variant = 0; };
	std::vector<Node> nodes; std::vector<int> freeNodes;
	int newNode() { if (!freeNodes.empty()) { int i = freeNodes.back(); freeNodes.pop_back(); nodes[size_t(i)] = Node{}; nodes[size_t(i)].alive = true; return i; } nodes.push_back(Node{}); nodes.back().alive = true; return int(nodes.size()) - 1; }
	Content nodeContent(const Node& n, int variantOverride = -1) const {
		const int v = variantOverride >= 0 ? variantOverride : n.variant; const float u = (n.x + n.span * 0.5f) / float(kCols), vv = (n.y + n.span * 0.5f) / float(kRows);
		if (v == 2) return solidAuthored(int(hash01(int(n.x * 4), int(n.y * 4), gen) * 4.f)); if (v == 3) return solidSampled(std::clamp(int(n.y), 0, kRows - 1) * kCols + std::clamp(int(n.x), 0, kCols - 1));
		return videoAt(u, vv, 1.f, false, false);
	}
	void leafTile(int ni, float delay) {
		Node& n = nodes[size_t(ni)]; Content c = nodeContent(n); Mask m = n.level >= 1 ? maskRounded(0.14f) : maskSquare();
		if (n.parent >= 0 && nodes[size_t(n.parent)].variant == 1) { c = videoAt(nodes[size_t(n.parent)].x * 0 + (nodes[size_t(n.parent)].x + nodes[size_t(n.parent)].span * 0.5f) / float(kCols), (nodes[size_t(n.parent)].y + nodes[size_t(n.parent)].span * 0.5f) / float(kRows), 1.6f, (n.slot & 1) != 0, (n.slot & 2) != 0); m = cornerToward(n.slot == 0 ? 0 : n.slot == 1 ? 1 : n.slot == 3 ? 2 : 3, 0.5f); }
		n.tile = addTile(-1, c, m, delay, 0.35f, n.span, n.x, n.y);
	}
	void buildHier() {
		occCells = false; occCellsMode = false; phaseName = "BLOCKS";
		for (int by = 0; by < 2; ++by) for (int bx = 0; bx < 4; ++bx) { const int i = newNode(); nodes[size_t(i)].x = float(bx * 4); nodes[size_t(i)].y = float(by * 4); nodes[size_t(i)].span = 4; nodes[size_t(i)].level = 0; nodes[size_t(i)].variant = rng.irange(0, 3) == 3 ? 2 : 0; leafTile(i, rng.range(0.f, 0.8f)); }
		for (int x = 0; x < kCols; ++x) { const int i = newNode(); nodes[size_t(i)].x = float(x); nodes[size_t(i)].y = 8; nodes[size_t(i)].span = 1; nodes[size_t(i)].level = 2; nodes[size_t(i)].variant = rng.chance(0.3f) ? 3 : 0; leafTile(i, rng.range(0.f, 0.8f)); }
		ms[0] = 1.5f;
	}
	void splitNode(int ni) {
		Node& n = nodes[size_t(ni)]; if (!n.leaf) return; if (n.tile >= 0) { dieTile(n.tile, 0.f); n.tile = -1; }
		n.leaf = false; n.variant = rng.irange(0, 2) == 0 ? 1 : (n.variant == 2 ? 2 : 0); const float h = n.span * 0.5f;
		for (int k = 0; k < 4; ++k) { const int c = newNode(); Node& cn = nodes[size_t(c)]; Node& p = nodes[size_t(ni)]; cn.x = p.x + float(k % 2) * h; cn.y = p.y + float(k / 2) * h; cn.span = h; cn.level = p.level + 1; cn.parent = ni; cn.slot = k; cn.variant = rng.chance(0.18f) ? (rng.chance(0.5f) ? 2 : 3) : 0; nodes[size_t(ni)].child[k] = c; leafTile(c, 0.12f + 0.07f * float(k)); }
	}
	void mergeNode(int ni) {
		Node& n = nodes[size_t(ni)]; for (int k = 0; k < 4; ++k) { const int c = n.child[k]; if (c >= 0) { if (nodes[size_t(c)].tile >= 0) dieTile(nodes[size_t(c)].tile, 0.04f * float(k)); nodes[size_t(c)].alive = false; freeNodes.push_back(c); n.child[k] = -1; } }
		n.leaf = true; n.variant = n.variant == 1 ? 0 : n.variant; leafTile(ni, 0.25f);
	}
	void stepHier(float dt) {
		const int D = int(std::lround(dialPhys(ModeId::HIERARCHICAL_MOSAIC, 0))), maxLevel = D; const float merge = dialPhys(ModeId::HIERARCHICAL_MOSAIC, 1);
		ms[0] -= dt;
		int leaves = 0; for (const auto& n : nodes) if (n.alive && n.leaf) ++leaves;
		// depth dial lowered: merge anything deeper than the limit
		for (size_t i = 0; i < nodes.size(); ++i) { Node& n = nodes[i]; if (!n.alive || n.leaf) continue; bool allLeaf = true; for (int k = 0; k < 4; ++k) if (n.child[k] < 0 || !nodes[size_t(n.child[k])].leaf) allLeaf = false; if (allLeaf && n.level + 1 > maxLevel) { mergeNode(int(i)); return; } }
		if (ms[0] > 0) return; ms[0] = rng.range(0.25f, 0.9f);
		phaseName = "SUBDIVIDING";
		const bool doMerge = rng.chance(merge * 0.8f) || leaves > 120;
		std::vector<int> cand;
		if (doMerge) { for (size_t i = 0; i < nodes.size(); ++i) { const Node& n = nodes[i]; if (!n.alive || n.leaf) continue; bool allLeaf = true; for (int k = 0; k < 4; ++k) if (n.child[k] < 0 || !nodes[size_t(n.child[k])].leaf) allLeaf = false; if (allLeaf) cand.push_back(int(i)); } if (!cand.empty()) { mergeNode(cand[size_t(rng.irange(0, int(cand.size()) - 1))]); phaseName = "RECOMBINING"; } return; }
		for (size_t i = 0; i < nodes.size(); ++i) { const Node& n = nodes[i]; if (n.alive && n.leaf && n.level < maxLevel && n.level < 3 && n.span > 0.55f) cand.push_back(int(i)); }
		if (!cand.empty()) splitNode(cand[size_t(rng.irange(0, int(cand.size()) - 1))]);
	}

	// ================================ 10 CHROMATIC EXCAVATION ==============================================================
	struct Island { float cx, cy, rmax, t, life; };
	std::vector<Island> islands; std::array<uint8_t, kCells> es{};
	Content excavColor(int cell) const { return hash01(cell % kCols / 2, cell / kCols / 2, gen) < 0.4f ? solidSampled(cell) : solidAuthored(int(hash01(cell % kCols / 3, cell / kCols / 2, gen + 1) * 4.f)); }
	Mask excavMask(int cell) const { return ((cell % kCols / 2) + (cell / kCols / 2)) % 2 ? maskRounded(0.2f) : maskSquare(); }
	void buildExcav() { phaseName = "COVERED"; es.fill(9); for (int c = 0; c < kCells; ++c) addTile(c, excavColor(c), excavMask(c), (c % kCols) * 0.03f + rng.range(0.f, 0.3f), 0.5f); ms[0] = 1.5f; }
	void stepExcav(float dt) {
		const float pressure = dialPhys(ModeId::CHROMATIC_EXCAVATION, 0), isz = dialPhys(ModeId::CHROMATIC_EXCAVATION, 1);
		ms[0] -= dt; const int maxIsl = 1 + int(std::lround(pressure * 4.f));
		if (ms[0] <= 0) { ms[0] = rng.range(1.2f, 3.5f) / (0.25f + pressure * 1.5f); if (int(islands.size()) < maxIsl) islands.push_back({rng.range(0.f, float(kCols)), rng.range(0.f, float(kRows)), isz * rng.range(0.7f, 1.1f), 0.f, rng.range(6.f, 11.f) / (0.6f + pressure)}); }
		for (auto& i : islands) i.t += dt; islands.erase(std::remove_if(islands.begin(), islands.end(), [](const Island& i) { return i.t >= i.life; }), islands.end());
		phaseName = islands.empty() ? "COVERED" : "EXCAVATING";
		ms[1] -= dt; if (ms[1] > 0) return; ms[1] = 0.1f;
		for (int c = 0; c < kCells; ++c) {
			const float px = float(c % kCols) + 0.5f, py = float(c / kCols) + 0.5f; uint8_t st = 9;
			for (const auto& i : islands) { const float p = i.t / i.life, r = i.rmax * std::pow(std::sin(3.14159265f * p), 0.8f), d = std::hypot(px - i.cx, py - i.cy); if (d < r - 0.9f) st = std::min<uint8_t>(st, 0); else if (d < r) st = std::min<uint8_t>(st, 1); }
			if (st == es[size_t(c)]) continue; es[size_t(c)] = st;
			if (st == 0) applyCell(c, vacant(), maskSquare(), rng.range(0.f, 0.15f));
			else if (st == 1) applyCell(c, videoAt((float(c % kCols) + 0.5f) / float(kCols), (float(c / kCols) + 0.5f) / float(kRows), 1.6f, (c % 2) == 0, false), maskRounded(0.3f), 0.f, 0.3f);
			else applyCell(c, excavColor(c), excavMask(c), rng.range(0.f, 0.3f), 0.45f);
		}
	}

	// ================================ 11 DOMINO TRANSLATION ================================================================
	void buildDomino() {
		phaseName = "READY"; std::vector<int> cells = shuffled(allCells()); std::vector<char> hole(kCells, 0); for (int i = 0; i < 7; ++i) hole[size_t(cells[size_t(i)])] = 1;
		for (int c = 0; c < kCells; ++c) if (!hole[size_t(c)]) { const Content ct = hash01(c, gen, 5) < 0.22f ? (hash01(c, gen, 6) < 0.2f ? solidAccent() : solidAuthored(c % kCols / 3)) : videoHome(c); addTile(c, ct, (c % 3 == 0) ? maskChamfer(true, false, true, false, 0.28f) : maskSquare(), rng.range(0.f, 0.8f), 0.5f); }
		ms[0] = 1.f; mi[0] = rng.irange(0, 3); mi[1] = 0;
	}
	void stepDomino(float dt) {
		const int maxChain = int(std::lround(dialPhys(ModeId::DOMINO_TRANSLATION, 0))); const float cad = dialPhys(ModeId::DOMINO_TRANSLATION, 1);
		ms[0] -= dt; if (ms[0] > 0 || movers() > 0) return; ms[0] = rng.range(0.7f, 1.3f) / cad;
		static const int dd[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; if (mi[1] <= 0) { mi[0] = rng.irange(0, 3); mi[1] = rng.irange(3, 5); } --mi[1];
		std::vector<int> holes; for (int c = 0; c < kCells; ++c) if (occupant[size_t(c)] < 0 && reserved[size_t(c)] < 0) holes.push_back(c);
		holes = shuffled(holes); phaseName = "PUSH";
		for (int h : holes) { for (int k = 0; k < 4; ++k) { const int* d = dd[(mi[0] + k) % 4]; if (tryPushChain(h, d[0], d[1], maxChain, 0.085f, 3)) return; } }
	}

	// ================================ 12 SYMMETRY COLLAPSE ===================================================================
	struct Patch { float cx, cy, r, rmax, t, life; };
	std::vector<Patch> patches; std::array<uint8_t, kCells> brk{};
	Content symContent(int cell, bool broken) const {
		const int cx = cell % kCols, cy = cell / kCols; if (broken) return videoAt((float(cx) + 0.5f) / float(kCols) + 0.04f * (hash01(cell, gen, 2) - 0.5f), (float(cy) + 0.5f) / float(kRows), 1.f, false, false);
		const int fxc = std::min(cx, kCols - 1 - cx), fyc = std::min(cy, kRows - 1 - cy);
		return videoAt((float(fxc) + 0.5f) / float(kCols), (float(fyc) + 0.5f) / float(kRows), 1.f, cx >= kCols / 2, cy > (kRows - 1) / 2);
	}
	Mask symMask(int cell, bool broken) const {
		if (broken) return maskSquare(); const int cx = cell % kCols, cy = cell / kCols; const bool left = cx < kCols / 2, top = cy <= (kRows - 1) / 2;
		const int toward = left ? (top ? 2 : 1) : (top ? 3 : 0); return ((cx + cy) & 1) ? cornerToward(toward, 0.9f) : maskRounded(0.14f);
	}
	void buildSymmetry() { phaseName = "SYMMETRIC"; brk.fill(0); for (int c = 0; c < kCells; ++c) addTile(c, symContent(c, false), symMask(c, false), (std::fabs(float(c % kCols) - 7.5f) + std::fabs(float(c / kCols) - 4.f)) * 0.07f, 0.5f); ms[0] = 0; mi[0] = 0; ms[1] = dialPhys(ModeId::SYMMETRY_COLLAPSE, 0) * rng.range(0.6f, 1.f); }
	void stepSymmetry(float dt) {
		const float hold = dialPhys(ModeId::SYMMETRY_COLLAPSE, 0), spread = dialPhys(ModeId::SYMMETRY_COLLAPSE, 1); (void)hold;
		ms[1] -= dt;
		if (mi[0] == 0) { phaseName = "SYMMETRIC"; if (ms[1] <= 0) { mi[0] = 1; ms[2] = rng.range(9.f, 15.f); ms[3] = 0; } }
		else if (mi[0] == 1) {
			phaseName = "COLLAPSING"; ms[2] -= dt; ms[3] -= dt; const int maxPatch = 2 + int(std::lround(spread * 5.f));
			if (ms[3] <= 0 && int(patches.size()) < maxPatch) { ms[3] = rng.range(0.8f, 2.2f); patches.push_back({rng.range(0.f, float(kCols)), rng.range(0.f, float(kRows)), 0.f, (2.f + spread * 3.f) * rng.range(0.8f, 1.2f), 0.f, rng.range(5.f, 9.f)}); }
			if (ms[2] <= 0) { mi[0] = 2; ms[2] = 4.f; }
		} else { phaseName = "REFORMING"; ms[2] -= dt; for (auto& p : patches) p.life = std::min(p.life, p.t + 0.01f); if (ms[2] <= 0 && patches.empty()) { mi[0] = 0; ms[1] = dialPhys(ModeId::SYMMETRY_COLLAPSE, 0) * rng.range(0.8f, 1.2f); } }
		for (auto& p : patches) { p.t += dt; p.cx += (hash01(int(p.rmax * 10), 1, gen) - 0.5f) * dt * 0.6f; const float k = p.t / p.life; p.r = p.rmax * std::sin(3.14159265f * std::min(1.f, k)); }
		patches.erase(std::remove_if(patches.begin(), patches.end(), [](const Patch& p) { return p.t >= p.life; }), patches.end());
		ms[4] -= dt; if (ms[4] > 0) return; ms[4] = 0.12f;
		for (int c = 0; c < kCells; ++c) {
			bool b = false; const float px = float(c % kCols) + 0.5f, py = float(c / kCols) + 0.5f; for (const auto& p : patches) if (std::hypot(px - p.cx, py - p.cy) < p.r) b = true;
			if (uint8_t(b) == brk[size_t(c)]) continue; brk[size_t(c)] = uint8_t(b); const int t = occupant[size_t(c)]; if (t >= 0) setContent(t, symContent(c, b), symMask(c, b), rng.range(0.f, 0.15f), 0.5f);
		}
	}

	// ================================ 13 TRAVELING APERTURE ====================================================================
	void buildAperture() { phaseName = "SEEKING"; ms[0] = rng.range(2.f, 14.f); ms[1] = rng.range(1.f, 8.f); ms[2] = rng.range(2.f, 14.f); ms[3] = rng.range(1.f, 8.f); ms[4] = 0; ms[5] = rng.range(1.3f, 1.8f); es.fill(9); }
	void stepAperture(float dt) {
		const float size = dialPhys(ModeId::TRAVELING_APERTURE, 0), speed = dialPhys(ModeId::TRAVELING_APERTURE, 1);
		// ms0/1 = centre, ms2/3 = waypoint, ms4 = pause, ms5 = aspect
		float dx = ms[2] - ms[0], dy = ms[3] - ms[1]; const float d = std::hypot(dx, dy);
		if (ms[4] > 0) ms[4] -= dt; else if (d < 0.3f) { ms[4] = rng.range(0.5f, 2.5f); ms[2] = rng.range(0.f, float(kCols)); ms[3] = rng.range(0.f, float(kRows)); ms[5] = rng.range(1.2f, 1.9f); } else { const float s = std::min(d, speed * dt); ms[0] += dx / d * s; ms[1] += dy / d * s; }
		phaseName = ms[4] > 0 ? "DWELL" : "TRAVELING";
		ms[6] -= dt; if (ms[6] > 0) return; ms[6] = 0.05f;
		auto inside = [&](int c, float scale) { const float px = float(c % kCols) + 0.5f - ms[0], py = float(c / kCols) + 0.5f - ms[1]; const float ax = size * ms[5] * scale, ay = size * 0.8f * scale; return (px * px) / (ax * ax) + (py * py) / (ay * ay) <= 1.f; };
		std::vector<char> in(kCells); for (int c = 0; c < kCells; ++c) in[size_t(c)] = inside(c, 1.f);
		for (int c = 0; c < kCells; ++c) {
			if (!in[size_t(c)]) { if (es[size_t(c)] != 0) { es[size_t(c)] = 0; applyCell(c, vacant(), maskSquare(), 0.f); } continue; }
			const bool rim = !inside(c, 0.72f); const uint8_t st = rim ? 2 : 1; const Mask m = adjMask(c, [&](int q) { return in[size_t(q)] != 0; }, 0.8f);
			Content ct = videoHome(c); if (rim) ct = videoAt((float(c % kCols) + 0.5f) / float(kCols), (float(c / kCols) + 0.5f) / float(kRows), 1.5f, (float(c % kCols) < ms[0]), false);
			const int occT = occupant[size_t(c)]; if (st != es[size_t(c)] || occT < 0 || !sameMask(m, tiles[size_t(occT)].maskTo)) { es[size_t(c)] = st; applyCell(c, ct, m, 0.f, 0.25f); }
		}
	}
};

} // namespace mosaic
