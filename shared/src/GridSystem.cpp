#include "GridSystem.h"
#include "ofMath.h"
#include "ofMathConstants.h"
#include "ofRandomDistributions.h"
#include "ofUtils.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
	constexpr float PHI = 1.61803398875f;

	constexpr float VLINE_JITTER_FACTOR = 0.04f; // fraction of canvas_width
	constexpr float LINE_MIN_SEPARATION = 20.0f; // px
	constexpr float CANVAS_EDGE_MARGIN = 80.0f; // px

	constexpr float INHERITANCE_WEIGHT = 0.72f;
	constexpr float FRESH_WEIGHT = 0.28f;

	constexpr float VLINE_DRIFT_AMPLITUDE = 8.0f; // px
	constexpr float HLINE_DRIFT_AMPLITUDE = 5.0f; // px
	constexpr float DRIFT_SPEED = 0.04f; // radians/second

	constexpr float FRAGMENT_LINE_MIN_DIST = 20.0f; // px
	constexpr float EXTENT_TRUNCATE_PROB = 0.40f; // each, left vs right
	constexpr float EXTENT_TRUNCATE_MAX = 0.60f;

	constexpr float OPACITY_STRUCTURAL_MIN = 0.25f;
	constexpr float OPACITY_STRUCTURAL_MAX = 0.35f;
	constexpr float OPACITY_FRAGMENT_MIN = 0.15f;
	constexpr float OPACITY_FRAGMENT_MAX = 0.22f;
	constexpr float OPACITY_SUBDIVISION_MIN = 0.18f;
	constexpr float OPACITY_SUBDIVISION_MAX = 0.28f;

	constexpr float SUBDIVISION_ZONE_THRESH = 0.38f; // fraction of canvas_width
	constexpr float SUBDIVISION_PROB = 0.25f;
	constexpr int SUBDIVISION_MAX_PER_CYCLE = 2;
	constexpr float SUBDIVISION_GROW_SPEED = 600.0f; // px/second

	constexpr float FRAGMENT_LINE_FADE_DELAY_MIN = 0.2f;
	constexpr float FRAGMENT_LINE_FADE_DELAY_MAX = 0.4f;
	constexpr float FRAGMENT_LINE_FADE_DUR_MIN = 0.8f;
	constexpr float FRAGMENT_LINE_FADE_DUR_MAX = 1.2f;
	constexpr float STRUCTURAL_DISSOLVE_DUR = 2.0f;
	constexpr float DIVIDER_DISSOLVE_DUR = 1.5f;
}

void GridSystem::setup(int canvasW_, int canvasH_, float dividerX_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
	dividerX = dividerX_;
}

std::vector<float> GridSystem::previousHomePositions(const std::vector<GridLine> & lines) const {
	std::vector<float> result;
	for (const auto & l : lines) {
		if (l.isStructural) {
			result.push_back(l.homePosition);
		}
	}
	return result;
}

float GridSystem::nearestLineDistance(float pos, const std::vector<GridLine> & lines) const {
	float minDist = std::numeric_limits<float>::max();
	for (const auto & l : lines) {
		minDist = std::min(minDist, std::abs(l.position - pos));
	}
	return minDist;
}

