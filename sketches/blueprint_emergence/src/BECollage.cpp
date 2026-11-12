#include "BECollage.h"

#include "ofGraphics.h"
#include "ofMesh.h"
#include "ofMath.h"
#include "ofUtils.h"

#include <algorithm>
#include <cmath>
#include <random>

using hud::PaletteToken;
using hud::RevealDir;

namespace {

constexpr float GEN_ASPECT = 16.0f / 9.0f; // assumed source aspect at generation time
constexpr int GRID_COLS = 24;
constexpr int GRID_ROWS = 13;

float easeOutCubic(float t) {
	t = ofClamp(t, 0.0f, 1.0f);
	float u = 1.0f - t;
	return 1.0f - u * u * u;
}

float overlapFraction(const ofRectangle & a, const ofRectangle & b) {
	float x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
	float x1 = std::min(a.x + a.width, b.x + b.width), y1 = std::min(a.y + a.height, b.y + b.height);
	if (x1 <= x0 || y1 <= y0) return 0.0f;
	float inter = (x1 - x0) * (y1 - y0);
	float smaller = std::min(a.width * a.height, b.width * b.height);
	return smaller > 0 ? inter / smaller : 0.0f;
}

// ── Layout builder ──────────────────────────────────────────────────────────
// Each template defines constraints (roles, relative sizes, which primitives
// may appear); the seed only jitters within them.
struct Builder {
	std::mt19937 rng;
	int W, H;
	float g, y0;
	hud::HudCollagePalette pal;
	CollageLayout out;
	std::vector<glm::vec2> usedCenters;
	int fragCounter = 0;

	Builder(uint32_t seed, int attempt, int w, int h)
		: rng(seed * 2654435761u ^ (static_cast<uint32_t>(attempt) * 0x9E3779B9u + 0x85EBCA6Bu))
		, W(w)
		, H(h) {
		g = static_cast<float>(W) / GRID_COLS;
		y0 = (static_cast<float>(H) - GRID_ROWS * g) * 0.5f;
		out.seed = seed;
		pal = hud::pickCollagePalette(seed);
		out.palette = pal;
	}

	float rnd(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }
	int irnd(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); }
	bool chance(float p) { return rnd(0.0f, 1.0f) < p; }

	ofRectangle R(float col, float row, float cols, float rows) const {
		return ofRectangle(col * g, y0 + row * g, cols * g, rows * g);
	}

	// Curated transform distribution (brief §11).
	void pickTransform(bool forceOdd, QuarterTurn & t, bool & fx, bool & fy) {
		fx = fy = false;
		t = QuarterTurn::Deg0;
		if (forceOdd && chance(0.75f)) {
			t = chance(0.5f) ? QuarterTurn::Deg90 : QuarterTurn::Deg270;
			fx = chance(0.12f);
			return;
		}
		float r = rnd(0.0f, 100.0f);
		if (r < 40.0f) {
		} else if (r < 60.0f)
			t = QuarterTurn::Deg90;
		else if (r < 70.0f)
			t = QuarterTurn::Deg180;
		else if (r < 85.0f)
			t = QuarterTurn::Deg270;
		else if (r < 92.5f)
			fx = true;
		else if (r < 97.5f)
			fy = true;
		else {
			t = chance(0.5f) ? QuarterTurn::Deg90 : QuarterTurn::Deg270;
			(chance(0.5f) ? fx : fy) = true;
		}
	}

	PaletteToken pickFrameToken() {
		float r = rnd(0.0f, 1.0f);
		if (r < 0.45f) return PaletteToken::Dark;
		if (r < 0.80f) return PaletteToken::Primary;
		return PaletteToken::Secondary;
	}

	int addFragment(CollageRole role, const ofRectangle & dest, bool forceOdd = false, bool circular = false) {
		CollageFragment f;
		f.role = role;
		f.destinationRect = dest;
		f.circular = circular;
		pickTransform(forceOdd, f.rotation, f.flipX, f.flipY);
		f.zLayer = (role == CollageRole::Hero) ? 0 : ++fragCounter;

		float sLo = 0.3f, sHi = 0.55f, pFrame = 0.6f, pSwap = 0.55f;
		switch (role) {
		case CollageRole::Hero: sLo = 0.38f; sHi = 0.7f; pFrame = 0.4f; pSwap = 0.0f; break;
		case CollageRole::Secondary: sLo = 0.3f; sHi = 0.55f; pFrame = 0.65f; pSwap = 0.5f; break;
		case CollageRole::Detail: sLo = 0.1f; sHi = 0.24f; pFrame = 0.85f; pSwap = 0.7f; break;
		case CollageRole::Strip: sLo = 0.3f; sHi = 0.6f; pFrame = 0.5f; pSwap = 0.55f; break;
		case CollageRole::Masked: sLo = 0.14f; sHi = 0.28f; pFrame = 0.0f; pSwap = 0.6f; break;
		}
		f.srcScale = rnd(sLo, sHi);

		// Crop size in normalized source units (so the displacement search can
		// keep the whole crop inside the frame).
		const float dw = std::max(dest.width, 1.0f), dh = std::max(dest.height, 1.0f);
		const float A = isOddTurn(f.rotation) ? dh / dw : dw / dh;
		const float full = std::min(1.0f, A / GEN_ASPECT);
		const float cwn = full * f.srcScale;
		const float chn = std::min(1.0f, cwn * GEN_ASPECT / A);
		const float hx = std::min(cwn * 0.5f, 0.5f), hy = std::min(chn * 0.5f, 0.5f);
		// Keep crop centers in the central band: the source frames carry dark
		// borders, and samples of pure border read as dead black rectangles.
		auto band = [](float half, float & lo, float & hi) {
			lo = std::max(half, 0.2f);
			hi = std::min(1.0f - half, 0.8f);
			if (lo >= hi) {
				lo = half;
				hi = 1.0f - half;
			}
			if (lo >= hi) lo = hi = 0.5f;
		};
		float xlo, xhi, ylo, yhi;
		band(hx, xlo, xhi);
		band(hy, ylo, yhi);

		// Source position is chosen to be far from where the fragment lands and
		// from crops already used, so the plate never reads as the source frame.
		const glm::vec2 destC(dest.getCenter().x / W, dest.getCenter().y / H);
		const int swaps = 5;
		for (int s = 0; s < swaps; s++) {
			glm::vec2 best(0.5f, 0.5f);
			float bestScore = -1.0f;
			for (int k = 0; k < 10; k++) {
				glm::vec2 c(xlo < xhi ? rnd(xlo, xhi) : xlo, ylo < yhi ? rnd(ylo, yhi) : ylo);
				float minUsed = 1.0f;
				for (const auto & u : usedCenters)
					minUsed = std::min(minUsed, glm::length(u - c));
				float score = glm::length(c - destC) + 0.8f * minUsed;
				if (score > bestScore) {
					bestScore = score;
					best = c;
				}
			}
			f.srcCenters.push_back(best);
			usedCenters.push_back(best);
		}

		f.hasFrame = chance(pFrame);
		f.frameWidth = (role == CollageRole::Detail) ? rnd(2.0f, 4.0f) : rnd(3.0f, 7.0f);
		f.paletteIndex = static_cast<int>(pickFrameToken());
		f.swapPeriod = chance(pSwap) ? rnd(13.0f, 24.0f) : 0.0f;
		f.showLabel = (role != CollageRole::Hero);

		f.revealDir = static_cast<RevealDir>(irnd(1, 4));
		f.dur = (role == CollageRole::Hero) ? 1.6f : 1.0f;
		// Dissolve: imagery goes first, in two alternating groups.
		f.dissolveDelay = static_cast<float>(out.fragments.size() % 2) * 2.4f + rnd(0.0f, 0.7f);
		f.dissolveDur = 1.3f;

		out.fragments.push_back(f);
		return static_cast<int>(out.fragments.size()) - 1;
	}

	void timing(int fi, int stage, float delay, float dur = -1.0f) {
		auto & f = out.fragments[fi];
		f.stage = stage;
		f.delay = delay;
		if (dur > 0) f.dur = dur;
	}

	void overlay(int fi, const ofRectangle & r, PaletteToken tok, float alpha) {
		auto & f = out.fragments[fi];
		f.hasOverlay = true;
		f.overlayRect = r;
		f.overlayColor = tok;
		f.overlayAlpha = alpha;
	}

	CollageGraphic & add(CollageBlockType type, const ofRectangle & r, PaletteToken col, int pass, int stage, float delay, float dur = 0.8f) {
		CollageGraphic gr;
		gr.type = type;
		gr.rect = r;
		gr.color = col;
		gr.pass = pass;
		gr.stage = stage;
		gr.delay = delay;
		gr.dur = dur;
		gr.dissolveDelay = 4.4f + rnd(0.0f, 2.3f);
		gr.dissolveDur = 1.0f;
		out.graphics.push_back(gr);
		return out.graphics.back();
	}

	void mark(float cx, float cy, float r, hud::RegistrationMarkStyle::Kind kind, int stage, float delay, bool pulse = false) {
		auto & m = add(CollageBlockType::REGISTRATION_MARK, ofRectangle(cx - r, cy - r, r * 2, r * 2), PaletteToken::Dark, 5, stage, delay, 0.3f);
		m.markKind = static_cast<int>(kind);
		m.pulse = pulse;
	}

	bool findFreeRect(float w, float h, const ofRectangle & region, ofRectangle & result) {
		if (region.width < w || region.height < h) return false;
		const float snap = g * 0.5f;
		for (int i = 0; i < 60; i++) {
			float x = std::round(rnd(region.x, region.x + region.width - w) / snap) * snap;
			float y = y0 + std::round((rnd(region.y, region.y + region.height - h) - y0) / snap) * snap;
			ofRectangle c(x, y, w, h);
			bool free = true;
			for (const auto & f : out.fragments) {
				ofRectangle e = f.destinationRect;
				e.x -= g * 0.4f;
				e.y -= g * 0.4f;
				e.width += g * 0.8f;
				e.height += g * 0.8f;
				if (e.intersects(c)) {
					free = false;
					break;
				}
			}
			if (free) {
				result = c;
				return true;
			}
		}
		return false;
	}

	// Samples are squares (or circles) placed in free space; returns fragment
	// index or -1 when the region is full.
	int addSample(const ofRectangle & region, float sizeCols, bool allowMasked, int stage, float delay) {
		ofRectangle r;
		if (!findFreeRect(sizeCols * g, sizeCols * g, region, r)) return -1;
		bool masked = allowMasked && chance(0.4f);
		int fi = addFragment(masked ? CollageRole::Masked : CollageRole::Detail, r, false, masked);
		timing(fi, stage, delay, 0.8f);
		return fi;
	}

	// ── Template A — vertical spine ────────────────────────────────────────
	void templateSpine() {
		out.templateName = "VERTICAL SPINE";
		const int heroCols = irnd(5, 6), heroRows = irnd(10, 11);
		const int heroC = irnd(2, 7), heroR = irnd(0, GRID_ROWS - heroRows);
		const ofRectangle hero = R(static_cast<float>(heroC), static_cast<float>(heroR), static_cast<float>(heroCols), static_cast<float>(heroRows));

		// Large color block anchoring the hero, bleeding off the bottom edge.
		float bbC = static_cast<float>(heroC - irnd(1, 2));
		float bbR = static_cast<float>(heroR + irnd(2, 4));
		ofRectangle bb(bbC * g, y0 + bbR * g, (heroCols + irnd(6, 9)) * g, H - (y0 + bbR * g));
		auto & big = add(CollageBlockType::SOLID_BLOCK, bb, PaletteToken::Primary, 0, 0, 0.6f, 1.0f);
		big.revealDir = RevealDir::LeftToRight;

		const float railC = static_cast<float>(heroC + heroCols);
		auto & rail = add(CollageBlockType::SOLID_BLOCK, R(railC, static_cast<float>(heroR), 1.0f, static_cast<float>(heroRows)), PaletteToken::Secondary, 0, 0, 1.6f, 0.9f);
		rail.revealDir = RevealDir::TopToBottom;
		auto & vt = add(CollageBlockType::TICK_STRIP, R(railC + 1.0f, static_cast<float>(heroR), 0.6f, static_cast<float>(heroRows)), PaletteToken::Dark, 5, 0, 2.4f, 1.6f);
		vt.vertical = true;

		const float rx = railC + 1.8f;
		const float rw = 23.0f - rx;

		auto & plate = add(CollageBlockType::INDEX_PLATE, R(rx, 0.35f, 3.2f, 0.62f), PaletteToken::Dark, 6, 0, 2.6f, 0.4f);
		plate.text = "PLATE " + ofToString(10 + static_cast<int>(out.seed % 90));
		add(CollageBlockType::DIVIDER_BAR, R(rx, 1.3f, rw, 0.1f), PaletteToken::Dark, 5, 0, 2.8f, 0.9f).revealDir = RevealDir::LeftToRight;
		add(CollageBlockType::GRID_BLOCK, R(rx - 0.3f, 1.5f, rw + 0.7f, 10.0f), PaletteToken::Dark, 1, 0, 1.2f, 1.6f).revealDir = RevealDir::LeftToRight;

		// Hero.
		int hi = addFragment(CollageRole::Hero, hero);
		out.fragments[hi].revealDir = RevealDir::TopToBottom;
		timing(hi, 0, 2.2f, 1.6f);
		overlay(hi, ofRectangle(hero.x, hero.y + hero.height * 0.58f, hero.width, hero.height * 0.42f), PaletteToken::Secondary, 0.38f);
		out.fragments[hi].stage = 0;

		// Strip crop (usually rotated).
		int cols = std::min(static_cast<int>(rw) - 1, irnd(7, 9)), rows = irnd(2, 3);
		float sc = rx + irnd(0, std::max(0, static_cast<int>(rw) - cols - 1));
		int si = addFragment(CollageRole::Strip, R(sc, 1.7f, static_cast<float>(cols), static_cast<float>(rows)), true);
		timing(si, 0, 3.4f, 1.0f);
		auto & tr = add(CollageBlockType::TICK_STRIP, R(rx, 1.7f + rows + 0.2f, rw, 0.5f), PaletteToken::Dark, 5, 0, 4.0f, 1.8f);
		tr.vertical = false;

		// Displaced samples.
		const ofRectangle region(rx * g, y0 + 5.0f * g, rw * g, 6.6f * g);
		int s1 = addSample(region, static_cast<float>(irnd(2, 3)), true, 0, 5.4f);
		int s2 = addSample(region, static_cast<float>(irnd(2, 3)), true, 0, 7.0f);
		(void)s1;
		(void)s2;
		// Density-stage extras.
		addSample(region, 2.0f, false, 1, 1.0f);
		add(CollageBlockType::TRANSLUCENT_BLOCK, R(rx + rw * 0.5f, 5.0f, rw * 0.5f, 3.0f), PaletteToken::Secondary, 3, 1, 2.0f, 1.0f).alpha = 0.3f;

		// Data + technical marks.
		auto & d1 = add(CollageBlockType::DATA_STRIP, R(rx, 12.1f, 9.0f, 0.3f), PaletteToken::Dark, 5, 0, 8.0f, 0.5f);
		d1.value = rnd(0.25f, 0.6f);
		auto & d2 = add(CollageBlockType::DATA_STRIP, R(rx, 12.6f, 6.0f, 0.2f), PaletteToken::Secondary, 5, 1, 3.0f, 0.5f);
		d2.segmented = false;
		d2.value = rnd(0.3f, 0.8f);
		add(CollageBlockType::CORNER_TAB, R(heroC - 0.6f, heroR + heroRows - 0.6f, 1.2f, 1.2f), PaletteToken::Dark, 5, 0, 4.4f, 0.2f);
		mark(1.0f * g, y0 + 1.0f * g, 0.35f * g, hud::RegistrationMarkStyle::Kind::Cross, 0, 1.8f);
		mark(23.0f * g, y0 + 12.2f * g, 0.4f * g, hud::RegistrationMarkStyle::Kind::Circle, 1, 0.5f, true);
	}

