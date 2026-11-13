#include "BEContactSheet.h"

#include "ofGraphics.h"
#include "ofPolyline.h"

#include <algorithm>
#include <cmath>

using hud::PaletteToken;

namespace {
constexpr float MARGIN_X = 28.0f;
constexpr float MARGIN_TOP = 52.0f; // title strip
constexpr float MARGIN_BOTTOM = 28.0f;
constexpr float GUTTER = 14.0f;
constexpr float CAPTION_H = 22.0f;

float smooth01(float t) {
	t = ofClamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}
}

float BEContactSheet::rnd(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng_); }
int BEContactSheet::rndi(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng_); }

ofColor BEContactSheet::col(PaletteToken t) const {
	if (palMix_ >= 1.0f) return pal_.resolve(t);
	return prevPal_.resolve(t).getLerped(pal_.resolve(t), palMix_);
}

void BEContactSheet::setup(int canvasW, int canvasH, const std::string & fontPath, int fontSize) {
	canvasW_ = canvasW;
	canvasH_ = canvasH;
	fontLoaded_ = font_.load(fontPath, fontSize, true, true);
	pal_ = prevPal_ = hud::pickCollagePalette(1u);
}

ofRectangle BEContactSheet::cellRect(int i) const {
	const float w = (canvasW_ - 2.0f * MARGIN_X - (COLS - 1) * GUTTER) / COLS;
	const float h = (canvasH_ - MARGIN_TOP - MARGIN_BOTTOM - (ROWS - 1) * GUTTER) / ROWS;
	const int c = i % COLS, r = i / COLS;
	return ofRectangle(MARGIN_X + c * (w + GUTTER), MARGIN_TOP + r * (h + GUTTER), w, h - CAPTION_H);
}

BEContactSheet::Crop BEContactSheet::newCrop(int cell) {
	Crop c;
	c.zoom = rnd(1.0f, 2.6f);
	const float half = 0.5f / c.zoom;
	c.cx = rnd(half, 1.0f - half);
	c.cy = rnd(half, 1.0f - half);
	// Avoid repeating the cell's current centre (a visible change every time).
	if (cells_[cell].cur.zoom > 0.0f && std::fabs(c.cx - cells_[cell].cur.cx) < 0.12f && std::fabs(c.cy - cells_[cell].cur.cy) < 0.12f)
		c.cx = ofClamp(1.0f - c.cx, half, 1.0f - half);
	return c;
}

void BEContactSheet::beginLayout(uint32_t seed) {
	rng_.seed(seed);
	const bool first = !built_;
	prevPal_ = first ? hud::pickCollagePalette(seed) : pal_;
	pal_ = hud::pickCollagePalette(seed);
	palMix_ = first ? 1.0f : 0.0f;
	for (int i = 0; i < CELLS; i++) {
		Cell & c = cells_[i];
		c.prev = c.cur;
		c.cur = newCrop(i);
		c.mix = first ? 1.0f : 0.0f;
		if (first) {
			c.enter = 0.0f;
			c.enterDelay = i * 0.3f + rnd(0.0f, 0.3f);
		}
		c.selectTarget = c.rejectTarget = 0.0f;
		c.frameNo = rndi(1, 36);
		c.nextRecrop = rnd(2.0f, 7.0f) + (first ? 4.0f : 0.0f);
		c.tinted = rnd(0.0f, 1.0f) < 0.35f;
	}
	if (first)
		for (auto & f : fx_) {
			f = BEFxSlot();
			f.silence = rnd(2.0f, 8.0f);
		}
	selectTimer_ = rnd(5.0f, 9.0f);
	rejectTimer_ = rnd(9.0f, 15.0f);
	swapTimer_ = rnd(12.0f, 20.0f);
	sweepTimer_ = rnd(8.0f, 14.0f);
	renumberTimer_ = rnd(4.0f, 8.0f);
	sweepPos_ = -1.0f;
	built_ = true;
}

