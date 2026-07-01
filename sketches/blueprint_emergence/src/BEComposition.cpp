#include "BEComposition.h"
#include "BELFOLanes.h"
#include "BESettings.h"
#include "BETriggers.h"
#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"
#include "hud/DataCardWidget.h"
#include "hud/GaugeWidget.h"
#include "hud/HudWidget.h"
#include "MotionExtraction.h"
#include "ofLog.h"
#include "ofMath.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

BEComposition::BEComposition() = default;
BEComposition::~BEComposition() = default;

namespace {
	int triggerIdx(BETrigger t) { return static_cast<int>(t); }
	int lfoIdx(BELFOLane l) { return static_cast<int>(l); }

	// GridState still samples a fixed coarse resolution (GRID_COLS x GRID_ROWS)
	// for activity tracking, independent of GridSystem's now-dynamic line grid.
	int gridStateCol(float x, int canvasW) {
		return static_cast<int>(x / (static_cast<float>(canvasW) / GRID_COLS));
	}
	int gridStateRow(float y, int canvasH) {
		return static_cast<int>(y / (static_cast<float>(canvasH) / GRID_ROWS));
	}
}

void BEComposition::setupBE(GridSystem * grid_, VideoSampler * videoSampler_, int canvasW_, int canvasH_) {
	videoSampler = videoSampler_;
	canvasW = canvasW_;
	canvasH = canvasH_;

	CompositionBase::Timing timing;
	timing.cycleDurationMin = CYCLE_DURATION_MIN;
	timing.cycleDurationMax = CYCLE_DURATION_MAX;
	timing.blankDuration = BLANK_DURATION;
	timing.densityDuration = DENSITY_DURATION;
	timing.dissolveDuration = DISSOLVE_DURATION;
	timing.resetHoldDuration = RESET_HOLD_DURATION;
	timing.placementIntervalMin = PLACEMENT_INTERVAL_MIN;
	timing.placementIntervalMax = PLACEMENT_INTERVAL_MAX;
	timing.placementIntervalDense = PLACEMENT_INTERVAL_DENSE;
	timing.dissolveFadeMin = DISSOLVE_FADE_MIN;
	timing.dissolveFadeMax = DISSOLVE_FADE_MAX;
	timing.maxFragments = MAX_FRAGMENTS;

	CompositionBase::setup(grid_, timing);
}

void BEComposition::onCycleStart() {
	zoneALight = randRangeF(0.0f, 1.0f) < 0.3f; // §06: Zone A uses GROUND_LIGHT in 30% of cycles

	circleSpawnedThisCycle = false;
	nextFragmentId = 0;

	for (int i = 0; i < NUM_SLOTS; i++) {
		slots[i].phase = SlotPhase::EMPTY;
		slots[i].timer = 0.0f;
		// Small per-slot stagger so all 4 don't attempt their first spawn on
		// the exact same frame once PLACEMENT begins.
		slots[i].timerTarget = static_cast<float>(i) * randRangeF(1.0f, 3.0f);
		slots[i].fragmentIndex = -1; // `fragments` was just cleared by startCycle()
	}

	secondsSinceLastPlacement = 0.0f;
	densityHighActive = false;
	densityCriticalActive = false;
	zoneScoreBoostRemaining = 0;
	gridDimCurrent = 0.0f;
	gridDimTarget = 0.0f;
	if (gridState) {
		gridState->clear();
	}

	// grid->clear() (called by CompositionBase::startCycle() before this)
	// already drops all occupancy, including any HUD reservation, so just
	// reset the widget's own state to match.
	hudWidget.reset();
	hudPhase = HudPhase::SILENCE;
	hudTimer = 0.0f;
	hudTimerTarget = randRangeF(SLOT_SILENCE_MIN, SLOT_SILENCE_MAX);

	ofLogNotice("BEComposition") << "cycle start, slots=" << NUM_SLOTS;
}