void GridSystem::startNewCycle() {
	vLines.clear();
	hLines.clear();
	occupied.clear();
	subdivisionEventsThisCycle = 0;
	structuralDissolving = false;
	structuralDissolveElapsed = 0.0f;
	dividerDissolving = false;
	dividerDissolveElapsed = 0.0f;
	dividerOpacity = 1.0f;

	float W = static_cast<float>(canvasW);
	float H = static_cast<float>(canvasH);

	// ── Vertical: golden-ratio subdivision ──
	std::vector<float> freshV;
	float primarySplit = W / PHI;
	freshV.push_back(primarySplit);
	freshV.push_back(primarySplit / PHI);
	float rightZoneWidth = W - primarySplit;
	freshV.push_back(primarySplit + rightZoneWidth / PHI);

	for (int extra = 0; extra < 5; extra++) {
		if (ofRandom(1.0f) < 0.65f) {
			std::vector<float> sorted = freshV;
			sorted.push_back(0.0f);
			sorted.push_back(W);
			std::sort(sorted.begin(), sorted.end());

			float bestWidth = -1.0f;
			float bestStart = 0.0f;
			float bestEnd = W;
			for (size_t i = 0; i + 1 < sorted.size(); i++) {
				float w = sorted[i + 1] - sorted[i];
				if (w > bestWidth) {
					bestWidth = w;
					bestStart = sorted[i];
					bestEnd = sorted[i + 1];
				}
			}
			float zoneW = bestEnd - bestStart;
			freshV.push_back(bestStart + zoneW / PHI);
		}
	}

	for (auto & pos : freshV) {
		pos += ofRandom(-W * VLINE_JITTER_FACTOR, W * VLINE_JITTER_FACTOR);
		pos = ofClamp(pos, CANVAS_EDGE_MARGIN, W - CANVAS_EDGE_MARGIN);
	}
	std::sort(freshV.begin(), freshV.end());

	for (size_t i = 0; i < freshV.size(); i++) {
		float fresh = freshV[i];
		float home = (i < previousVHome.size())
			? previousVHome[i] * INHERITANCE_WEIGHT + fresh * FRESH_WEIGHT
			: fresh;

		GridLine line;
		line.isStructural = true;
		line.homePosition = home;
		line.position = home;
		line.driftPhase = ofRandom(0.0f, TWO_PI);
		line.targetOpacity = ofRandom(OPACITY_STRUCTURAL_MIN, OPACITY_STRUCTURAL_MAX);
		line.opacity = line.targetOpacity;
		line.anim = GridLine::Anim::STEADY;
		vLines.push_back(line);
	}

	// ── Horizontal: weighted-random thirds ──
	std::vector<float> freshH;
	float zoneStarts[3] = { 0.0f, H / 3.0f, H * 2.0f / 3.0f };
	float zoneEnds[3] = { H / 3.0f, H * 2.0f / 3.0f, H };

	for (int z = 0; z < 3; z++) {
		if (ofRandom(1.0f) < 0.30f) {
			continue; // this zone gets no line
		}
		int linesInZone = (ofRandom(1.0f) < 0.5f) ? 1 : 2;
		float zoneMid = (zoneStarts[z] + zoneEnds[z]) * 0.5f;
		float sigma = (zoneEnds[z] - zoneStarts[z]) * 0.28f;

		for (int n = 0; n < linesInZone; n++) {
			float candidate = ofClamp(ofRandomGaussian(zoneMid, sigma), zoneStarts[z], zoneEnds[z]);
			bool tooClose = false;
			for (float existing : freshH) {
				if (std::abs(existing - candidate) < LINE_MIN_SEPARATION) {
					tooClose = true;
					break;
				}
			}
			if (!tooClose) {
				freshH.push_back(candidate);
			}
		}
	}

	while (freshH.size() < 2) {
		float candidate = ofRandom(CANVAS_EDGE_MARGIN, H - CANVAS_EDGE_MARGIN);
		bool tooClose = false;
		for (float existing : freshH) {
			if (std::abs(existing - candidate) < LINE_MIN_SEPARATION) {
				tooClose = true;
				break;
			}
		}
		if (!tooClose) {
			freshH.push_back(candidate);
		}
	}
	std::sort(freshH.begin(), freshH.end());

	for (size_t i = 0; i < freshH.size(); i++) {
		float fresh = freshH[i];
		float home = (i < previousHHome.size())
			? previousHHome[i] * INHERITANCE_WEIGHT + fresh * FRESH_WEIGHT
			: fresh;

		GridLine line;
		line.isStructural = true;
		line.homePosition = home;
		line.position = home;
		line.driftPhase = ofRandom(0.0f, TWO_PI);
		line.targetOpacity = ofRandom(OPACITY_STRUCTURAL_MIN, OPACITY_STRUCTURAL_MAX);
		line.opacity = line.targetOpacity;
		line.anim = GridLine::Anim::STEADY;
		hLines.push_back(line);
	}
}

void GridSystem::clear() {
	// Drop occupancy and anything fragment-derived/subdivision; structural
	// homePositions are snapshotted separately (see beginStructuralDissolve())
	// so they survive into the next startNewCycle() blend.
	occupied.clear();
	vLines.erase(std::remove_if(vLines.begin(), vLines.end(),
					 [](const GridLine & l) { return !l.isStructural; }),
		vLines.end());
	hLines.erase(std::remove_if(hLines.begin(), hLines.end(),
					 [](const GridLine & l) { return !l.isStructural; }),
		hLines.end());
}

