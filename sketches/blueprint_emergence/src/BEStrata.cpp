#include "BEStrata.h"

#include "ofGraphics.h"
#include "ofLog.h"

#include <algorithm>
#include <cmath>

using hud::PaletteToken;

namespace {
constexpr float MARGIN = 24.0f;
constexpr float GUTTER = 6.0f;
constexpr float LABEL_W = 92.0f;
constexpr float MAX_SPEED = 0.075f; // source-widths per second
constexpr float TICK_SPACING = 14.0f;
constexpr float REVEAL_SECONDS = 1.1f;

float smooth01(float t) {
	t = ofClamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}
float fract(float v) { return v - std::floor(v); }
}

float BEStrata::rnd(float a, float b) {
	return std::uniform_real_distribution<float>(a, b)(rng_);
}
int BEStrata::rndi(int a, int b) {
	return std::uniform_int_distribution<int>(a, b)(rng_);
}

ofColor BEStrata::col(PaletteToken t) const {
	if (palMix_ >= 1.0f) return pal_.resolve(t);
	return prevPal_.resolve(t).getLerped(pal_.resolve(t), palMix_);
}

void BEStrata::setup(int canvasW, int canvasH, const std::string & fontPath, int fontSize) {
	canvasW_ = canvasW;
	canvasH_ = canvasH;
	fontLoaded_ = font_.load(fontPath, fontSize, true, true);
	pal_ = hud::pickCollagePalette(1u);
	prevPal_ = pal_;
}

ofRectangle BEStrata::laneRect(int i) const {
	const float laneH = (canvasH_ - 2.0f * MARGIN - (NUM_LANES - 1) * GUTTER) / NUM_LANES;
	const float x = MARGIN + LABEL_W + GUTTER;
	return ofRectangle(x, MARGIN + i * (laneH + GUTTER), canvasW_ - MARGIN - x, laneH);
}

void BEStrata::spawnSegment(Lane & l, int slot) {
	for (int attempt = 0; attempt < 10; attempt++) {
		const float w = rnd(0.26f, 0.7f);
		const float x0 = rnd(0.0f, 1.0f - w);
		const float x1 = x0 + w;
		bool clash = false;
		for (int k = 0; k < MAX_SEGS; k++) {
			if (k == slot || !l.segs[k].alive) continue;
			if (x0 < l.segs[k].x1 + 0.04f && x1 > l.segs[k].x0 - 0.04f) clash = true;
		}
		if (clash) continue;
		Segment & s = l.segs[slot];
		s.alive = true;
		s.x0 = x0;
		s.x1 = x1;
		s.uOffset = rnd(0.0f, 1.0f);
		s.reveal = 0.0f;
		s.revealTarget = 1.0f;
		s.hold = rnd(7.0f, 18.0f);
		s.leftToRight = rnd(0.0f, 1.0f) < 0.5f;
		s.framed = rnd(0.0f, 1.0f) < 0.3f;
		return;
	}
}

void BEStrata::buildLane(int i, bool initial) {
	Lane & l = lanes_[i];
	const float dir = rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
	l.speedTarget = dir * rnd(0.012f, 0.06f);
	l.speedRate = rnd(0.6f, 1.4f);
	l.freeze = 0.0f;
	l.tint = (i % 3 == 0) ? 0.22f : (i % 3 == 1 ? 0.0f : 0.14f);
	l.useSecondary = (i % 2 == 1);
	l.nextRamp = rnd(4.0f, 10.0f);
	l.nextTurnover = rnd(3.0f, 8.0f);
	if (initial) {
		l.speed = l.speedTarget;
		l.scroll = rnd(0.0f, 1.0f);
		l.row = i;
		l.rowWipe = 1.0f;
		l.enter = 0.0f;
		l.enterDelay = i * 0.35f + rnd(0.0f, 0.4f);
		for (auto & s : l.segs) s = Segment();
		spawnSegment(l, 0);
		if (rnd(0.0f, 1.0f) < 0.6f) spawnSegment(l, 1);
	} else {
		// Recompose: current segments leave in a stagger, turnover refills the lane.
		for (auto & s : l.segs) {
			if (!s.alive) continue;
			s.hold = rnd(0.0f, 2.5f);
		}
		l.nextTurnover = rnd(0.5f, 3.0f);
	}
}

