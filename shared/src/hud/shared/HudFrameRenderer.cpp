#include "HudFrameRenderer.h"
#include "HudUtils.h"

namespace hud {

void HudFrameRenderer::draw(const HudBounds & b, const WidgetColors & colors, const FrameOptions & options, float t) const {
	if (!options.showFrame || options.style == FrameStyle::None) return;

	ofPushStyle();
	ofNoFill();
	// Stroke width scales with bounds (su) rather than staying a fixed pixel
	// width, so the frame actually reads as responsive at small/large sizes.
	ofSetLineWidth(std::max(0.5f, su(b, options.thickness)));
	ofColor c = scaledAlpha(colors.primary, options.opacity);

	if (colors.background.a > 0) {
		ofFill();
		ofSetColor(colors.background);
		ofDrawRectangle(b.x, b.y, b.width, b.height);
		ofNoFill();
	}

	switch (options.style) {
	case FrameStyle::Corners: drawCorners(b, c, options); break;
	case FrameStyle::Box: drawBox(b, c, options); break;
	case FrameStyle::Brackets: drawBrackets(b, c, options); break;
	case FrameStyle::Organic: drawOrganic(b, c, options, t); break;
	case FrameStyle::Registration: drawRegistrationMarks(b, c, options); break;
	case FrameStyle::None: break;
	}

	if (options.showTicks) drawTicks(b, scaledAlpha(colors.muted, options.opacity), options);
	if (options.showScanLines) {
		ofSetColor(scaledAlpha(colors.muted, 0.22f * options.opacity));
		float y = b.y + std::fmod(t * 24.0f, std::max(1.0f, b.height));
		ofDrawLine(b.x + options.padding, y, b.x + b.width - options.padding, y);
	}
	ofPopStyle();
}

void HudFrameRenderer::drawCorners(const HudBounds & b, const ofColor & c, const FrameOptions & o) const {
	float p = o.padding;
	float l = std::min(o.cornerLength, b.minDim() * 0.22f);
	ofSetColor(c);
	ofDrawLine(b.x + p, b.y + p, b.x + p + l, b.y + p);
	ofDrawLine(b.x + p, b.y + p, b.x + p, b.y + p + l);
	ofDrawLine(b.x + b.width - p, b.y + p, b.x + b.width - p - l, b.y + p);
	ofDrawLine(b.x + b.width - p, b.y + p, b.x + b.width - p, b.y + p + l);
	ofDrawLine(b.x + p, b.y + b.height - p, b.x + p + l, b.y + b.height - p);
	ofDrawLine(b.x + p, b.y + b.height - p, b.x + p, b.y + b.height - p - l);
	ofDrawLine(b.x + b.width - p, b.y + b.height - p, b.x + b.width - p - l, b.y + b.height - p);
	ofDrawLine(b.x + b.width - p, b.y + b.height - p, b.x + b.width - p, b.y + b.height - p - l);
}

void HudFrameRenderer::drawBox(const HudBounds & b, const ofColor & c, const FrameOptions & o) const {
	ofSetColor(c);
	ofDrawRectangle(b.x + o.padding, b.y + o.padding, b.width - 2 * o.padding, b.height - 2 * o.padding);
}

void HudFrameRenderer::drawBrackets(const HudBounds & b, const ofColor & c, const FrameOptions & o) const {
	drawCorners(b, c, o);
	ofSetColor(c);
	float midY = b.y + b.height * 0.5f;
	float midX = b.x + b.width * 0.5f;
	float l = std::min(o.cornerLength * 0.8f, b.minDim() * 0.15f);
	ofDrawLine(b.x + o.padding, midY - l, b.x + o.padding, midY + l);
	ofDrawLine(b.x + b.width - o.padding, midY - l, b.x + b.width - o.padding, midY + l);
	ofDrawLine(midX - l, b.y + o.padding, midX + l, b.y + o.padding);
	ofDrawLine(midX - l, b.y + b.height - o.padding, midX + l, b.y + b.height - o.padding);
}

void HudFrameRenderer::drawOrganic(const HudBounds & b, const ofColor & c, const FrameOptions & o, float t) const {
	ofSetColor(c);
	ofPolyline line;
	int n = 18;
	float p = o.padding;
	for (int i = 0; i <= n; ++i) {
		float u = i / static_cast<float>(n);
		float wobble = std::sin(u * TWO_PI * 2.0f + t * 0.8f) * 2.0f;
		line.addVertex(b.x + p + u * (b.width - 2 * p), b.y + p + wobble);
	}
	for (int i = 0; i <= n; ++i) {
		float u = i / static_cast<float>(n);
		float wobble = std::cos(u * TWO_PI * 2.0f + t * 0.7f) * 2.0f;
		line.addVertex(b.x + b.width - p + wobble, b.y + p + u * (b.height - 2 * p));
	}
	for (int i = 0; i <= n; ++i) {
		float u = i / static_cast<float>(n);
		float wobble = std::sin(u * TWO_PI * 2.0f + t * 0.6f) * 2.0f;
		line.addVertex(b.x + b.width - p - u * (b.width - 2 * p), b.y + b.height - p + wobble);
	}
	for (int i = 0; i <= n; ++i) {
		float u = i / static_cast<float>(n);
		float wobble = std::cos(u * TWO_PI * 2.0f + t * 0.5f) * 2.0f;
		line.addVertex(b.x + p + wobble, b.y + b.height - p - u * (b.height - 2 * p));
	}
	line.close();
	line.draw();
}

// Technical-drawing crop marks: a small "+" straddling each corner point,
// distinct from drawCorners' L-brackets (which sit inset from the corner
// rather than centered on it).
void HudFrameRenderer::drawRegistrationMarks(const HudBounds & b, const ofColor & c, const FrameOptions & o) const {
	float p = o.padding;
	float l = std::min(o.cornerLength, b.minDim() * 0.18f);
	ofSetColor(c);

	auto drawMark = [&](float cx, float cy) {
		ofDrawLine(cx - l, cy, cx + l, cy);
		ofDrawLine(cx, cy - l, cx, cy + l);
		ofNoFill();
		ofDrawCircle(cx, cy, l * 0.35f);
	};

	drawMark(b.x + p, b.y + p);
	drawMark(b.x + b.width - p, b.y + p);
	drawMark(b.x + p, b.y + b.height - p);
	drawMark(b.x + b.width - p, b.y + b.height - p);
}

void HudFrameRenderer::drawTicks(const HudBounds & b, const ofColor & c, const FrameOptions & o) const {
	ofSetColor(c);
	float p = o.padding;
	float tickH = std::max(2.0f, su(b, 4.0f));
	int ticks = std::max(4, static_cast<int>(b.width / 45.0f));
	for (int i = 1; i < ticks; ++i) {
		float x = b.x + p + (b.width - 2 * p) * (i / static_cast<float>(ticks));
		ofDrawLine(x, b.y + p, x, b.y + p + tickH);
		ofDrawLine(x, b.y + b.height - p, x, b.y + b.height - p - tickH);
	}
}

} // namespace hud