void BEContactSheet::directorTick(float dt) {
	// SELECT: circle one cell (and un-circle any previous pick).
	selectTimer_ -= dt;
	if (selectTimer_ <= 0.0f) {
		selectTimer_ = rnd(6.0f, 11.0f);
		for (auto & c : cells_) c.selectTarget = 0.0f;
		const int k = rndi(0, CELLS - 1);
		if (cells_[k].reject < 0.1f) {
			cells_[k].selectTarget = 1.0f;
			cells_[k].flash = 1.0f;
			eventCount_++;
		}
	}
	// REJECT: strike a cell, drain it, then refill with a fresh crop.
	rejectTimer_ -= dt;
	if (rejectTimer_ <= 0.0f) {
		rejectTimer_ = rnd(10.0f, 17.0f);
		const int k = rndi(0, CELLS - 1);
		if (cells_[k].selectTarget < 0.5f && cells_[k].rejectTarget < 0.5f) {
			cells_[k].rejectTarget = 1.0f;
			eventCount_++;
		}
	}
	// SWAP: two cells trade contents behind a cross-fade.
	swapTimer_ -= dt;
	if (swapTimer_ <= 0.0f) {
		swapTimer_ = rnd(13.0f, 22.0f);
		const int a = rndi(0, CELLS - 1);
		int b = rndi(0, CELLS - 1);
		if (b == a) b = (a + 1) % CELLS;
		std::swap(cells_[a].cur, cells_[b].cur);
		std::swap(cells_[a].frameNo, cells_[b].frameNo);
		cells_[a].prev = cells_[b].cur;
		cells_[b].prev = cells_[a].cur;
		cells_[a].mix = cells_[b].mix = 0.0f;
		cells_[a].flash = cells_[b].flash = 1.0f;
		eventCount_++;
	}
	// SWEEP: highlight passes across the sheet in reading order.
	if (sweepPos_ >= 0.0f) {
		const float prevPos = sweepPos_;
		sweepPos_ += dt * 5.0f;
		for (int i = 0; i < CELLS; i++)
			if (prevPos < i + 0.5f && sweepPos_ >= i + 0.5f) cells_[i].flash = 1.0f;
		if (sweepPos_ > CELLS + 1) sweepPos_ = -1.0f;
	} else {
		sweepTimer_ -= dt;
		if (sweepTimer_ <= 0.0f) {
			sweepTimer_ = rnd(14.0f, 24.0f);
			sweepPos_ = 0.0f;
			eventCount_++;
		}
	}
	// RENUMBER: frame labels advance in a burst.
	renumberTimer_ -= dt;
	if (renumberTimer_ <= 0.0f) {
		renumberTimer_ = rnd(4.0f, 9.0f);
		for (auto & c : cells_) c.frameNo = (c.frameNo + rndi(1, 6) - 1) % 36 + 1;
		frameCounter_ += rndi(1, 5);
		eventCount_++;
	}
}

void BEContactSheet::update(float dt) {
	if (!built_) return;
	dt = ofClamp(dt, 0.0f, 0.25f);
	time_ += dt;
	if (palMix_ < 1.0f) palMix_ = std::min(1.0f, palMix_ + dt / 1.5f);
	directorTick(dt);
	stepFxSlots(fx_, CELLS, dt, rng_, shaderLib_, 2);

	for (int i = 0; i < CELLS; i++) {
		Cell & c = cells_[i];
		if (c.enterDelay > 0.0f)
			c.enterDelay -= dt;
		else
			c.enter = std::min(1.0f, c.enter + dt / 0.9f);
		c.flash = std::max(0.0f, c.flash - dt / 0.8f);
		if (c.mix < 1.0f) c.mix = std::min(1.0f, c.mix + dt / 1.2f);
		c.select += (c.selectTarget - c.select) * std::min(1.0f, dt * 3.0f);
		// Reject drains in, then once fully drained refills with a new crop.
		c.reject += (c.rejectTarget - c.reject) * std::min(1.0f, dt * 2.2f);
		if (c.rejectTarget > 0.5f && c.reject > 0.97f) {
			c.prev = c.cur;
			c.cur = newCrop(i);
			c.mix = 0.0f;
			c.rejectTarget = 0.0f;
			c.frameNo = rndi(1, 36);
		}
		c.nextRecrop -= dt;
		if (c.nextRecrop <= 0.0f) {
			c.nextRecrop = rnd(4.0f, 10.0f);
			if (c.rejectTarget < 0.5f && c.enter >= 1.0f) {
				c.prev = c.cur;
				c.cur = newCrop(i);
				c.mix = 0.0f;
				eventCount_++;
			}
		}
	}
}

void BEContactSheet::drawText(const std::string & s, float x, float y) const {
	if (fontLoaded_)
		font_.drawString(s, x, y);
	else
		ofDrawBitmapString(s, x, y);
}