void GridSystem::updateLine(GridLine & line, float dt, float driftAmplitude, float driftSpeed) {
	if (line.isStructural && line.anim != GridLine::Anim::DISSOLVING) {
		float drift = std::sin(ofGetElapsedTimef() * driftSpeed + line.driftPhase) * driftAmplitude;
		line.position = line.homePosition + drift;
	}

	if (line.growing) {
		float step = (SUBDIVISION_GROW_SPEED * dt) / static_cast<float>(canvasW);
		if (line.growsFromRight) {
			line.extentStart = std::max(line.growTargetExtentStart, line.extentStart - step);
			if (line.extentStart <= line.growTargetExtentStart) {
				line.growing = false;
			}
		} else {
			line.extentEnd = std::min(line.growTargetExtentEnd, line.extentEnd + step);
			if (line.extentEnd >= line.growTargetExtentEnd) {
				line.growing = false;
			}
		}
	}

	switch (line.anim) {
	case GridLine::Anim::WAITING:
		line.animTimer -= dt;
		if (line.animTimer <= 0.0f) {
			line.anim = GridLine::Anim::FADING_IN;
			line.animTimer = 0.0f;
		}
		break;

	case GridLine::Anim::FADING_IN: {
		line.animTimer += dt;
		float t = ofClamp(line.animTimer / line.fadeDuration, 0.0f, 1.0f);
		float eased = 1.0f - (1.0f - t) * (1.0f - t); // ease-out quad
		line.opacity = eased * line.targetOpacity;
		if (t >= 1.0f) {
			line.anim = GridLine::Anim::STEADY;
		}
		break;
	}

	case GridLine::Anim::STEADY:
		line.opacity = line.targetOpacity;
		break;

	case GridLine::Anim::DISSOLVING: {
		line.animTimer += dt;
		float t = ofClamp(line.animTimer / line.fadeDuration, 0.0f, 1.0f);
		line.opacity = line.targetOpacity * (1.0f - t);
		break;
	}
	}
}

void GridSystem::update(float dt) {
	for (auto & l : vLines) {
		updateLine(l, dt, VLINE_DRIFT_AMPLITUDE, DRIFT_SPEED);
	}
	for (auto & l : hLines) {
		updateLine(l, dt, HLINE_DRIFT_AMPLITUDE, DRIFT_SPEED);
	}

	auto isGone = [](const GridLine & l) {
		return l.anim == GridLine::Anim::DISSOLVING && l.opacity <= 0.0f;
	};
	vLines.erase(std::remove_if(vLines.begin(), vLines.end(), isGone), vLines.end());
	hLines.erase(std::remove_if(hLines.begin(), hLines.end(), isGone), hLines.end());

	if (structuralDissolving) {
		structuralDissolveElapsed += dt;
		if (!dividerDissolving && structuralDissolveElapsed >= STRUCTURAL_DISSOLVE_DUR) {
			dividerDissolving = true;
		}
	}
	if (dividerDissolving) {
		dividerDissolveElapsed += dt;
		dividerOpacity = std::max(0.0f, 1.0f - dividerDissolveElapsed / DIVIDER_DISSOLVE_DUR);
	}
}

bool GridSystem::isRectFree(const ofRectangle & bounds, float maxOverlapFraction) const {
	float area = bounds.width * bounds.height;
	if (area <= 0.0f) {
		return true;
	}
	float occupiedArea = 0.0f;
	for (const auto & r : occupied) {
		ofRectangle inter = bounds.getIntersection(r.bounds);
		occupiedArea += inter.width * inter.height;
	}
	return (occupiedArea / area) <= maxOverlapFraction;
}

void GridSystem::reserve(int fragmentId, const ofRectangle & bounds) {
	occupied.push_back({ fragmentId, bounds });
}

void GridSystem::releaseFragment(int fragmentId) {
	occupied.erase(std::remove_if(occupied.begin(), occupied.end(),
					   [fragmentId](const OccupiedRect & r) { return r.fragmentId == fragmentId; }),
		occupied.end());
}

std::vector<ofRectangle> GridSystem::getOccupiedRects() const {
	std::vector<ofRectangle> result;
	result.reserve(occupied.size());
	for (const auto & r : occupied) {
		result.push_back(r.bounds);
	}
	return result;
}

std::vector<float> GridSystem::getSnapXPositions() const {
	std::vector<float> result = { 0.0f, static_cast<float>(canvasW), dividerX };
	for (const auto & l : vLines) {
		result.push_back(l.position);
	}
	std::sort(result.begin(), result.end());
	return result;
}

std::vector<float> GridSystem::getSnapYPositions() const {
	std::vector<float> result = { 0.0f, static_cast<float>(canvasH) };
	for (const auto & l : hLines) {
		result.push_back(l.position);
	}
	std::sort(result.begin(), result.end());
	return result;
}

