#include "BEFragment.h"
#include "BESettings.h"
#include "glm/glm.hpp"
#include "ofGraphics.h"
#include "ofMath.h"
#include <algorithm>
#include <cmath>

void BEFragment::setupBE(Fragment::Params params, GeometryType geometryType_, int canvasW, int canvasH) {
	geometryType = geometryType_;
	targetRadius = params.maskRadius;

	switch (geometryType) {
	case GeometryType::SLIVER:
		params.arrivalDuration = SLIDE_IN_DURATION;
		break;
	case GeometryType::CIRCLE:
		params.arrivalDuration = IRIS_OPEN_DURATION;
		params.circularMask = true;
		break;
	default:
		params.arrivalDuration = SCAN_REVEAL_DURATION + BORDER_DRAW_DURATION;
		break;
	}

	Fragment::setup(params);

	if (geometryType == GeometryType::SLIVER) {
		glm::vec2 center = glm::vec2(params.bounds.getCenter());
		float distLeft = center.x;
		float distRight = canvasW - center.x;
		float distTop = center.y;
		float distBottom = canvasH - center.y;
		float minDist = std::min({ distLeft, distRight, distTop, distBottom });

		glm::vec2 finalPos = glm::vec2(params.bounds.getPosition());
		if (minDist == distLeft) {
			slideStartPos = finalPos - glm::vec2(params.bounds.width, 0);
		} else if (minDist == distRight) {
			slideStartPos = finalPos + glm::vec2(params.bounds.width, 0);
		} else if (minDist == distTop) {
			slideStartPos = finalPos - glm::vec2(0, params.bounds.height);
		} else {
			slideStartPos = finalPos + glm::vec2(0, params.bounds.height);
		}
	}
}

void BEFragment::drawArrival(float /*t*/) const {
	float elapsed = getStateElapsedSeconds();
	switch (geometryType) {
	case GeometryType::SLIVER:
		drawSlideIn(elapsed);
		break;
	case GeometryType::CIRCLE:
		drawIrisOpen(elapsed);
		break;
	default:
		drawScanReveal(elapsed);
		break;
	}
}

void BEFragment::drawStable() const {
	if (geometryType == GeometryType::CIRCLE) {
		glm::vec2 pos = getDrawPosition();
		glm::vec2 center = pos + glm::vec2(bounds.width * 0.5f, bounds.height * 0.5f);

		ofSetCircleResolution(96);

		// Draw the live video/image fill first.
		drawMaskedFill(targetRadius);

		// Ghost ring persists from STABLE onward and fades with the fragment.
		ofSetColor(255, 255, 255, static_cast<int>(80 * opacity));
		ofNoFill();
		ofDrawCircle(center.x, center.y, targetRadius * GHOST_RING_SCALE);
		ofFill();
		return;
	}

	if (geometryType == GeometryType::SLIVER) {
		float sinceArrival = getStateElapsedSeconds();
		if (sinceArrival <= AFTERIMAGE_FADE_DURATION) {
			float alpha = 1.0f - (sinceArrival / AFTERIMAGE_FADE_DURATION);
			ofSetColor(255, 255, 255, static_cast<int>(255 * alpha));
			ofNoFill();
			ofDrawRectangle(slideStartPos.x, slideStartPos.y, bounds.width, bounds.height);
			ofFill();
		}
	}

	Fragment::drawStable();
}