	// ── Template C — scientific plate ──────────────────────────────────────
	void templatePlate() {
		out.templateName = "SCIENTIFIC PLATE";
		const float cw = 9.5f, rh = 5.25f;
		ofRectangle cell[4];
		for (int k = 0; k < 4; k++)
			cell[k] = R(2.0f + (k % 2) * (cw + 1.0f), 1.0f + (k / 2) * (rh + 1.0f), cw, rh);
		const int empty = irnd(0, 3);
		int filled[3], n = 0;
		for (int k = 0; k < 4; k++)
			if (k != empty) filled[n++] = k;
		std::shuffle(filled, filled + 3, rng);

		add(CollageBlockType::PARTIAL_FRAME, R(1.5f, 0.5f, 21.0f, 12.5f), PaletteToken::Dark, 5, 0, 0.5f, 1.2f).strokeWidth = 2.0f;
		auto & plate = add(CollageBlockType::INDEX_PLATE, R(2.0f, 0.0f, 3.4f, 0.45f), PaletteToken::Dark, 6, 0, 1.0f, 0.4f);
		plate.text = "PLATE " + ofToString(10 + static_cast<int>(out.seed % 90));

		// Empty cell: grid + registration target (+ density-stage sample inset).
		const ofRectangle ec = cell[empty];
		add(CollageBlockType::GRID_BLOCK, ec, PaletteToken::Dark, 1, 0, 1.4f, 1.4f).revealDir = RevealDir::LeftToRight;
		add(CollageBlockType::PARTIAL_FRAME, ec, PaletteToken::Dark, 5, 0, 1.8f, 0.8f).strokeWidth = 1.5f;
		mark(ec.getCenter().x, ec.getCenter().y, 0.5f * g, hud::RegistrationMarkStyle::Kind::TargetDot, 0, 2.2f, true);

		// Filled cell 0 — landscape hero with offset frame.
		{
			const ofRectangle & c = cell[filled[0]];
			float w = rnd(7.5f, 9.5f), h = rnd(4.2f, 5.25f);
			float ox = rnd(0.0f, cw - w), oy = rnd(0.0f, rh - h);
			int fi = addFragment(CollageRole::Hero, ofRectangle(c.x + ox * g, c.y + oy * g, w * g, h * g));
			out.fragments[fi].revealDir = RevealDir::LeftToRight;
			timing(fi, 0, 2.6f, 1.5f);
			const auto & d = out.fragments[fi].destinationRect;
			overlay(fi, ofRectangle(d.x + d.width * 0.5f, d.y, d.width * 0.5f, d.height), PaletteToken::Secondary, 0.3f);
			out.fragments[fi].stage = 0;
		}
		// Filled cell 1 — structural color plate behind a rotated secondary crop.
		{
			const ofRectangle & c = cell[filled[1]];
			auto & blk = add(CollageBlockType::SOLID_BLOCK, c, PaletteToken::Primary, 0, 0, 1.0f, 0.9f);
			blk.revealDir = RevealDir::TopToBottom;
			float w = rnd(4.5f, 6.0f), h = rnd(4.0f, 4.8f);
			float ox = rnd(0.4f, cw - w - 0.4f), oy = rnd(0.2f, std::max(0.25f, rh - h - 0.2f));
			int fi = addFragment(CollageRole::Secondary, ofRectangle(c.x + ox * g, c.y + oy * g, w * g, h * g), true);
			timing(fi, 0, 4.4f, 1.0f);
		}
		// Filled cell 2 — circular detail on a disc backplate.
		{
			const ofRectangle & c = cell[filled[2]];
			float d = rnd(4.2f, 5.0f);
			float cx = c.getCenter().x + rnd(-0.6f, 0.6f) * g, cy = c.getCenter().y + rnd(-0.2f, 0.2f) * g;
			auto & back = add(CollageBlockType::CIRCLE_SAMPLE_BACKPLATE, ofRectangle(cx + 0.3f * g - (d * 0.5f + 0.35f) * g, cy + 0.3f * g - (d * 0.5f + 0.35f) * g, (d + 0.7f) * g, (d + 0.7f) * g), PaletteToken::Secondary, 0, 0, 3.2f, 0.7f);
			back.filled = true;
			int fi = addFragment(CollageRole::Masked, ofRectangle(cx - d * 0.5f * g, cy - d * 0.5f * g, d * g, d * g), false, true);
			timing(fi, 0, 5.6f, 0.9f);
		}

		// Measurement strips crossing multiple cells.
		add(CollageBlockType::TICK_STRIP, R(2.0f, 1.0f + rh + 0.25f, 20.0f, 0.5f), PaletteToken::Dark, 5, 0, 6.2f, 2.4f);
		auto & vt = add(CollageBlockType::TICK_STRIP, R(1.0f, 1.0f, 0.5f, 11.5f), PaletteToken::Dark, 5, 0, 6.6f, 2.0f);
		vt.vertical = true;
		auto & d1 = add(CollageBlockType::DATA_STRIP, R(2.0f, 12.72f, 6.0f, 0.22f), PaletteToken::Dark, 5, 0, 7.4f, 0.5f);
		d1.value = rnd(0.2f, 0.6f);

		// Density: sample inset over the empty grid cell, more marks.
		ofRectangle inset(ec.x + 0.5f * g, ec.y + 0.5f * g, ec.width - g, ec.height - g);
		ofRectangle spot;
		if (findFreeRect(2.4f * g, 2.4f * g, inset, spot)) {
			int fi = addFragment(CollageRole::Detail, spot);
			timing(fi, 1, 0.8f, 0.8f);
		}
		auto & d2 = add(CollageBlockType::DATA_STRIP, R(9.0f, 12.72f, 4.0f, 0.22f), PaletteToken::Secondary, 5, 1, 1.6f, 0.5f);
		d2.segmented = false;
		d2.value = rnd(0.3f, 0.9f);
		mark(1.25f * g, y0 + (1.0f + rh + 0.5f) * g, 0.28f * g, hud::RegistrationMarkStyle::Kind::Cross, 1, 0.4f);
		mark(22.0f * g, y0 + 12.7f * g, 0.3f * g, hud::RegistrationMarkStyle::Kind::Square, 1, 2.4f);
	}

	// ── Template E — broken poster ─────────────────────────────────────────
	void templatePoster() {
		out.templateName = "BROKEN POSTER";
		const int heroCols = irnd(13, 15), heroRows = irnd(12, 14);
		const int heroC = irnd(-2, 0), heroR = irnd(-1, 0);
		ofRectangle hero = R(static_cast<float>(heroC), static_cast<float>(heroR), static_cast<float>(heroCols), static_cast<float>(heroRows));

		const float heroRight = static_cast<float>(heroC + heroCols);
		const float accC = heroRight - irnd(2, 3);
		const float accR = static_cast<float>(irnd(1, 4));
		auto & acc = add(CollageBlockType::SOLID_BLOCK, ofRectangle(accC * g, y0 + accR * g, (24.5f - accC) * g, irnd(5, 7) * g), PaletteToken::Primary, 0, 0, 0.5f, 0.8f);
		acc.revealDir = RevealDir::RightToLeft;

		int hi = addFragment(CollageRole::Hero, hero);
		out.fragments[hi].allowOverlap = true; // deliberately bleeds off canvas
		out.fragments[hi].revealDir = RevealDir::LeftToRight;
		timing(hi, 0, 1.4f, 1.8f);
		overlay(hi, ofRectangle(hero.x, hero.y, hero.width * 0.3f, hero.height), PaletteToken::Primary, 0.28f);

		const float secW = 4.6f, secH = 6.4f;
		float secC = accC + irnd(1, 2);
		float secR = accR + irnd(1, 2);
		int si = addFragment(CollageRole::Secondary, R(secC, secR, secW, secH), true);
		auto & sf = out.fragments[si];
		sf.allowOverlap = true;
		sf.hasFrame = true;
		sf.frameWidth = 8.0f;
		sf.paletteIndex = static_cast<int>(chance(0.5f) ? PaletteToken::Light : PaletteToken::Dark);
		sf.swapPeriod = rnd(14.0f, 22.0f);
		timing(si, 0, 4.0f, 1.1f);

		// Sparse black data plate.
		float px = secC + secW + 0.4f;
		float pw = std::max(3.0f, std::min(5.6f, 23.6f - px));
		ofRectangle plateR = R(std::min(px, 23.6f - pw), 10.3f, pw, 2.3f);
		add(CollageBlockType::SOLID_BLOCK, plateR, PaletteToken::Dark, 3, 0, 5.2f, 0.5f).revealDir = RevealDir::BottomToTop;
		auto & pb = add(CollageBlockType::DATA_STRIP, ofRectangle(plateR.x + 0.4f * g, plateR.y + 0.5f * g, plateR.width - 0.8f * g, 0.3f * g), PaletteToken::Light, 6, 0, 6.0f, 0.5f);
		pb.value = rnd(0.3f, 0.8f);
		auto & pt = add(CollageBlockType::INDEX_PLATE, ofRectangle(plateR.x, plateR.y + plateR.height - 0.7f * g, plateR.width, 0.5f * g), PaletteToken::Dark, 6, 0, 6.2f, 0.4f);
		pt.text = "FIG. " + ofToString(10 + static_cast<int>(out.seed % 90));

		// Thin circle outline, partially off canvas.
		auto & ring = add(CollageBlockType::CIRCLE_SAMPLE_BACKPLATE, ofRectangle(0, 0, 0, 0), PaletteToken::Dark, 5, 0, 3.0f, 1.0f);
		float rcx = W * rnd(0.78f, 0.92f), rcy = H * rnd(0.08f, 0.3f), rr = 3.5f * g;
		ring.rect = ofRectangle(rcx - rr, rcy - rr, rr * 2, rr * 2);
		ring.filled = false;
		ring.strokeWidth = 1.5f;
		ring.alpha = 0.6f;

		auto & vt = add(CollageBlockType::TICK_STRIP, R(23.2f, 0.5f, 0.5f, 12.0f), PaletteToken::Dark, 5, 0, 6.8f, 2.0f);
		vt.vertical = true;
		auto & ht = add(CollageBlockType::TICK_STRIP, R(1.0f, 12.5f, 12.0f, 0.5f), PaletteToken::Dark, 5, 0, 7.2f, 2.0f);
		(void)ht;
		add(CollageBlockType::CORNER_TAB, R(accC - 0.4f, accR - 0.4f, 0.9f, 0.9f), PaletteToken::Dark, 5, 0, 4.8f, 0.2f);
		mark(accC * g, y0 + accR * g, 0.4f * g, hud::RegistrationMarkStyle::Kind::Cross, 0, 3.6f);
		mark(1.0f * g, 12.0f * g + y0, 0.3f * g, hud::RegistrationMarkStyle::Kind::SplitCircle, 1, 0.6f, true);

		// Density: detail sample + translucent wash.
		addSample(ofRectangle(secC * g, y0, 12.0f * g, 4.0f * g), 2.4f, true, 1, 0.8f);
		add(CollageBlockType::TRANSLUCENT_BLOCK, R(accC, accR + 0.5f, 1.8f, 7.5f), PaletteToken::Secondary, 3, 1, 1.8f, 1.0f).alpha = 0.35f;
	}

	// ── Template D — asymmetric cross ──────────────────────────────────────
	// Perpendicular crop strips crossing at an offset point, balanced by an
	// accent counterweight block on the opposite side.
	void templateCross() {
		out.templateName = "ASYMMETRIC CROSS";
		const int hw = irnd(14, 17), hh = irnd(2, 3);
		const int hc = irnd(1, 23 - hw), hr = irnd(4, 6);
		const int vw = irnd(2, 3), vh = irnd(9, 11);
		const int vc = hc + irnd(3, hw - 4);
		const int vr = irnd(0, std::min(hr, GRID_ROWS - vh));

		// Counterweight on the side with more free canvas.
		const bool blockRight = (hc + hw * 0.5f) < 12.0f;
		const float bc = blockRight ? 24.0f - irnd(6, 8) : -0.5f;
		const float brow = (hr < 6) ? static_cast<float>(irnd(7, 8)) : static_cast<float>(irnd(0, 1));
		auto & big = add(CollageBlockType::SOLID_BLOCK, R(bc, brow, 7.5f, static_cast<float>(irnd(5, 6))), PaletteToken::Primary, 0, 0, 0.5f, 1.0f);
		big.revealDir = blockRight ? RevealDir::RightToLeft : RevealDir::LeftToRight;
		add(CollageBlockType::GRID_BLOCK, R(1.0f, 0.6f, 22.0f, 11.8f), PaletteToken::Dark, 1, 0, 1.0f, 1.8f).revealDir = RevealDir::LeftToRight;

		int hi = addFragment(CollageRole::Strip, R(static_cast<float>(hc), static_cast<float>(hr), static_cast<float>(hw), static_cast<float>(hh)));
		out.fragments[hi].revealDir = RevealDir::LeftToRight;
		out.fragments[hi].role = CollageRole::Hero; // the long strip is the anchor image
		out.fragments[hi].showLabel = false;
		timing(hi, 0, 1.6f, 1.5f);
		int vi = addFragment(CollageRole::Strip, R(static_cast<float>(vc), static_cast<float>(vr), static_cast<float>(vw), static_cast<float>(vh)), true);
		out.fragments[vi].allowOverlap = true; // crosses the horizontal strip
		out.fragments[vi].zLayer += 40;
		out.fragments[vi].hasFrame = true;
		out.fragments[vi].frameWidth = 5.0f;
		out.fragments[vi].revealDir = RevealDir::TopToBottom;
		timing(vi, 0, 3.0f, 1.2f);

		// Measurement at the crossing.
		const float cx = (vc + vw * 0.5f) * g, cy = y0 + (hr + hh * 0.5f) * g;
		mark(cx, cy, 0.5f * g, hud::RegistrationMarkStyle::Kind::Circle, 0, 4.4f, true);
		auto & ht = add(CollageBlockType::TICK_STRIP, R(static_cast<float>(hc), hr + hh + 0.2f, static_cast<float>(hw), 0.5f), PaletteToken::Dark, 5, 0, 4.0f, 1.8f);
		(void)ht;
		auto & vt = add(CollageBlockType::TICK_STRIP, R(vc + vw + 0.2f, static_cast<float>(vr), 0.5f, static_cast<float>(vh)), PaletteToken::Dark, 5, 0, 4.6f, 1.8f);
		vt.vertical = true;
		auto & plate = add(CollageBlockType::INDEX_PLATE, R(static_cast<float>(hc), hr - 0.75f, 3.0f, 0.55f), PaletteToken::Dark, 6, 0, 2.6f, 0.4f);
		plate.text = "SECT. " + ofToString(10 + static_cast<int>(out.seed % 90));
		auto & d1 = add(CollageBlockType::DATA_STRIP, R(static_cast<float>(hc), std::min(12.4f, hr + hh + 1.0f), 8.0f, 0.25f), PaletteToken::Dark, 5, 0, 6.0f, 0.5f);
		d1.value = rnd(0.2f, 0.6f);
		overlay(hi, ofRectangle(out.fragments[hi].destinationRect.x, out.fragments[hi].destinationRect.y, out.fragments[hi].destinationRect.width * 0.35f, out.fragments[hi].destinationRect.height), PaletteToken::Secondary, 0.3f);

		// Samples in the free quadrants.
		const ofRectangle freeA(1.0f * g, y0, std::max(2.0f, static_cast<float>(vc) - 1.0f) * g, std::max(2.0f, hr - 0.6f) * g);
		const ofRectangle freeB((vc + vw + 1.0f) * g, y0 + (hr + hh + 1.2f) * g, (23.0f - vc - vw - 1.0f) * g, (11.5f - hr - hh) * g);
		addSample(freeA, 2.4f, true, 0, 5.4f);
		addSample(freeB, 2.6f, false, 0, 6.8f);
		addSample(freeB, 2.0f, true, 1, 1.0f);
		add(CollageBlockType::TRANSLUCENT_BLOCK, R(static_cast<float>(vc) - 1.0f, hr + 0.5f, vw + 2.0f, 3.0f), PaletteToken::Secondary, 3, 1, 1.6f, 1.0f).alpha = 0.3f;
		mark(1.0f * g, y0 + 12.4f * g, 0.3f * g, hud::RegistrationMarkStyle::Kind::Cross, 1, 0.4f);
	}

