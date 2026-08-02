#include "FTFragmentPool.h"

namespace {
const std::vector<std::string> kEffectPool = {
	"desaturate", "invert", "recolor", "threshold", "dither", "solarize", "scanlines", "channelshift",
	"heatmap_recolor"
};
}

void FTFragmentPool::setup(int maxFragments_, float spawnMinDistance_, float spawnMinInterval_,
	bool idlePulseEnabled_, float idlePulseInterval_,
	float sustainSeconds_, float decaySeconds_, float minSize_, float maxSize_,
	int numPlayheads) {
	maxFragments = maxFragments_;
	spawnMinDistance = spawnMinDistance_;
	spawnMinInterval = spawnMinInterval_;
	idlePulseEnabled = idlePulseEnabled_;
	idlePulseInterval = idlePulseInterval_;
	sustainSeconds = sustainSeconds_;
	decaySeconds = decaySeconds_;
	minSize = minSize_;
	maxSize = maxSize_;

	playheadInUse.assign(std::max(1, numPlayheads), false);
	fragments.clear();

	// Default HUD theme — matches the palette already established for this
	// repo's hud:: widgets (README.md's example setup).
	theme.colors.primary = ofColor(124, 232, 230, 220);
	theme.colors.secondary = ofColor(103, 255, 142, 200);
	theme.colors.accent = ofColor(244, 255, 106, 220);
	theme.colors.muted = ofColor(124, 232, 230, 70);
	// Background alpha must stay low (see sibling sketches: blueprint_emergence
	// 36, quadrant-crosshair 18, temporal-fields 40) — HudFrameRenderer::draw()
	// fills the full widget bounds with this color whenever alpha > 0,
	// UNCONDITIONALLY, regardless of FrameStyle (see HudFrameRenderer.cpp's
	// `if (colors.background.a > 0)` block, which runs before the style
	// switch). This was the actual cause of the "flat, detail-free fill"
	// bug: 160 (63% opacity) drowned the video in a near-opaque wash after
	// every fragment's content draw; switching Box->Corners never touched
	// this separate fill, only drawBox()'s own one.
	theme.colors.background = ofColor(0, 20, 16, 36);
	theme.frame.style = hud::FrameStyle::Corners;
	theme.frame.showTicks = true;
	theme.additive = false; // fragments overlap heavily; additive blending would blow out fast
}

int FTFragmentPool::acquirePlayhead() {
	for (size_t i = 0; i < playheadInUse.size(); ++i) {
		if (!playheadInUse[i]) {
			playheadInUse[i] = true;
			return static_cast<int>(i);
		}
	}
	return -1;
}

void FTFragmentPool::releasePlayhead(int idx) {
	if (idx >= 0 && idx < static_cast<int>(playheadInUse.size())) {
		playheadInUse[idx] = false;
	}
}

std::string FTFragmentPool::makeHexLabel() {
	int order = ++spawnCounter;
	int elapsedTag = static_cast<int>(ofGetElapsedTimef()) & 0xFF;
	char buf[16];
	snprintf(buf, sizeof(buf), "0x%04X.%02X", order & 0xFFFF, elapsedTag);
	return std::string(buf);
}