void BEFragment::drawScanReveal(float elapsed) const {
	glm::vec2 pos = getDrawPosition();

	if (elapsed <= SCAN_REVEAL_DURATION) {
		float t = ofClamp(elapsed / SCAN_REVEAL_DURATION, 0.0f, 1.0f);
		float revealedH = bounds.height * t;
		float revealFrac = revealedH / bounds.height;

		if (hasMedia()) {
			ofSetColor(255, static_cast<int>(255 * opacity));
			videoTexture->drawSubsection(pos.x, pos.y, bounds.width, revealedH,
				videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height * revealFrac);
		} else {
			ofSetColor(placeholderColor, static_cast<int>(255 * opacity));
			ofDrawRectangle(pos.x, pos.y, bounds.width, revealedH);
		}

		ofSetColor(255, 255, 255, 200);
		ofDrawLine(pos.x, pos.y + revealedH, pos.x + bounds.width, pos.y + revealedH);
	} else {
		if (hasMedia()) {
			ofSetColor(255, static_cast<int>(255 * opacity));
			videoTexture->drawSubsection(pos.x, pos.y, bounds.width, bounds.height,
				videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height);
		} else {
			ofSetColor(placeholderColor, static_cast<int>(255 * opacity));
			ofDrawRectangle(pos.x, pos.y, bounds.width, bounds.height);
		}

		float borderT = ofClamp((elapsed - SCAN_REVEAL_DURATION) / BORDER_DRAW_DURATION, 0.0f, 1.0f);
		drawClockwiseBorder(pos, bounds.width, bounds.height, borderT);
	}
}

void BEFragment::drawClockwiseBorder(glm::vec2 pos, float w, float h, float t) const {
	glm::vec2 corners[5] = {
		pos,
		pos + glm::vec2(w, 0),
		pos + glm::vec2(w, h),
		pos + glm::vec2(0, h),
		pos
	};

	float perimeter = 2.0f * (w + h);
	float target = perimeter * ofClamp(t, 0.0f, 1.0f);

	ofSetColor(255, 255, 255, 200);
	float drawn = 0.0f;
	for (int i = 0; i < 4 && drawn < target; i++) {
		glm::vec2 a = corners[i];
		glm::vec2 b = corners[i + 1];
		float segLen = glm::distance(a, b);
		float remain = target - drawn;
		if (remain >= segLen) {
			ofDrawLine(a.x, a.y, b.x, b.y);
			drawn += segLen;
		} else {
			glm::vec2 p = a + (b - a) * (remain / segLen);
			ofDrawLine(a.x, a.y, p.x, p.y);
			drawn = target;
		}
	}
}

void BEFragment::drawSlideIn(float elapsed) const {
	float t = ofClamp(elapsed / SLIDE_IN_DURATION, 0.0f, 1.0f);
	float tEased = 1.0f - (1.0f - t) * (1.0f - t);

	glm::vec2 finalPos = glm::vec2(bounds.getPosition());
	glm::vec2 pos = glm::mix(slideStartPos, finalPos, tEased);

	if (hasMedia()) {
		ofSetColor(255, static_cast<int>(255 * opacity));
		videoTexture->drawSubsection(pos.x, pos.y, bounds.width, bounds.height,
			videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height);
	} else {
		ofSetColor(placeholderColor, static_cast<int>(255 * opacity));
		ofDrawRectangle(pos.x, pos.y, bounds.width, bounds.height);
	}
}

void BEFragment::drawIrisOpen(float elapsed) const {
	float t = ofClamp(elapsed / IRIS_OPEN_DURATION, 0.0f, 1.0f);
	float tEased = 1.0f - std::pow(1.0f - t, 3.0f);

	float fillRadius = targetRadius * tEased;
	float outlineRadius = targetRadius * std::min(1.0f, tEased + IRIS_OPEN_LEAD_FRACTION);

	glm::vec2 pos = getDrawPosition();
	glm::vec2 center = pos + glm::vec2(bounds.width * 0.5f, bounds.height * 0.5f);

	ofSetCircleResolution(96);

	// This now works for live video because Fragment::drawMaskedFill() uses a
	// textured circle mesh for circularMask fragments instead of shader discard.
	drawMaskedFill(fillRadius);

	// Lead outline drawn slightly ahead of the fill.
	ofSetColor(255, 255, 255, 200);
	ofNoFill();
	ofDrawCircle(center.x, center.y, outlineRadius);
	ofFill();
}