void BEStrata::beginLayout(uint32_t seed) {
	seed_ = seed;
	rng_.seed(seed);
	const bool first = !built_;
	prevPal_ = first ? hud::pickCollagePalette(seed) : pal_;
	pal_ = hud::pickCollagePalette(seed);
	palMix_ = first ? 1.0f : 0.0f;
	for (int i = 0; i < NUM_LANES; i++) buildLane(i, first);
	if (!first) {
		// Rows reshuffle on recompose (a few swaps, never a free rotation).
		for (int k = 0; k < 3; k++) {
			const int a = rndi(0, NUM_LANES - 1), b = rndi(0, NUM_LANES - 1);
			if (a == b) continue;
			std::swap(lanes_[a].row, lanes_[b].row);
			lanes_[a].rowWipe = lanes_[b].rowWipe = 0.0f;
		}
	}
	if (first)
		for (auto & f : fx_) {
			f = BEFxSlot();
			f.silence = rnd(2.0f, 8.0f);
		}
	syncTimer_ = rnd(25.0f, 45.0f);
	syncHold_ = 0.0f;
	swapTimer_ = rnd(8.0f, 14.0f);
	scanX_ = rnd(0.0f, 1.0f);
	scanSpeed_ = rnd(0.03f, 0.06f);
	if (first) time_ = 0.0f;
	built_ = true;
}

void BEStrata::directorTick(float dt) {
	// Group sync: all lanes ease to one shared speed, hold, then diverge.
	if (syncHold_ > 0.0f) {
		syncHold_ -= dt;
		if (syncHold_ <= 0.0f) {
			for (auto & l : lanes_) {
				const float dir = rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
				l.speedTarget = dir * rnd(0.012f, 0.06f);
				l.speedRate = rnd(0.5f, 1.2f);
			}
		}
	} else {
		syncTimer_ -= dt;
		if (syncTimer_ <= 0.0f) {
			syncHold_ = rnd(2.5f, 4.0f);
			syncSpeed_ = (rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f) * rnd(0.02f, 0.05f);
			for (auto & l : lanes_) {
				l.speedTarget = syncSpeed_;
				l.speedRate = 2.0f;
				l.freeze = 0.0f;
				l.markPulse = 1.0f;
			}
			eventCount_++;
			syncTimer_ = rnd(30.0f, 55.0f);
		}
	}

	// Row swap: two lanes trade source rows behind a brief dim.
	swapTimer_ -= dt;
	if (swapTimer_ <= 0.0f) {
		const int a = rndi(0, NUM_LANES - 1);
		int b = rndi(0, NUM_LANES - 1);
		if (b == a) b = (a + 1) % NUM_LANES;
		std::swap(lanes_[a].row, lanes_[b].row);
		lanes_[a].rowWipe = lanes_[b].rowWipe = 0.0f;
		lanes_[a].markPulse = lanes_[b].markPulse = 1.0f;
		eventCount_++;
		swapTimer_ = rnd(9.0f, 16.0f);
	}
}