void FTFragmentPool::spawnOne(glm::vec2 pos, FTContentMode mode, const std::string& modeCEffectName,
	TimeOffsetVideoBuffer& videoBuffer) {
	FTFragmentSpawnParams p;
	p.position = pos;
	p.size = ofRandom(minSize, maxSize);
	p.mode = mode;
	p.hexLabel = makeHexLabel();
	p.spawnIndex = spawnCounter; // makeHexLabel() already incremented this above

	if (mode == FTContentMode::TIME_SLICE) {
		int idx = acquirePlayhead();
		if (idx >= 0) {
			p.playheadIndex = idx;
			// Reach maximum history depth roughly by end of life — see
			// TimeOffsetVideoBuffer::rampPlayheadTo's normalized-offset docs.
			p.driftRate = 1.0f / (sustainSeconds + decaySeconds);
			videoBuffer.jumpPlayhead(idx, 0.0f);
			videoBuffer.rampPlayheadTo(idx, 1.0f, p.driftRate);
		} else {
			// Playhead pool exhausted (Section 9 resolution: the pool stays
			// small — 5-8 — rather than growing to match maxFragments).
			// Falling back to a live Mode-B fragment instead of skipping the
			// spawn; not every fragment needs to be Mode A at once.
			p.mode = FTContentMode::EFFECT_VARIED;
			p.effectName = kEffectPool[static_cast<size_t>(ofRandom(kEffectPool.size()))];
		}
	} else if (mode == FTContentMode::EFFECT_VARIED) {
		p.effectName = kEffectPool[static_cast<size_t>(ofRandom(kEffectPool.size()))];
	} else { // EFFECT_PARAM_VARIANT
		p.effectName = modeCEffectName.empty() ? "channelshift" : modeCEffectName;
		if (p.effectName == "channelshift") {
			p.shiftVal = ofRandom(0.003f, 0.02f);
		} else if (p.effectName == "recolor") {
			ofColor c;
			c.setHsb(static_cast<unsigned char>(ofRandom(0, 255)), 200, 255);
			p.tint = glm::vec3(c.r / 255.f, c.g / 255.f, c.b / 255.f);
		} else if (p.effectName == "threshold") {
			p.thresholdVal = ofRandom(0.3f, 0.7f);
		} else if (p.effectName == "dither") {
			p.ditherPx = ofRandom(2.f, 8.f);
		} else if (p.effectName == "heatmap_recolor") {
			p.heatmapPalette = static_cast<int>(ofRandom(4.f));
		}
	}

	FTFragment frag;
	frag.spawn(p, sustainSeconds, decaySeconds, theme);
	fragments.push_back(std::move(frag));
	// Fire from the vector's own storage, not the local `frag` — it was
	// moved-from by push_back above.
	if (onFragmentSpawned) onFragmentSpawned(fragments.back());
}

void FTFragmentPool::update(float dt, const CrosshairState& crosshairState, TimeOffsetVideoBuffer& videoBuffer,
	FTContentMode activeMode, const std::string& modeCEffectName,
	float effectiveMinDistance, float effectiveMinInterval) {
	for (auto& f : fragments) f.update(dt);

	sinceLastSpawn += dt;

	glm::vec2 pos(crosshairState.cx, crosshairState.cy);
	float dist = glm::distance(pos, lastSpawnPos);
	bool movementSpawn = dist >= effectiveMinDistance && sinceLastSpawn >= effectiveMinInterval;
	bool idleSpawn = idlePulseEnabled && sinceLastSpawn >= idlePulseInterval;

	if (movementSpawn || idleSpawn) {
		spawnOne(pos, activeMode, modeCEffectName, videoBuffer);
		lastSpawnPos = pos;
		sinceLastSpawn = 0.f;
	}

	// Hard cap: evict oldest immediately on overflow, independent of age-fade.
	while (static_cast<int>(fragments.size()) > maxFragments) {
		FTFragment& oldest = fragments.front();
		if (onFragmentEvicted) onFragmentEvicted(oldest);
		if (oldest.getMode() == FTContentMode::TIME_SLICE && oldest.getPlayheadIndex() >= 0) {
			releasePlayhead(oldest.getPlayheadIndex());
		}
		fragments.erase(fragments.begin());
	}

	// Age-based decay eviction.
	for (size_t i = 0; i < fragments.size();) {
		if (fragments[i].isExpired()) {
			if (fragments[i].getMode() == FTContentMode::TIME_SLICE && fragments[i].getPlayheadIndex() >= 0) {
				releasePlayhead(fragments[i].getPlayheadIndex());
			}
			fragments.erase(fragments.begin() + i);
		} else {
			++i;
		}
	}
}

void FTFragmentPool::draw(TimeOffsetVideoBuffer& videoBuffer, ShaderLibrary& shaderLib) {
	// Content draws (both the plain-texture and shader paths in
	// FTFragment::drawContent) rely on alpha blending being enabled for
	// their fade-out to render correctly. Don't assume it's already on —
	// other draw calls earlier in the frame (or left over from the previous
	// frame, since GL state persists across frames) may have disabled it.
	ofEnableAlphaBlending();
	for (auto& f : fragments) {
		f.draw(videoBuffer, shaderLib);
	}
	ofDisableAlphaBlending();
}