	// ── Template E — stacked registration ──────────────────────────────────
	// Aligned vertical samples hanging from one registration rule, ruler system
	// above, color mass behind one column.
	void templateStack() {
		out.templateName = "STACKED REGISTRATION";
		const int n = irnd(3, 4), colW = irnd(3, 4);
		const int total = n * colW + (n - 1);
		const int c0 = irnd(1, std::max(1, 23 - total));
		const float rt = static_cast<float>(irnd(1, 2)); // registration rule row
		const bool hang = chance(0.7f); // hang from the rule, else stand on the baseline
		int tallest = 0, tallH = 0;
		int rows[4];
		for (int k = 0; k < n; k++) {
			rows[k] = irnd(4, 10);
			if (rows[k] > tallH) {
				tallH = rows[k];
				tallest = k;
			}
		}
		const float baseline = 12.0f;

		add(CollageBlockType::DIVIDER_BAR, R(static_cast<float>(c0) - 0.6f, rt - 0.35f, total + 1.2f, 0.1f), PaletteToken::Dark, 5, 0, 0.8f, 1.2f).revealDir = RevealDir::LeftToRight;
		add(CollageBlockType::TICK_STRIP, R(static_cast<float>(c0) - 0.6f, rt - 0.9f, total + 1.2f, 0.5f), PaletteToken::Dark, 5, 0, 1.2f, 2.0f);
		auto & plate = add(CollageBlockType::INDEX_PLATE, R(static_cast<float>(c0) - 0.6f, rt - 1.0f - 0.55f, 3.4f, 0.5f), PaletteToken::Dark, 6, 0, 1.6f, 0.4f);
		plate.text = "STACK " + ofToString(10 + static_cast<int>(out.seed % 90));

		// Color mass behind a middle column, offset so it reads as a misregistration.
		const int mk = n / 2;
		const float mcx = c0 + mk * (colW + 1) - 1.0f;
		auto & mass = add(CollageBlockType::SOLID_BLOCK, R(mcx, rt + 1.0f, colW + 2.0f, 9.0f), PaletteToken::Primary, 0, 0, 0.6f, 1.0f);
		mass.revealDir = RevealDir::TopToBottom;

		for (int k = 0; k < n; k++) {
			const float col = static_cast<float>(c0 + k * (colW + 1));
			const float h = static_cast<float>(rows[k]);
			const float row = hang ? rt : baseline - h;
			const bool isHero = (k == tallest);
			int fi = addFragment(isHero ? CollageRole::Hero : (rows[k] < 6 ? CollageRole::Detail : CollageRole::Secondary),
				R(col, row, static_cast<float>(colW), h), !isHero && chance(0.5f));
			out.fragments[fi].revealDir = hang ? RevealDir::TopToBottom : RevealDir::BottomToTop;
			timing(fi, 0, 1.6f + 0.9f * k, 1.1f);
			mark(col * g + colW * 0.5f * g, y0 + (rt - 0.35f) * g, 0.22f * g, hud::RegistrationMarkStyle::Kind::Cross, 0, 2.6f + 0.5f * k);
			auto & bar = add(CollageBlockType::DATA_STRIP, R(col, baseline + 0.45f, static_cast<float>(colW), 0.22f), k % 2 ? PaletteToken::Secondary : PaletteToken::Dark, 5, k % 2, 3.0f + 0.4f * k, 0.5f);
			bar.value = rnd(0.2f, 0.9f);
			bar.segmented = (k % 2 == 0);
		}
		auto & vt = add(CollageBlockType::TICK_STRIP, R(static_cast<float>(c0) - 1.2f, rt, 0.5f, 10.5f), PaletteToken::Dark, 5, 0, 4.4f, 2.0f);
		vt.vertical = true;
		add(CollageBlockType::GRID_BLOCK, R(static_cast<float>(c0 + total) + 0.6f, rt, std::min(6.0f, 22.5f - (c0 + total)), 8.0f), PaletteToken::Dark, 1, 0, 3.0f, 1.8f).revealDir = RevealDir::LeftToRight;
		mark(23.0f * g, y0 + 12.2f * g, 0.35f * g, hud::RegistrationMarkStyle::Kind::TargetDot, 1, 0.5f, true);
		const ofRectangle gridSide((c0 + total + 1.0f) * g, y0 + (rt + 0.8f) * g, std::max(2.5f, 22.0f - (c0 + total)) * g, 6.0f * g);
		addSample(gridSide, 2.4f, true, 1, 1.0f);
		add(CollageBlockType::TRANSLUCENT_BLOCK, R(static_cast<float>(c0), rt + 3.0f, static_cast<float>(total), 2.0f), PaletteToken::Secondary, 3, 1, 2.0f, 1.0f).alpha = 0.28f;
	}

	bool valid() const {
		const ofRectangle canvas(-1, -1, W + 2, H + 2);
		for (size_t i = 0; i < out.fragments.size(); i++) {
			const auto & a = out.fragments[i];
			if (!a.allowOverlap && !(canvas.inside(a.destinationRect.getMin()) && canvas.inside(a.destinationRect.getMax())))
				return false;
			for (size_t j = i + 1; j < out.fragments.size(); j++) {
				const auto & b = out.fragments[j];
				if (a.allowOverlap || b.allowOverlap) continue;
				if (overlapFraction(a.destinationRect, b.destinationRect) > 0.08f) return false;
			}
		}
		return out.fragments.size() >= 3;
	}

	void finalize() {
		std::stable_sort(out.fragments.begin(), out.fragments.end(),
			[](const CollageFragment & a, const CollageFragment & b) { return a.zLayer < b.zLayer; });
		int n = 0;
		for (auto & f : out.fragments) {
			if (f.role == CollageRole::Hero) continue;
			char tag[8];
			snprintf(tag, sizeof(tag), "S-%02d", ++n);
			f.labelTag = tag;
		}
		// Printed-plate residue: the largest back block and one mark outlive the imagery.
		CollageGraphic * bestBlock = nullptr;
		CollageGraphic * firstMark = nullptr;
		for (auto & gr : out.graphics) {
			if (gr.type == CollageBlockType::SOLID_BLOCK && gr.pass == 0 && gr.color == PaletteToken::Primary) {
				if (!bestBlock || gr.rect.width * gr.rect.height > bestBlock->rect.width * bestBlock->rect.height) bestBlock = &gr;
			}
			if (gr.type == CollageBlockType::REGISTRATION_MARK && !firstMark) firstMark = &gr;
		}
		if (bestBlock) bestBlock->linger = true;
		if (firstMark) firstMark->linger = true;
	}
};

} // namespace


// ════════════════════════════════════════════════════════════════════════════
// BECollage — persistent registry + deterministic evolution director
// ════════════════════════════════════════════════════════════════════════════

namespace {
constexpr float PALETTE_FADE = 1.5f;
constexpr float MAX_DT = 0.25f; // bound a suspended frame so it cannot cause an event storm

inline uint64_t hmix(uint64_t h, uint64_t v) {
	h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
	return h;
}
inline int q8(float v) { return static_cast<int>(std::lround(v * 8.0f)); }
inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float areaOf(const ofRectangle & r) { return std::max(0.0f, r.width) * std::max(0.0f, r.height); }
} // namespace

CollageLayout BECollage::generateLayout(uint32_t seed, int canvasW, int canvasH, int forcedTemplate) {
	uint32_t h = seed * 2246822519u;
	h ^= h >> 13;
	const int tmpl = (forcedTemplate >= 0) ? forcedTemplate % TEMPLATE_COUNT : static_cast<int>(h % TEMPLATE_COUNT);

	Builder last(seed, 0, canvasW, canvasH);
	for (int attempt = 0; attempt < 14; attempt++) {
		Builder b(seed, attempt, canvasW, canvasH);
		b.out.templateIndex = tmpl;
		switch (tmpl) {
		case 0: b.templateSpine(); break;
		case 1: b.templatePlate(); break;
		case 2: b.templatePoster(); break;
		case 3: b.templateCross(); break;
		default: b.templateStack(); break;
		}
		b.finalize();
		if (b.valid() || attempt == 13) return b.out;
	}
	return last.out;
}

// ── setup / lifecycle ───────────────────────────────────────────────────────

void BECollage::setup(int canvasW, int canvasH, const std::string & fontPath, int fontSize) {
	W_ = canvasW;
	H_ = canvasH;
	g_ = static_cast<float>(W_) / GRID_COLS;
	y0_ = (static_cast<float>(H_) - GRID_ROWS * g_) * 0.5f;
	fontLoaded_ = font_.load(fontPath, fontSize, true, true);
	if (!fontLoaded_) ofLogWarning("BECollage") << "font not loaded (" << fontPath << "); using bitmap font";
	elements_.reserve(160);
	queue_.reserve(64);
	drawOrder_.reserve(160);
}

void BECollage::setTuning(const CollageTuning & t) {
	tuning_ = t;
	if (hasLayout_) scheduleTimers();
}

void BECollage::scheduleTimers() {
	accMicro_ = accLocal_ = swellAcc_ = recomposeAcc_ = anchorAcc_ = 0.0f;
	nextMicro_ = rnd(tuning_.micro);
	nextLocal_ = rnd(tuning_.local);
	nextSwell_ = rnd(tuning_.swell);
	nextRecompose_ = rnd(tuning_.recompose);
	nextAnchorRotate_ = rnd(tuning_.anchorRotate);
	activity_ = CollageActivity::Active;
	level_ = levelTarget_ = tuning_.activeLevel;
	activityLeft_ = rnd(tuning_.activeDwell);
}

void BECollage::beginLayout(uint32_t seed, bool force) {
	if (locked_ && !force) return;
	const int forcedTmpl = forcedTemplate_;
	forcedTemplate_ = -1;
	lastSeed_ = seed;

	if (!hasLayout_ || force) {
		elements_.clear();
		queue_.clear();
		log_.clear();
		eventCount_ = 0;
		for (int i = 0; i < COLLAGE_EVENT_COUNT; i++) {
			perEventCount_[i] = 0;
			lastEventTime_[i] = -100.0f;
		}
		swellCount_ = recomposeCount_ = 0;
		nextId_ = 1;
		nextGroup_ = 1;
		zCounter_ = 0;
		labelCounter_ = 0;
		anchorId_ = 0;
		touched_ = 0;
		time_ = 0.0f;
		dissolving_ = false;
		dissolveT0_ = -1.0f;
		lastRegion_ = -1;
		rng_.seed(seed * 2654435761u ^ 0xC011A6E5u);

		layout_ = generateLayout(seed, W_, H_, forcedTmpl);
		activePal_ = fromPal_ = layout_.palette;
		paletteMix_ = 1.0f;
		hasLayout_ = true;
		scheduleTimers();
		spawnFromLayout(layout_, 0.0f, 9.0f, nullptr, 1.0f);
		pickAnchors();
		ofLogNotice("BECollage") << "layout seed=" << seed << " template=" << layout_.templateName
								 << " palette=" << layout_.palette.name << " elements=" << elements_.size()
								 << " profile=" << tuning_.name;
		return;
	}

	int tmpl = forcedTmpl;
	if (tmpl < 0) {
		tmpl = irnd(0, TEMPLATE_COUNT - 2);
		if (tmpl >= layout_.templateIndex) tmpl++;
	}
	recompose(seed, tmpl, false);
}

bool BECollage::swapTemplate(int index) {
	if (!hasLayout_) return false;
	recompose(layout_.seed + 7919u * static_cast<uint32_t>(index + 1), index % TEMPLATE_COUNT, false);
	return true;
}

void BECollage::applyPaletteTarget(const hud::HudCollagePalette & p) {
	fromPal_ = activePal_;
	layout_.palette = p;
	paletteMix_ = 0.0f;
}

void BECollage::setPhase(CollagePhase phase) {
	if (locked_) return;
	if (phase == CollagePhase::Dissolve) {
		if (!dissolving_) {
			dissolving_ = true;
			dissolveT0_ = time_;
			startDissolveClear();
		}
	} else {
		dissolving_ = false;
	}
}

bool BECollage::dissolveFinished() const {
	if (locked_ || !hasLayout_) return true;
	return dissolving_ && (time_ - dissolveT0_) >= DISSOLVE_TOTAL_SECONDS;
}

void BECollage::debugSetTimeline(float seconds, float densityStart) {
	directorEnabled_ = false;
	locked_ = false;
	if (densityStart < 0.0f) {
		elements_.erase(std::remove_if(elements_.begin(), elements_.end(), [](const CollageElement & e) { return e.stage > 0; }), elements_.end());
	}
	const float step = 0.1f;
	for (float t = 0.0f; t < seconds; t += step)
		update(step);
	directorEnabled_ = true;
	locked_ = true;
}

// ── registry ────────────────────────────────────────────────────────────────

CollageElement & BECollage::addElement(CollageElement e) {
	e.id = nextId_++;
	e.state = ElemState::Pending;
	e.stateT = 0.0f;
	if (e.video) {
		e.frame.snap(e.hasFrame ? 1.0f : 0.0f);
		e.overlay.snap(e.hasOverlay ? 1.0f : 0.0f);
		e.cover.snap(1.0f);
	} else {
		e.alpha.snap(1.0f);
		e.ext.snap(1.0f);
	}
	e.rectFrom = e.rectTo = e.rect;
	e.moveT = 1.0f;
	elements_.push_back(std::move(e));
	return elements_.back();
}

CollageElement * BECollage::find(uint32_t id) {
	for (auto & e : elements_)
		if (e.id == id) return &e;
	return nullptr;
}
const CollageElement * BECollage::find(uint32_t id) const {
	for (const auto & e : elements_)
		if (e.id == id) return &e;
	return nullptr;
}

void BECollage::startExit(CollageElement & e, float dur) {
	if (e.state == ElemState::Exiting) return;
	e.state = ElemState::Exiting;
	e.stateT = 0.0f;
	e.exitDur = dur > 0.0f ? dur : rnd(tuning_.exitDur);
	e.exitAt = -1.0f;
	e.anchor = false;
}

