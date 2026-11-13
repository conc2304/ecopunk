#include "BESectionCut.h"

#include "ofGraphics.h"

#include <algorithm>
#include <cmath>

using hud::PaletteToken;

namespace {
constexpr float AREA_X = 40.0f, AREA_TOP = 66.0f, AREA_BOTTOM = 44.0f;
constexpr float CROSS_GAP = 26.0f;

float smooth01(float t) {
	t = ofClamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}
}

float BESectionCut::rnd(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng_); }

ofColor BESectionCut::col(PaletteToken t) const {
	if (palMix_ >= 1.0f) return pal_.resolve(t);
	return prevPal_.resolve(t).getLerped(pal_.resolve(t), palMix_);
}

void BESectionCut::setup(int canvasW, int canvasH, const std::string & fontPath, int fontSize) {
	canvasW_ = canvasW;
	canvasH_ = canvasH;
	fontLoaded_ = font_.load(fontPath, fontSize, true, true);
	pal_ = prevPal_ = hud::pickCollagePalette(1u);
}

void BESectionCut::drawText(const std::string & s, float x, float y) const {
	if (fontLoaded_)
		font_.drawString(s, x, y);
	else
		ofDrawBitmapString(s, x, y);
}

ofRectangle BESectionCut::area() const {
	return ofRectangle(AREA_X, AREA_TOP, canvasW_ - 2.0f * AREA_X, canvasH_ - AREA_TOP - AREA_BOTTOM);
}