void BEComposition::setTriggerBus(TriggerBus * bus) {
	triggerBus = bus;
	if (triggerBus == nullptr) {
		return;
	}

	triggerBus->addListener(triggerIdx(BETrigger::DENSITY_HIGH), [this](int) {
		densityHighActive = true;
		ofLogNotice("BEComposition") << "DENSITY_HIGH: placement interval -" << (1.0f - TRIGGER_DENSITY_HIGH_INTERVAL_SCALE) * 100.0f << "%, desat max -> " << TRIGGER_DENSITY_HIGH_DESAT_MAX;
	});

	triggerBus->addListener(triggerIdx(BETrigger::DENSITY_CRITICAL), [this](int) {
		densityCriticalActive = true;
		gridDimTarget = TRIGGER_DENSITY_CRIT_GRID_DIM;
		forceEnterDensity();
		ofLogNotice("BEComposition") << "DENSITY_CRITICAL: forcing early DENSITY phase";
	});

	triggerBus->addListener(triggerIdx(BETrigger::ZONE_IMBALANCE), [this](int) {
		zoneScoreBoostRemaining = 2;
		ofLogNotice("BEComposition") << "ZONE_IMBALANCE: biasing next " << zoneScoreBoostRemaining << " placements toward the sparse zone";
	});
}

BECompositionState BEComposition::getState() const {
	BECompositionState s;
	s.fragmentCount = static_cast<int>(fragments.size());
	s.zoneACount = countZoneFragments(true);
	s.zoneBCount = countZoneFragments(false);
	s.secondsSinceLastPlacement = secondsSinceLastPlacement;
	s.circleHasBeenPlaced = circleSpawnedThisCycle;
	s.currentPhase = static_cast<int>(getPhase());
	s.cycleElapsed = getCycleElapsedSeconds();
	return s;
}

float BEComposition::lfoDesatNudgeForGroup(int group) const {
	if (lfoBank == nullptr) {
		return 0.0f;
	}
	BELFOLane lane = (group % 2 == 0) ? BELFOLane::FRAG_DESAT_OFFSET_A : BELFOLane::FRAG_DESAT_OFFSET_B;
	return lfoBank->get(lfoIdx(lane)) * LFO_DESAT_NUDGE_MAX;
}

void BEComposition::evaluateStateTriggers() {
	if (triggerBus == nullptr) {
		return;
	}

	int zoneA = countZoneFragments(true);
	int zoneB = countZoneFragments(false);
	int total = static_cast<int>(fragments.size());

	triggerBus->setConditionActive(triggerIdx(BETrigger::ZONE_IMBALANCE),
		std::abs(zoneA - zoneB) >= TRIGGER_ZONE_IMBALANCE_DIFF);
	triggerBus->setConditionActive(triggerIdx(BETrigger::DENSITY_HIGH),
		total >= TRIGGER_DENSITY_HIGH_COUNT);
	triggerBus->setConditionActive(triggerIdx(BETrigger::DENSITY_CRITICAL),
		total >= TRIGGER_DENSITY_CRIT_COUNT);
	triggerBus->setConditionActive(triggerIdx(BETrigger::LONG_SILENCE),
		secondsSinceLastPlacement >= TRIGGER_LONG_SILENCE_SECS);
}

void BEComposition::onUpdate(float dt) {
	secondsSinceLastPlacement += dt;

	if (gridState) {
		gridState->update();
	}

	evaluateStateTriggers();

	if (gridDimCurrent != gridDimTarget) {
		gridDimCurrent += (gridDimTarget - gridDimCurrent) * ofClamp(dt / 3.0f, 0.0f, 1.0f);
	}

	if (lfoBank) {
		for (auto & f : fragments) {
			static_cast<BEFragment *>(f.get())->setDesatNudge(lfoDesatNudgeForGroup(static_cast<BEFragment *>(f.get())->getEffectGroup()));
		}
	}

	updateSlots(dt);
	updateHudWidget(dt);
}