float GridSystem::addStructuralX(float pos) {
	pos = ofClamp(pos, CANVAS_EDGE_MARGIN, canvasW - CANVAS_EDGE_MARGIN);
	GridLine line;
	line.isStructural = true;
	line.homePosition = pos;
	line.position = pos;
	line.driftPhase = ofRandom(0.0f, TWO_PI);
	line.targetOpacity = ofRandom(OPACITY_STRUCTURAL_MIN, OPACITY_STRUCTURAL_MAX);
	line.opacity = line.targetOpacity;
	line.anim = GridLine::Anim::STEADY;
	vLines.push_back(line);
	return pos;
}

float GridSystem::addStructuralY(float pos) {
	pos = ofClamp(pos, CANVAS_EDGE_MARGIN, canvasH - CANVAS_EDGE_MARGIN);
	GridLine line;
	line.isStructural = true;
	line.homePosition = pos;
	line.position = pos;
	line.driftPhase = ofRandom(0.0f, TWO_PI);
	line.targetOpacity = ofRandom(OPACITY_STRUCTURAL_MIN, OPACITY_STRUCTURAL_MAX);
	line.opacity = line.targetOpacity;
	line.anim = GridLine::Anim::STEADY;
	hLines.push_back(line);
	return pos;
}

void GridSystem::contributeFragmentEdges(int fragmentId, const ofRectangle & bounds,
	const std::vector<ofRectangle> & otherBounds) {
	float W = static_cast<float>(canvasW);
	float H = static_cast<float>(canvasH);

	auto tryAddVertical = [&](float x) {
		if (nearestLineDistance(x, vLines) < FRAGMENT_LINE_MIN_DIST) {
			return;
		}
		if (std::abs(x - dividerX) < FRAGMENT_LINE_MIN_DIST) {
			return; // keep clear of the divider's visual role
		}

		float leftExtent = 0.0f;
		float rightExtent = W;
		for (const auto & other : otherBounds) {
			float otherRight = other.getRight();
			float otherLeft = other.getLeft();
			if (otherRight > leftExtent && otherRight < x) {
				leftExtent = otherRight;
			}
			if (otherLeft < rightExtent && otherLeft > x) {
				rightExtent = otherLeft;
			}
		}

		float roll = ofRandom(1.0f);
		if (roll < EXTENT_TRUNCATE_PROB) {
			leftExtent += ofRandom(0.0f, (x - leftExtent) * EXTENT_TRUNCATE_MAX);
		} else if (roll < EXTENT_TRUNCATE_PROB * 2.0f) {
			rightExtent -= ofRandom(0.0f, (rightExtent - x) * EXTENT_TRUNCATE_MAX);
		}

		GridLine line;
		line.position = x;
		line.fragmentId = fragmentId;
		line.extentStart = ofClamp(leftExtent / W, 0.0f, 1.0f);
		line.extentEnd = ofClamp(rightExtent / W, 0.0f, 1.0f);
		line.targetOpacity = ofRandom(OPACITY_FRAGMENT_MIN, OPACITY_FRAGMENT_MAX);
		line.opacity = 0.0f;
		line.anim = GridLine::Anim::WAITING;
		line.animTimer = ofRandom(FRAGMENT_LINE_FADE_DELAY_MIN, FRAGMENT_LINE_FADE_DELAY_MAX);
		line.fadeDuration = ofRandom(FRAGMENT_LINE_FADE_DUR_MIN, FRAGMENT_LINE_FADE_DUR_MAX);
		vLines.push_back(line);
	};

	auto tryAddHorizontal = [&](float y) {
		if (nearestLineDistance(y, hLines) < FRAGMENT_LINE_MIN_DIST) {
			return;
		}

		float topExtent = 0.0f;
		float bottomExtent = H;
		for (const auto & other : otherBounds) {
			float otherBottom = other.getBottom();
			float otherTop = other.getTop();
			if (otherBottom > topExtent && otherBottom < y) {
				topExtent = otherBottom;
			}
			if (otherTop < bottomExtent && otherTop > y) {
				bottomExtent = otherTop;
			}
		}

		float roll = ofRandom(1.0f);
		if (roll < EXTENT_TRUNCATE_PROB) {
			topExtent += ofRandom(0.0f, (y - topExtent) * EXTENT_TRUNCATE_MAX);
		} else if (roll < EXTENT_TRUNCATE_PROB * 2.0f) {
			bottomExtent -= ofRandom(0.0f, (bottomExtent - y) * EXTENT_TRUNCATE_MAX);
		}

		GridLine line;
		line.position = y;
		line.fragmentId = fragmentId;
		line.extentStart = ofClamp(topExtent / H, 0.0f, 1.0f);
		line.extentEnd = ofClamp(bottomExtent / H, 0.0f, 1.0f);
		line.targetOpacity = ofRandom(OPACITY_FRAGMENT_MIN, OPACITY_FRAGMENT_MAX);
		line.opacity = 0.0f;
		line.anim = GridLine::Anim::WAITING;
		line.animTimer = ofRandom(FRAGMENT_LINE_FADE_DELAY_MIN, FRAGMENT_LINE_FADE_DELAY_MAX);
		line.fadeDuration = ofRandom(FRAGMENT_LINE_FADE_DUR_MIN, FRAGMENT_LINE_FADE_DUR_MAX);
		hLines.push_back(line);
	};

	tryAddVertical(bounds.getLeft());
	tryAddVertical(bounds.getRight());
	tryAddHorizontal(bounds.getTop());
	tryAddHorizontal(bounds.getBottom());
}