BECollage::Vis BECollage::visOf(const CollageElement & e) const {
	Vis v;
	switch (e.state) {
	case ElemState::Pending: v.reveal = 0.0f; break;
	case ElemState::Entering: v.reveal = easeOutCubic(e.stateT / std::max(e.enterDur, 0.01f)); break;
	case ElemState::Active: v.reveal = 1.0f; break;
	case ElemState::Exiting:
		v.reveal = 1.0f;
		v.fade = 1.0f - clamp01(e.stateT / std::max(e.exitDur, 0.01f));
		break;
	}
	return v;
}

void BECollage::updateElement(CollageElement & e, float dt, float cover) {
	switch (e.state) {
	case ElemState::Pending:
		if (time_ >= e.spawnAt) {
			e.state = ElemState::Entering;
			e.stateT = 0.0f;
			e.born = time_;
		}
		break;
	case ElemState::Entering:
		e.stateT += dt;
		if (e.stateT >= e.enterDur) {
			e.state = ElemState::Active;
			e.stateT = 0.0f;
		}
		break;
	case ElemState::Active:
		if (e.exitAt >= 0.0f && time_ >= e.exitAt) startExit(e);
		break;
	case ElemState::Exiting:
		e.stateT += dt;
		break;
	}
	if (e.moveT < 1.0f) {
		e.moveT = std::min(1.0f, e.moveT + dt / std::max(e.moveDur, 0.05f));
		const float t = e.moveT;
		const float u = t < 0.5f ? 4.0f * t * t * t : 1.0f - 4.0f * (1.0f - t) * (1.0f - t) * (1.0f - t);
		e.rect.x = e.rectFrom.x + (e.rectTo.x - e.rectFrom.x) * u;
		e.rect.y = e.rectFrom.y + (e.rectTo.y - e.rectFrom.y) * u;
		e.rect.width = e.rectFrom.width + (e.rectTo.width - e.rectFrom.width) * u;
		e.rect.height = e.rectFrom.height + (e.rectTo.height - e.rectFrom.height) * u;
		if (e.moveT >= 1.0f) e.rect = e.rectTo;
	}
	if (e.cropT < 1.0f) e.cropT = std::min(1.0f, e.cropT + dt / std::max(e.cropDur, 0.05f));
	e.frame.update(dt);
	e.overlay.update(dt);
	e.cover.update(dt);
	e.alpha.update(dt);
	e.ext.update(dt);
	e.value.update(dt);
	if (e.bind == 1) { // honest binding: this bar follows the live video coverage
		const float target = clamp01(cover / 0.5f);
		e.value.snap(e.value.cur + (target - e.value.cur) * std::min(1.0f, dt * 1.5f));
	}
	e.flash = std::max(0.0f, e.flash - dt / 1.4f);
}

int BECollage::fragmentCount() const {
	int n = 0;
	for (const auto & e : elements_)
		if (e.video && e.state != ElemState::Exiting && e.exitAt < 0.0f) n++;
	return n;
}
int BECollage::graphicCount() const {
	int n = 0;
	for (const auto & e : elements_)
		if (!e.video && e.state != ElemState::Exiting && e.exitAt < 0.0f) n++;
	return n;
}
int BECollage::inTransitionCount() const {
	int n = 0;
	for (const auto & e : elements_)
		if (e.state != ElemState::Pending && e.inTransition()) n++;
	return n;
}

float BECollage::videoCover() const {
	float a = 0.0f;
	for (const auto & e : elements_)
		if (e.video && e.state != ElemState::Exiting && e.exitAt < 0.0f) a += areaOf(e.finalRect());
	return a / (static_cast<float>(W_) * H_);
}

float BECollage::sideBalance() const {
	float l = 0.0f, r = 0.0f;
	for (const auto & e : elements_) {
		if (!e.video || e.state == ElemState::Exiting || e.exitAt >= 0.0f) continue;
		(e.finalRect().getCenter().x < W_ * 0.5f ? l : r) += areaOf(e.finalRect());
	}
	return (r - l) / std::max(1.0f, l + r);
}

int BECollage::regionOf(const ofRectangle & r) const {
	const glm::vec3 c = r.getCenter();
	return (c.x < W_ * 0.5f ? 0 : 1) + (c.y < H_ * 0.5f ? 0 : 2);
}

bool BECollage::placementFree(const ofRectangle & r, uint32_t ignoreA, uint32_t ignoreB, float maxOverlap) const {
	for (const auto & e : elements_) {
		if (!e.video || e.state == ElemState::Exiting || e.exitAt >= 0.0f) continue;
		if (e.id == ignoreA || e.id == ignoreB) continue;
		ofRectangle x = e.finalRect();
		x.x -= g_ * 0.3f;
		x.y -= g_ * 0.3f;
		x.width += g_ * 0.6f;
		x.height += g_ * 0.6f;
		if (overlapFraction(x, r) > maxOverlap) return false;
	}
	return true;
}

bool BECollage::findPlacement(float w, float h, ofRectangle & out, uint32_t ia, uint32_t ib) {
	if (w > W_ - g_ || h > GRID_ROWS * g_) return false;
	const float snap = g_ * 0.5f;
	const float bal = sideBalance();
	float bestScore = -1e9f;
	bool found = false;
	for (int i = 0; i < 40; i++) {
		const float x = std::round(rnd(g_ * 0.5f, W_ - w - g_ * 0.5f) / snap) * snap;
		const float y = y0_ + std::round(rnd(0.0f, GRID_ROWS * g_ - h) / snap) * snap;
		const ofRectangle r(x, y, w, h);
		if (!placementFree(r, ia, ib, 0.04f)) continue;
		float score = -bal * (r.getCenter().x - W_ * 0.5f) / W_ * 1.5f + rnd(0.0f, 0.6f);
		if (regionOf(r) == lastRegion_) score -= 0.4f;
		if (score > bestScore) {
			bestScore = score;
			out = r;
			found = true;
		}
	}
	return found;
}

// ── fragment construction ───────────────────────────────────────────────────

void BECollage::pickTransform(bool forceOdd, QuarterTurn & t, bool & fx, bool & fy) {
	fx = fy = false;
	t = QuarterTurn::Deg0;
	if (forceOdd && chance(0.75f)) {
		t = chance(0.5f) ? QuarterTurn::Deg90 : QuarterTurn::Deg270;
		fx = chance(0.12f);
		return;
	}
	const float r = rnd(0.0f, 100.0f); // curated distribution: 40/20/10/15/7.5/5/2.5
	if (r < 40.0f) {
	} else if (r < 60.0f)
		t = QuarterTurn::Deg90;
	else if (r < 70.0f)
		t = QuarterTurn::Deg180;
	else if (r < 85.0f)
		t = QuarterTurn::Deg270;
	else if (r < 92.5f)
		fx = true;
	else if (r < 97.5f)
		fy = true;
	else {
		t = chance(0.5f) ? QuarterTurn::Deg90 : QuarterTurn::Deg270;
		(chance(0.5f) ? fx : fy) = true;
	}
}

CollageCrop BECollage::chooseCrop(const ofRectangle & dest, QuarterTurn rot, CollageRole role) {
	float sLo = 0.3f, sHi = 0.55f;
	switch (role) {
	case CollageRole::Hero: sLo = 0.38f; sHi = 0.7f; break;
	case CollageRole::Secondary: sLo = 0.3f; sHi = 0.55f; break;
	case CollageRole::Detail: sLo = 0.1f; sHi = 0.24f; break;
	case CollageRole::Strip: sLo = 0.3f; sHi = 0.6f; break;
	case CollageRole::Masked: sLo = 0.14f; sHi = 0.28f; break;
	}
	CollageCrop c;
	c.rot = rot;
	c.scale = rnd(sLo, sHi);
	const float dw = std::max(dest.width, 1.0f), dh = std::max(dest.height, 1.0f);
	const float A = isOddTurn(rot) ? dh / dw : dw / dh;
	const float full = std::min(1.0f, A / GEN_ASPECT);
	const float cwn = full * c.scale;
	const float chn = std::min(1.0f, cwn * GEN_ASPECT / A);
	const float hx = std::min(cwn * 0.5f, 0.5f), hy = std::min(chn * 0.5f, 0.5f);
	auto band = [](float half, float & lo, float & hi) {
		lo = std::max(half, 0.2f);
		hi = std::min(1.0f - half, 0.8f);
		if (lo >= hi) {
			lo = half;
			hi = 1.0f - half;
		}
		if (lo >= hi) lo = hi = 0.5f;
	};
	float xlo, xhi, ylo, yhi;
	band(hx, xlo, xhi);
	band(hy, ylo, yhi);

	const glm::vec2 destC(dest.getCenter().x / W_, dest.getCenter().y / H_);
	glm::vec2 best(0.5f, 0.5f);
	float bestScore = -1.0f;
	for (int k = 0; k < 10; k++) {
		const glm::vec2 cand(xlo < xhi ? rnd(xlo, xhi) : xlo, ylo < yhi ? rnd(ylo, yhi) : ylo);
		float minUsed = 1.0f;
		for (const auto & e : elements_)
			if (e.video && e.state != ElemState::Exiting) minUsed = std::min(minUsed, glm::length(e.crop.center - cand));
		const float score = glm::length(cand - destC) + 0.8f * minUsed;
		if (score > bestScore) {
			bestScore = score;
			best = cand;
		}
	}
	c.center = best;
	return c;
}

CollageElement BECollage::makeVideoElement(CollageRole role, const ofRectangle & dest, bool forceOdd, bool circular) {
	CollageElement e;
	e.video = true;
	e.role = role;
	e.rect = dest;
	e.circular = circular;
	QuarterTurn rot;
	bool fx, fy;
	pickTransform(forceOdd, rot, fx, fy);
	e.crop = chooseCrop(dest, rot, role);
	e.crop.flipX = fx;
	e.crop.flipY = fy;
	e.prevCrop = e.crop;
	float pFrame = 0.6f;
	switch (role) {
	case CollageRole::Hero: pFrame = 0.4f; break;
	case CollageRole::Secondary: pFrame = 0.65f; break;
	case CollageRole::Detail: pFrame = 0.85f; break;
	case CollageRole::Strip: pFrame = 0.5f; break;
	case CollageRole::Masked: pFrame = 0.0f; break;
	}
	e.hasFrame = chance(pFrame);
	e.frameWidth = (role == CollageRole::Detail) ? rnd(2.0f, 4.0f) : rnd(3.0f, 7.0f);
	const float fr = rnd(0.0f, 1.0f);
	e.paletteIndex = static_cast<int>(fr < 0.45f ? PaletteToken::Dark : (fr < 0.8f ? PaletteToken::Primary : PaletteToken::Secondary));
	e.enterDur = rnd(tuning_.enterDur);
	e.exitDur = rnd(tuning_.exitDur);
	e.revealDir = static_cast<RevealDir>(irnd(1, 4));
	e.minHold = tuning_.holdVideoMin * rnd(1.0f, 1.5f);
	e.zLayer = (role == CollageRole::Hero) ? 0 : ++zCounter_;
	e.showLabel = (role != CollageRole::Hero);
	if (e.showLabel) {
		char tag[8];
		snprintf(tag, sizeof(tag), "S-%02d", (++labelCounter_) % 100);
		e.labelTag = tag;
	}
	e.spawnAt = time_;
	return e;
}

void BECollage::spawnFromLayout(const CollageLayout & L, float baseT, float stageOffset, const CollageElement * keepAnchor, float delayScale) {
	const bool hasAnchor = keepAnchor != nullptr;
	const ofRectangle anchorRect = hasAnchor ? keepAnchor->finalRect() : ofRectangle();
	ofRectangle blockAnchorRect;
	bool hasBlockAnchor = false;
	for (const auto & e : elements_)
		if (!e.video && e.anchor && e.state != ElemState::Exiting && e.exitAt < 0.0f) {
			blockAnchorRect = e.finalRect();
			hasBlockAnchor = true;
		}

	for (const auto & f : L.fragments) {
		if (fragmentCount() >= tuning_.maxFragments) break;
		if (hasAnchor && overlapFraction(f.destinationRect, anchorRect) > 0.1f) continue;
		if (!f.allowOverlap && !placementFree(f.destinationRect, 0, 0, 0.1f)) continue;
		CollageElement e;
		e.video = true;
		e.role = f.role;
		e.rect = f.destinationRect;
		e.circular = f.circular;
		e.crop.center = f.srcCenters.empty() ? glm::vec2(0.5f) : f.srcCenters[0];
		e.crop.scale = f.srcScale;
		e.crop.rot = f.rotation;
		e.crop.flipX = f.flipX;
		e.crop.flipY = f.flipY;
		e.prevCrop = e.crop;
		e.hasFrame = f.hasFrame;
		e.frameWidth = f.frameWidth;
		e.paletteIndex = f.paletteIndex;
		e.revealDir = f.revealDir;
		e.enterDur = f.dur;
		e.exitDur = rnd(tuning_.exitDur);
		e.zLayer = (f.role == CollageRole::Hero) ? 0 : ++zCounter_;
		e.showLabel = f.showLabel;
		e.labelTag = f.labelTag;
		e.stage = f.stage;
		e.minHold = tuning_.holdVideoMin * rnd(1.0f, 1.5f);
		if (f.hasOverlay) {
			const ofRectangle & d = f.destinationRect;
			const ofRectangle & o = f.overlayRect;
			e.hasOverlay = true;
			e.overlayFrac = ofRectangle((o.x - d.x) / d.width, (o.y - d.y) / d.height, o.width / d.width, o.height / d.height);
			e.overlayAxis = (o.width < d.width * 0.99f) ? 0 : 1;
			e.overlayColor = f.overlayColor;
			e.overlayAlphaMax = f.overlayAlpha;
		}
		e.spawnAt = (f.stage == 0) ? baseT + f.delay * delayScale : baseT + stageOffset + f.delay * delayScale;
		addElement(std::move(e));
	}

	for (const auto & g : L.graphics) {
		if (graphicCount() >= tuning_.maxGraphics) break;
		if (hasBlockAnchor && g.type == CollageBlockType::SOLID_BLOCK && g.pass == 0 && overlapFraction(g.rect, blockAnchorRect) > 0.5f) continue;
		CollageElement e;
		e.video = false;
		e.type = g.type;
		e.rect = g.rect;
		e.color = g.color;
		e.textColor = g.textColor;
		e.alphaMax = g.alpha;
		e.pass = g.pass;
		e.revealDir = g.revealDir;
		e.vertical = g.vertical;
		e.filled = g.filled;
		e.segmented = g.segmented;
		e.pulse = g.pulse;
		e.markKind = g.markKind;
		e.strokeWidth = g.strokeWidth;
		e.text = g.text;
		e.linger = g.linger;
		e.enterDur = g.dur;
		e.exitDur = rnd(tuning_.exitDur);
		e.stage = g.stage;
		e.minHold = tuning_.holdGraphicMin * rnd(1.0f, 1.8f);
		e.value.snap(g.value);
		e.spawnAt = (g.stage == 0) ? baseT + g.delay * delayScale : baseT + stageOffset + g.delay * delayScale;
		if (g.type == CollageBlockType::DATA_STRIP) {
			bool anyBound = false;
			for (const auto & o : elements_)
				if (!o.video && o.bind == 1 && o.state != ElemState::Exiting) anyBound = true;
			if (!anyBound) e.bind = 1;
		}
		addElement(std::move(e));
	}
}