// Quadrant-style slot lifecycle, driven every frame regardless of which
// PLACEMENT_INTERVAL_* CompositionBase used to use for organic growth — see
// usesAutomaticPlacementTimer() (false for this composition).
void BEComposition::updateSlots(float dt) {
	bool acceptingNewSpawns = (getPhase() == CyclePhase::PLACEMENT || getPhase() == CyclePhase::DENSITY);

	for (int i = 0; i < NUM_SLOTS; i++) {
		SlotState & slot = slots[i];

		switch (slot.phase) {
		case SlotPhase::EMPTY:
			slot.timer += dt;
			if (acceptingNewSpawns && slot.timer >= slot.timerTarget) {
				bool placed = spawnFragmentInSlot(i);
				if (placed) {
					slot.phase = SlotPhase::ARRIVING;
					slot.timer = 0.0f;
					slot.timerTarget = 0.0f;
					ofLogNotice("BEComposition") << "slot " << i << " -> ARRIVING (fragmentIndex=" << slot.fragmentIndex << ")";
				} else {
					// No valid spot this attempt — brief backoff, try again soon
					// rather than re-running the full candidate search every frame.
					slot.timer = 0.0f;
					slot.timerTarget = 1.0f;
				}
			}
			break;

		case SlotPhase::ARRIVING: {
			Fragment * frag = (slot.fragmentIndex >= 0 && slot.fragmentIndex < static_cast<int>(fragments.size()))
				? fragments[slot.fragmentIndex].get()
				: nullptr;
			if (frag && frag->getState() != Fragment::State::ARRIVING) {
				// Arrival animation finished — begin the guaranteed-visible hold
				// window. This is what fixes "slides in and immediately gets
				// removed": dissolve eligibility no longer depends on a global
				// random schedule, it depends on this slot's own timer.
				slot.phase = SlotPhase::HOLD;
				slot.timer = 0.0f;
				slot.timerTarget = randRangeF(SLOT_HOLD_MIN, SLOT_HOLD_MAX);
				ofLogNotice("BEComposition") << "slot " << i << " -> HOLD for " << slot.timerTarget << "s";
			}
			break;
		}

		case SlotPhase::HOLD: {
			slot.timer += dt;
			if (slot.timer >= slot.timerTarget || !acceptingNewSpawns) {
				Fragment * frag = (slot.fragmentIndex >= 0 && slot.fragmentIndex < static_cast<int>(fragments.size()))
					? fragments[slot.fragmentIndex].get()
					: nullptr;
				if (frag && !frag->isDead()) {
					float fade = randRangeF(SLOT_DISSOLVE_FADE_MIN, SLOT_DISSOLVE_FADE_MAX);
					frag->startDissolve(fade);
					grid->beginLineDissolveForFragment(frag->getId(), fade);
				}
				slot.phase = SlotPhase::DISSOLVING;
				ofLogNotice("BEComposition") << "slot " << i << " -> DISSOLVING after holding " << slot.timer
											 << "s (target was " << slot.timerTarget << "s, acceptingNewSpawns=" << acceptingNewSpawns << ")";
			}
			break;
		}

		case SlotPhase::DISSOLVING: {
			Fragment * frag = (slot.fragmentIndex >= 0 && slot.fragmentIndex < static_cast<int>(fragments.size()))
				? fragments[slot.fragmentIndex].get()
				: nullptr;
			if (frag && frag->isDead()) {
				grid->releaseFragment(frag->getId());
				slot.phase = SlotPhase::EMPTY;
				slot.timer = 0.0f;
				slot.timerTarget = randRangeF(SLOT_SILENCE_MIN, SLOT_SILENCE_MAX);
				ofLogNotice("BEComposition") << "slot " << i << " -> EMPTY (silence " << slot.timerTarget << "s)";
			}
			break;
		}
		}
	}
}

// HUD widget — independent of the 4 video slots; occasionally claims
// unoccupied grid space (via the same GridSystem occupancy fragments use)
// instead of a video fragment. Max one active at a time.
void BEComposition::updateHudWidget(float dt) {
	bool acceptingNewSpawns = (getPhase() == CyclePhase::PLACEMENT || getPhase() == CyclePhase::DENSITY);

	if (hudWidget) {
		hudWidget->update(dt);
	}

	switch (hudPhase) {
	case HudPhase::SILENCE:
		hudTimer += dt;
		if (acceptingNewSpawns && hudTimer >= hudTimerTarget) {
			if (randRangeF(0.0f, 1.0f) < HUD_WIDGET_PROBABILITY && trySpawnHudWidget()) {
				hudPhase = HudPhase::ACTIVE;
				hudTimer = 0.0f;
				hudTimerTarget = randRangeF(HUD_HOLD_MIN, HUD_HOLD_MAX);
			} else {
				hudTimer = 0.0f;
				hudTimerTarget = randRangeF(SLOT_SILENCE_MIN, SLOT_SILENCE_MAX);
			}
		}
		break;

	case HudPhase::ACTIVE:
		hudTimer += dt;
		if (hudTimer >= hudTimerTarget || !acceptingNewSpawns) {
			grid->releaseFragment(HUD_FRAGMENT_ID);
			hudWidget.reset();
			hudPhase = HudPhase::SILENCE;
			hudTimer = 0.0f;
			hudTimerTarget = randRangeF(SLOT_SILENCE_MIN, SLOT_SILENCE_MAX);
		}
		break;
	}
}