void BEStrata::update(float dt) {
	if (!built_) return;
	dt = ofClamp(dt, 0.0f, 0.25f);
	time_ += dt;
	if (palMix_ < 1.0f) palMix_ = std::min(1.0f, palMix_ + dt / 1.5f);
	scanX_ = fract(scanX_ + scanSpeed_ * dt);
	directorTick(dt);
	stepFxSlots(fx_, NUM_LANES, dt, rng_, shaderLib_, 2);

	for (int i = 0; i < NUM_LANES; i++) {
		Lane & l = lanes_[i];
		if (l.enterDelay > 0.0f) {
			l.enterDelay -= dt;
		} else {
			l.enter = std::min(1.0f, l.enter + dt / 1.0f);
		}
		if (l.freeze > 0.0f) {
			l.freeze -= dt;
			l.speedTarget = 0.0f;
			if (l.freeze <= 0.0f) {
				const float dir = rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
				l.speedTarget = dir * rnd(0.012f, 0.06f);
			}
		}
		l.speed += (l.speedTarget - l.speed) * std::min(1.0f, dt * l.speedRate);
		l.scroll = fract(l.scroll + l.speed * dt);
		l.tickPhase += l.speed * dt * 1000.0f;
		l.markPulse = std::max(0.0f, l.markPulse - dt / 1.2f);
		l.rowWipe = std::min(1.0f, l.rowWipe + dt / 0.9f);

		// Speed ramp (a freeze 25% of the time).
		if (syncHold_ <= 0.0f) {
			l.nextRamp -= dt;
			if (l.nextRamp <= 0.0f) {
				l.nextRamp = rnd(4.0f, 12.0f);
				l.markPulse = 1.0f;
				eventCount_++;
				if (rnd(0.0f, 1.0f) < 0.25f) {
					l.freeze = rnd(1.2f, 3.0f);
					l.speedRate = rnd(1.5f, 3.0f);
				} else {
					const float flip = rnd(0.0f, 1.0f) < 0.4f ? -1.0f : 1.0f;
					const float mag = rnd(0.012f, MAX_SPEED * 0.85f);
					l.speedTarget = (l.speedTarget < 0.0f ? -1.0f : 1.0f) * flip * mag;
					l.speedRate = rnd(0.5f, 1.5f);
				}
			}
		}

		// Segment lifecycle.
		int alive = 0;
		for (auto & s : l.segs) {
			if (!s.alive) continue;
			alive++;
			const float step = dt / REVEAL_SECONDS;
			if (s.revealTarget > s.reveal) {
				s.reveal = std::min(s.revealTarget, s.reveal + step);
			} else if (s.revealTarget < s.reveal) {
				s.reveal = std::max(s.revealTarget, s.reveal - step);
			}
			if (s.revealTarget >= 1.0f && s.reveal >= 1.0f && l.enter >= 1.0f) {
				s.hold -= dt;
				if (s.hold <= 0.0f) s.revealTarget = 0.0f;
			}
			if (s.revealTarget <= 0.0f && s.reveal <= 0.001f) {
				s.alive = false;
				alive--;
			}
		}
		l.nextTurnover -= dt;
		if (l.nextTurnover <= 0.0f) {
			l.nextTurnover = rnd(3.0f, 8.0f);
			// Retire one settled segment when there are 2+, refill empty slots.
			if (alive >= 2) {
				const int k = rndi(0, MAX_SEGS - 1);
				if (l.segs[k].alive && l.segs[k].revealTarget >= 1.0f) {
					l.segs[k].revealTarget = 0.0f;
					eventCount_++;
				}
			}
		}
		if (alive < 2) {
			for (int k = 0; k < MAX_SEGS && alive < 2; k++) {
				if (!l.segs[k].alive) {
					spawnSegment(l, k);
					if (l.segs[k].alive) alive++;
				}
			}
		}
	}
}

void BEStrata::drawText(const std::string & s, float x, float y) const {
	if (fontLoaded_)
		font_.drawString(s, x, y);
	else
		ofDrawBitmapString(s, x, y);
}

