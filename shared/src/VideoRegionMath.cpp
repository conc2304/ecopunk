#include "VideoRegionMath.h"
#include <algorithm>
#include <cmath>

// Deliberately does not include VideoRegionMath.h's ofRectangle-dependent
// wrappers' transitive headers here beyond what VideoRegionMath.h already
// pulls in for Rect's declaration site — every function body below touches
// only Rect (plain floats), so this translation unit has no openFrameworks
// dependency and links standalone (see test/Makefile.tests).

namespace VideoRegionMath {

	Rect normalizedToSourcePixelsRect(const Rect & normalizedBounds, float sourceWidth, float sourceHeight) {
		return Rect {
			normalizedBounds.x * sourceWidth,
			normalizedBounds.y * sourceHeight,
			normalizedBounds.width * sourceWidth,
			normalizedBounds.height * sourceHeight
		};
	}

	Rect clampRectToBounds(const Rect & rect, const Rect & bounds) {
		if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
			return Rect { bounds.x, bounds.y, 0.0f, 0.0f };
		}

		float w = std::min(std::max(rect.width, 0.0f), bounds.width);
		float h = std::min(std::max(rect.height, 0.0f), bounds.height);

		float x = std::min(std::max(rect.x, bounds.x), bounds.x + bounds.width - w);
		float y = std::min(std::max(rect.y, bounds.y), bounds.y + bounds.height - h);

		return Rect { x, y, w, h };
	}

	Rect computeCropFillSourceRect(float texWidth, float texHeight, const Rect & destRect) {
		if (texWidth <= 0.0f || texHeight <= 0.0f || destRect.width <= 0.0f || destRect.height <= 0.0f) {
			return Rect { 0.0f, 0.0f, texWidth, texHeight };
		}

		float texAspect = texWidth / texHeight;
		float destAspect = destRect.width / destRect.height;

		if (texAspect > destAspect) {
			float srcW = texHeight * destAspect;
			return Rect { (texWidth - srcW) * 0.5f, 0.0f, srcW, texHeight };
		}

		float srcH = texWidth / destAspect;
		return Rect { 0.0f, (texHeight - srcH) * 0.5f, texWidth, srcH };
	}

	Rect mapNormalizedSourceRectToScreen(
		const Rect & normalizedRect,
		float sourceWidth,
		float sourceHeight,
		const Rect & cropFillSourceRect,
		const Rect & destRect) {
		if (cropFillSourceRect.width <= 0.0f || cropFillSourceRect.height <= 0.0f) {
			return Rect { destRect.x, destRect.y, 0.0f, 0.0f };
		}

		Rect sourcePx = normalizedToSourcePixelsRect(normalizedRect, sourceWidth, sourceHeight);

		float scale = destRect.width / cropFillSourceRect.width;

		return Rect {
			destRect.x + (sourcePx.x - cropFillSourceRect.x) * scale,
			destRect.y + (sourcePx.y - cropFillSourceRect.y) * scale,
			sourcePx.width * scale,
			sourcePx.height * scale
		};
	}

	float rectIoU(const Rect & a, const Rect & b) {
		if (a.width <= 0.0f || a.height <= 0.0f || b.width <= 0.0f || b.height <= 0.0f) {
			return 0.0f;
		}

		float ix0 = std::max(a.x, b.x);
		float iy0 = std::max(a.y, b.y);
		float ix1 = std::min(a.x + a.width, b.x + b.width);
		float iy1 = std::min(a.y + a.height, b.y + b.height);

		float iw = std::max(0.0f, ix1 - ix0);
		float ih = std::max(0.0f, iy1 - iy0);
		float intersection = iw * ih;
		if (intersection <= 0.0f) {
			return 0.0f;
		}

		float unionArea = (a.width * a.height) + (b.width * b.height) - intersection;
		return unionArea > 0.0f ? intersection / unionArea : 0.0f;
	}

	void rectCenter(const Rect & r, float & outX, float & outY) {
		outX = r.x + r.width * 0.5f;
		outY = r.y + r.height * 0.5f;
	}

	float distance(float ax, float ay, float bx, float by) {
		float dx = ax - bx;
		float dy = ay - by;
		return std::sqrt(dx * dx + dy * dy);
	}

	Rect lerpRect(const Rect & from, const Rect & to, float positionFactor, float sizeFactor) {
		auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

		float fromCx, fromCy, toCx, toCy;
		rectCenter(from, fromCx, fromCy);
		rectCenter(to, toCx, toCy);

		float cx = lerp(fromCx, toCx, positionFactor);
		float cy = lerp(fromCy, toCy, positionFactor);
		float w = lerp(from.width, to.width, sizeFactor);
		float h = lerp(from.height, to.height, sizeFactor);

		return Rect { cx - w * 0.5f, cy - h * 0.5f, w, h };
	}

	std::vector<int> greedyAssociateByDistance(
		const std::vector<Rect> & trackRects,
		const std::vector<Rect> & detectionRects,
		float maxDistance,
		float minIoU) {
		struct Candidate {
			int trackIdx;
			int detIdx;
			float distance;
		};

		std::vector<Candidate> candidates;
		for (size_t ti = 0; ti < trackRects.size(); ti++) {
			float tcx, tcy;
			rectCenter(trackRects[ti], tcx, tcy);

			for (size_t di = 0; di < detectionRects.size(); di++) {
				float dcx, dcy;
				rectCenter(detectionRects[di], dcx, dcy);

				float dist = distance(tcx, tcy, dcx, dcy);
				float iou = rectIoU(trackRects[ti], detectionRects[di]);

				if (dist <= maxDistance || iou >= minIoU) {
					candidates.push_back({ static_cast<int>(ti), static_cast<int>(di), dist });
				}
			}
		}

		std::sort(candidates.begin(), candidates.end(), [](const Candidate & a, const Candidate & b) {
			return a.distance < b.distance;
		});

		std::vector<int> matchedDetectionForTrack(trackRects.size(), -1);
		std::vector<bool> detUsed(detectionRects.size(), false);

		for (const Candidate & c : candidates) {
			if (matchedDetectionForTrack[c.trackIdx] != -1 || detUsed[c.detIdx]) {
				continue;
			}
			matchedDetectionForTrack[c.trackIdx] = c.detIdx;
			detUsed[c.detIdx] = true;
		}

		return matchedDetectionForTrack;
	}

}