// Source pixel rect for a crop: destination aspect, zoomed in by c.zoom.
ofRectangle BEContactSheet::cropSource(const ofTexture * v, const ofRectangle & r, const Crop & c) const {
	const float tw = v->getWidth(), th = v->getHeight();
	const float dAr = r.width / std::max(r.height, 1.0f);
	float sh = th / c.zoom, sw = sh * dAr;
	if (sw > tw) {
		sw = tw;
		sh = sw / dAr;
	}
	return ofRectangle(ofClamp(c.cx * tw - sw * 0.5f, 0.0f, tw - sw), ofClamp(c.cy * th - sh * 0.5f, 0.0f, th - sh), sw, sh);
}

void BEContactSheet::drawCrop(const ofTexture * v, const ofRectangle & r, const Crop & c, float alpha) const {
	if (alpha <= 0.002f) return;
	if (v == nullptr || !v->isAllocated()) {
		ofSetColor(col(PaletteToken::Dark), static_cast<int>(200 * alpha));
		ofFill();
		ofDrawRectangle(r);
		return;
	}
	const ofRectangle s = cropSource(v, r, c);
	ofSetColor(255, 255, 255, static_cast<int>(255 * alpha));
	v->drawSubsection(r.x, r.y, r.width, r.height, s.x, s.y, s.width, s.height);
}

