#include "BlobAnalysisOverlay.h"

#include "VideoRegionMath.h"
#include "VideoRegionMathOf.h"

#include "ofGraphics.h"
#include "ofLog.h"
#include "ofMath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using blobanalysis::Element;

namespace {

// One phosphor accent for the whole instrument; the warm tone is reserved
// for "something is wrong / lost" so it stays meaningful.
const ofFloatColor kAccent(0.62f, 1.00f, 0.86f, 1.0f);
const ofFloatColor kAlert(1.00f, 0.67f, 0.25f, 1.0f);

constexpr float kTwoPi = 6.28318530717959f;

// The scene renders at 1280x720 but is shown scaled down inside the
// runtime's viewport (~60%), so the text face is loaded large (kFontSize)
// to stay legible. kLineH is the matching baseline-to-baseline spacing.
//
// ofTrueTypeFont, not ofDrawBitmapString: inside the runtime's scene FBO
// (ofIsVFlipped() == true) the bitmap font's glyphs come out upside-down;
// vector/atlas glyphs from a TrueType face do not. The face is the same
// one Blueprint uses for its scene text (bin/data/fonts).
constexpr char kFontPath[] = "fonts/MadawaskaRiver.otf";
constexpr int kFontSize = 22;
constexpr float kLineH = 28.0f;

float clamp01(float x) { return std::min(1.0f, std::max(0.0f, x)); }
float easeOutCubic(float t) {
	t = clamp01(t);
	float u = 1.0f - t;
	return 1.0f - u * u * u;
}
float hash01(int a, int b) {
	uint32_t h = static_cast<uint32_t>(a) * 2654435761u ^ (static_cast<uint32_t>(b) + 0x9e3779b9u) * 2246822519u;
	h ^= h >> 15;
	h *= 2246822519u;
	h ^= h >> 13;
	return static_cast<float>(h & 0xffffu) / 65535.0f;
}
float cross2(const glm::vec2 & o, const glm::vec2 & a, const glm::vec2 & b) {
	return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

} // namespace

void BlobAnalysisOverlay::reset() {
	nodes_.clear();
	pings_.clear();
	history_.clear();
	texts_.clear();
	historyTimer_ = 0.0f;
	time_ = 0.0f;
}

void BlobAnalysisOverlay::ensureFont() {
	if (fontTried_) {
		return;
	}
	fontTried_ = true;
	if (!font_.load(kFontPath, kFontSize, true, true)) {
		ofLogError("BlobAnalysisOverlay") << "could not load " << kFontPath << " — overlay text disabled (lines still draw)";
	}
}

BlobAnalysisOverlay::Node * BlobAnalysisOverlay::find(int id) {
	for (Node & n : nodes_) {
		if (n.id == id) {
			return &n;
		}
	}
	return nullptr;
}

void BlobAnalysisOverlay::update(float dt, const std::vector<VideoRegion> & regions, float srcW, float srcH, const ofRectangle & dest) {
	dt = std::min(dt, 0.25f);
	time_ += dt;

	for (Node & n : nodes_) {
		n.seen = false;
	}

	if (srcW > 0.0f && srcH > 0.0f) {
		const VideoRegionMath::Rect destR = VideoRegionMath::toRect(dest);
		const VideoRegionMath::Rect crop = VideoRegionMath::computeCropFillSourceRect(srcW, srcH, destR);
		for (const VideoRegion & region : regions) {
			ofRectangle r = VideoRegionMath::toOf(VideoRegionMath::mapNormalizedSourceRectToScreen(
				VideoRegionMath::toRect(region.smoothedNormalizedBounds), srcW, srcH, crop, destR));
			Node * n = find(region.id);
			if (n == nullptr) {
				if (static_cast<int>(nodes_.size()) >= kMaxNodes) {
					continue;
				}
				nodes_.emplace_back();
				n = &nodes_.back();
				n->id = region.id;
				Ping p;
				p.id = region.id;
				p.rect = r;
				p.c = glm::vec2(r.getCenter().x, r.getCenter().y);
				p.life = 1.1f;
				if (static_cast<int>(pings_.size()) < kMaxPings) {
					pings_.push_back(p);
				}
			}
			n->seen = true;
			n->rect = r;
			n->c = glm::vec2(r.getCenter().x, r.getCenter().y);
			n->normC = glm::vec2(region.smoothedNormalizedBounds.getCenter().x, region.smoothedNormalizedBounds.getCenter().y);
			n->conf = region.confidence;
			n->area = region.smoothedNormalizedBounds.width * region.smoothedNormalizedBounds.height;
			n->age = region.ageSeconds;
			n->missing = region.missingSeconds;
			n->lockT += dt;
			n->trailTimer += dt;
			if (n->trailTimer >= 0.06f) {
				n->trailTimer = 0.0f;
				n->trail.push_back(n->c);
				if (n->trail.size() > 28) {
					n->trail.erase(n->trail.begin());
				}
			}
		}
	}

	// Tracks that disappeared since last frame -> a "lost" ping at their last box.
	for (size_t i = 0; i < nodes_.size();) {
		if (!nodes_[i].seen) {
			Ping p;
			p.id = nodes_[i].id;
			p.rect = nodes_[i].rect;
			p.c = nodes_[i].c;
			p.life = 1.3f;
			p.lost = true;
			if (static_cast<int>(pings_.size()) < kMaxPings) {
				pings_.push_back(p);
			}
			nodes_.erase(nodes_.begin() + static_cast<long>(i));
		} else {
			i++;
		}
	}

	for (Ping & p : pings_) {
		p.age += dt;
	}
	pings_.erase(std::remove_if(pings_.begin(), pings_.end(), [](const Ping & p) { return p.age >= p.life; }), pings_.end());

	historyTimer_ += dt;
	if (historyTimer_ >= 0.5f) {
		historyTimer_ = 0.0f;
		history_.push_back(static_cast<int>(nodes_.size()));
		if (static_cast<int>(history_.size()) > kHistory) {
			history_.erase(history_.begin());
		}
	}
}

// ---------------------------------------------------------------- primitives

void BlobAnalysisOverlay::line(glm::vec2 a, glm::vec2 b, float alphaA, float alphaB, bool alert) {
	alphaA = clamp01(alphaA);
	alphaB = clamp01(alphaB);
	if (alphaA <= 0.004f && alphaB <= 0.004f) {
		return;
	}
	const ofFloatColor & c = alert ? kAlert : kAccent;
	lines_.addVertex(glm::vec3(a, 0.0f));
	lines_.addColor(ofFloatColor(c.r, c.g, c.b, alphaA));
	lines_.addVertex(glm::vec3(b, 0.0f));
	lines_.addColor(ofFloatColor(c.r, c.g, c.b, alphaB));
}

void BlobAnalysisOverlay::dashed(glm::vec2 a, glm::vec2 b, float alpha, float dash, float gap, float offset, bool alert) {
	glm::vec2 d = b - a;
	float len = glm::length(d);
	if (len < 1.0f || alpha <= 0.004f) {
		return;
	}
	d /= len;
	float period = dash + gap;
	float pos = -std::fmod(offset, period);
	while (pos < len) {
		float s0 = std::max(0.0f, pos);
		float s1 = std::min(len, pos + dash);
		if (s1 > s0) {
			line(a + d * s0, a + d * s1, alpha, alert);
		}
		pos += period;
	}
}

void BlobAnalysisOverlay::ring(glm::vec2 c, float r, float alpha, int segments, float startAngle, float arc, bool dashedRing, bool alert) {
	if (r < 0.5f || alpha <= 0.004f) {
		return;
	}
	segments = std::max(4, segments);
	float step = arc / static_cast<float>(segments);
	for (int i = 0; i < segments; i++) {
		if (dashedRing && (i % 2) == 1) {
			continue;
		}
		float a0 = startAngle + step * static_cast<float>(i);
		float a1 = a0 + step;
		line(c + r * glm::vec2(std::cos(a0), std::sin(a0)), c + r * glm::vec2(std::cos(a1), std::sin(a1)), alpha, alert);
	}
}

void BlobAnalysisOverlay::cross(glm::vec2 c, float r, float alpha, bool alert) {
	line(c - glm::vec2(r, 0.0f), c + glm::vec2(r, 0.0f), alpha, alert);
	line(c - glm::vec2(0.0f, r), c + glm::vec2(0.0f, r), alpha, alert);
}

void BlobAnalysisOverlay::diamond(glm::vec2 c, float r, float alpha, bool alert) {
	glm::vec2 t = c + glm::vec2(0.0f, -r), rt = c + glm::vec2(r, 0.0f), b = c + glm::vec2(0.0f, r), l = c + glm::vec2(-r, 0.0f);
	line(t, rt, alpha, alert);
	line(rt, b, alpha, alert);
	line(b, l, alpha, alert);
	line(l, t, alpha, alert);
}

void BlobAnalysisOverlay::cornerBracket(const ofRectangle & r, float arm, float alpha, bool alert) {
	const float x0 = r.getLeft(), x1 = r.getRight(), y0 = r.getTop(), y1 = r.getBottom();
	line({ x0, y0 }, { x0 + arm, y0 }, alpha, alert);
	line({ x0, y0 }, { x0, y0 + arm }, alpha, alert);
	line({ x1, y0 }, { x1 - arm, y0 }, alpha, alert);
	line({ x1, y0 }, { x1, y0 + arm }, alpha, alert);
	line({ x0, y1 }, { x0 + arm, y1 }, alpha, alert);
	line({ x0, y1 }, { x0, y1 - arm }, alpha, alert);
	line({ x1, y1 }, { x1 - arm, y1 }, alpha, alert);
	line({ x1, y1 }, { x1, y1 - arm }, alpha, alert);
}

void BlobAnalysisOverlay::text(const std::string & s, float x, float y, float scale, float alpha, bool alert) {
	if (alpha <= 0.02f || s.empty()) {
		return;
	}
	texts_.push_back({ s, x, y, scale, clamp01(alpha), alert });
}

// ------------------------------------------------------------------ elements

void BlobAnalysisOverlay::drawFrame(float i, const ofRectangle & dest, float pulse) {
	const float m = 14.0f;
	const float a = i * (0.55f + 0.45f * pulse);
	const float L = dest.getLeft() + m, R = dest.getRight() - m, T = dest.getTop() + m, B = dest.getBottom() - m;
	const float arm = 30.0f;

	// Registration corners.
	line({ L, T }, { L + arm, T }, a);
	line({ L, T }, { L, T + arm }, a);
	line({ R, T }, { R - arm, T }, a);
	line({ R, T }, { R, T + arm }, a);
	line({ L, B }, { L + arm, B }, a);
	line({ L, B }, { L, B - arm }, a);
	line({ R, B }, { R - arm, B }, a);
	line({ R, B }, { R, B - arm }, a);

	// Edge ticks (long every 5th), marching slowly so the rule feels live.
	const float pitch = 32.0f;
	const float drift = std::fmod(time_ * 6.0f, pitch);
	int k = 0;
	for (float x = L + arm + 6.0f - drift + pitch; x < R - arm - 6.0f; x += pitch, k++) {
		if (x < L + arm + 6.0f) continue;
		float len = (k % 5 == 0) ? 9.0f : 4.0f;
		line({ x, T }, { x, T + len }, a * 0.55f);
		line({ x, B }, { x, B - len }, a * 0.55f);
	}
	k = 0;
	for (float y = T + arm + 6.0f - drift + pitch; y < B - arm - 6.0f; y += pitch, k++) {
		if (y < T + arm + 6.0f) continue;
		float len = (k % 5 == 0) ? 9.0f : 4.0f;
		line({ L, y }, { L + len, y }, a * 0.55f);
		line({ R, y }, { R - len, y }, a * 0.55f);
	}

	// Centre crosshair with a slowly turning dashed ring.
	glm::vec2 c(dest.getCenter().x, dest.getCenter().y);
	const float gap = 6.0f;
	line(c + glm::vec2(-26.0f, 0.0f), c + glm::vec2(-gap, 0.0f), a * 0.7f);
	line(c + glm::vec2(gap, 0.0f), c + glm::vec2(26.0f, 0.0f), a * 0.7f);
	line(c + glm::vec2(0.0f, -26.0f), c + glm::vec2(0.0f, -gap), a * 0.7f);
	line(c + glm::vec2(0.0f, gap), c + glm::vec2(0.0f, 26.0f), a * 0.7f);
	ring(c, 38.0f, a * 0.4f, 24, time_ * 0.35f, kTwoPi, true);
}

void BlobAnalysisOverlay::drawScan(float i, const ofRectangle & dest, float scanX) {
	const float T = dest.getTop(), B = dest.getBottom();
	// Head line plus a short fading wake behind it.
	line({ scanX, T }, { scanX, B }, i * 0.55f);
	for (int k = 1; k <= 7; k++) {
		float x = scanX - static_cast<float>(k) * 5.0f;
		if (x < dest.getLeft()) break;
		line({ x, T }, { x, B }, i * 0.20f * (1.0f - static_cast<float>(k) / 8.0f));
	}
	// Ruler ticks riding the head at the top and bottom.
	line({ scanX - 7.0f, T + 2.0f }, { scanX + 7.0f, T + 2.0f }, i * 0.8f);
	line({ scanX - 7.0f, B - 2.0f }, { scanX + 7.0f, B - 2.0f }, i * 0.8f);
	char buf[24];
	std::snprintf(buf, sizeof(buf), "X %04d", static_cast<int>(scanX - dest.getLeft()));
	text(buf, scanX + 10.0f, T + 30.0f, 1.0f, i * 0.8f);
}

void BlobAnalysisOverlay::drawTrails(float i) {
	for (const Node & n : nodes_) {
		const size_t count = n.trail.size();
		for (size_t k = 1; k < count; k++) {
			float t0 = static_cast<float>(k - 1) / static_cast<float>(count);
			float t1 = static_cast<float>(k) / static_cast<float>(count);
			line(n.trail[k - 1], n.trail[k], i * 0.65f * t0, i * 0.65f * t1);
		}
		// Sample dots every few points make speed readable.
		for (size_t k = 0; k < count; k += 4) {
			diamond(n.trail[k], 1.5f, i * 0.5f * static_cast<float>(k) / static_cast<float>(std::max<size_t>(1, count)));
		}
	}
}

void BlobAnalysisOverlay::drawRings(float i) {
	int shown = 0;
	for (const Node & n : nodes_) {
		if (++shown > 6) break;
		const float maxR = std::max(n.rect.width, n.rect.height) * 0.7f + 34.0f;
		for (int k = 0; k < 3; k++) {
			float phase = std::fmod(time_ * 0.32f + hash01(n.id, 7) + static_cast<float>(k) / 3.0f, 1.0f);
			ring(n.c, phase * maxR, i * 0.55f * (1.0f - phase), 28, 0.0f, kTwoPi, false);
		}
		// Range tick on the first ring's bearing line.
		float bearing = time_ * 0.8f + hash01(n.id, 3) * kTwoPi;
		line(n.c, n.c + maxR * glm::vec2(std::cos(bearing), std::sin(bearing)), i * 0.22f, 0.0f);
	}
}

void BlobAnalysisOverlay::drawHull(float i) {
	const size_t n = nodes_.size();
	if (n < 2) {
		return;
	}
	std::vector<glm::vec2> pts;
	pts.reserve(n);
	glm::vec2 com(0.0f);
	for (const Node & node : nodes_) {
		pts.push_back(node.c);
		com += node.c;
	}
	com /= static_cast<float>(n);

	std::vector<glm::vec2> hull;
	if (n == 2) {
		hull = pts;
	} else {
		std::sort(pts.begin(), pts.end(), [](const glm::vec2 & a, const glm::vec2 & b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
		hull.assign(2 * n, glm::vec2(0.0f));
		size_t k = 0;
		for (size_t p = 0; p < n; p++) {
			while (k >= 2 && cross2(hull[k - 2], hull[k - 1], pts[p]) <= 0.0f) k--;
			hull[k++] = pts[p];
		}
		for (size_t p = n - 1, t = k + 1; p > 0; p--) {
			while (k >= t && cross2(hull[k - 2], hull[k - 1], pts[p - 1]) <= 0.0f) k--;
			hull[k++] = pts[p - 1];
		}
		hull.resize(k - 1);
	}

	const float march = time_ * 26.0f;
	for (size_t k = 0; k < hull.size(); k++) {
		glm::vec2 a = hull[k];
		glm::vec2 b = hull[(k + 1) % hull.size()];
		if (n == 2 && k == 1) break;
		dashed(a, b, i * 0.6f, 10.0f, 7.0f, march);
	}
	// Centre of mass marker: circle + crosshair, slightly pulsing.
	float r = 7.0f + 2.0f * std::sin(time_ * 3.0f);
	ring(com, r, i * 0.8f, 16, 0.0f, kTwoPi, false);
	cross(com, r + 6.0f, i * 0.8f);
	text("COM", com.x + r + 8.0f, com.y - 4.0f, 1.0f, i * 0.8f);
}

void BlobAnalysisOverlay::drawPlexus(float i, int linksPerNode, const ofRectangle & dest) {
	const size_t n = nodes_.size();
	if (n == 0) {
		return;
	}
	const float maxD = std::sqrt(dest.width * dest.width + dest.height * dest.height) * 0.6f;
	std::vector<std::pair<int, int>> pairs; // node indices, a < b
	std::vector<std::pair<float, int>> order;
	for (size_t a = 0; a < n; a++) {
		order.clear();
		for (size_t b = 0; b < n; b++) {
			if (a != b) order.emplace_back(glm::distance(nodes_[a].c, nodes_[b].c), static_cast<int>(b));
		}
		std::sort(order.begin(), order.end());
		for (int k = 0; k < linksPerNode && k < static_cast<int>(order.size()); k++) {
			int lo = std::min(static_cast<int>(a), order[static_cast<size_t>(k)].second);
			int hi = std::max(static_cast<int>(a), order[static_cast<size_t>(k)].second);
			std::pair<int, int> key(lo, hi);
			if (std::find(pairs.begin(), pairs.end(), key) == pairs.end()) pairs.push_back(key);
		}
	}
	for (const auto & p : pairs) {
		const Node & a = nodes_[static_cast<size_t>(p.first)];
		const Node & b = nodes_[static_cast<size_t>(p.second)];
		float d = glm::distance(a.c, b.c);
		float fall = clamp01(1.0f - d / maxD);
		float al = i * (0.18f + 0.55f * fall);
		line(a.c, b.c, al);
		// Travelling pulse: a short bright segment sliding along the link.
		float ph = std::fmod(time_ * 0.28f + hash01(a.id, b.id), 1.0f);
		float ph2 = std::min(1.0f, ph + 0.05f);
		line(a.c + (b.c - a.c) * ph, a.c + (b.c - a.c) * ph2, std::min(1.0f, al * 2.2f));
		// Midpoint tick perpendicular to the link, like a measurement mark.
		glm::vec2 mid = (a.c + b.c) * 0.5f;
		glm::vec2 dir = (d > 0.5f) ? (b.c - a.c) / d : glm::vec2(1.0f, 0.0f);
		glm::vec2 perp(-dir.y, dir.x);
		line(mid - perp * 4.0f, mid + perp * 4.0f, al * 1.3f);
	}
	for (const Node & node : nodes_) {
		float pulse = 0.6f + 0.4f * std::sin(time_ * 4.0f + hash01(node.id, 1) * kTwoPi);
		diamond(node.c, 3.5f, i * (0.55f + 0.4f * pulse));
	}
}

void BlobAnalysisOverlay::drawBrackets(float i, float scanX, float scanI) {
	for (const Node & n : nodes_) {
		const float lock = easeOutCubic(n.lockT / 0.4f);
		const float grow = (1.0f - lock) * 36.0f;
		ofRectangle r = n.rect;
		r.x -= grow;
		r.y -= grow;
		r.width += 2.0f * grow;
		r.height += 2.0f * grow;

		const bool missing = n.missing > 0.12f;
		const float flick = missing ? (std::sin(time_ * 38.0f + static_cast<float>(n.id)) > 0.0f ? 0.9f : 0.2f) : 1.0f;
		float lit = 0.0f;
		if (scanI > 0.02f) {
			float d = scanX - n.rect.getLeft();
			if (d >= 0.0f && d < 240.0f) {
				lit = (1.0f - d / 240.0f) * scanI * 0.6f;
			}
		}
		const float al = clamp01(i * (0.3f + 0.7f * lock) * flick * (0.6f + 0.4f * clamp01(n.conf)) + lit);
		const float arm = std::min(30.0f, std::max(8.0f, std::min(r.width, r.height) * 0.25f));
		cornerBracket(r, arm, al, missing);

		// Faint full outline plus mid-edge ticks.
		if (i > 0.2f) {
			const float x0 = r.getLeft(), x1 = r.getRight(), y0 = r.getTop(), y1 = r.getBottom();
			const float f = al * 0.16f;
			line({ x0, y0 }, { x1, y0 }, f, missing);
			line({ x1, y0 }, { x1, y1 }, f, missing);
			line({ x1, y1 }, { x0, y1 }, f, missing);
			line({ x0, y1 }, { x0, y0 }, f, missing);
			const float mx = (x0 + x1) * 0.5f, my = (y0 + y1) * 0.5f;
			line({ mx, y0 }, { mx, y0 + 5.0f }, al * 0.7f, missing);
			line({ mx, y1 }, { mx, y1 - 5.0f }, al * 0.7f, missing);
			line({ x0, my }, { x0 + 5.0f, my }, al * 0.7f, missing);
			line({ x1, my }, { x1 - 5.0f, my }, al * 0.7f, missing);
			cross(n.c, 4.0f, al * 0.8f, missing);
		}
	}
}

void BlobAnalysisOverlay::drawEvents(float i) {
	for (const Ping & p : pings_) {
		const float t = clamp01(p.age / p.life);
		char buf[24];
		if (!p.lost) {
			// Acquire: ring sweeps outward while four ticks close in.
			const float e = easeOutCubic(t);
			const float al = i * (1.0f - t);
			ring(p.c, 6.0f + 54.0f * e, al, 24, 0.0f, kTwoPi, false);
			const float tick = 16.0f * (1.0f - e) + 4.0f;
			const float off = 20.0f;
			line(p.c + glm::vec2(-off, 0.0f), p.c + glm::vec2(-off + tick, 0.0f), al);
			line(p.c + glm::vec2(off, 0.0f), p.c + glm::vec2(off - tick, 0.0f), al);
			line(p.c + glm::vec2(0.0f, -off), p.c + glm::vec2(0.0f, -off + tick), al);
			line(p.c + glm::vec2(0.0f, off), p.c + glm::vec2(0.0f, off - tick), al);
			std::snprintf(buf, sizeof(buf), "ACQ %02d", p.id % 100);
			text(buf, p.c.x + 30.0f, p.c.y - 26.0f, 1.0f, al);
		} else {
			// Lost: the last box expands and dissolves in the alert tone.
			const float e = easeOutCubic(t);
			const float al = i * (1.0f - t);
			ofRectangle r = p.rect;
			const float g = 4.0f + 26.0f * e;
			r.x -= g;
			r.y -= g;
			r.width += 2.0f * g;
			r.height += 2.0f * g;
			cornerBracket(r, std::max(8.0f, std::min(r.width, r.height) * 0.2f), al, true);
			line(p.c + glm::vec2(-8.0f, -8.0f), p.c + glm::vec2(8.0f, 8.0f), al, true);
			line(p.c + glm::vec2(-8.0f, 8.0f), p.c + glm::vec2(8.0f, -8.0f), al, true);
			std::snprintf(buf, sizeof(buf), "LOST %02d", p.id % 100);
			text(buf, p.rect.getLeft(), p.rect.getTop() - 10.0f, 1.0f, al, true);
		}
	}
}

void BlobAnalysisOverlay::drawTags(float i, const ofRectangle & dest) {
	// Label the biggest tracks first, at most kMaxTags of them, and only
	// where the label fits: each candidate tries the four leader directions
	// (preferred one first) and is dropped if every placement would leave
	// the frame or collide with the readout block / an earlier label. A busy
	// scene therefore shows fewer, cleaner tags instead of a pile of text.
	constexpr int kMaxTags = 5;
	std::vector<const Node *> order;
	for (const Node & n : nodes_) order.push_back(&n);
	std::sort(order.begin(), order.end(), [](const Node * a, const Node * b) { return a->area > b->area; });

	std::vector<ofRectangle> placed;
	placed.emplace_back(dest.getLeft(), dest.getTop(), 430.0f, 160.0f); // readout block is reserved space
	int shown = 0;
	for (const Node * n : order) {
		if (shown >= kMaxTags) break;
		const float reveal = clamp01((n->lockT - 0.2f) / 0.5f);
		if (reveal <= 0.0f) continue;
		const float al = i * reveal;

		char buf[96];
		std::snprintf(buf, sizeof(buf), "ID %02d  A %.1f%%\nT+%.1fs\nX %.2f Y %.2f", n->id % 100, n->area * 100.0f, n->age, n->normC.x, n->normC.y);
		const float tw = font_.isLoaded() ? font_.getStringBoundingBox(buf, 0.0f, 0.0f).width : 160.0f;
		const float th = 3.0f * kLineH;

		const float prefX = (n->rect.getRight() > dest.getRight() - 260.0f) ? -1.0f : 1.0f;
		const float prefY = (n->rect.getTop() < dest.getTop() + 110.0f) ? 1.0f : -1.0f;

		bool ok = false;
		glm::vec2 anchor, p1, p2;
		float tx = 0.0f, ty = 0.0f;
		ofRectangle box;
		for (int attempt = 0; attempt < 4 && !ok; attempt++) {
			const float dirX = (attempt & 1) ? -prefX : prefX;
			const float dirY = (attempt & 2) ? -prefY : prefY;
			anchor = glm::vec2(dirX > 0.0f ? n->rect.getRight() : n->rect.getLeft(), dirY < 0.0f ? n->rect.getTop() : n->rect.getBottom());
			p1 = anchor + glm::vec2(dirX * 22.0f, dirY * 22.0f) * reveal;
			p2 = p1 + glm::vec2(dirX * 16.0f * reveal, 0.0f);
			tx = dirX > 0.0f ? p2.x + 6.0f : p2.x - 6.0f - tw;
			ty = p2.y - 8.0f;
			box = ofRectangle(tx - 4.0f, ty - 22.0f, tw + 8.0f, th + 8.0f);
			ok = box.getLeft() >= dest.getLeft() && box.getRight() <= dest.getRight()
				&& box.getTop() >= dest.getTop() && box.getBottom() <= dest.getBottom();
			for (size_t k = 0; ok && k < placed.size(); k++) {
				ok = !box.intersects(placed[k]);
			}
		}
		if (!ok) continue;
		placed.push_back(box);
		shown++;

		line(anchor, p1, al);
		line(p1, p2, al);
		diamond(anchor, 2.5f, al);
		text(buf, tx, ty, 1.0f, al);
		// Area bar under the text block (full scale = the detector's max area, 5% of frame).
		const float by = ty + th - 6.0f;
		const float bw = std::min(110.0f, tw);
		line({ tx, by }, { tx + bw, by }, al * 0.3f);
		line({ tx, by }, { tx + bw * clamp01(n->area / 0.05f), by }, al);
		line({ tx, by - 3.0f }, { tx, by + 3.0f }, al);
		line({ tx + bw, by - 3.0f }, { tx + bw, by + 3.0f }, al * 0.5f);
	}
}

void BlobAnalysisOverlay::drawReadout(float i, const blobanalysis::State & state, const Readout & ro, const ofRectangle & dest) {
	const float L = dest.getLeft() + 30.0f;
	const float T = dest.getTop() + 46.0f;
	char buf[128];
	text("SPECIMEN TRACK // ANALYSIS", L, T, 1.0f, i);
	std::snprintf(buf, sizeof(buf), "%s   CYC %02d  STG %d/%d", ro.compositionName.c_str(), ro.cycle + 1, ro.stage + 1, ro.stageCount);
	text(buf, L, T + kLineH, 1.0f, i * 0.8f);
	std::snprintf(buf, sizeof(buf), "TRK %02d  FRG %02d/%02d  COV %02d%%", ro.tracks, ro.fragments, ro.maxFragments,
		static_cast<int>(ro.coverage * 100.0f + 0.5f));
	text(buf, L, T + 2.0f * kLineH, 1.0f, i * 0.8f);
	std::snprintf(buf, sizeof(buf), "PAL %s  BG %s", ro.paletteName.c_str(), ro.lookName.c_str());
	text(buf, L, T + 3.0f * kLineH, 1.0f, i * 0.6f);

	// Growth gauge: how far through this cycle's build-up the scene is.
	const float gy = T + 3.0f * kLineH + 24.0f;
	const float gw = 220.0f;
	line({ L, gy }, { L + gw, gy }, i * 0.25f);
	line({ L, gy }, { L + gw * clamp01(state.growth), gy }, i);
	for (int k = 0; k <= ro.stageCount; k++) {
		float x = L + gw * static_cast<float>(k) / static_cast<float>(std::max(1, ro.stageCount));
		line({ x, gy - 3.0f }, { x, gy + 3.0f }, i * 0.5f);
	}
	text("GROWTH", L + gw + 10.0f, gy + 6.0f, 1.0f, i * 0.6f);

	// Track-count sparkline, bottom-left.
	const float sx = dest.getLeft() + 30.0f;
	const float sy = dest.getBottom() - 56.0f;
	const float sw = 220.0f, sh = 44.0f;
	line({ sx, sy }, { sx + sw, sy }, i * 0.3f);
	line({ sx, sy - sh }, { sx, sy }, i * 0.3f);
	int peak = 4;
	for (int v : history_) peak = std::max(peak, v);
	const size_t count = history_.size();
	if (count >= 2) {
		for (size_t k = 1; k < count; k++) {
			glm::vec2 a(sx + sw * static_cast<float>(k - 1) / static_cast<float>(kHistory - 1), sy - sh * static_cast<float>(history_[k - 1]) / static_cast<float>(peak));
			glm::vec2 b(sx + sw * static_cast<float>(k) / static_cast<float>(kHistory - 1), sy - sh * static_cast<float>(history_[k]) / static_cast<float>(peak));
			line(a, b, i * 0.9f);
		}
		glm::vec2 last(sx + sw * static_cast<float>(count - 1) / static_cast<float>(kHistory - 1), sy - sh * static_cast<float>(history_.back()) / static_cast<float>(peak));
		diamond(last, 2.5f, i);
	}
	text("TRACKS / 24s", sx, sy + 24.0f, 1.0f, i * 0.6f);
}

void BlobAnalysisOverlay::drawBanner(const blobanalysis::State & state, const Readout & ro, const ofRectangle & dest) {
	const float a = state.transitionPulse;
	if (a <= 0.03f) {
		return;
	}
	// Composition-change banner: the new composition's name, centred at the
	// top, with brackets that close in as it fades.
	const std::string title = "// " + ro.compositionName + " //";
	const float scale = 1.6f;
	const float w = (font_.isLoaded() ? font_.stringWidth(title) : static_cast<float>(title.size()) * 12.0f) * scale;
	const float x = dest.getCenter().x - w * 0.5f;
	const float y = dest.getTop() + dest.height * 0.30f;
	text(title, x, y, scale, a);
	const float spread = 14.0f + 40.0f * (1.0f - a);
	line({ x - spread, y - 18.0f }, { x - spread, y + 6.0f }, a);
	line({ x - spread, y - 18.0f }, { x - spread + 8.0f, y - 18.0f }, a);
	line({ x - spread, y + 6.0f }, { x - spread + 8.0f, y + 6.0f }, a);
	line({ x + w + spread, y - 18.0f }, { x + w + spread, y + 6.0f }, a);
	line({ x + w + spread, y - 18.0f }, { x + w + spread - 8.0f, y - 18.0f }, a);
	line({ x + w + spread, y + 6.0f }, { x + w + spread - 8.0f, y + 6.0f }, a);
}

// ---------------------------------------------------------------------- draw

void BlobAnalysisOverlay::draw(const blobanalysis::State & state, const Readout & readout, const ofRectangle & dest) {
	ensureFont();
	lines_.clear();
	lines_.setMode(OF_PRIMITIVE_LINES);
	texts_.clear();

	const blobanalysis::ElementSet & e = state.elements;

	// Safe area for everything anchored to the frame itself (corner marks,
	// edge ticks, readouts, banner, scan sweep): the runtime shows the 16:9
	// scene through a ~1.52:1 chrome viewport with a centre cover-crop
	// (MediaViewportMesh), hiding roughly 7% of each side, and the viewport
	// has chamfered corners. Blob-attached elements (brackets, plexus, tags'
	// anchors) deliberately use the full frame since they follow the video.
	const ofRectangle safe(dest.x + dest.width * 0.095f, dest.y + dest.height * 0.05f, dest.width * 0.81f, dest.height * 0.90f);

	const float scanPeriod = 7.0f;
	const float scanX = safe.getLeft() + safe.width * std::fmod(time_ / scanPeriod, 1.0f);

	if (e[blobanalysis::kFrame] > 0.02f) drawFrame(e[blobanalysis::kFrame], safe, state.transitionPulse);
	if (e[blobanalysis::kScan] > 0.02f) drawScan(e[blobanalysis::kScan], safe, scanX);
	if (e[blobanalysis::kRings] > 0.02f) drawRings(e[blobanalysis::kRings]);
	if (e[blobanalysis::kHull] > 0.02f) drawHull(e[blobanalysis::kHull]);
	if (e[blobanalysis::kTrails] > 0.02f) drawTrails(e[blobanalysis::kTrails]);
	if (e[blobanalysis::kPlexus] > 0.02f) drawPlexus(e[blobanalysis::kPlexus], state.linksPerNode, dest);
	if (e[blobanalysis::kBrackets] > 0.02f) drawBrackets(e[blobanalysis::kBrackets], scanX, e[blobanalysis::kScan]);
	if (e[blobanalysis::kEvents] > 0.02f) drawEvents(e[blobanalysis::kEvents]);
	if (e[blobanalysis::kTags] > 0.02f) drawTags(e[blobanalysis::kTags], safe);
	if (e[blobanalysis::kReadout] > 0.02f) drawReadout(e[blobanalysis::kReadout], state, readout, safe);
	drawBanner(state, readout, safe);

	ofPushStyle();
	ofEnableAlphaBlending();
	ofDisableDepthTest();
	ofSetColor(255);
	ofSetLineWidth(2.0f);
	if (lines_.getNumVertices() > 0) {
		lines_.draw();
	}
	if (!font_.isLoaded()) {
		texts_.clear(); // face missing: lines still draw, text is skipped
	}
	for (const PendingText & t : texts_) {
		const ofFloatColor & c = t.alert ? kAlert : kAccent;
		ofSetColor(static_cast<int>(c.r * 255.0f), static_cast<int>(c.g * 255.0f), static_cast<int>(c.b * 255.0f), static_cast<int>(t.alpha * 255.0f));
		ofPushMatrix();
		ofTranslate(t.x, t.y);
		ofScale(t.scale, t.scale);
		font_.drawString(t.s, 0.0f, 0.0f);
		ofPopMatrix();
	}
	ofPopStyle();
}