void BESectionCut::beginLayout(uint32_t seed) {
	rng_.seed(seed);
	const bool first = !built_;
	prevPal_ = first ? hud::pickCollagePalette(seed) : pal_;
	pal_ = hud::pickCollagePalette(seed);
	palMix_ = first ? 1.0f : 0.0f;
	if (first) {
		enter_ = 0.0f;
		orient_ = rnd(0.0f, 1.0f) < 0.6f ? 0 : 1;
		cut_ = cutTarget_ = rnd(0.4f, 0.6f);
		gap_ = 0.0f;
		gapTarget_ = rnd(30.0f, 50.0f);
		hatchMix_ = hatchTarget_ = rnd(0.0f, 1.0f) < 0.5f ? 0.0f : 1.0f;
		sec_ = secTarget_ = 0.0f;
		zoom_ = rnd(1.0f, 1.5f);
		cx_ = 0.5f;
		cy_ = 0.5f;
		reorienting_ = false;
	} else {
		reorienting_ = true; // recompose: close, flip, reopen
		gapTarget_ = 0.0f;
		secTarget_ = 0.0f;
	}
	vx_ = rnd(0.006f, 0.014f) * (rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
	vy_ = rnd(0.004f, 0.010f) * (rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
	moveTimer_ = rnd(4.0f, 7.0f);
	swapTimer_ = rnd(12.0f, 20.0f);
	secTimer_ = rnd(14.0f, 22.0f);
	reorientTimer_ = rnd(32.0f, 46.0f);
	if (first)
		for (auto & f : fx_) {
			f = BEFxSlot();
			f.silence = rnd(2.0f, 8.0f);
		}
	built_ = true;
}

void BESectionCut::update(float dt) {
	if (!built_) return;
	dt = ofClamp(dt, 0.0f, 0.25f);
	time_ += dt;
	enter_ = std::min(1.0f, enter_ + dt / 1.2f);
	if (palMix_ < 1.0f) palMix_ = std::min(1.0f, palMix_ + dt / 1.5f);

	// Slow drift of the picture under the cut (video motion continues too).
	const float half = 0.5f / zoom_;
	cx_ += vx_ * dt;
	cy_ += vy_ * dt;
	if (cx_ < half || cx_ > 1.0f - half) {
		vx_ = -vx_;
		cx_ = ofClamp(cx_, half, 1.0f - half);
	}
	if (cy_ < half || cy_ > 1.0f - half) {
		vy_ = -vy_;
		cy_ = ofClamp(cy_, half, 1.0f - half);
	}

	if (!reorienting_) {
		moveTimer_ -= dt;
		if (moveTimer_ <= 0.0f) {
			moveTimer_ = rnd(5.0f, 9.0f);
			cutTarget_ = rnd(0.28f, 0.72f);
			gapTarget_ = rnd(22.0f, 64.0f);
			eventCount_++;
		}
		swapTimer_ -= dt;
		if (swapTimer_ <= 0.0f) {
			swapTimer_ = rnd(12.0f, 20.0f);
			hatchTarget_ = 1.0f - hatchTarget_;
			eventCount_++;
		}
		secTimer_ -= dt;
		if (secTimer_ <= 0.0f) {
			if (secTarget_ < 0.5f) {
				secTarget_ = 1.0f;
				secTimer_ = rnd(7.0f, 10.0f); // open for a while
			} else {
				secTarget_ = 0.0f;
				secTimer_ = rnd(14.0f, 24.0f);
			}
			eventCount_++;
		}
		reorientTimer_ -= dt;
		if (reorientTimer_ <= 0.0f) {
			reorienting_ = true;
			gapTarget_ = 0.0f;
			secTarget_ = 0.0f;
			eventCount_++;
		}
	}

	stepFxSlots(fx_, 3, dt, rng_, shaderLib_, 2);
	cut_ += (cutTarget_ - cut_) * std::min(1.0f, dt * 0.9f);
	gap_ += (gapTarget_ - gap_) * std::min(1.0f, dt * 2.2f);
	hatchMix_ += (hatchTarget_ - hatchMix_) * std::min(1.0f, dt * 2.0f);
	sec_ += (secTarget_ - sec_) * std::min(1.0f, dt * 2.0f);

	if (reorienting_ && gap_ < 1.0f && sec_ < 0.05f) {
		orient_ = 1 - orient_;
		zoom_ = rnd(1.0f, 1.6f);
		cx_ = rnd(0.5f / zoom_, 1.0f - 0.5f / zoom_);
		cy_ = rnd(0.5f / zoom_, 1.0f - 0.5f / zoom_);
		cutTarget_ = cut_ = rnd(0.35f, 0.65f);
		gapTarget_ = rnd(30.0f, 52.0f);
		sheetNo_++;
		reorienting_ = false;
		moveTimer_ = rnd(4.0f, 7.0f);
		swapTimer_ = rnd(12.0f, 20.0f);
		secTimer_ = rnd(14.0f, 22.0f);
		reorientTimer_ = rnd(32.0f, 46.0f);
	}
}

void BESectionCut::hatchRect(const ofRectangle & r, float spacing, const ofColor & c, float alpha) const {
	if (r.width < 2.0f || r.height < 2.0f || alpha <= 0.002f) return;
	hatchMesh_.clear();
	hatchMesh_.setMode(OF_PRIMITIVE_LINES);
	const ofFloatColor fc(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, alpha);
	const float h = r.height, w = r.width;
	// 45-degree lines running bottom-left to top-right, clipped to the rect.
	for (float k = 0.0f; k < w + h; k += spacing) {
		const float tmin = std::max(0.0f, (k - w) / h), tmax = std::min(1.0f, k / h);
		if (tmin >= tmax) continue;
		hatchMesh_.addVertex(glm::vec3(r.x + k - tmin * h, r.y + tmin * h, 0));
		hatchMesh_.addColor(fc);
		hatchMesh_.addVertex(glm::vec3(r.x + k - tmax * h, r.y + tmax * h, 0));
		hatchMesh_.addColor(fc);
	}
	ofSetColor(255);
	ofSetLineWidth(1.0f);
	hatchMesh_.draw();
}

void BESectionCut::drawPiece(const ofTexture * v, const ofRectangle & a, const ofRectangle & d, float hatch, float alpha, int seedIdx, int fxIdx) const {
	if (d.width < 1.0f || d.height < 1.0f) return;
	if (v != nullptr && v->isAllocated()) {
		const float tw = v->getWidth(), th = v->getHeight();
		const float ar = a.width / a.height;
		float sh = th / zoom_, sw = sh * ar;
		if (sw > tw) {
			sw = tw;
			sh = sw / ar;
		}
		const float sx0 = ofClamp(cx_ * tw - sw * 0.5f, 0.0f, tw - sw);
		const float sy0 = ofClamp(cy_ * th - sh * 0.5f, 0.0f, th - sh);
		const float sx = sx0 + (d.x - a.x) / a.width * sw;
		const float sy = sy0 + (d.y - a.y) / a.height * sh;
		ofSetColor(255, 255, 255, static_cast<int>(255 * alpha));
		v->drawSubsection(d.x, d.y, d.width, d.height, sx, sy, d.width / a.width * sw, d.height / a.height * sh);
		// Shared shader effect over this piece's crop.
		drawEffectOverlay(v, ofRectangle(sx, sy, d.width / a.width * sw, d.height / a.height * sh), d, fx_[fxIdx], alpha);
	} else {
		ofSetColor(col(PaletteToken::Dark), static_cast<int>(200 * alpha));
		ofFill();
		ofDrawRectangle(d);
	}
	if (hatch > 0.01f) {
		hud::GraphicBlockStyle paper;
		paper.fill = col(PaletteToken::Background);
		paper.alpha = 0.38f * hatch * alpha;
		hud::GraphicBlock::draw(d, paper);
		hud::GraphicBlockStyle tint;
		tint.fill = col(seedIdx % 2 == 0 ? PaletteToken::Primary : PaletteToken::Secondary);
		tint.alpha = 0.22f * hatch * alpha;
		hud::GraphicBlock::draw(d, tint);
		hatchRect(d, 9.0f, col(PaletteToken::Dark), 0.38f * hatch * alpha);
	}
	hud::GraphicFrameStyle fs;
	fs.color = col(PaletteToken::Dark);
	fs.alpha = (0.55f + 0.35f * hatch) * alpha;
	fs.strokeWidth = 1.4f + 1.0f * hatch;
	hud::GraphicFrame::draw(d, fs);
}

void BESectionCut::draw(const ofTexture * video) const {
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

	const float en = smooth01(enter_);
	const ofRectangle a = area();
	const bool vert = (orient_ == 0);
	const float gap = gap_;
	char buf[80];

	// Piece rects around the cut.
	ofRectangle p1, p2;
	float cutPos;
	if (vert) {
		cutPos = a.x + cut_ * a.width;
		p1 = ofRectangle(a.x, a.y, cutPos - gap * 0.5f - a.x, a.height);
		p2 = ofRectangle(cutPos + gap * 0.5f, a.y, a.getRight() - (cutPos + gap * 0.5f), a.height);
	} else {
		cutPos = a.y + cut_ * a.height;
		p1 = ofRectangle(a.x, a.y, a.width, cutPos - gap * 0.5f - a.y);
		p2 = ofRectangle(a.x, cutPos + gap * 0.5f, a.width, a.getBottom() - (cutPos + gap * 0.5f));
	}
	const float h1 = 1.0f - hatchMix_, h2 = hatchMix_;

	// Gap fill: faint poche band so the removed strip reads as cut material.
	if (gap > 2.0f) {
		const ofRectangle g = vert ? ofRectangle(cutPos - gap * 0.5f, a.y, gap, a.height)
								   : ofRectangle(a.x, cutPos - gap * 0.5f, a.width, gap);
		hud::GraphicBlockStyle band;
		band.fill = col(PaletteToken::Dark);
		band.alpha = 0.07f * en;
		hud::GraphicBlock::draw(g, band);
	}

	drawPiece(video, a, p1, h1 * en, en, 0, 0);

	// Second piece, optionally split by the cross cut (B—B).
	if (sec_ > 0.03f) {
		const float g2 = CROSS_GAP * smooth01(sec_);
		ofRectangle q1, q2;
		if (vert) {
			const float my = p2.y + p2.height * 0.5f;
			q1 = ofRectangle(p2.x, p2.y, p2.width, my - g2 * 0.5f - p2.y);
			q2 = ofRectangle(p2.x, my + g2 * 0.5f, p2.width, p2.getBottom() - (my + g2 * 0.5f));
		} else {
			const float mx = p2.x + p2.width * 0.5f;
			q1 = ofRectangle(p2.x, p2.y, mx - g2 * 0.5f - p2.x, p2.height);
			q2 = ofRectangle(mx + g2 * 0.5f, p2.y, p2.getRight() - (mx + g2 * 0.5f), p2.height);
		}
		drawPiece(video, a, q1, h2 * en, en, 1, 1);
		drawPiece(video, a, q2, h2 * en * 0.6f, en, 0, 2);
		// Cross-cut label.
		ofSetColor(col(PaletteToken::Dark), static_cast<int>(255 * smooth01(sec_) * en));
		const glm::vec2 lp = vert ? glm::vec2(p2.x + 10.0f, p2.y + p2.height * 0.5f + 4.0f)
								  : glm::vec2(p2.x + p2.width * 0.5f - 20.0f, p2.y + 18.0f);
		drawText("B-B", lp.x, lp.y);
	} else {
		drawPiece(video, a, p2, h2 * en, en, 1, 1);
	}

	// Dash-dot cut line through the gap, with section markers at both ends.
	ofSetColor(col(PaletteToken::Dark), static_cast<int>(220 * en));
	ofSetLineWidth(1.5f);
	const float total = vert ? a.height : a.width;
	for (float s = -12.0f; s < total + 12.0f; s += 26.0f) {
		const float s0 = std::max(s, -12.0f), s1 = s + 16.0f;
		if (vert) {
			ofDrawLine(cutPos, a.y + s0, cutPos, a.y + std::min(s1, total + 12.0f));
			ofDrawLine(cutPos, a.y + s + 20.0f, cutPos, a.y + s + 22.0f);
		} else {
			ofDrawLine(a.x + s0, cutPos, a.x + std::min(s1, total + 12.0f), cutPos);
			ofDrawLine(a.x + s + 20.0f, cutPos, a.x + s + 22.0f, cutPos);
		}
	}
	auto marker = [&](const glm::vec2 & at, const glm::vec2 & dir, const char * letter) {
		const glm::vec2 perp(-dir.y, dir.x);
		const glm::vec2 tip = at + dir * 12.0f;
		ofSetColor(col(PaletteToken::Primary), static_cast<int>(255 * en));
		ofFill();
		ofDrawTriangle(tip.x, tip.y, at.x + perp.x * 8.0f, at.y + perp.y * 8.0f, at.x - perp.x * 8.0f, at.y - perp.y * 8.0f);
		ofSetColor(col(PaletteToken::Dark), static_cast<int>(255 * en));
		drawText(letter, at.x - dir.x * 22.0f - 4.0f, at.y - dir.y * 22.0f + 5.0f);
	};
	if (vert) {
		marker(glm::vec2(cutPos, a.y - 12.0f), glm::vec2(1, 0), "A");
		marker(glm::vec2(cutPos, a.getBottom() + 12.0f), glm::vec2(1, 0), "A");
	} else {
		marker(glm::vec2(a.x - 14.0f, cutPos), glm::vec2(0, 1), "A");
		marker(glm::vec2(a.getRight() + 14.0f, cutPos), glm::vec2(0, 1), "A");
	}

	// Dimension arrow across the gap.
	if (gap > 8.0f) {
		const float la = smooth01(gap / 30.0f) * en;
		ofSetColor(col(PaletteToken::Dark), static_cast<int>(230 * la));
		ofSetLineWidth(1.2f);
		const glm::vec2 m = vert ? glm::vec2(cutPos, a.y + a.height * 0.5f) : glm::vec2(a.x + a.width * 0.5f, cutPos);
		const glm::vec2 d = vert ? glm::vec2(1, 0) : glm::vec2(0, 1);
		const glm::vec2 pp(-d.y, d.x);
		const glm::vec2 e0 = m - d * (gap * 0.5f), e1 = m + d * (gap * 0.5f);
		ofDrawLine(e0.x - d.x * 14.0f, e0.y - d.y * 14.0f, e1.x + d.x * 14.0f, e1.y + d.y * 14.0f);
		ofFill();
		ofDrawTriangle(e0.x, e0.y, e0.x - d.x * 7.0f + pp.x * 3.0f, e0.y - d.y * 7.0f + pp.y * 3.0f, e0.x - d.x * 7.0f - pp.x * 3.0f, e0.y - d.y * 7.0f - pp.y * 3.0f);
		ofDrawTriangle(e1.x, e1.y, e1.x + d.x * 7.0f + pp.x * 3.0f, e1.y + d.y * 7.0f + pp.y * 3.0f, e1.x + d.x * 7.0f - pp.x * 3.0f, e1.y + d.y * 7.0f - pp.y * 3.0f);
		std::snprintf(buf, sizeof(buf), "%.0f", gap);
		drawText(buf, m.x - 8.0f + (vert ? 0.0f : 14.0f), m.y - 10.0f + (vert ? 0.0f : 18.0f));
	}

	// Title strip: tick rail with a position marker on the cut axis.
	hud::TickRailStyle rail;
	rail.color = col(PaletteToken::Dark);
	rail.alpha = 0.7f * en;
	rail.minorSpacing = 10.0f;
	hud::TickRail::draw(ofRectangle(28.0f, 30.0f, canvasW_ - 56.0f, 10.0f), rail);
	ofSetColor(col(PaletteToken::Dark), static_cast<int>(255 * en));
	std::snprintf(buf, sizeof(buf), "SECTION A-A  SHEET %02d  SCALE 1:%d", sheetNo_, static_cast<int>(std::lround(zoom_ * 4.0f)));
	drawText(buf, 28.0f, 22.0f);
	std::snprintf(buf, sizeof(buf), "CUT %s %.0f", vert ? "X" : "Y", (vert ? cutPos - a.x : cutPos - a.y));
	drawText(buf, canvasW_ - 28.0f - 130.0f, 22.0f);
	if (vert) {
		ofSetColor(col(PaletteToken::Primary), static_cast<int>(255 * en));
		ofDrawTriangle(cutPos, 43.0f, cutPos - 5.0f, 52.0f, cutPos + 5.0f, 52.0f);
	}

	hud::RegistrationMarkStyle rm;
	rm.kind = hud::RegistrationMarkStyle::Kind::Cross;
	rm.color = col(PaletteToken::Dark);
	rm.alpha = 0.7f * en;
	hud::RegistrationMark::draw(glm::vec2(12.0f, 12.0f), 6.0f, rm);
	hud::RegistrationMark::draw(glm::vec2(canvasW_ - 12.0f, 12.0f), 6.0f, rm);
	hud::RegistrationMark::draw(glm::vec2(12.0f, canvasH_ - 12.0f), 6.0f, rm);
	hud::RegistrationMark::draw(glm::vec2(canvasW_ - 12.0f, canvasH_ - 12.0f), 6.0f, rm);

	ofPopStyle();
}

GrammarStats BESectionCut::stats() const {
	GrammarStats st;
	st.total = 2;
	if (enter_ < 0.5f) return st;
	st.visible = 2;
	st.left = 1;
	st.right = 1;
	return st;
}