void BECollage::pickAnchors() {
	// One video anchor: keep the current one while it lives, else the hero / largest image.
	CollageElement * keep = anchorId_ ? find(anchorId_) : nullptr;
	if (keep && (keep->state == ElemState::Exiting || keep->exitAt >= 0.0f)) keep = nullptr;
	if (!keep) {
		float best = -1.0f;
		for (auto & e : elements_) {
			if (!e.video || e.state == ElemState::Exiting || e.exitAt >= 0.0f) continue;
			float s = areaOf(e.finalRect()) * (e.role == CollageRole::Hero ? 1.5f : 1.0f);
			if (s > best) {
				best = s;
				keep = &e;
			}
		}
	}
	for (auto & e : elements_)
		if (e.video) e.anchor = false;
	anchorId_ = 0;
	if (keep) {
		keep->anchor = true;
		anchorId_ = keep->id;
	}
	// One graphic anchor: the largest color mass.
	CollageElement * mass = nullptr;
	float best = -1.0f;
	for (auto & e : elements_) {
		if (e.video) continue;
		e.anchor = false;
		if (e.type != CollageBlockType::SOLID_BLOCK || e.pass != 0 || e.state == ElemState::Exiting || e.exitAt >= 0.0f) continue;
		const float s = areaOf(e.finalRect());
		if (s > best) {
			best = s;
			mass = &e;
		}
	}
	if (mass) mass->anchor = true;
}

// ── update / director ───────────────────────────────────────────────────────

void BECollage::update(float dt) {
	if (!hasLayout_ || locked_) return;
	dt = std::min(std::max(dt, 0.0f), MAX_DT);
	time_ += dt;

	if (paletteMix_ < 1.0f) {
		paletteMix_ = std::min(1.0f, paletteMix_ + dt / PALETTE_FADE);
		const auto & to = layout_.palette;
		const float m = paletteMix_;
		activePal_.name = to.name;
		activePal_.background = fromPal_.background.getLerped(to.background, m);
		activePal_.primary = fromPal_.primary.getLerped(to.primary, m);
		activePal_.secondary = fromPal_.resolve(PaletteToken::Secondary).getLerped(to.resolve(PaletteToken::Secondary), m);
		activePal_.dark = fromPal_.dark.getLerped(to.dark, m);
		activePal_.light = fromPal_.light.getLerped(to.light, m);
	}

	if (directorEnabled_) directorUpdate(dt);

	const float cover = videoCover();
	for (auto & e : elements_)
		updateElement(e, dt, cover);

	elements_.erase(std::remove_if(elements_.begin(), elements_.end(),
						[](const CollageElement & e) { return e.state == ElemState::Exiting && e.stateT >= e.exitDur; }),
		elements_.end());
	if (anchorId_ && !find(anchorId_)) pickAnchors();
}

void BECollage::directorUpdate(float dt) {
	updateActivity(dt);

	accMicro_ += dt * level_;
	if (accMicro_ >= nextMicro_) {
		if (fireTier(true)) {
			accMicro_ = 0.0f;
			nextMicro_ = rnd(tuning_.micro);
		} else {
			accMicro_ = nextMicro_ - 0.4f; // rejected / saturated: retry soon, never stall
		}
	}
	accLocal_ += dt * level_;
	if (accLocal_ >= nextLocal_) {
		if (fireTier(false)) {
			accLocal_ = 0.0f;
			nextLocal_ = rnd(tuning_.local);
		} else {
			accLocal_ = nextLocal_ - 0.5f;
		}
	}

	processQueue();

	anchorAcc_ += dt;
	if (anchorAcc_ >= nextAnchorRotate_) {
		anchorAcc_ = 0.0f;
		nextAnchorRotate_ = rnd(tuning_.anchorRotate);
		// Promote a different large image so no hero stays forever.
		CollageElement * best = nullptr;
		float bestA = 0.04f * W_ * H_;
		for (auto & e : elements_) {
			if (!e.video || e.anchor || e.state != ElemState::Active || e.exitAt >= 0.0f) continue;
			if (time_ - e.born < e.minHold * 2.0f) continue;
			const float a = areaOf(e.rect) * rnd(0.8f, 1.2f);
			if (a > bestA) {
				bestA = a;
				best = &e;
			}
		}
		if (best) {
			if (CollageElement * old = anchorId_ ? find(anchorId_) : nullptr) old->anchor = false;
			best->anchor = true;
			anchorId_ = best->id;
		}
	}

	recomposeAcc_ += dt;
	if (recomposeAcc_ >= nextRecompose_ && !dissolving_) {
		int tmpl = irnd(0, TEMPLATE_COUNT - 2);
		if (tmpl >= layout_.templateIndex) tmpl++;
		recompose(static_cast<uint32_t>(rng_()), tmpl, false);
	}
}

void BECollage::updateActivity(float dt) {
	swellAcc_ += dt;
	if (activity_ != CollageActivity::Swell && !dissolving_ && swellAcc_ >= nextSwell_) startSwell();

	activityLeft_ -= dt;
	if (activityLeft_ <= 0.0f) {
		switch (activity_) {
		case CollageActivity::Active:
		case CollageActivity::Quiet:
			if (activity_ == CollageActivity::Active && chance(tuning_.quietChance)) {
				activity_ = CollageActivity::Quiet;
				levelTarget_ = tuning_.quietLevel;
				activityLeft_ = rnd(tuning_.quietDwell);
			} else {
				activity_ = CollageActivity::Active;
				levelTarget_ = tuning_.activeLevel;
				activityLeft_ = rnd(tuning_.activeDwell);
			}
			break;
		case CollageActivity::Swell:
			startSettle();
			break;
		case CollageActivity::Settle:
			if (chance(tuning_.quietChance)) {
				activity_ = CollageActivity::Quiet;
				levelTarget_ = tuning_.quietLevel;
				activityLeft_ = rnd(tuning_.quietDwell);
			} else {
				activity_ = CollageActivity::Active;
				levelTarget_ = tuning_.activeLevel;
				activityLeft_ = rnd(tuning_.activeDwell);
			}
			break;
		}
	}
	// Asymmetric envelope: fast rise, slow decay.
	const float tau = (levelTarget_ > level_) ? tuning_.riseSeconds : tuning_.decaySeconds;
	level_ += (levelTarget_ - level_) * std::min(1.0f, dt / std::max(tau, 0.05f));
}

bool BECollage::fireTier(bool micro) {
	const int limit = std::max(1, static_cast<int>(std::lround(tuning_.maxConcurrent * std::max(level_, 0.5f))));
	if (inTransitionCount() > limit) return false;
	return runWeighted(micro);
}

bool BECollage::runWeighted(bool micro) {
	float w[COLLAGE_EVENT_COUNT] = {};
	const float cover = videoCover();
	const int frags = fragmentCount(), gfx = graphicCount();
	const bool sparse = cover < tuning_.coverLo || frags < tuning_.minFragments;
	const bool crowded = cover > tuning_.coverHi || gfx > tuning_.maxGraphics * 0.85f || frags >= tuning_.maxFragments;
	for (int i = 0; i < COLLAGE_EVENT_COUNT; i++) {
		const CollageEvent e = static_cast<CollageEvent>(i);
		if (micro ? !collageEventIsMicro(e) : !collageEventIsLocal(e)) continue;
		float x = tuning_.weights[i];
		const float since = time_ - lastEventTime_[i];
		if (since < 6.0f) x *= 0.25f + 0.75f * (since / 6.0f); // avoid repeating the same event
		switch (e) {
		case CollageEvent::AddMicroGraphic: x *= crowded ? 0.2f : (sparse ? 1.3f : 1.0f); break;
		case CollageEvent::RemoveMicroGraphic: x *= crowded ? 3.0f : (gfx < tuning_.maxGraphics * 0.3f ? 0.2f : 1.0f); break;
		case CollageEvent::Split: x *= crowded ? 0.0f : (sparse ? 1.5f : 1.0f); break;
		case CollageEvent::Merge: x *= crowded ? 3.0f : (sparse ? 0.0f : 1.0f); break;
		case CollageEvent::ReplaceFragment:
		case CollageEvent::StripReplace: x *= sparse ? 1.5f : 1.0f; break;
		default: break;
		}
		if (dissolving_ && (e == CollageEvent::AddMicroGraphic || e == CollageEvent::ReplaceFragment || e == CollageEvent::StripReplace
							   || e == CollageEvent::Split || e == CollageEvent::Merge))
			x = 0.0f;
		w[i] = x;
	}
	for (int attempt = 0; attempt < COLLAGE_EVENT_COUNT; attempt++) {
		float sum = 0.0f;
		for (int i = 0; i < COLLAGE_EVENT_COUNT; i++)
			sum += w[i];
		if (sum <= 0.0f) return false;
		float pick = rnd(0.0f, sum);
		int chosen = -1;
		for (int i = 0; i < COLLAGE_EVENT_COUNT; i++) {
			if (w[i] <= 0.0f) continue;
			chosen = i;
			pick -= w[i];
			if (pick <= 0.0f) break;
		}
		if (chosen < 0) return false;
		if (runEvent(static_cast<CollageEvent>(chosen), 0, -1)) return true;
		w[chosen] = 0.0f;
	}
	return false;
}

void BECollage::enqueue(float delay, CollageEvent e, uint32_t target, int param) {
	if (static_cast<int>(queue_.size()) >= tuning_.maxQueue) return;
	QueuedEvent q { time_ + delay, e, target, param };
	auto it = std::upper_bound(queue_.begin(), queue_.end(), q.at, [](float t, const QueuedEvent & x) { return t < x.at; });
	queue_.insert(it, q);
}

void BECollage::processQueue() {
	for (int guard = 0; guard < 8 && !queue_.empty() && queue_.front().at <= time_; guard++) {
		const QueuedEvent q = queue_.front();
		queue_.erase(queue_.begin());
		runEvent(q.type, q.target, q.param);
	}
}

void BECollage::logEvent(CollageEvent e, uint32_t id) {
	log_.push_back({ time_, e, id, fragmentCount(), graphicCount() });
	if (log_.size() > 512) log_.pop_front();
}

bool BECollage::runEvent(CollageEvent e, uint32_t target, int param) {
	if (dissolving_ && (e == CollageEvent::AddMicroGraphic || e == CollageEvent::ReplaceFragment || e == CollageEvent::StripReplace
						   || e == CollageEvent::Split || e == CollageEvent::Merge || e == CollageEvent::SwellMass))
		return false;
	touched_ = 0;
	bool ok = false;
	switch (e) {
	case CollageEvent::OverprintFade: ok = evOverprint(target); break;
	case CollageEvent::FrameEdge: ok = evFrameEdge(target); break;
	case CollageEvent::GridReveal: ok = evReveal(target, CollageBlockType::GRID_BLOCK); break;
	case CollageEvent::TickExtend: ok = evReveal(target, CollageBlockType::TICK_STRIP); break;
	case CollageEvent::MarkPulse: ok = evMarkPulse(target); break;
	case CollageEvent::MarkMove: ok = evMarkMove(target); break;
	case CollageEvent::DataUpdate: ok = evDataUpdate(target); break;
	case CollageEvent::AddMicroGraphic: ok = evAddMicro(target, param); break;
	case CollageEvent::RemoveMicroGraphic: ok = evRemoveMicro(target, param > 0); break;
	case CollageEvent::CropSwap: ok = evCropSwap(target); break;
	case CollageEvent::MoveResize: ok = evMoveResize(target); break;
	case CollageEvent::ReplaceFragment: ok = evReplace(target, false); break;
	case CollageEvent::StripReplace: ok = evReplace(target, true); break;
	case CollageEvent::TransformChange: ok = evTransform(target); break;
	case CollageEvent::Split: ok = evSplit(target); break;
	case CollageEvent::Merge: ok = evMerge(target); break;
	case CollageEvent::BlockSlide: ok = evBlockSlide(target); break;
	case CollageEvent::Montage: ok = evMontage(target); break;
	case CollageEvent::SwellMass: ok = evSwellMass(target); break;
	default: break;
	}
	if (ok) {
		const int i = static_cast<int>(e);
		eventCount_++;
		perEventCount_[i]++;
		lastEventTime_[i] = time_;
		logEvent(e, touched_);
	}
	return ok;
}

// ── choreography ────────────────────────────────────────────────────────────

void BECollage::startSwell() {
	activity_ = CollageActivity::Swell;
	levelTarget_ = tuning_.swellLevel;
	activityLeft_ = rnd(tuning_.swellLength);
	swellAcc_ = 0.0f;
	nextSwell_ = rnd(tuning_.swell);
	swellCount_++;
	logEvent(CollageEvent::SwellMass, 0);
	// A coordinated, bounded revision: color mass swells, some images subdivide or
	// reposition, measurement graphics wipe — staggered, never simultaneous.
	enqueue(0.0f, CollageEvent::SwellMass);
	enqueue(0.6f + rnd(0.0f, 0.5f), fragmentCount() + 1 < tuning_.maxFragments ? CollageEvent::Split : CollageEvent::MoveResize);
	enqueue(1.6f + rnd(0.0f, 0.6f), CollageEvent::ReplaceFragment);
	enqueue(2.3f, CollageEvent::GridReveal);
	enqueue(2.8f, CollageEvent::TickExtend);
	enqueue(3.4f + rnd(0.0f, 0.5f), fragmentCount() >= 5 ? CollageEvent::Merge : CollageEvent::CropSwap);
	enqueue(4.0f, CollageEvent::TransformChange);
}

void BECollage::startSettle() {
	activity_ = CollageActivity::Settle;
	levelTarget_ = tuning_.settleLevel;
	activityLeft_ = rnd(tuning_.settleLength);
	logEvent(CollageEvent::Settle, 0);
	// Restore negative space: thin out excess graphics, consolidate crowded imagery.
	const int excess = graphicCount() - static_cast<int>(tuning_.maxGraphics * 0.55f);
	for (int i = 0; i < std::min(4, std::max(0, excess)); i++)
		enqueue(0.3f + 0.5f * i, CollageEvent::RemoveMicroGraphic, 0, 1);
	if (fragmentCount() > 5 || videoCover() > tuning_.coverHi) enqueue(1.0f, CollageEvent::Merge);
	enqueue(2.5f, CollageEvent::BlockSlide);
}

void BECollage::startDissolveClear() {
	logEvent(CollageEvent::DissolveClear, 0);
	elements_.erase(std::remove_if(elements_.begin(), elements_.end(), [](const CollageElement & e) { return e.state == ElemState::Pending; }), elements_.end());
	queue_.clear();
	int keptVideo = 0;
	for (auto & e : elements_) {
		if (e.state == ElemState::Exiting || e.exitAt >= 0.0f) continue;
		if (e.video) {
			if (e.anchor || keptVideo < 1) {
				keptVideo += e.anchor ? 0 : 1;
				continue;
			}
			if (chance(0.7f)) e.exitAt = time_ + rnd(0.2f, 3.0f); // imagery leaves first, staggered
		} else if (!e.anchor && !e.linger && chance(0.55f)) {
			e.exitAt = time_ + rnd(0.8f, 4.0f); // graphic residue mostly lingers
		}
	}
	activity_ = CollageActivity::Quiet;
	levelTarget_ = tuning_.quietLevel;
	activityLeft_ = rnd(tuning_.quietDwell);
}