void GridSystem::maybeSubdivide() {
	if (subdivisionEventsThisCycle >= SUBDIVISION_MAX_PER_CYCLE) {
		return;
	}

	std::vector<float> sorted = { 0.0f, static_cast<float>(canvasW) };
	for (const auto & l : vLines) {
		sorted.push_back(l.position);
	}
	std::sort(sorted.begin(), sorted.end());

	for (size_t i = 0; i + 1 < sorted.size(); i++) {
		float left = sorted[i];
		float right = sorted[i + 1];
		float zoneW = right - left;
		if (zoneW <= canvasW * SUBDIVISION_ZONE_THRESH) {
			continue;
		}
		if (ofRandom(1.0f) >= SUBDIVISION_PROB) {
			continue;
		}

		float newPos = ofLerp(left, right, ofRandom(0.35f, 0.65f));
		bool growsFromRight = ofRandom(1.0f) < 0.5f;

		GridLine line;
		line.position = newPos;
		line.isSubdivision = true;
		line.fragmentId = -1;
		line.targetOpacity = ofRandom(OPACITY_SUBDIVISION_MIN, OPACITY_SUBDIVISION_MAX);
		line.opacity = 0.0f;
		line.anim = GridLine::Anim::FADING_IN;
		line.animTimer = 0.0f;

		float growDistancePx = growsFromRight ? (right - newPos) : (newPos - left);
		line.fadeDuration = std::max(0.3f, growDistancePx / SUBDIVISION_GROW_SPEED);
		line.growing = true;
		line.growsFromRight = growsFromRight;

		float leftNorm = left / canvasW;
		float rightNorm = right / canvasW;
		if (growsFromRight) {
			line.extentStart = rightNorm;
			line.extentEnd = rightNorm;
			line.growTargetExtentStart = leftNorm;
			line.growTargetExtentEnd = rightNorm;
		} else {
			line.extentStart = leftNorm;
			line.extentEnd = leftNorm;
			line.growTargetExtentStart = leftNorm;
			line.growTargetExtentEnd = rightNorm;
		}

		vLines.push_back(line);
		subdivisionEventsThisCycle++;
		return;
	}
}

void GridSystem::beginLineDissolveForFragment(int fragmentId, float fadeDuration) {
	for (auto & l : vLines) {
		if (l.fragmentId == fragmentId) {
			l.anim = GridLine::Anim::DISSOLVING;
			l.animTimer = 0.0f;
			l.fadeDuration = fadeDuration;
		}
	}
	for (auto & l : hLines) {
		if (l.fragmentId == fragmentId) {
			l.anim = GridLine::Anim::DISSOLVING;
			l.animTimer = 0.0f;
			l.fadeDuration = fadeDuration;
		}
	}
}

void GridSystem::beginStructuralDissolve() {
	if (structuralDissolving) {
		return;
	}

	// Snapshot now, before DISSOLVING lines get erased by update() — this is
	// what survives the dissolve/reset boundary for the next startNewCycle().
	previousVHome = previousHomePositions(vLines);
	previousHHome = previousHomePositions(hLines);

	structuralDissolving = true;
	structuralDissolveElapsed = 0.0f;

	auto markDissolving = [](GridLine & l) {
		if (l.isStructural || (l.isSubdivision && l.fragmentId == -1)) {
			l.anim = GridLine::Anim::DISSOLVING;
			l.animTimer = 0.0f;
			l.fadeDuration = STRUCTURAL_DISSOLVE_DUR;
		}
	};
	for (auto & l : vLines) {
		markDissolving(l);
	}
	for (auto & l : hLines) {
		markDissolving(l);
	}
}

bool GridSystem::isDissolveFadeComplete() const {
	return dividerDissolving && dividerDissolveElapsed >= DIVIDER_DISSOLVE_DUR;
}
