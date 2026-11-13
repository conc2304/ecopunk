#include "BEExplodedView.h"

#include "ofGraphics.h"

#include <algorithm>
#include <cmath>
#include <numeric>

using hud::PaletteToken;

namespace {
constexpr float GAP = 34.0f; // spread per part index at full explosion (rows/cols)
constexpr float SHEAR = 26.0f; // oblique component of the explosion axis
constexpr float PULL_PX = 84.0f; // a pulled-out part's extra travel
constexpr float GRID_SPREAD_X = 52.0f;
constexpr float GRID_SPREAD_Y = 40.0f;
const char * const NAMES[BEExplodedView::MAX_PARTS] = { "OUTER SKIN", "PLATE A", "CORE", "PLATE B", "BASE", "MOUNT" };

float smooth01(float t) {
	t = ofClamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}
}

float BEExplodedView::rnd(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng_); }

ofColor BEExplodedView::col(PaletteToken t) const {
	if (palMix_ >= 1.0f) return pal_.resolve(t);
	return prevPal_.resolve(t).getLerped(pal_.resolve(t), palMix_);
}

void BEExplodedView::setup(int canvasW, int canvasH, const std::string & fontPath, int fontSize) {
	canvasW_ = canvasW;
	canvasH_ = canvasH;
	fontLoaded_ = font_.load(fontPath, fontSize, true, true);
	pal_ = prevPal_ = hud::pickCollagePalette(1u);
}

void BEExplodedView::drawText(const std::string & s, float x, float y) const {
	if (fontLoaded_)
		font_.drawString(s, x, y);
	else
		ofDrawBitmapString(s, x, y);
}

float BEExplodedView::textWidth(const std::string & s) const {
	return fontLoaded_ ? font_.getStringBoundingBox(s, 0, 0).width : 8.0f * s.size();
}

BEExplodedView::Crop BEExplodedView::newCrop() {
	Crop c;
	c.zoom = rnd(1.0f, 2.2f);
	const float half = 0.5f / c.zoom;
	c.cx = rnd(half, 1.0f - half);
	c.cy = rnd(half, 1.0f - half);
	return c;
}

// Chooses the next layout (never the same kind twice in a row).
void BEExplodedView::pickLayout() {
	const Kind prevKind = lay_.kind;
	struct W {
		Kind k;
		float w;
	};
	W table[] = { { Kind::Rows, 0.26f }, { Kind::Cols, 0.16f }, { Kind::Grid22, 0.32f }, { Kind::Grid23, 0.26f } };
	float total = 0.0f;
	for (auto & t : table)
		if (!(haveLayout_ && t.k == prevKind)) total += t.w;
	float r = rnd(0.0f, total);
	Kind k = Kind::Rows;
	for (auto & t : table) {
		if (haveLayout_ && t.k == prevKind) continue;
		k = t.k;
		if ((r -= t.w) <= 0.0f) break;
	}
	Layout L;
	L.kind = k;
	L.n = (k == Kind::Rows || k == Kind::Cols) ? 5 : (k == Kind::Grid22 ? 4 : 6);
	L.portrait = (k != Kind::Cols) && rnd(0.0f, 1.0f) < 0.35f;
	const bool grid = (k == Kind::Grid22 || k == Kind::Grid23);
	const float a = rnd(0.0f, 1.0f);
	if (grid) {
		// Grids favour the centre with labels on BOTH sides.
		if (a < 0.6f) {
			L.anchor = Anchor::Center;
			L.bothSides = true;
		} else {
			L.anchor = a < 0.8f ? Anchor::Left : Anchor::Right;
		}
	} else {
		L.anchor = a < 0.3f ? Anchor::Left : (a < 0.6f ? Anchor::Right : Anchor::Center);
	}
	// Single-sided labels sit opposite the hero; a centred hero picks a side.
	if (!L.bothSides) L.labelsLeft = (L.anchor == Anchor::Right) || (L.anchor == Anchor::Center && rnd(0.0f, 1.0f) < 0.5f);
	lay_ = L;
	haveLayout_ = true;
}