void BECollage::recompose(uint32_t seed, int tmpl, bool immediate) {
	rng_.seed(static_cast<uint32_t>(rng_()) ^ seed);
	CollageLayout L = generateLayout(seed, W_, H_, tmpl);
	if (chance(0.4f)) L.palette = layout_.palette; // sometimes keep the colour family
	const bool paletteChanged = std::strcmp(L.palette.name, layout_.palette.name) != 0;
	if (paletteChanged) applyPaletteTarget(L.palette);
	layout_ = L;
	if (!paletteChanged) layout_.palette = L.palette;

	elements_.erase(std::remove_if(elements_.begin(), elements_.end(), [](const CollageElement & e) { return e.state == ElemState::Pending; }), elements_.end());
	queue_.clear();

	const CollageElement * anchor = anchorId_ ? find(anchorId_) : nullptr;
	if (anchor && (anchor->state == ElemState::Exiting || anchor->exitAt >= 0.0f)) anchor = nullptr;
	const uint32_t anchorId = anchor ? anchor->id : 0;
	for (auto & e : elements_) {
		if (e.state == ElemState::Exiting || e.id == anchorId) continue;
		e.anchor = false;
		if (!e.video && e.linger && chance(0.5f)) continue; // printed residue survives the change
		e.exitAt = time_ + (immediate ? 0.0f : rnd(0.0f, 3.5f)); // staggered retirement
	}
	spawnFromLayout(L, time_ + 0.8f, 4.5f, anchor, 0.6f);
	pickAnchors();

	dissolving_ = false;
	dissolveT0_ = -1.0f;
	recomposeCount_++;
	recomposeAcc_ = 0.0f;
	nextRecompose_ = rnd(tuning_.recompose);
	logEvent(CollageEvent::Recompose, anchorId);
	ofLogNotice("BECollage") << "recompose seed=" << seed << " template=" << layout_.templateName
							 << " palette=" << layout_.palette.name << " elements=" << elements_.size();
}

// ── element pickers ─────────────────────────────────────────────────────────

CollageElement * BECollage::pickFragment(uint32_t target, bool allowAnchor, bool needHold) {
	auto ok = [&](const CollageElement & e) {
		return e.video && e.state == ElemState::Active && e.exitAt < 0.0f && (allowAnchor || !e.anchor)
			&& (!needHold || time_ - e.born >= e.minHold);
	};
	if (target) {
		CollageElement * e = find(target);
		return (e && ok(*e)) ? e : nullptr;
	}
	std::vector<CollageElement *> c;
	c.reserve(16);
	for (auto & e : elements_)
		if (ok(e)) c.push_back(&e);
	if (c.empty()) return nullptr;
	if (c.size() > 1 && lastRegion_ >= 0 && chance(0.7f)) { // avoid repeating the same region
		std::vector<CollageElement *> other;
		for (auto * e : c)
			if (regionOf(e->rect) != lastRegion_) other.push_back(e);
		if (!other.empty()) c.swap(other);
	}
	CollageElement * pick = c[irnd(0, static_cast<int>(c.size()) - 1)];
	lastRegion_ = regionOf(pick->rect);
	return pick;
}

CollageElement * BECollage::pickGraphic(uint32_t target, const std::function<bool(const CollageElement &)> & pred, bool needHold) {
	auto ok = [&](const CollageElement & e) {
		return !e.video && e.state == ElemState::Active && e.exitAt < 0.0f && (!needHold || time_ - e.born >= e.minHold) && pred(e);
	};
	if (target) {
		CollageElement * e = find(target);
		return (e && ok(*e)) ? e : nullptr;
	}
	std::vector<CollageElement *> c;
	c.reserve(24);
	for (auto & e : elements_)
		if (ok(e)) c.push_back(&e);
	if (c.empty()) return nullptr;
	return c[irnd(0, static_cast<int>(c.size()) - 1)];
}

// ── micro events ────────────────────────────────────────────────────────────

bool BECollage::evOverprint(uint32_t target) {
	CollageElement * f = pickFragment(target, true, false);
	if (!f) return false;
	if (!f->hasOverlay) {
		f->hasOverlay = true;
		if (chance(0.5f)) {
			const float w = rnd(0.35f, 0.6f);
			f->overlayFrac = ofRectangle(chance(0.5f) ? 0.0f : 1.0f - w, 0.0f, w, 1.0f);
			f->overlayAxis = 0;
		} else {
			const float h = rnd(0.35f, 0.6f);
			f->overlayFrac = ofRectangle(0.0f, chance(0.5f) ? 0.0f : 1.0f - h, 1.0f, h);
			f->overlayAxis = 1;
		}
		f->overlayColor = chance(0.5f) ? PaletteToken::Primary : PaletteToken::Secondary;
		f->overlayAlphaMax = rnd(0.25f, 0.42f);
		f->overlay.snap(0.0f);
		f->cover.snap(1.0f);
		f->overlay.set(1.0f, 0.9f);
	} else if (chance(0.5f)) {
		f->overlay.set(f->overlay.to > 0.5f ? 0.0f : 1.0f, 0.9f);
	} else {
		f->cover.set(rnd(0.3f, 1.0f), 1.1f);
		if (f->overlay.to < 0.5f) f->overlay.set(1.0f, 0.7f);
	}
	touched_ = f->id;
	return true;
}

bool BECollage::evFrameEdge(uint32_t target) {
	CollageElement * f = pickFragment(target, true, false);
	if (!f || f->circular) return false;
	if (f->frame.to > 0.5f) {
		f->frame.set(0.0f, 0.8f); // retract
	} else {
		f->hasFrame = true;
		f->frameWidth = rnd(3.0f, 7.0f);
		f->frame.set(1.0f, 1.0f); // draw on
	}
	touched_ = f->id;
	return true;
}

bool BECollage::evReveal(uint32_t target, CollageBlockType type) {
	CollageElement * g = pickGraphic(target, [type](const CollageElement & e) { return e.type == type; }, false);
	if (!g) return false;
	g->ext.set(g->ext.to > 0.55f ? rnd(0.0f, 0.35f) : 1.0f, 1.4f);
	touched_ = g->id;
	return true;
}

bool BECollage::evMarkPulse(uint32_t target) {
	CollageElement * g = pickGraphic(target, [](const CollageElement & e) { return e.type == CollageBlockType::REGISTRATION_MARK; }, false);
	if (!g) return false;
	g->flash = 1.0f;
	touched_ = g->id;
	return true;
}

bool BECollage::evMarkMove(uint32_t target) {
	CollageElement * g = pickGraphic(target, [](const CollageElement & e) { return e.type == CollageBlockType::REGISTRATION_MARK && e.moveT >= 1.0f; }, true);
	if (!g) return false;
	ofRectangle to = g->rect;
	const float d = static_cast<float>(irnd(1, 3)) * g_ * (chance(0.5f) ? 1.0f : -1.0f);
	(chance(0.5f) ? to.x : to.y) += d;
	if (to.x < 0 || to.y < 0 || to.x + to.width > W_ || to.y + to.height > H_) return false;
	g->rectFrom = g->rect;
	g->rectTo = to;
	g->moveT = 0.0f;
	g->moveDur = rnd(0.35f, 0.6f); // a snap, not a drift
	touched_ = g->id;
	return true;
}

bool BECollage::evDataUpdate(uint32_t target) {
	CollageElement * g = pickGraphic(target, [](const CollageElement & e) { return e.type == CollageBlockType::DATA_STRIP && e.bind == 0; }, false);
	if (!g) return false;
	g->value.set(rnd(0.15f, 0.95f), rnd(0.8f, 1.5f)); // decorative bar (no semantic binding)
	touched_ = g->id;
	return true;
}

bool BECollage::evAddMicro(uint32_t target, int param) {
	(void)target;
	if (graphicCount() >= tuning_.maxGraphics) return false;
	std::vector<const CollageElement *> frs;
	frs.reserve(16);
	for (const auto & e : elements_)
		if (e.video && e.state != ElemState::Exiting && e.exitAt < 0.0f) frs.push_back(&e);
	int kind = (param >= 0 && param < 6) ? param : irnd(0, 5);
	if (frs.empty()) kind = 5;
	const CollageElement * f = frs.empty() ? nullptr : frs[irnd(0, static_cast<int>(frs.size()) - 1)];
	const ofRectangle canvas(0, 0, static_cast<float>(W_), static_cast<float>(H_));
	auto inCanvas = [&](const ofRectangle & r) { return r.x >= -1 && r.y >= -1 && r.x + r.width <= W_ + 1 && r.y + r.height <= H_ + 1; };

	CollageElement e;
	e.video = false;
	e.enterDur = rnd(0.5f, 1.0f);
	e.exitDur = rnd(tuning_.exitDur);
	e.minHold = tuning_.holdGraphicMin * rnd(1.0f, 1.8f);
	e.spawnAt = time_;
	e.color = PaletteToken::Dark;
	e.pass = 5;

	switch (kind) {
	case 0: { // tick rail along an image edge
		const ofRectangle r = f->finalRect();
		e.type = CollageBlockType::TICK_STRIP;
		switch (irnd(0, 3)) {
		case 0: e.rect = ofRectangle(r.x + r.width * 0.05f, r.y + r.height + 0.2f * g_, r.width * rnd(0.7f, 1.0f), 0.45f * g_); e.vertical = false; break;
		case 1: e.rect = ofRectangle(r.x + r.width * 0.05f, r.y - 0.65f * g_, r.width * rnd(0.7f, 1.0f), 0.45f * g_); e.vertical = false; break;
		case 2: e.rect = ofRectangle(r.x + r.width + 0.2f * g_, r.y + r.height * 0.05f, 0.45f * g_, r.height * rnd(0.7f, 1.0f)); e.vertical = true; break;
		default: e.rect = ofRectangle(r.x - 0.65f * g_, r.y + r.height * 0.05f, 0.45f * g_, r.height * rnd(0.7f, 1.0f)); e.vertical = true; break;
		}
		e.enterDur = rnd(1.0f, 1.8f);
		break;
	}
	case 1: { // registration / sample marker at an image corner
		const ofRectangle r = f->finalRect();
		const float cx = (chance(0.5f) ? r.x : r.x + r.width) + (chance(0.5f) ? -0.45f : 0.45f) * g_;
		const float cy = (chance(0.5f) ? r.y : r.y + r.height) + (chance(0.5f) ? -0.45f : 0.45f) * g_;
		const float rad = rnd(0.28f, 0.4f) * g_;
		e.type = CollageBlockType::REGISTRATION_MARK;
		e.rect = ofRectangle(cx - rad, cy - rad, rad * 2, rad * 2);
		e.markKind = irnd(0, 4);
		e.pulse = chance(0.4f);
		e.enterDur = 0.3f;
		break;
	}
	case 2: { // segmented / solid bar under an image
		const ofRectangle r = f->finalRect();
		e.type = CollageBlockType::DATA_STRIP;
		e.rect = ofRectangle(r.x, r.y + r.height + 0.3f * g_, std::max(2.0f * g_, r.width * rnd(0.5f, 1.0f)), 0.22f * g_);
		e.segmented = chance(0.6f);
		e.color = chance(0.3f) ? PaletteToken::Secondary : PaletteToken::Dark;
		e.value.snap(rnd(0.2f, 0.9f));
		bool anyBound = false;
		for (const auto & o : elements_)
			if (!o.video && o.bind == 1 && o.state != ElemState::Exiting) anyBound = true;
		if (!anyBound) e.bind = 1; // one bar honestly tracks video coverage
		e.enterDur = 0.5f;
		break;
	}
	case 3: { // translucent overprint partly across an image
		const ofRectangle r = f->finalRect();
		const float w = r.width * rnd(0.35f, 0.7f);
		const bool left = chance(0.5f);
		e.type = CollageBlockType::TRANSLUCENT_BLOCK;
		e.rect = ofRectangle(left ? r.x - 0.5f * g_ : r.x + r.width - w + 0.5f * g_, r.y - rnd(0.0f, 0.4f) * g_, w + 0.5f * g_, r.height * rnd(0.6f, 1.1f));
		e.color = chance(0.5f) ? PaletteToken::Primary : PaletteToken::Secondary;
		e.alphaMax = rnd(0.24f, 0.4f);
		e.pass = 3;
		e.revealDir = left ? RevealDir::LeftToRight : RevealDir::RightToLeft;
		break;
	}
	case 4: { // partial frame corners around an image
		ofRectangle r = f->finalRect();
		r.x -= 0.3f * g_;
		r.y -= 0.3f * g_;
		r.width += 0.6f * g_;
		r.height += 0.6f * g_;
		e.type = CollageBlockType::PARTIAL_FRAME;
		e.rect = r;
		e.strokeWidth = 2.0f;
		break;
	}
	default: { // sparse technical grid patch in open space
		const float w = std::round(rnd(5.0f, 9.0f)) * g_, h = std::round(rnd(3.0f, 5.0f)) * g_;
		e.type = CollageBlockType::GRID_BLOCK;
		e.rect = ofRectangle(std::round(rnd(0.0f, W_ - w) / g_) * g_, y0_ + std::round(rnd(0.0f, GRID_ROWS * g_ - h) / g_) * g_, w, h);
		e.pass = 1;
		e.revealDir = static_cast<RevealDir>(irnd(1, 4));
		e.enterDur = rnd(1.2f, 1.9f);
		break;
	}
	}
	if (!inCanvas(e.rect)) return false;
	touched_ = addElement(std::move(e)).id;
	return true;
}

bool BECollage::evRemoveMicro(uint32_t target, bool excess) {
	CollageElement * g = pickGraphic(target, [&](const CollageElement & e) {
		if (e.anchor || e.linger) return false;
		if (e.type == CollageBlockType::SOLID_BLOCK && e.pass == 0) return false;
		if (e.bind == 1 && !excess) return false;
		return true;
	}, true);
	if (!g) return false;
	startExit(*g);
	touched_ = g->id;
	return true;
}

// ── local events ────────────────────────────────────────────────────────────

bool BECollage::evCropSwap(uint32_t target) {
	CollageElement * f = pickFragment(target, true, true);
	if (!f || f->cropT < 1.0f) return false;
	CollageCrop c = chooseCrop(f->rect, f->crop.rot, f->role);
	c.rot = f->crop.rot;
	c.flipX = f->crop.flipX;
	c.flipY = f->crop.flipY;
	f->prevCrop = f->crop;
	f->crop = c;
	f->cropT = 0.0f;
	f->cropDur = rnd(tuning_.cropDur);
	f->cropWipe = static_cast<RevealDir>(irnd(1, 4));
	touched_ = f->id;
	return true;
}

bool BECollage::evTransform(uint32_t target) {
	CollageElement * f = pickFragment(target, true, true);
	if (!f || f->cropT < 1.0f) return false;
	QuarterTurn rot = f->crop.rot;
	bool fx = f->crop.flipX, fy = f->crop.flipY;
	for (int i = 0; i < 8; i++) {
		pickTransform(f->role == CollageRole::Strip && chance(0.5f), rot, fx, fy);
		if (rot != f->crop.rot || fx != f->crop.flipX || fy != f->crop.flipY) break;
	}
	if (rot == f->crop.rot && fx == f->crop.flipX && fy == f->crop.flipY) return false;
	CollageCrop c = chooseCrop(f->rect, rot, f->role);
	c.center = f->crop.center; // same sample, new orientation: reads as a deliberate re-print
	c.rot = rot;
	c.flipX = fx;
	c.flipY = fy;
	f->prevCrop = f->crop;
	f->crop = c;
	f->cropT = 0.0f;
	f->cropDur = rnd(0.8f, 1.2f);
	f->cropWipe = static_cast<RevealDir>(irnd(1, 4)); // covered by a wipe, never a free spin
	touched_ = f->id;
	return true;
}