void BEStrata::drawSegment(const Lane & l, int laneIdx, const Segment & s, const ofRectangle & lr, const ofTexture * video) const {
	const float rev = smooth01(s.reveal);
	if (rev <= 0.002f) return;
	const float dw = (s.x1 - s.x0) * lr.width;
	const float dx = lr.x + s.x0 * lr.width;
	const float vw = dw * rev;
	const float vx = s.leftToRight ? dx : dx + dw - vw;
	const ofRectangle vr(vx, lr.y, vw, lr.height);
	const float alpha = smooth01(l.enter) * (0.4f + 0.6f * l.rowWipe);

	if (video != nullptr && video->isAllocated()) {
		const float tw = video->getWidth(), th = video->getHeight();
		float bandH = th / NUM_LANES;
		float srcW = bandH * dw / std::max(lr.height, 1.0f);
		if (srcW > tw * 0.98f) {
			srcW = tw * 0.98f;
			bandH = srcW * lr.height / std::max(dw, 1.0f);
		}
		const float cy = (l.row + 0.5f) * th / NUM_LANES;
		const float sy = ofClamp(cy - bandH * 0.5f, 0.0f, th - bandH);
		const float a = s.leftToRight ? 0.0f : 1.0f - rev;
		float sx = fract(l.scroll + s.uOffset) * tw + a * srcW;
		float sw = srcW * rev;
		sx = sx - std::floor(sx / tw) * tw;
		ofSetColor(255, 255, 255, static_cast<int>(255 * alpha));
		if (sx + sw <= tw) {
			video->drawSubsection(vr.x, vr.y, vr.width, vr.height, sx, sy, sw, bandH);
			drawEffectOverlay(video, ofRectangle(sx, sy, sw, bandH), vr, fx_[laneIdx], alpha);
		} else {
			const float w1 = tw - sx;
			const float d1 = vr.width * (w1 / sw);
			video->drawSubsection(vr.x, vr.y, d1, vr.height, sx, sy, w1, bandH);
			video->drawSubsection(vr.x + d1, vr.y, vr.width - d1, vr.height, 0.0f, sy, sw - w1, bandH);
			drawEffectOverlay(video, ofRectangle(sx, sy, w1, bandH), ofRectangle(vr.x, vr.y, d1, vr.height), fx_[laneIdx], alpha);
			drawEffectOverlay(video, ofRectangle(0.0f, sy, sw - w1, bandH), ofRectangle(vr.x + d1, vr.y, vr.width - d1, vr.height), fx_[laneIdx], alpha);
		}
	} else {
		ofSetColor(col(PaletteToken::Dark), static_cast<int>(200 * alpha));
		ofFill();
		ofDrawRectangle(vr);
	}

	if (l.tint > 0.0f) {
		hud::GraphicBlockStyle tint;
		tint.fill = col(l.useSecondary ? PaletteToken::Secondary : PaletteToken::Primary);
		tint.alpha = l.tint * alpha;
		hud::GraphicBlock::draw(vr, tint);
	}
	if (s.framed) {
		hud::GraphicFrameStyle f;
		f.color = col(PaletteToken::Dark);
		f.alpha = 0.85f * alpha;
		f.strokeWidth = 2.0f;
		f.reveal = rev;
		hud::GraphicFrame::draw(ofRectangle(dx, lr.y, dw, lr.height), f);
	}
}