ofRectangle BEExplodedView::heroRect() const {
	float w, h;
	if (lay_.portrait) {
		w = canvasW_ * 0.22f;
		h = canvasH_ * 0.58f;
	} else {
		w = canvasW_ * (lay_.bothSides ? 0.38f : 0.42f);
		h = canvasH_ * (lay_.bothSides ? 0.44f : 0.46f);
	}
	const float side = (lay_.kind == Kind::Cols ? 150.0f : 120.0f); // room for the spread
	float cx;
	switch (lay_.anchor) {
	case Anchor::Left: cx = w * 0.5f + side + 20.0f; break;
	case Anchor::Right: cx = canvasW_ - w * 0.5f - side - 20.0f; break;
	default: cx = lay_.bothSides ? canvasW_ * 0.5f : canvasW_ * (lay_.labelsLeft ? 0.58f : 0.42f); break;
	}
	return ofRectangle(cx - w * 0.5f, canvasH_ * 0.56f - h * 0.5f, w, h);
}

ofRectangle BEExplodedView::sourceRect(const ofTexture * v, const Crop & c, const ofRectangle & hero) const {
	const float tw = v->getWidth(), th = v->getHeight();
	const float dAr = hero.width / std::max(hero.height, 1.0f);
	float sh = th / c.zoom, sw = sh * dAr;
	if (sw > tw) {
		sw = tw;
		sh = sw / dAr;
	}
	return ofRectangle(ofClamp(c.cx * tw - sw * 0.5f, 0.0f, tw - sw), ofClamp(c.cy * th - sh * 0.5f, 0.0f, th - sh), sw, sh);
}

ofRectangle BEExplodedView::partBase(int i, const ofRectangle & hero) const {
	switch (lay_.kind) {
	case Kind::Rows: return ofRectangle(hero.x, hero.y + i * hero.height / lay_.n, hero.width, hero.height / lay_.n);
	case Kind::Cols: return ofRectangle(hero.x + i * hero.width / lay_.n, hero.y, hero.width / lay_.n, hero.height);
	default: {
		const int rows = gridRows();
		const int c = i % 2, r = i / 2;
		return ofRectangle(hero.x + c * hero.width / 2.0f, hero.y + r * hero.height / rows, hero.width / 2.0f, hero.height / rows);
	}
	}
}

// The part's portion of the hero crop (position-consistent with the assembled image).
ofRectangle BEExplodedView::partSource(const ofTexture * v, const Crop & c, const ofRectangle & hero, int i) const {
	const ofRectangle src = sourceRect(v, c, hero);
	const ofRectangle b = partBase(i, hero);
	return ofRectangle(src.x + (b.x - hero.x) / hero.width * src.width, src.y + (b.y - hero.y) / hero.height * src.height,
		b.width / hero.width * src.width, b.height / hero.height * src.height);
}

glm::vec2 BEExplodedView::partOffset(int i) const {
	const float e = expl_;
	if (isGrid()) {
		const int rows = gridRows();
		const int c = i % 2, r = i / 2;
		const float rk = r - (rows - 1) * 0.5f;
		const float sx = (c == 0 ? -1.0f : 1.0f);
		return glm::vec2(sx * GRID_SPREAD_X * e + rk * SHEAR * 0.8f * shearSign_ * e + sx * pull_[i] * PULL_PX,
			rk * GRID_SPREAD_Y * 2.0f * e);
	}
	const float k = i - (lay_.n - 1) * 0.5f;
	if (lay_.kind == Kind::Rows)
		return glm::vec2(k * SHEAR * shearSign_ * e + pull_[i] * PULL_PX, k * GAP * e);
	return glm::vec2(k * GAP * 1.35f * e, k * SHEAR * 0.7f * shearSign_ * e + pull_[i] * PULL_PX * 0.7f);
}

bool BEExplodedView::partOnLeft(int i) const {
	if (lay_.bothSides) return (i % 2) == 0;
	return lay_.labelsLeft;
}