bool BECollage::evMoveResize(uint32_t target) {
	CollageElement * f = pickFragment(target, false, true);
	if (!f || f->moveT < 1.0f || f->cropT < 1.0f) return false;
	const ofRectangle r = f->rect;
	const float snap = g_ * 0.5f;
	const bool grow = fragmentCount() < tuning_.maxFragments && videoCover() < tuning_.coverLo;
	for (int i = 0; i < 14; i++) {
		float w = r.width + irnd(grow ? 0 : -2, 2) * g_ * 0.5f * 2.0f;
		float h = r.height + irnd(grow ? 0 : -2, 2) * g_ * 0.5f * 2.0f;
		w = ofClamp(w, 2.0f * g_, 11.0f * g_);
		h = ofClamp(h, 2.0f * g_, 9.0f * g_);
		if (f->circular) h = w;
		ofRectangle nr(std::round((r.x + irnd(-3, 3) * snap) / snap) * snap, y0_ + std::round((r.y - y0_ + irnd(-3, 3) * snap) / snap) * snap, w, h);
		if (nr.x < 0.3f * g_ || nr.x + nr.width > W_ - 0.3f * g_ || nr.y < y0_ || nr.y + nr.height > y0_ + GRID_ROWS * g_) continue;
		if (std::fabs(nr.x - r.x) + std::fabs(nr.y - r.y) + std::fabs(nr.width - r.width) + std::fabs(nr.height - r.height) < g_ * 0.9f) continue;
		if (!placementFree(nr, f->id, 0, 0.04f)) continue;
		f->rectFrom = f->rect;
		f->rectTo = nr;
		f->moveT = 0.0f;
		f->moveDur = rnd(tuning_.moveDur);
		if (f->pendingDiversify) { // split child: also take a different sample + orientation
			f->pendingDiversify = false;
			CollageCrop c = chooseCrop(nr, f->crop.rot, f->role);
			f->prevCrop = f->crop;
			f->crop = c;
			f->cropT = 0.0f;
			f->cropDur = rnd(tuning_.cropDur);
			f->cropWipe = static_cast<RevealDir>(irnd(1, 4));
		}
		touched_ = f->id;
		return true;
	}
	return false;
}

bool BECollage::evReplace(uint32_t target, bool strip) {
	CollageElement * old = pickFragment(target, false, true);
	if (!old) return false;
	const ofRectangle orect = old->finalRect();
	const uint32_t oldId = old->id;
	const CollageRole oldRole = old->role;
	const bool oldWide = orect.width >= orect.height;
	CollageRole role = strip ? CollageRole::Strip : (oldRole == CollageRole::Hero ? CollageRole::Secondary : oldRole);
	float w, h;
	bool circ = false;
	if (strip) {
		if (!oldWide) { // swap the strip's orientation
			w = std::round(rnd(7.0f, 10.0f)) * g_;
			h = std::round(rnd(2.0f, 3.0f)) * g_;
		} else {
			w = std::round(rnd(2.0f, 3.0f)) * g_;
			h = std::round(rnd(6.0f, 8.0f)) * g_;
		}
	} else if (oldRole == CollageRole::Masked) {
		w = h = std::round(rnd(2.0f, 3.5f)) * g_;
		circ = true;
	} else {
		w = ofClamp(orect.width * rnd(0.8f, 1.3f), 2.0f * g_, 9.0f * g_);
		h = ofClamp(orect.height * rnd(0.8f, 1.3f), 2.0f * g_, 8.0f * g_);
	}
	const bool growOnly = fragmentCount() < tuning_.maxFragments && (videoCover() < tuning_.coverLo || fragmentCount() < tuning_.minFragments);
	ofRectangle r;
	if (!findPlacement(w, h, r, growOnly ? 0 : oldId)) {
		if (growOnly) return false;
		r = orect; // in place: new image arrives over the departing one
		if (strip) return false;
	}
	CollageElement e = makeVideoElement(role, r, true, circ);
	e.spawnAt = time_;
	const uint32_t id = addElement(std::move(e)).id;
	if (!growOnly) {
		if (CollageElement * o = find(oldId)) o->exitAt = time_ + rnd(0.15f, 0.7f); // replacement starts before the old one is gone
	}
	touched_ = id;
	return true;
}

bool BECollage::evSplit(uint32_t target) {
	CollageElement * f = pickFragment(target, false, true);
	if (!f || f->circular || f->moveT < 1.0f || f->cropT < 1.0f) return false;
	const ofRectangle r = f->rect;
	const bool horiz = r.width >= r.height;
	const float len = horiz ? r.width : r.height;
	if (len < 3.6f * g_) return false;
	int n = (len >= 9.0f * g_ && chance(0.5f)) ? 3 : 2;
	if (fragmentCount() - 1 + n > tuning_.maxFragments) n = 2;
	if (fragmentCount() - 1 + n > tuning_.maxFragments) return false;
	const uint32_t parentId = f->id;
	const float gap = 0.25f * g_;
	float wts[3], sum = 0.0f;
	for (int i = 0; i < n; i++) {
		wts[i] = rnd(0.8f, 1.3f);
		sum += wts[i];
	}
	const float avail = len - gap * (n - 1);
	const int group = nextGroup_++;
	float pos = horiz ? r.x : r.y;
	uint32_t ids[3] = { 0, 0, 0 };
	for (int i = 0; i < n; i++) {
		const float part = avail * wts[i] / sum;
		const ofRectangle cr = horiz ? ofRectangle(pos, r.y, part, r.height) : ofRectangle(r.x, pos, r.width, part);
		pos += part + gap;
		const float minSide = std::min(cr.width, cr.height);
		const CollageRole role = minSide < 2.6f * g_ ? CollageRole::Strip : (areaOf(cr) < 9.0f * g_ * g_ ? CollageRole::Detail : CollageRole::Secondary);
		CollageElement c = makeVideoElement(role, cr, i != 0 || chance(0.5f), false);
		c.group = group;
		c.pendingDiversify = true;
		c.spawnAt = time_ + 0.1f * i;
		c.enterDur = rnd(0.5f, 0.8f);
		c.minHold = 1.0f;
		ids[i] = addElement(std::move(c)).id;
	}
	if (CollageElement * p = find(parentId)) startExit(*p, 0.35f);
	for (int i = 0; i < n; i++)
		enqueue(rnd(1.8f, 3.5f), CollageEvent::MoveResize, ids[i]); // children diversify out of the shared footprint
	touched_ = ids[0];
	return true;
}

bool BECollage::evMerge(uint32_t target) {
	if (fragmentCount() <= tuning_.minFragments) return false;
	std::vector<CollageElement *> c;
	c.reserve(16);
	for (auto & e : elements_)
		if (e.video && e.state == ElemState::Active && e.exitAt < 0.0f && !e.anchor && !e.circular && e.moveT >= 1.0f && e.cropT >= 1.0f
			&& time_ - e.born >= e.minHold)
			c.push_back(&e);
	if (c.size() < 2) return false;
	CollageElement *bestA = nullptr, *bestB = nullptr;
	float bestScore = 2.2f;
	ofRectangle bestU;
	for (size_t i = 0; i < c.size(); i++) {
		for (size_t j = i + 1; j < c.size(); j++) {
			if (target && c[i]->id != target && c[j]->id != target) continue;
			const ofRectangle u = c[i]->rect.getUnion(c[j]->rect);
			const float ua = areaOf(u), sa = areaOf(c[i]->rect) + areaOf(c[j]->rect);
			const float asp = u.width / std::max(1.0f, u.height);
			if (ua > 0.28f * W_ * H_ || asp < 0.35f || asp > 3.2f) continue;
			if (u.x < 0.3f * g_ || u.x + u.width > W_ - 0.3f * g_ || u.y < y0_ || u.y + u.height > y0_ + GRID_ROWS * g_) continue;
			if (!placementFree(u, c[i]->id, c[j]->id, 0.04f)) continue;
			const float score = ua / std::max(1.0f, sa);
			if (score < bestScore) {
				bestScore = score;
				bestA = c[i];
				bestB = c[j];
				bestU = u;
			}
		}
	}
	if (!bestA) return false;
	const uint32_t aId = bestA->id, bId = bestB->id;
	const int group = nextGroup_++;
	CollageElement m = makeVideoElement(areaOf(bestU) > 0.14f * W_ * H_ ? CollageRole::Hero : CollageRole::Secondary, bestU, true, false);
	if (m.role == CollageRole::Hero) m.zLayer = ++zCounter_;
	m.group = group;
	m.spawnAt = time_;
	m.enterDur = rnd(0.7f, 1.0f);
	touched_ = addElement(std::move(m)).id;
	if (CollageElement * a = find(aId)) startExit(*a, 0.5f);
	if (CollageElement * b = find(bId)) startExit(*b, 0.5f);
	return true;
}

bool BECollage::evBlockSlide(uint32_t target) {
	CollageElement * b = pickGraphic(target, [](const CollageElement & e) {
		return (e.type == CollageBlockType::SOLID_BLOCK || e.type == CollageBlockType::TRANSLUCENT_BLOCK || e.type == CollageBlockType::DIVIDER_BAR)
			&& e.moveT >= 1.0f;
	}, true);
	if (!b) return false;
	ofRectangle to = b->rect;
	const int mode = irnd(0, 2);
	const float d = static_cast<float>(irnd(1, 3)) * g_;
	const int side = irnd(0, 3);
	if (mode == 0) { // slide
		(side < 2 ? to.x : to.y) += (side % 2 ? d : -d);
	} else if (mode == 1) { // expand one edge
		switch (side) {
		case 0: to.x -= d; to.width += d; break;
		case 1: to.width += d; break;
		case 2: to.y -= d; to.height += d; break;
		default: to.height += d; break;
		}
	} else { // contract one edge
		switch (side) {
		case 0: to.x += d; to.width -= d; break;
		case 1: to.width -= d; break;
		case 2: to.y += d; to.height -= d; break;
		default: to.height -= d; break;
		}
	}
	if (to.width < 1.2f * g_ || to.height < 0.08f * g_ || (to.height < 1.2f * g_ && b->type != CollageBlockType::DIVIDER_BAR)) return false;
	if (to.x < -1.0f * g_ || to.y < -1.0f * g_ || to.x + to.width > W_ + g_ || to.y + to.height > H_ + g_) return false;
	if (areaOf(to) > 0.5f * W_ * H_) return false;
	b->rectFrom = b->rect;
	b->rectTo = to;
	b->moveT = 0.0f;
	b->moveDur = rnd(tuning_.moveDur) * 0.9f;
	if (b->type == CollageBlockType::TRANSLUCENT_BLOCK) b->alpha.set(rnd(0.5f, 1.0f), 1.0f);
	touched_ = b->id;
	return true;
}

bool BECollage::evMontage(uint32_t target) {
	CollageElement * f = pickFragment(target, true, false);
	if (!f) return false;
	const glm::vec3 fc = f->rect.getCenter();
	const uint32_t fid = f->id;
	std::vector<std::pair<float, uint32_t>> near;
	for (const auto & e : elements_) {
		if (!e.video || e.id == fid || e.state != ElemState::Active || e.exitAt >= 0.0f) continue;
		near.push_back({ glm::length(e.rect.getCenter() - fc), e.id });
	}
	std::sort(near.begin(), near.end());
	const size_t before = queue_.size();
	float off = 0.0f;
	enqueue(off, chance(0.5f) ? CollageEvent::CropSwap : CollageEvent::TransformChange, fid);
	for (size_t i = 0; i < std::min<size_t>(3, near.size()); i++) {
		off += rnd(0.35f, 0.7f); // staggered, related changes
		enqueue(off, (i % 2 == 0) ? CollageEvent::CropSwap : CollageEvent::TransformChange, near[i].second);
	}
	if (f->hasOverlay) enqueue(off + 0.3f, CollageEvent::OverprintFade, fid);
	touched_ = fid;
	return queue_.size() > before;
}

bool BECollage::evSwellMass(uint32_t target) {
	(void)target;
	CollageElement * mass = pickGraphic(0, [](const CollageElement & e) { return e.type == CollageBlockType::SOLID_BLOCK && e.pass == 0 && e.moveT >= 1.0f; }, false);
	if (mass) { // the color mass expands toward the nearest canvas edge
		ofRectangle to = mass->rect;
		const float d = static_cast<float>(irnd(2, 4)) * g_;
		if (to.getCenter().x < W_ * 0.5f) {
			to.x -= d;
			to.width += d;
		} else
			to.width += d;
		if (to.getCenter().y < H_ * 0.5f) {
			to.y -= d * 0.5f;
			to.height += d * 0.5f;
		} else
			to.height += d * 0.5f;
		if (areaOf(to) > 0.5f * W_ * H_) return false;
		mass->rectFrom = mass->rect;
		mass->rectTo = to;
		mass->moveT = 0.0f;
		mass->moveDur = 1.8f;
		touched_ = mass->id;
		return true;
	}
	if (graphicCount() >= tuning_.maxGraphics) return false;
	CollageElement e;
	e.video = false;
	e.type = CollageBlockType::SOLID_BLOCK;
	e.pass = 0;
	e.color = chance(0.6f) ? PaletteToken::Primary : PaletteToken::Secondary;
	const float w = std::round(rnd(6.0f, 9.0f)) * g_, h = std::round(rnd(4.0f, 6.0f)) * g_;
	const bool right = chance(0.5f);
	e.rect = ofRectangle(right ? W_ - w + 0.5f * g_ : -0.5f * g_, y0_ + std::round(rnd(0.0f, GRID_ROWS * g_ - h) / g_) * g_, w, h);
	e.revealDir = right ? RevealDir::RightToLeft : RevealDir::LeftToRight;
	e.enterDur = 1.4f;
	e.exitDur = rnd(tuning_.exitDur);
	e.minHold = tuning_.holdGraphicMin * 2.0f;
	e.spawnAt = time_;
	touched_ = addElement(std::move(e)).id;
	return true;
}

// ── stats / introspection ───────────────────────────────────────────────────

BECollage::FragmentStats BECollage::fragmentStats() const {
	FragmentStats st;
	if (!hasLayout_) return st;
	st.total = tuning_.maxFragments;
	for (const auto & e : elements_) {
		if (!e.video || !visOf(e).visible()) continue;
		st.visible++;
		(e.rect.getCenter().x < W_ * 0.5f ? st.left : st.right)++;
	}
	return st;
}

uint64_t BECollage::stateHash() const {
	uint64_t h = 1469598103934665603ULL;
	for (const auto & e : elements_) {
		h = hmix(h, e.id);
		h = hmix(h, static_cast<uint64_t>(e.state));
		h = hmix(h, static_cast<uint64_t>(q8(e.rect.x)));
		h = hmix(h, static_cast<uint64_t>(q8(e.rect.y)));
		h = hmix(h, static_cast<uint64_t>(q8(e.rect.width)));
		h = hmix(h, static_cast<uint64_t>(q8(e.rect.height)));
		h = hmix(h, static_cast<uint64_t>(q8(e.crop.center.x * 100)));
		h = hmix(h, static_cast<uint64_t>(e.crop.rot) * 4 + (e.crop.flipX ? 1 : 0) + (e.crop.flipY ? 2 : 0));
	}
	h = hmix(h, eventCount_);
	return h;
}

// ── drawing ─────────────────────────────────────────────────────────────────