bool BEComposition::trySpawnHudWidget() {
	// Roughly fragment-sized — big enough to read, small enough to fit in
	// the same grid a video fragment would have used.
	glm::vec2 wRange = { 213.3f * 1.5f, 213.3f * 3.0f };
	glm::vec2 hRange = { 90.0f * 1.5f, 90.0f * 3.0f };

	std::vector<float> xs = grid->getSnapXPositions();
	std::vector<float> ys = grid->getSnapYPositions();

	for (int attempt = 0; attempt < PLACEMENT_MAX_ATTEMPTS; attempt++) {
		float x1, x2, y1, y2;
		if (!findSnapSpan(xs, wRange.x, wRange.y, x1, x2)) continue;
		if (!findSnapSpan(ys, hRange.x, hRange.y, y1, y2)) continue;

		ofRectangle candidate(x1, y1, x2 - x1, y2 - y1);
		if (!grid->isRectFree(candidate, 0.10f)) continue;

		std::unique_ptr<hud::HudWidget> widget;
		if (randRangeI(0, 1) == 0) {
			auto card = std::make_unique<hud::DataCardWidget>();
			hud::DataCardOptions opts;
			opts.title = "FRAGMENT LOG";
			opts.subtitle = "OBSERVATION ACTIVE";
			opts.value = ofToString(static_cast<int>(fragments.size())) + "/" + ofToString(NUM_SLOTS);
			opts.meter = ofRandom(0.3f, 0.9f);
			card->setOptions(opts);
			widget = std::move(card);
		} else {
			auto gauge = std::make_unique<hud::GaugeWidget>();
			hud::GaugeOptions opts;
			opts.label = "ACTIVITY";
			opts.value = gridState ? gridState->getAverageActivity() : 0.5f;
			gauge->setOptions(opts);
			widget = std::move(gauge);
		}

		widget->setup();
		widget->setBounds(candidate.x, candidate.y, candidate.width, candidate.height);

		grid->reserve(HUD_FRAGMENT_ID, candidate);
		hudWidget = std::move(widget);
		return true;
	}
	return false;
}

GeometryType BEComposition::pickGeometryType() const {
	int roll = randRangeI(0, 89); // RECT 50 / SLIVER 20 / SQUARE 20, sums to 90
	if (roll < 50) return GeometryType::RECT;
	if (roll < 70) return GeometryType::SLIVER;
	return GeometryType::SQUARE;
}

// Size ranges translated from the old 1280x720 / 6x8-cell scheme into pixel
// ranges, since fragments now snap to whatever lines/edges currently exist
// rather than a fixed cell multiple. Reference cell ~= 213x90px.
void BEComposition::pickSizeRange(GeometryType type, glm::vec2 & wRange, glm::vec2 & hRange) const {
	constexpr float cellW = 213.3f;
	constexpr float cellH = 90.0f;
	switch (type) {
	case GeometryType::RECT:
		wRange = { cellW * 1.0f, cellW * 4.0f };
		hRange = { cellH * 1.0f, cellH * 3.0f };
		break;
	case GeometryType::SQUARE:
		wRange = { cellW * 1.7f, cellW * 2.3f };
		hRange = { cellH * 1.7f, cellH * 2.3f };
		break;
	case GeometryType::SLIVER:
		if (randRangeI(0, 1) == 0) {
			wRange = { cellW * 0.8f, cellW * 1.2f };
			hRange = { cellH * 4.0f, cellH * 6.0f };
		} else {
			wRange = { cellW * 4.0f, cellW * 6.0f };
			hRange = { cellH * 0.8f, cellH * 1.2f };
		}
		break;
	case GeometryType::CIRCLE:
		break; // handled separately in placeCircleFragment()
	}
}

int BEComposition::countZoneFragments(bool zoneA) const {
	float dividerX = grid->getDividerX();
	int count = 0;
	for (const auto & f : fragments) {
		glm::vec2 center = glm::vec2(f->getBounds().getCenter());
		bool inZoneA = center.x < dividerX;
		if (inZoneA == zoneA) {
			count++;
		}
	}
	return count;
}

ofColor BEComposition::pickPlaceholderColor() const {
	return ofColor::fromHsb(randRangeI(0, 255), 120, 180);
}