void BEExplodedView::beginLayout(uint32_t seed) {
	rng_.seed(seed);
	const bool first = !built_;
	prevPal_ = first ? hud::pickCollagePalette(seed) : pal_;
	pal_ = hud::pickCollagePalette(seed);
	palMix_ = first ? 1.0f : 0.0f;
	if (first) {
		enter_ = 0.0f;
		expl_ = 0.0f;
		phase_ = Phase::Assembled;
		timer_ = 2.0f;
		haveLayout_ = false;
		pickLayout();
		shearSign_ = rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
		cur_ = prev_ = newCrop();
		mix_ = 1.0f;
		for (int i = 0; i < MAX_PARTS; i++) {
			pull_[i] = pullTarget_[i] = pullHold_[i] = flash_[i] = 0.0f;
			fx_[i] = BEFxSlot();
			fx_[i].silence = rnd(2.0f, 8.0f);
		}
	} else {
		// Recompose: collapse the assembly, then rebuild in a new layout.
		phase_ = Phase::Collapsing;
		progress_ = std::min(1.0f, std::max(progress_, expl_));
	}
	nextPull_ = rnd(2.5f, 4.5f);
	built_ = true;
}

void BEExplodedView::update(float dt) {
	if (!built_) return;
	dt = ofClamp(dt, 0.0f, 0.25f);
	time_ += dt;
	enter_ = std::min(1.0f, enter_ + dt / 1.2f);
	if (palMix_ < 1.0f) palMix_ = std::min(1.0f, palMix_ + dt / 1.5f);
	if (mix_ < 1.0f) mix_ = std::min(1.0f, mix_ + dt / 1.4f);

	switch (phase_) {
	case Phase::Assembled:
		expl_ = 0.0f;
		timer_ -= dt;
		if (timer_ <= 0.0f) {
			phase_ = Phase::Exploding;
			progress_ = 0.0f;
			eventCount_++;
		}
		break;
	case Phase::Exploding:
		progress_ = std::min(1.0f, progress_ + dt / 2.6f);
		expl_ = smooth01(progress_);
		if (progress_ >= 1.0f) {
			phase_ = Phase::Held;
			timer_ = rnd(8.0f, 14.0f);
		}
		break;
	case Phase::Held:
		expl_ = 1.0f + 0.05f * std::sin(time_ * 0.9f);
		timer_ -= dt;
		nextPull_ -= dt;
		if (nextPull_ <= 0.0f) {
			nextPull_ = rnd(2.2f, 4.0f);
			const int k = std::uniform_int_distribution<int>(0, lay_.n - 1)(rng_);
			if (pullHold_[k] <= 0.0f) {
				pullTarget_[k] = (rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f) * rnd(0.7f, 1.0f);
				pullHold_[k] = rnd(2.0f, 3.2f);
				flash_[k] = 1.0f;
				eventCount_++;
			}
		}
		if (timer_ <= 0.0f) {
			phase_ = Phase::Collapsing;
			progress_ = 1.0f;
		}
		break;
	case Phase::Collapsing:
		progress_ = std::max(0.0f, progress_ - dt / 2.0f);
		expl_ = smooth01(progress_);
		for (int i = 0; i < MAX_PARTS; i++) pullTarget_[i] = 0.0f, pullHold_[i] = 0.0f;
		if (progress_ <= 0.0f) {
			// Rebuilt: new content and a new layout (anchor, kind, format, labels).
			prev_ = cur_;
			cur_ = newCrop();
			mix_ = 0.0f;
			pickLayout();
			shearSign_ = rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
			enter_ = 0.0f;
			assy_++;
			phase_ = Phase::Assembled;
			timer_ = rnd(1.5f, 3.0f);
			eventCount_++;
		}
		break;
	}

	for (int i = 0; i < MAX_PARTS; i++) {
		if (pullHold_[i] > 0.0f) {
			pullHold_[i] -= dt;
			if (pullHold_[i] <= 0.0f) pullTarget_[i] = 0.0f;
		}
		pull_[i] += (pullTarget_[i] - pull_[i]) * std::min(1.0f, dt * 3.0f);
		flash_[i] = std::max(0.0f, flash_[i] - dt / 1.4f);
	}
	// Shared shader effects on the parts (at most two at a time).
	stepFxSlots(fx_, lay_.n, dt, rng_, shaderLib_, 2);
}