void BEStrata::draw(const ofTexture * video) const {
	if (!built_) return;
	ofPushStyle();
	ofEnableAlphaBlending();
	ofFill();
	ofSetColor(col(PaletteToken::Background));
	ofDrawRectangle(0, 0, canvasW_, canvasH_);

	hud::TechnicalGridStyle grid;
	grid.color = col(PaletteToken::Dark);
	grid.majorSpacing = 96.0f;
	grid.majorAlpha = 0.16f;
	grid.minorAlpha = 0.06f;
	hud::TechnicalGrid::draw(ofRectangle(0, 0, canvasW_, canvasH_), grid);

	for (int i = 0; i < NUM_LANES; i++) {
		const Lane & l = lanes_[i];
		const ofRectangle lr = laneRect(i);
		const float e = smooth01(l.enter);
		if (e <= 0.001f) continue;

		// Lane well + baseline rule.
		hud::GraphicBlockStyle well;
		well.fill = col(PaletteToken::Dark);
		well.alpha = 0.06f * e;
		hud::GraphicBlock::draw(lr, well);

		for (const auto & s : l.segs) {
			if (s.alive) drawSegment(l, i, s, lr, video);
		}

		// Scrolling tick rail on the lane's top edge (one batched line mesh).
		tickMesh_.clear();
		tickMesh_.setMode(OF_PRIMITIVE_LINES);
		float o = std::fmod(l.tickPhase, TICK_SPACING);
		if (o < 0.0f) o += TICK_SPACING;
		const int base = static_cast<int>(std::floor(l.tickPhase / TICK_SPACING));
		const ofColor tc = col(PaletteToken::Dark);
		const ofFloatColor tf(tc.r / 255.0f, tc.g / 255.0f, tc.b / 255.0f, 0.7f * e);
		for (int k = 0; k * TICK_SPACING - o < lr.width; k++) {
			const float x = k * TICK_SPACING - o;
			if (x < 0.0f) continue;
			const int idx = k + base;
			const float len = (((idx % 5) + 5) % 5 == 0) ? 12.0f : 5.0f;
			tickMesh_.addVertex(glm::vec3(lr.x + x, lr.y, 0));
			tickMesh_.addColor(tf);
			tickMesh_.addVertex(glm::vec3(lr.x + x, lr.y + len, 0));
			tickMesh_.addColor(tf);
		}
		ofSetColor(255);
		ofSetLineWidth(1.0f);
		tickMesh_.draw();

		// Baseline.
		ofSetColor(col(PaletteToken::Dark), static_cast<int>(150 * e));
		ofDrawRectangle(lr.x, lr.getBottom() - 1.0f, lr.width * e, 1.0f);

		// Label cell: id + signed speed + speed bar.
		const ofRectangle cell(MARGIN, lr.y, LABEL_W, lr.height);
		hud::GraphicBlockStyle cs;
		cs.fill = col(i % 3 == 0 ? PaletteToken::Primary : (i % 3 == 1 ? PaletteToken::Dark : PaletteToken::Secondary));
		cs.alpha = e;
		cs.reveal = e;
		cs.revealDir = hud::RevealDir::LeftToRight;
		hud::GraphicBlock::draw(cell, cs);
		ofSetColor(col(PaletteToken::Light), static_cast<int>(255 * e));
		char buf[48];
		std::snprintf(buf, sizeof(buf), "S%d", i + 1);
		drawText(buf, cell.x + 8.0f, cell.y + 18.0f);
		std::snprintf(buf, sizeof(buf), "%+.2f", l.speed * 10.0f);
		drawText(buf, cell.x + 8.0f, cell.y + cell.height - 16.0f);
		hud::DataBarStyle bar;
		bar.fill = col(PaletteToken::Light);
		bar.track = col(PaletteToken::Light);
		bar.value = ofClamp(std::fabs(l.speed) / MAX_SPEED, 0.0f, 1.0f);
		bar.alpha = e;
		hud::DataBar::draw(ofRectangle(cell.x + 8.0f, cell.getBottom() - 8.0f, cell.width - 16.0f, 3.0f), bar);

		// Registration mark at the lane's right end, pulsing on events.
		hud::RegistrationMarkStyle rm;
		rm.kind = (i % 2 == 0) ? hud::RegistrationMarkStyle::Kind::Cross : hud::RegistrationMarkStyle::Kind::Circle;
		rm.color = col(PaletteToken::Dark);
		rm.alpha = (0.45f + 0.5f * l.markPulse) * e;
		hud::RegistrationMark::draw(glm::vec2(lr.getRight() - 10.0f, lr.y + lr.height * 0.5f), 4.0f + 3.0f * l.markPulse, rm);
	}

	// Vertical scan datum sweeping across all lanes.
	const float sx = MARGIN + LABEL_W + GUTTER + scanX_ * (canvasW_ - 2.0f * MARGIN - LABEL_W - GUTTER);
	ofSetColor(col(PaletteToken::Primary), 150);
	ofDrawRectangle(sx, MARGIN - 6.0f, 1.5f, canvasH_ - 2.0f * MARGIN + 12.0f);
	hud::RegistrationMarkStyle top;
	top.kind = hud::RegistrationMarkStyle::Kind::TargetDot;
	top.color = col(PaletteToken::Primary);
	top.alpha = 0.9f;
	hud::RegistrationMark::draw(glm::vec2(sx, MARGIN - 8.0f), 4.0f, top);
	hud::RegistrationMark::draw(glm::vec2(sx, canvasH_ - MARGIN + 8.0f), 4.0f, top);

	ofPopStyle();
}

GrammarStats BEStrata::stats() const {
	GrammarStats st;
	st.total = NUM_LANES;
	for (const auto & l : lanes_) {
		for (const auto & s : l.segs) {
			if (!s.alive || s.reveal < 0.5f) continue;
			st.visible++;
			if ((s.x0 + s.x1) * 0.5f < 0.5f)
				st.left++;
			else
				st.right++;
		}
	}
	return st;
}

uint32_t BEStrata::stateHash() const {
	uint32_t h = 2166136261u;
	auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
	for (const auto & l : lanes_) {
		mix(static_cast<uint32_t>(l.row));
		mix(static_cast<uint32_t>(std::lround(l.scroll * 1000.0f)));
		mix(static_cast<uint32_t>(std::lround(l.speedTarget * 10000.0f)));
		for (const auto & s : l.segs) mix(s.alive ? static_cast<uint32_t>(std::lround(s.x0 * 1000.0f)) + 1u : 0u);
	}
	return h;
}