// Finds the pair of adjacent-or-near candidate positions in `positions`
// (already sorted) whose span best matches [lo, hi]. Returns false if no
// pair falls within range.
bool BEComposition::findSnapSpan(const std::vector<float> & positions, float lo, float hi, float & outStart, float & outEnd) const {
	std::vector<std::pair<float, float>> inRange;
	for (size_t i = 0; i < positions.size(); i++) {
		for (size_t j = i + 1; j < positions.size(); j++) {
			float span = positions[j] - positions[i];
			if (span > hi) break; // positions sorted, spans only grow from here
			if (span >= lo) {
				inRange.push_back({ positions[i], positions[j] });
			}
		}
	}
	if (inRange.empty()) {
		return false;
	}
	const auto & chosen = inRange[randRangeI(0, static_cast<int>(inRange.size()) - 1)];
	outStart = chosen.first;
	outEnd = chosen.second;
	return true;
}

std::vector<ofRectangle> BEComposition::otherFragmentBounds(const Fragment * exclude) const {
	std::vector<ofRectangle> result;
	result.reserve(fragments.size());
	for (const auto & f : fragments) {
		if (f.get() != exclude && f) {
			result.push_back(f->getBounds());
		}
	}
	return result;
}

void BEComposition::requestVideoTexture(BEFragment * fragment, int pxW, int pxH) const {
	if (videoSampler == nullptr || !videoSampler->hasMedia()) return;
	// Guard against the video player having reported dimensions but not yet
	// decoded its first frame — storing an unallocated texture pointer causes
	// "texture has not been allocated" errors when drawSubsection() runs.
	if (!videoSampler->getTexture().isAllocated()) return;

	int videoW = videoSampler->getVideoWidth();
	int videoH = videoSampler->getVideoHeight();
	if (videoW <= 0 || videoH <= 0) return;

	// Pick a crop whose aspect ratio matches the fragment so no stretching occurs.
	float fragAspect = static_cast<float>(pxW) / static_cast<float>(pxH);
	float videoAspect = static_cast<float>(videoW) / static_cast<float>(videoH);

	float cropW, cropH;
	if (fragAspect >= videoAspect) {
		cropW = static_cast<float>(videoW);
		cropH = cropW / fragAspect;
	} else {
		cropH = static_cast<float>(videoH);
		cropW = cropH * fragAspect;
	}

	int maxCropX = videoW - static_cast<int>(cropW);
	int maxCropY = videoH - static_cast<int>(cropH);
	int cropX = (maxCropX > 0) ? (rand() % maxCropX) : 0;
	int cropY = (maxCropY > 0) ? (rand() % maxCropY) : 0;

	fragment->setVideoSource(&videoSampler->getTexture(), ofRectangle(cropX, cropY, cropW, cropH));
}