void BEExplodedView::draw(const ofTexture * video) const {
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

	// Global fade: in on (re)build, out over the last part of a collapse.
	float en = smooth01(enter_);
	if (phase_ == Phase::Collapsing) en *= smooth01(progress_ / 0.25f);
	const float ex01 = smooth01(std::min(expl_, 1.0f));
	const ofRectangle hero = heroRect();
	const int n = lay_.n;
	char buf[80];

	// Title strip.
	hud::TickRailStyle rail;
	rail.color = col(PaletteToken::Dark);
	rail.alpha = 0.7f * en;
	rail.minorSpacing = 10.0f;
	hud::TickRail::draw(ofRectangle(28.0f, 30.0f, canvasW_ - 56.0f, 10.0f), rail);
	ofSetColor(col(PaletteToken::Dark), static_cast<int>(255 * en));
	std::snprintf(buf, sizeof(buf), "EXPLODED VIEW  ASSY-%03d", assy_);
	drawText(buf, 28.0f, 22.0f);
	std::snprintf(buf, sizeof(buf), "EXPLOSION %.2f", expl_);
	drawText(buf, canvasW_ - 28.0f - 140.0f, 22.0f);

	// Part geometry (current offsets).
	std::array<ofRectangle, MAX_PARTS> rect;
	std::array<glm::vec2, MAX_PARTS> ctr, ctr0;
	for (int i = 0; i < n; i++) {
		const ofRectangle b = partBase(i, hero);
		const glm::vec2 o = partOffset(i);
		rect[i] = ofRectangle(b.x + o.x, b.y + o.y, b.width, b.height);
		ctr[i] = glm::vec2(rect[i].x + rect[i].width * 0.5f, rect[i].y + rect[i].height * 0.5f);
		ctr0[i] = glm::vec2(b.x + b.width * 0.5f, b.y + b.height * 0.5f);
	}
	const glm::vec2 heroC(hero.x + hero.width * 0.5f, hero.y + hero.height * 0.5f);

	// Dashed explosion axes: one through the centres (rows/cols) or radial (grids).
	if (ex01 > 0.03f) {
		ofSetColor(col(PaletteToken::Dark), static_cast<int>(150 * ex01 * en));
		ofSetLineWidth(1.2f);
		auto dashed = [&](const glm::vec2 & a, const glm::vec2 & b) {
			const float total = glm::length(b - a);
			if (total < 1.0f) return;
			const glm::vec2 d = (b - a) / total;
			for (float s = 0.0f; s < total; s += 14.0f) {
				const glm::vec2 p0 = a + d * s, p1 = a + d * std::min(s + 8.0f, total);
				ofDrawLine(p0.x, p0.y, p1.x, p1.y);
			}
		};
		if (isGrid()) {
			for (int i = 0; i < n; i++) {
				glm::vec2 d = ctr[i] - heroC;
				const float len = std::max(glm::length(d), 1.0f);
				dashed(heroC, ctr[i] + d / len * 36.0f);
			}
		} else {
			glm::vec2 d = ctr[n - 1] - ctr[0];
			const float len = std::max(glm::length(d), 1.0f);
			d /= len;
			dashed(ctr[0] - d * 60.0f, ctr[n - 1] + d * 60.0f);
		}
	}

	// Parts, back to front.
	const bool haveVideo = video != nullptr && video->isAllocated();
	for (int i = 0; i < n; i++) {
		const ofRectangle & r = rect[i];
		hud::GraphicBlockStyle sh;
		sh.fill = col(PaletteToken::Dark);
		sh.alpha = 0.14f * ex01 * en;
		hud::GraphicBlock::draw(ofRectangle(r.x + 9.0f, r.y + 9.0f, r.width, r.height), sh);

		if (haveVideo) {
			auto drawPart = [&](const Crop & c, float alpha) {
				if (alpha <= 0.002f) return;
				const ofRectangle ps = partSource(video, c, hero, i);
				ofSetColor(255, 255, 255, static_cast<int>(255 * alpha * en));
				video->drawSubsection(r.x, r.y, r.width, r.height, ps.x, ps.y, ps.width, ps.height);
			};
			if (mix_ < 1.0f) drawPart(prev_, 1.0f);
			drawPart(cur_, smooth01(mix_));
			// Shared shader effect over the part's current crop.
			drawEffectOverlay(video, partSource(video, cur_, hero, i), r, fx_[i], en);
		} else {
			ofSetColor(col(PaletteToken::Dark), static_cast<int>(200 * en));
			ofDrawRectangle(r);
		}

		hud::GraphicBlockStyle tint;
		tint.fill = col(i % 2 == 0 ? PaletteToken::Primary : PaletteToken::Secondary);
		tint.alpha = (0.10f + 0.16f * ex01 + 0.35f * flash_[i]) * en;
		hud::GraphicBlock::draw(r, tint);

		hud::GraphicFrameStyle fs;
		fs.color = col(PaletteToken::Dark);
		fs.alpha = (0.45f + 0.4f * ex01 + 0.15f * flash_[i]) * en;
		fs.strokeWidth = 1.4f + 1.2f * flash_[i];
		hud::GraphicFrame::draw(r, fs);
	}

	// Callouts: each side lists its parts top-to-bottom (sorted by part y), so
	// leaders rarely cross. Bubble + label sit on the side the part belongs to.
	const float top = canvasH_ * 0.28f, bottom = canvasH_ * 0.86f;
	const float rightX = canvasW_ * 0.80f, leftX = canvasW_ * 0.20f;
	for (int sideIsLeft = 0; sideIsLeft < 2; sideIsLeft++) {
		std::array<int, MAX_PARTS> idx;
		int m = 0;
		for (int i = 0; i < n; i++)
			if (partOnLeft(i) == (sideIsLeft == 1)) idx[m++] = i;
		std::sort(idx.begin(), idx.begin() + m, [&](int a, int b) { return ctr[a].y < ctr[b].y; });
		for (int k = 0; k < m; k++) {
			const int i = idx[k];
			const float cy = top + (bottom - top) * (m == 1 ? 0.5f : static_cast<float>(k) / (m - 1));
			const float la = ex01 * en;
			const bool left = (sideIsLeft == 1);
			const float bx = left ? leftX - 12.0f : rightX + 12.0f; // bubble centre
			const glm::vec2 a(left ? rect[i].x : rect[i].getRight(), rect[i].y + rect[i].height * 0.5f);
			const glm::vec2 e(left ? bx + 46.0f : bx - 46.0f, cy);
			ofSetColor(col(PaletteToken::Dark), static_cast<int>(200 * la));
			ofSetLineWidth(1.2f);
			ofDrawLine(a.x, a.y, e.x, e.y);
			ofDrawLine(e.x, e.y, bx + (left ? 12.0f : -12.0f), cy);
			hud::RegistrationMarkStyle dot;
			dot.kind = hud::RegistrationMarkStyle::Kind::TargetDot;
			dot.color = col(PaletteToken::Dark);
			dot.alpha = la;
			hud::RegistrationMark::draw(a, 3.5f, dot);

			ofSetColor(col(i % 2 == 0 ? PaletteToken::Primary : PaletteToken::Secondary), static_cast<int>(255 * la));
			ofFill();
			ofDrawCircle(bx, cy, 12.0f + 2.0f * flash_[i]);
			ofSetColor(col(PaletteToken::Light), static_cast<int>(255 * la));
			std::snprintf(buf, sizeof(buf), "%d", i + 1);
			drawText(buf, bx - 4.0f, cy + 4.0f);

			const std::string name = NAMES[(i + assy_) % MAX_PARTS];
			const float tw = textWidth(name);
			const float tx = left ? bx - 20.0f - tw : bx + 20.0f;
			ofSetColor(col(PaletteToken::Dark), static_cast<int>(255 * la));
			drawText(name, tx, cy + 4.0f);
			hud::DataBarStyle bar;
			bar.fill = col(PaletteToken::Dark);
			bar.track = col(PaletteToken::Dark);
			bar.value = ofClamp(glm::length(partOffset(i)) / 120.0f, 0.0f, 1.0f);
			bar.alpha = la;
			const float bw = 96.0f;
			hud::DataBar::draw(ofRectangle(left ? bx - 20.0f - bw : bx + 20.0f, cy + 9.0f, bw, 3.0f), bar);
		}
	}

	// Dimension bracket along the spread, on the side opposite the labels (single-sided only).
	if (ex01 > 0.05f && !lay_.bothSides) {
		float y0 = 1e9f, y1 = -1e9f, xmin = 1e9f, xmax = -1e9f;
		for (int i = 0; i < n; i++) {
			y0 = std::min(y0, rect[i].y);
			y1 = std::max(y1, rect[i].getBottom());
			xmin = std::min(xmin, rect[i].x);
			xmax = std::max(xmax, rect[i].getRight());
		}
		const float x = lay_.labelsLeft ? std::min(canvasW_ - 60.0f, xmax + 34.0f) : std::max(60.0f, xmin - 34.0f);
		const float la = ex01 * en;
		ofSetColor(col(PaletteToken::Dark), static_cast<int>(210 * la));
		ofSetLineWidth(1.2f);
		ofDrawLine(x, y0, x, y1);
		ofDrawLine(x - 6.0f, y0, x + 6.0f, y0);
		ofDrawLine(x - 6.0f, y1, x + 6.0f, y1);
		ofDrawLine(x, y0, x - 3.0f, y0 + 8.0f);
		ofDrawLine(x, y0, x + 3.0f, y0 + 8.0f);
		ofDrawLine(x, y1, x - 3.0f, y1 - 8.0f);
		ofDrawLine(x, y1, x + 3.0f, y1 - 8.0f);
		std::snprintf(buf, sizeof(buf), "%.0f", y1 - y0);
		drawText(buf, x + (lay_.labelsLeft ? 10.0f : -40.0f), (y0 + y1) * 0.5f + 4.0f);
	}

	// Assembled reference thumbnail (always the whole hero crop), top-right.
	{
		const float th = 110.0f, tw = std::min(170.0f, th * hero.width / hero.height);
		// Sits on the labelled side (top-right unless the labels are on the left only).
		const float tx = (lay_.labelsLeft && !lay_.bothSides) ? 28.0f : canvasW_ - 28.0f - tw;
		const ofRectangle r(tx, 56.0f, tw, tw * hero.height / hero.width);
		if (haveVideo) {
			const ofRectangle src = sourceRect(video, cur_, hero);
			ofSetColor(255, 255, 255, static_cast<int>(255 * en));
			video->drawSubsection(r.x, r.y, r.width, r.height, src.x, src.y, src.width, src.height);
		}
		hud::GraphicFrameStyle fs;
		fs.color = col(PaletteToken::Dark);
		fs.alpha = 0.8f * en;
		hud::GraphicFrame::draw(r, fs);
		ofSetColor(col(PaletteToken::Dark), static_cast<int>(255 * en));
		drawText("ASSEMBLED REF", r.x, r.getBottom() + 15.0f);
	}

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

GrammarStats BEExplodedView::stats() const {
	GrammarStats st;
	st.total = lay_.n;
	if (enter_ < 0.5f) return st;
	st.visible = lay_.n;
	for (int i = 0; i < lay_.n; i++) (partOnLeft(i) ? st.left : st.right)++;
	return st;
}