ofRectangle BECollage::resolveSourceUv(const CollageCrop & c, const ofRectangle & dest, float ar) const {
	const float dw = std::max(dest.width, 1.0f), dh = std::max(dest.height, 1.0f);
	const float A = isOddTurn(c.rot) ? dh / dw : dw / dh;
	float full = std::min(1.0f, A / ar);
	float cwn = full * c.scale;
	float chn = cwn * ar / A;
	if (chn > 1.0f) {
		chn = 1.0f;
		cwn = chn * A / ar;
	}
	const float cx = ofClamp(c.center.x, cwn * 0.5f, 1.0f - cwn * 0.5f);
	const float cy = ofClamp(c.center.y, chn * 0.5f, 1.0f - chn * 0.5f);
	return ofRectangle(cx - cwn * 0.5f, cy - chn * 0.5f, cwn, chn);
}

void BECollage::drawFragmentQuad(const ofRectangle & d, const CollageCrop & c, const ofTexture * tex, RevealDir wipe, float wipeReveal, float alpha, bool circ) const {
	float s0 = 0, s1 = 1, t0 = 0, t1 = 1;
	if (!circ) {
		switch (wipe) {
		case RevealDir::LeftToRight: s1 = wipeReveal; break;
		case RevealDir::RightToLeft: s0 = 1 - wipeReveal; break;
		case RevealDir::TopToBottom: t1 = wipeReveal; break;
		case RevealDir::BottomToTop: t0 = 1 - wipeReveal; break;
		case RevealDir::None: break;
		}
	}
	alpha = ofClamp(alpha, 0.0f, 1.0f);
	if (tex == nullptr || !tex->isAllocated()) {
		ofSetColor(activePal_.dark, static_cast<int>(200 * alpha));
		ofFill();
		ofDrawRectangle(d.x + s0 * d.width, d.y + t0 * d.height, (s1 - s0) * d.width, (t1 - t0) * d.height);
		return;
	}
	const float tw = tex->getWidth(), th = tex->getHeight();
	const float ar = tw / std::max(th, 1.0f);
	const ofRectangle uv = resolveSourceUv(c, d, ar);
	const CollageUvQuad q = makeCollageUvQuad(uv.x, uv.y, uv.x + uv.width, uv.y + uv.height, c.rot, c.flipX, c.flipY);

	auto vert = [&](float s, float t) {
		mesh_.addVertex(glm::vec3(d.x + s * d.width, d.y + t * d.height, 0));
		const CollageVec2 u = bilerpCollageUv(q, s, t);
		mesh_.addTexCoord(tex->getCoordFromPoint(u.x * tw, u.y * th));
	};
	mesh_.clear();
	mesh_.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
	if (circ) {
		const int seg = tuning_.lowQuality ? 32 : 64;
		vert(0.5f, 0.5f);
		for (int i = 0; i <= seg; i++) {
			const float a = static_cast<float>(i) / seg * TWO_PI;
			vert(0.5f + 0.5f * std::cos(a), 0.5f + 0.5f * std::sin(a));
		}
	} else {
		if (s1 - s0 <= 0.0005f || t1 - t0 <= 0.0005f) return;
		vert(s0, t0);
		vert(s1, t0);
		vert(s1, t1);
		vert(s0, t1);
	}
	ofSetColor(255, 255, 255, static_cast<int>(255 * alpha));
	tex->bind();
	mesh_.draw();
	tex->unbind();
}

void BECollage::drawText(const std::string & s, float x, float y) const {
	if (fontLoaded_)
		font_.drawString(s, x, y);
	else
		ofDrawBitmapString(s, x, y);
}

void BECollage::drawLabelPlate(const ofRectangle & r0, const std::string & text, float alpha) const {
	ofRectangle r = r0;
	if (fontLoaded_)
		r.width = font_.getStringBoundingBox(text, 0, 0).width + 14.0f;
	else
		r.width = text.size() * 8.0f + 12.0f;
	hud::GraphicBlockStyle bs;
	bs.fill = activePal_.dark;
	bs.alpha = alpha;
	hud::GraphicBlock::draw(r, bs);
	if (alpha > 0.5f) {
		ofSetColor(activePal_.light, static_cast<int>(255 * alpha));
		const float ty = r.y + r.height * 0.5f + (fontLoaded_ ? font_.getSize() * 0.32f : 4.0f);
		drawText(text, r.x + 6.0f, ty);
	}
}

void BECollage::drawGraphic(const CollageElement & g, const Vis & v) const {
	const float a = g.alphaMax * g.alpha.cur * v.fade;
	const ofColor c = col(g.color);
	switch (g.type) {
	case CollageBlockType::SOLID_BLOCK:
	case CollageBlockType::TRANSLUCENT_BLOCK:
	case CollageBlockType::DIVIDER_BAR:
	case CollageBlockType::CORNER_TAB: {
		hud::GraphicBlockStyle s;
		s.fill = c;
		s.alpha = a;
		s.revealDir = g.revealDir;
		s.reveal = (g.revealDir == RevealDir::None) ? 1.0f : v.reveal;
		if (g.revealDir == RevealDir::None) s.alpha = a * v.reveal;
		hud::GraphicBlock::draw(g.rect, s);
		break;
	}
	case CollageBlockType::OUTLINE_FRAME:
	case CollageBlockType::PARTIAL_FRAME: {
		hud::GraphicFrameStyle s;
		s.mode = (g.type == CollageBlockType::PARTIAL_FRAME) ? hud::GraphicFrameStyle::Mode::CornersOnly : hud::GraphicFrameStyle::Mode::Full;
		s.color = c;
		s.alpha = a * v.reveal;
		s.strokeWidth = g.strokeWidth;
		s.cornerLength = g.rect.width * 0.08f + 14.0f * v.reveal;
		hud::GraphicFrame::draw(g.rect, s);
		break;
	}
	case CollageBlockType::GRID_BLOCK: {
		hud::TechnicalGridStyle s;
		s.majorSpacing = static_cast<float>(W_) / GRID_COLS;
		s.minorDivisions = 2;
		s.color = c;
		s.alpha = a;
		s.revealDir = (g.revealDir == RevealDir::None) ? RevealDir::LeftToRight : g.revealDir;
		s.reveal = v.reveal * g.ext.cur;
		hud::TechnicalGrid::draw(g.rect, s);
		break;
	}
	case CollageBlockType::TICK_STRIP: {
		hud::TickRailStyle s;
		s.vertical = g.vertical;
		s.color = c;
		s.alpha = a;
		s.minorSpacing = 9.0f;
		s.majorEvery = 5;
		s.reveal = v.reveal * g.ext.cur;
		hud::TickRail::draw(g.rect, s);
		break;
	}
	case CollageBlockType::DATA_STRIP: {
		hud::DataBarStyle s;
		s.vertical = g.vertical;
		s.segmented = g.segmented;
		s.segments = 14;
		s.value = ofClamp(g.value.cur, 0.0f, 1.0f);
		s.fill = c;
		s.track = (g.color == PaletteToken::Light) ? activePal_.light : activePal_.dark;
		s.alpha = a * v.reveal;
		hud::DataBar::draw(g.rect, s);
		break;
	}
	case CollageBlockType::CIRCLE_SAMPLE_BACKPLATE: {
		const glm::vec2 ctr(g.rect.getCenter().x, g.rect.getCenter().y);
		const float r = g.rect.width * 0.5f * v.reveal;
		if (r < 1.0f) break;
		ofPushStyle();
		ofSetCircleResolution(tuning_.lowQuality ? 36 : 72);
		ofSetColor(c, static_cast<int>(c.a * ofClamp(a, 0.0f, 1.0f)));
		if (g.filled) {
			ofFill();
			ofDrawCircle(ctr.x, ctr.y, r);
		} else {
			ofNoFill();
			ofSetLineWidth(g.strokeWidth);
			ofDrawCircle(ctr.x, ctr.y, r);
			ofSetLineWidth(1.0f);
		}
		ofPopStyle();
		break;
	}
	case CollageBlockType::REGISTRATION_MARK: {
		hud::RegistrationMarkStyle s;
		s.kind = static_cast<hud::RegistrationMarkStyle::Kind>(g.markKind);
		s.color = c;
		const float pulse = g.pulse ? (0.65f + 0.35f * std::sin(time_ * 3.0f)) : 1.0f;
		s.alpha = a * v.reveal * pulse;
		hud::RegistrationMark::draw(glm::vec2(g.rect.getCenter().x, g.rect.getCenter().y), g.rect.width * 0.5f * (1.0f + 0.4f * g.flash), s);
		break;
	}
	case CollageBlockType::INDEX_PLATE: {
		hud::GraphicBlockStyle s;
		s.fill = c;
		s.alpha = a;
		s.revealDir = RevealDir::LeftToRight;
		s.reveal = v.reveal;
		hud::GraphicBlock::draw(g.rect, s);
		if (v.reveal > 0.85f && !g.text.empty()) {
			ofSetColor(col(g.textColor), static_cast<int>(255 * ofClamp(a, 0.0f, 1.0f)));
			const float ty = g.rect.y + g.rect.height * 0.5f + (fontLoaded_ ? font_.getSize() * 0.32f : 4.0f);
			drawText(g.text, g.rect.x + 8.0f, ty);
		}
		break;
	}
	}
}

void BECollage::draw(const ofTexture * video) const {
	if (!hasLayout_) return;

	ofPushStyle();
	ofEnableAlphaBlending();

	// 1. paper
	ofFill();
	ofSetColor(activePal_.background);
	ofDrawRectangle(0, 0, W_, H_);

	auto graphicsPass = [&](int pass) {
		for (const auto & e : elements_) {
			if (e.video || e.pass != pass) continue;
			const Vis v = visOf(e);
			if (v.visible()) drawGraphic(e, v);
		}
	};

	// 2-3. color blocks, grids
	graphicsPass(0);
	graphicsPass(1);

	// 4. video fragments (one quad each; prior crop stays underneath a wipe swap)
	drawOrder_.clear();
	for (const auto & e : elements_)
		if (e.video && visOf(e).visible()) drawOrder_.push_back(&e);
	std::stable_sort(drawOrder_.begin(), drawOrder_.end(), [](const CollageElement * a, const CollageElement * b) { return a->zLayer < b->zLayer; });
	for (const CollageElement * ep : drawOrder_) {
		const CollageElement & f = *ep;
		const Vis v = visOf(f);
		const float alpha = f.circular ? v.fade * v.reveal : v.fade;
		if (f.cropT < 1.0f && f.state != ElemState::Entering) {
			drawFragmentQuad(f.rect, f.prevCrop, video, RevealDir::None, 1.0f, alpha, f.circular);
			drawFragmentQuad(f.rect, f.crop, video, f.cropWipe, easeOutCubic(f.cropT), alpha, f.circular);
		} else {
			drawFragmentQuad(f.rect, f.crop, video, f.revealDir, v.reveal, alpha, f.circular);
		}
	}

	// 5. fragment overlays, then free translucent blocks
	for (const CollageElement * ep : drawOrder_) {
		const CollageElement & f = *ep;
		if (!f.hasOverlay || f.overlay.cur < 0.001f) continue;
		const Vis v = visOf(f);
		const float gate = clamp01((v.reveal - 0.5f) / 0.5f);
		if (gate <= 0.0f) continue;
		ofRectangle o(f.rect.x + f.overlayFrac.x * f.rect.width, f.rect.y + f.overlayFrac.y * f.rect.height,
			f.overlayFrac.width * f.rect.width, f.overlayFrac.height * f.rect.height);
		if (f.overlayAxis == 0)
			o.width *= f.cover.cur;
		else
			o.height *= f.cover.cur;
		hud::GraphicBlockStyle s;
		s.fill = col(f.overlayColor);
		s.alpha = f.overlayAlphaMax * f.overlay.cur * v.fade * gate;
		hud::GraphicBlock::draw(o, s);
	}
	graphicsPass(3);

	// 6. fragment frames
	for (const CollageElement * ep : drawOrder_) {
		const CollageElement & f = *ep;
		const Vis v = visOf(f);
		if (f.circular) {
			ofPushStyle();
			ofSetCircleResolution(tuning_.lowQuality ? 36 : 72);
			ofNoFill();
			ofSetLineWidth(2.0f);
			ofSetColor(activePal_.dark, static_cast<int>(255 * v.fade * v.reveal));
			ofDrawCircle(f.rect.getCenter().x, f.rect.getCenter().y, f.rect.width * 0.5f);
			ofSetLineWidth(1.0f);
			ofPopStyle();
			continue;
		}
		if (f.frame.cur < 0.001f) continue;
		hud::GraphicFrameStyle s;
		s.color = col(static_cast<hud::PaletteToken>(f.paletteIndex));
		s.strokeWidth = f.frameWidth;
		s.outset = 0.0f;
		s.alpha = v.fade;
		s.reveal = std::min(f.frame.cur, clamp01((v.reveal - 0.3f) / 0.7f)); // frame draws on after the image
		hud::GraphicFrame::draw(f.rect, s);
	}

	// 7-8. small technical primitives, then labels
	graphicsPass(5);
	graphicsPass(6);
	for (const CollageElement * ep : drawOrder_) {
		const CollageElement & f = *ep;
		const Vis v = visOf(f);
		if (!f.showLabel || v.reveal < 0.9f) continue;
		const float ar = (video && video->isAllocated()) ? video->getWidth() / std::max(video->getHeight(), 1.0f) : GEN_ASPECT;
		const ofRectangle uv = resolveSourceUv(f.crop, f.rect, ar);
		char buf[48];
		snprintf(buf, sizeof(buf), "%s  %.2f/%.2f", f.labelTag.c_str(), uv.getCenter().x, uv.getCenter().y);
		const float plateH = 17.0f, plateW = 112.0f;
		float py = f.rect.y + f.rect.height + 3.0f;
		if (py + plateH > H_) py = f.rect.y - plateH - 3.0f;
		drawLabelPlate(ofRectangle(f.rect.x, py, plateW, plateH), buf, v.fade);
	}

	ofPopStyle();
}

void BECollage::debugDrawOrientationChart(const ofTexture * tex) const {
	ofPushStyle();
	ofEnableAlphaBlending();
	ofSetColor(236, 232, 220);
	ofFill();
	ofDrawRectangle(0, 0, W_, H_);
	const float tile = 150.0f, gap = 24.0f;
	const float x0 = (W_ - (4 * tile + 3 * gap)) * 0.5f, y0 = (H_ - (4 * tile + 3 * gap)) * 0.5f;
	static const char * turnNames[] = { "0", "90", "180", "270" };
	static const char * flipNames[] = { "none", "flipX", "flipY", "flipX+Y" };
	for (int r = 0; r < 4; r++)
		for (int c = 0; c < 4; c++) {
			CollageCrop crop;
			crop.center = glm::vec2(0.5f, 0.5f);
			crop.scale = 1.0f;
			crop.rot = static_cast<QuarterTurn>(r);
			crop.flipX = (c == 1 || c == 3);
			crop.flipY = (c == 2 || c == 3);
			const ofRectangle d(x0 + c * (tile + gap), y0 + r * (tile + gap), tile, tile);
			drawFragmentQuad(d, crop, tex, RevealDir::None, 1.0f, 1.0f, false);
			ofSetColor(30);
			ofDrawBitmapString(std::string(turnNames[r]) + " deg " + flipNames[c], d.x, d.y - 6.0f);
		}
	ofPopStyle();
}