bool BEComposition::placeCircleFragment(int slotIndex) {
	float cellW = static_cast<float>(canvasW) / GRID_COLS;
	float cellH = static_cast<float>(canvasH) / GRID_ROWS;
	float avgCell = (cellW + cellH) * 0.5f;
	float regionHalf = CIRCLE_REGION_SIZE * 0.5f;

	// The circle is a hero/compositional event, not a normal occupancy-constrained
	// fragment. It is allowed to overlap existing material heavily, matching the
	// reference image where the circular crop dominates and cuts across the grid.
	for (int attempt = 0; attempt < CIRCLE_PLACEMENT_MAX_ATTEMPTS; attempt++) {
		float diameterFromCells = randRangeF(CIRCLE_DIAMETER_MIN_CELLS, CIRCLE_DIAMETER_MAX_CELLS) * avgCell;
		float minHeroDiameter = static_cast<float>(canvasW) * 0.36f;
		float maxHeroDiameter = static_cast<float>(canvasW) * 0.50f;
		float diameter = ofClamp(diameterFromCells, minHeroDiameter, maxHeroDiameter);
		float radius = diameter * 0.5f;

		float minCx = radius + cellW * 0.25f;
		float maxCx = std::min(static_cast<float>(canvasW) - radius - cellW * 0.25f, static_cast<float>(canvasW) * 0.48f);
		if (maxCx < minCx) maxCx = minCx;

		float minCy = radius + cellH * 0.25f;
		float maxCy = static_cast<float>(canvasH) - radius - cellH * 0.25f;
		if (maxCy < minCy) maxCy = minCy;

		float cx = randRangeF(minCx, maxCx);
		float cy = ofClamp(CIRCLE_REGION_CENTER_Y + randRangeF(-regionHalf, regionHalf), minCy, maxCy);

		ofRectangle pixelBounds(cx - radius, cy - radius, diameter, diameter);

		float visibleLeft = std::max(pixelBounds.getLeft(), 0.0f);
		float visibleTop = std::max(pixelBounds.getTop(), 0.0f);
		float visibleRight = std::min(pixelBounds.getRight(), static_cast<float>(canvasW));
		float visibleBottom = std::min(pixelBounds.getBottom(), static_cast<float>(canvasH));
		float visibleW = visibleRight - visibleLeft;
		float visibleH = visibleBottom - visibleTop;
		if (visibleW <= 0.0f || visibleH <= 0.0f) continue;
		float visibleFraction = (visibleW * visibleH) / (diameter * diameter);
		if (visibleFraction < 0.70f) continue;

		// Reserve only a small core near the circle center. Reserving the full
		// bounding box would make this hero circle block too much of the remaining
		// canvas, especially because it is intentionally large and overlapping.
		ofRectangle coreRect(cx - cellW * 1.5f, cy - cellH * 1.5f, cellW * 3.0f, cellH * 3.0f);

		float phaseOffset = randRangeF(0.0f, glm::two_pi<float>());
		ofColor color = pickPlaceholderColor();

		Fragment::Params params;
		params.bounds = pixelBounds;
		params.placeholderColor = color;
		params.phaseOffset = phaseOffset;
		params.driftAmp = glm::vec2(DRIFT_AMP_X, DRIFT_AMP_Y);
		params.driftFreq = glm::vec2(DRIFT_FREQ_X, DRIFT_FREQ_Y);
		params.desaturateRampDuration = DESATURATE_RAMP_DURATION;
		params.desaturateMax = densityHighActive ? TRIGGER_DENSITY_HIGH_DESAT_MAX : DESATURATE_MAX;
		params.circularMask = true;
		params.maskRadius = radius;

		auto frag = std::make_unique<BEFragment>();
		frag->setupBE(params, GeometryType::CIRCLE, canvasW, canvasH);
		frag->setId(nextFragmentId++);
		requestVideoTexture(frag.get(), static_cast<int>(diameter), static_cast<int>(diameter));

		Fragment * newFragPtr = frag.get();
		grid->reserve(newFragPtr->getId(), coreRect);
		// No notifyFragmentPlaced() — circle skips the measurement-line step (§06).
		// Circle also doesn't contributeFragmentEdges() — its circular silhouette
		// has no meaningful "edge" for the rectilinear grid to follow.

		int & claimedIndex = slots[slotIndex].fragmentIndex;
		if (claimedIndex >= 0 && claimedIndex < static_cast<int>(fragments.size())) {
			notifyFragmentRemoved(fragments[claimedIndex].get());
			fragments[claimedIndex] = std::move(frag);
		} else {
			fragments.push_back(std::move(frag));
			claimedIndex = static_cast<int>(fragments.size()) - 1;
		}

		if (gridState) {
			gridState->accumulateRect(gridStateCol(coreRect.x, canvasW), gridStateRow(coreRect.y, canvasH),
				std::max(1, static_cast<int>(coreRect.width / (canvasW / static_cast<float>(GRID_COLS)))),
				std::max(1, static_cast<int>(coreRect.height / (canvasH / static_cast<float>(GRID_ROWS)))), 1.0f);
		}
		if (triggerBus) {
			triggerBus->fireImmediate(triggerIdx(BETrigger::CIRCLE_PLACED));
		}
		secondsSinceLastPlacement = 0.0f;
		circleSpawnedThisCycle = true;

		ofLogNotice("BEComposition") << "circle placed at (" << cx << "," << cy
									 << ") radius=" << radius << "px visibleFraction=" << visibleFraction;
		return true;
	}

	ofLogNotice("BEComposition") << "circle placement failed; will retry on a future slot respawn";
	return false;
}