void BEContactSheet::draw(const ofTexture * video) const {
	if (!built_) return;
	ofPushStyle();
	ofEnableAlphaBlending();
	ofFill();
	ofSetColor(col(PaletteToken::Background));
	ofDrawRectangle(0, 0, canvasW_, canvasH_);

	hud::TechnicalGridStyle grid;
	grid.color = col(PaletteToken::Dark);
	grid.majorSpacing = 96.0f;
	grid.majorAlpha = 0.12f;
	grid.minorAlpha = 0.05f;
	hud::TechnicalGrid::draw(ofRectangle(0, 0, canvasW_, canvasH_), grid);

	// Title strip: tick rail + sheet id.
	hud::TickRailStyle rail;
	rail.color = col(PaletteToken::Dark);
	rail.alpha = 0.7f;
	rail.minorSpacing = 10.0f;
	hud::TickRail::draw(ofRectangle(MARGIN_X, 30.0f, canvasW_ - 2.0f * MARGIN_X, 10.0f), rail);
	char buf[64];
	ofSetColor(col(PaletteToken::Dark));
	std::snprintf(buf, sizeof(buf), "PROOF SHEET  %04d", frameCounter_);
	drawText(buf, MARGIN_X, 22.0f);

	// Draw lifted (selected) cells last so they sit above their neighbours.
	std::array<int, CELLS> order;
	for (int i = 0; i < CELLS; i++) order[i] = i;
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return cells_[a].select < cells_[b].select; });

	for (int idx : order) {
		const Cell & c = cells_[idx];
		const float e = smooth01(c.enter);
		if (e <= 0.002f) continue;
		const float lift = smooth01(c.select);
		ofRectangle r = cellRect(idx);
		const float grow = 1.0f + 0.06f * lift;
		const glm::vec2 ctr(r.x + r.width * 0.5f, r.y + r.height * 0.5f);
		r = ofRectangle(ctr.x - r.width * grow * 0.5f, ctr.y - r.height * grow * 0.5f, r.width * grow, r.height * grow);

		// Mounting plate behind the frame.
		hud::GraphicBlockStyle plate;
		plate.fill = col(PaletteToken::Dark);
		plate.alpha = 0.10f * e;
		hud::GraphicBlock::draw(ofRectangle(r.x - 6.0f, r.y - 6.0f, r.width + 12.0f, r.height + 12.0f + CAPTION_H * 0.6f), plate);

		// Content: previous crop under the new one for the cross-fade.
		const float drain = 1.0f - 0.75f * smooth01(c.reject);
		if (c.mix < 1.0f) drawCrop(video, r, c.prev, e * drain);
		drawCrop(video, r, c.cur, e * drain * smooth01(c.mix));
		if (video != nullptr && video->isAllocated()) // shared shader effect over the current crop
			drawEffectOverlay(video, cropSource(video, r, c.cur), r, fx_[idx], e * drain);

		if (c.tinted) {
			hud::GraphicBlockStyle t;
			t.fill = col(PaletteToken::Primary);
			t.alpha = 0.2f * e;
			hud::GraphicBlock::draw(r, t);
		}
		if (c.reject > 0.02f) {
			hud::GraphicBlockStyle paper;
			paper.fill = col(PaletteToken::Background);
			paper.alpha = 0.55f * smooth01(c.reject);
			hud::GraphicBlock::draw(r, paper);
		}
		if (c.flash > 0.01f) {
			hud::GraphicBlockStyle f;
			f.fill = col(PaletteToken::Light);
			f.alpha = 0.35f * c.flash * e;
			hud::GraphicBlock::draw(r, f);
		}

		// Frame: full when selected, corner ticks otherwise.
		hud::GraphicFrameStyle fs;
		fs.color = col(lift > 0.5f ? PaletteToken::Primary : PaletteToken::Dark);
		fs.alpha = (0.55f + 0.45f * lift) * e;
		fs.strokeWidth = 1.5f + 1.5f * lift;
		fs.mode = lift > 0.3f ? hud::GraphicFrameStyle::Mode::Full : hud::GraphicFrameStyle::Mode::CornersOnly;
		fs.cornerLength = 18.0f;
		fs.reveal = e;
		hud::GraphicFrame::draw(r, fs);

		// Caption: frame number left, small data bar right.
		ofSetColor(col(PaletteToken::Dark), static_cast<int>(255 * e));
		std::snprintf(buf, sizeof(buf), "F%02d  X%.1f", c.frameNo, c.cur.zoom);
		drawText(buf, r.x, r.getBottom() + 15.0f);
		hud::DataBarStyle bar;
		bar.fill = col(PaletteToken::Dark);
		bar.track = col(PaletteToken::Dark);
		bar.value = ofClamp((c.cur.zoom - 1.0f) / 1.6f, 0.0f, 1.0f);
		bar.alpha = e;
		hud::DataBar::draw(ofRectangle(r.getRight() - 70.0f, r.getBottom() + 8.0f, 70.0f, 4.0f), bar);

		// SELECT: a hand-drawn-feeling ellipse grease mark.
		if (c.select > 0.02f) {
			ofSetColor(col(PaletteToken::Secondary), static_cast<int>(230 * smooth01(c.select)));
			ofNoFill();
			ofSetLineWidth(3.0f);
			ofPolyline ring;
			const int n = 56;
			const float sweep = TWO_PI * (0.18f + 0.82f * smooth01(c.select)) * 1.05f;
			for (int k = 0; k <= n; k++) {
				const float a = -HALF_PI + sweep * k / n;
				const float wob = 1.0f + 0.03f * std::sin(a * 3.0f + idx);
				ring.addVertex(ctr.x + std::cos(a) * r.width * 0.52f * wob, ctr.y + std::sin(a) * r.height * 0.54f * wob);
			}
			ring.draw();
			ofFill();
		}
		// REJECT: X strike across the cell.
		if (c.reject > 0.05f) {
			const float k = smooth01(c.reject);
			ofSetColor(col(PaletteToken::Dark), static_cast<int>(230 * k));
			ofSetLineWidth(3.0f);
			ofDrawLine(r.x, r.y, r.x + r.width * std::min(1.0f, k * 1.6f), r.y + r.height * std::min(1.0f, k * 1.6f));
			if (k > 0.45f) {
				const float k2 = std::min(1.0f, (k - 0.45f) * 1.8f);
				ofDrawLine(r.getRight(), r.y, r.getRight() - r.width * k2, r.y + r.height * k2);
			}
		}
	}

	// Registration marks at the sheet corners.
	hud::RegistrationMarkStyle rm;
	rm.kind = hud::RegistrationMarkStyle::Kind::Cross;
	rm.color = col(PaletteToken::Dark);
	rm.alpha = 0.7f;
	hud::RegistrationMark::draw(glm::vec2(12.0f, 12.0f), 6.0f, rm);
	hud::RegistrationMark::draw(glm::vec2(canvasW_ - 12.0f, 12.0f), 6.0f, rm);
	hud::RegistrationMark::draw(glm::vec2(12.0f, canvasH_ - 12.0f), 6.0f, rm);
	hud::RegistrationMark::draw(glm::vec2(canvasW_ - 12.0f, canvasH_ - 12.0f), 6.0f, rm);

	ofPopStyle();
}

GrammarStats BEContactSheet::stats() const {
	GrammarStats st;
	st.total = CELLS;
	for (int i = 0; i < CELLS; i++) {
		if (cells_[i].enter < 0.5f || cells_[i].reject > 0.5f) continue;
		st.visible++;
		const int col = i % COLS;
		if (col == 0 || (col == 1 && i / COLS == 0))
			st.left++;
		else
			st.right++;
	}
	return st;
}