bool BEComposition::spawnFragmentInSlot(int slotIndex) {
	// Rare hero circle event — at most once per cycle, low probability per
	// spawn attempt rather than tied to a cumulative organic-placement count
	// (which doesn't mean much once slots can each respawn many times).
	if (!circleSpawnedThisCycle && randRangeF(0.0f, 1.0f) < CIRCLE_SPAWN_PROBABILITY) {
		if (placeCircleFragment(slotIndex)) {
			return true;
		}
		// Couldn't find room for the circle this attempt — fall through to a
		// normal placement so the slot doesn't sit empty for nothing.
	}

	GeometryType type = pickGeometryType();
	glm::vec2 wRange, hRange;
	pickSizeRange(type, wRange, hRange);

	std::vector<float> xs = grid->getSnapXPositions();
	std::vector<float> ys = grid->getSnapYPositions();

	// This slot's own (about-to-be-replaced) fragment, if any — excluded from
	// "nearest neighbor" / edge-contribution scoring so a slot doesn't treat
	// its own outgoing content as a neighbor.
	int & claimedIndex = slots[slotIndex].fragmentIndex;
	Fragment * excludeFrag = (claimedIndex >= 0 && claimedIndex < static_cast<int>(fragments.size()))
		? fragments[claimedIndex].get()
		: nullptr;

	std::vector<Candidate> candidates;
	float dividerX = grid->getDividerX();
	glm::vec2 canvasCenter(canvasW / 2.0f, canvasH / 2.0f);
	float maxCenterDist = glm::distance(glm::vec2(0, 0), canvasCenter);
	float canvasDiag = glm::distance(glm::vec2(0, 0), glm::vec2(canvasW, canvasH));
	float idealGap = 1.5f * 150.0f; // reference gap, no fixed cell size to derive this from anymore
	int zoneACount = countZoneFragments(true);
	int zoneBCount = countZoneFragments(false);

	for (int attempt = 0; attempt < PLACEMENT_MAX_ATTEMPTS; attempt++) {
		float x1, x2, y1, y2;
		if (!findSnapSpan(xs, wRange.x, wRange.y, x1, x2)) continue;
		if (!findSnapSpan(ys, hRange.x, hRange.y, y1, y2)) continue;

		ofRectangle candidateBounds(x1, y1, x2 - x1, y2 - y1);
		if (!grid->isRectFree(candidateBounds, 0.30f)) {
			continue;
		}

		glm::vec2 center = glm::vec2(candidateBounds.getCenter());

		float centerDist = glm::distance(center, canvasCenter);
		float centerScore = (maxCenterDist > 0) ? centerDist / maxCenterDist : 0.0f;

		float nearestDist = std::numeric_limits<float>::max();
		Fragment * nearest = nullptr;
		for (const auto & f : fragments) {
			if (f.get() == excludeFrag || !f) continue;
			glm::vec2 fc = glm::vec2(f->getBounds().getCenter());
			float d = glm::distance(center, fc);
			if (d < nearestDist) {
				nearestDist = d;
				nearest = f.get();
			}
		}

		float proximityScore = 0.0f;
		if (nearest != nullptr) {
			proximityScore = 1.0f - ofClamp(fabs(nearestDist - idealGap) / canvasDiag, 0.0f, 1.0f);
		}

		bool inZoneA = center.x < dividerX;
		float zoneScore = 0.0f;
		if (inZoneA && zoneACount < zoneBCount) {
			zoneScore = 1.0f;
		} else if (!inZoneA && zoneBCount < zoneACount) {
			zoneScore = 1.0f;
		}

		float zoneWeight = PLACEMENT_SCORE_W_ZONE
			* (zoneScoreBoostRemaining > 0 ? TRIGGER_ZONE_IMBALANCE_SCORE_MULT : 1.0f);

		float activityPenalty = gridState
			? gridState->get(gridStateCol(center.x, canvasW), gridStateRow(center.y, canvasH)) * GRIDSTATE_PLACEMENT_PENALTY
			: 0.0f;

		float jitter = randRangeF(-PLACEMENT_SCORE_JITTER, PLACEMENT_SCORE_JITTER);
		float score = PLACEMENT_SCORE_W_CENTER * centerScore
			+ PLACEMENT_SCORE_W_PROXIMITY * proximityScore
			+ zoneWeight * zoneScore
			+ jitter
			- activityPenalty;

		candidates.push_back({ candidateBounds, score, nearest });
	}

	ofRectangle placedBounds;
	Fragment * nearestForLine = nullptr;

	if (!candidates.empty()) {
		const Candidate & best = *std::max_element(candidates.begin(), candidates.end(),
			[](const Candidate & a, const Candidate & b) { return a.score < b.score; });
		placedBounds = best.bounds;
		nearestForLine = best.nearest;
	} else {
		// No existing pair of snap positions satisfies this size — manufacture
		// new structural lines (§06: "the first fragment always generates its
		// own grid"). Bypasses the occupancy/scoring check; the manufactured
		// span is by definition unoccupied space relative to the new lines.
		float targetW = randRangeF(wRange.x, wRange.y);
		float targetH = randRangeF(hRange.x, hRange.y);
		float x1 = xs[randRangeI(0, static_cast<int>(xs.size()) - 1)];
		float y1 = ys[randRangeI(0, static_cast<int>(ys.size()) - 1)];
		float x2 = grid->addStructuralX(ofClamp(x1 + targetW, 0.0f, static_cast<float>(canvasW)));
		float y2 = grid->addStructuralY(ofClamp(y1 + targetH, 0.0f, static_cast<float>(canvasH)));
		if (x2 < x1) std::swap(x1, x2);
		if (y2 < y1) std::swap(y1, y2);
		if (x2 - x1 < 1.0f || y2 - y1 < 1.0f) {
			return false;
		}
		placedBounds = ofRectangle(x1, y1, x2 - x1, y2 - y1);
	}

	float phaseOffset = randRangeF(0.0f, glm::two_pi<float>());
	ofColor color = pickPlaceholderColor();

	Fragment::Params params;
	params.bounds = placedBounds;
	params.placeholderColor = color;
	params.phaseOffset = phaseOffset;
	params.driftAmp = glm::vec2(DRIFT_AMP_X, DRIFT_AMP_Y);
	params.driftFreq = glm::vec2(DRIFT_FREQ_X, DRIFT_FREQ_Y);
	params.desaturateRampDuration = DESATURATE_RAMP_DURATION;
	params.desaturateMax = densityHighActive ? TRIGGER_DENSITY_HIGH_DESAT_MAX : DESATURATE_MAX;
	// circularMask / maskRadius stay false/0 for RECT/SLIVER/SQUARE

	auto frag = std::make_unique<BEFragment>();
	frag->setupBE(params, type, canvasW, canvasH);
	frag->setId(nextFragmentId++);

	bool showMotionContent = motionEx != nullptr && randRangeF(0.0f, 1.0f) < MOTION_CONTENT_PROBABILITY;
	if (showMotionContent) {
		ofTexture & motionTex = motionEx->getMotionTexture();
		if (motionTex.isAllocated()) {
			frag->setVideoSource(&motionTex, ofRectangle(0, 0, motionTex.getWidth(), motionTex.getHeight()));
		} else {
			showMotionContent = false; // fall through to requestVideoTexture below
		}
	}
	if (!showMotionContent) {
		requestVideoTexture(frag.get(),
			static_cast<int>(placedBounds.width), static_cast<int>(placedBounds.height));
	}
	frag->setShaderLibrary(shaderLib);
	frag->setEffectGroup(randRangeI(0, 1));

	Fragment * newFragPtr = frag.get();
	grid->reserve(newFragPtr->getId(), placedBounds);
	grid->contributeFragmentEdges(newFragPtr->getId(), placedBounds, otherFragmentBounds(excludeFrag));

	if (claimedIndex >= 0 && claimedIndex < static_cast<int>(fragments.size())) {
		notifyFragmentRemoved(fragments[claimedIndex].get());
		fragments[claimedIndex] = std::move(frag);
	} else {
		fragments.push_back(std::move(frag));
		claimedIndex = static_cast<int>(fragments.size()) - 1;
	}
	notifyFragmentPlaced(newFragPtr, nearestForLine);

	if (gridState) {
		gridState->accumulateRect(
			gridStateCol(placedBounds.getLeft(), canvasW), gridStateRow(placedBounds.getTop(), canvasH),
			std::max(1, static_cast<int>(placedBounds.width / (canvasW / static_cast<float>(GRID_COLS)))),
			std::max(1, static_cast<int>(placedBounds.height / (canvasH / static_cast<float>(GRID_ROWS)))), 1.0f);
	}
	if (zoneScoreBoostRemaining > 0) {
		zoneScoreBoostRemaining--;
	}
	secondsSinceLastPlacement = 0.0f;

	grid->maybeSubdivide();

	return true;
}

bool BEComposition::attemptPlacement() {
	// Unused: usesAutomaticPlacementTimer() returns false, so CompositionBase
	// never calls this. Real placement happens per-slot via
	// spawnFragmentInSlot(), driven by updateSlots() from onUpdate().
	return false;
}
