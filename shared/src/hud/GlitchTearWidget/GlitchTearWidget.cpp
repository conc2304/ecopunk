#include "GlitchTearWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

void GlitchTearWidget::setup() {
	slices.clear();
	nextTriggerT = ofRandom(options.triggerIntervalMin, options.triggerIntervalMax);
}

void GlitchTearWidget::spawnTear() {
	int n = static_cast<int>(ofRandom(options.sliceCountMin, options.sliceCountMax + 1));
	for (int i = 0; i < n; i++) {
		Slice s;
		s.vertical = ofRandom(1.0f) < options.verticalChance;
		s.posNorm = ofRandom(0.0f, 1.0f);
		s.sizeNorm = ofRandom(options.thicknessMin, options.thicknessMax);
		s.jitterNorm = ofRandom(-options.maxJitter, options.maxJitter);
		s.lifeMax = ofRandom(options.tearDurationMin, options.tearDurationMax);
		s.life = s.lifeMax;
		slices.push_back(s);
	}
}

void GlitchTearWidget::trigger() {
	spawnTear();
}

void GlitchTearWidget::update(float dt) {
	HudWidget::update(dt);

	nextTriggerT -= dt * motion.speed;
	if (nextTriggerT <= 0.0f) {
		spawnTear();
		nextTriggerT = ofRandom(options.triggerIntervalMin, options.triggerIntervalMax);
	}

	std::vector<Slice> alive;
	alive.reserve(slices.size());
	for (auto & s : slices) {
		s.life -= dt;
		if (s.life > 0.0f) alive.push_back(s);
	}
	slices = std::move(alive);
}

void GlitchTearWidget::draw() {
	if (slices.empty()) return;
	ofPushStyle();
	if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD);
	else ofEnableAlphaBlending();
	ofFill();

	for (auto & s : slices) {
		float a = ofClamp(s.life / s.lifeMax, 0.0f, 1.0f);

		if (!s.vertical) {
			float y = bounds.y + s.posNorm * bounds.height;
			float h = std::max(1.0f, s.sizeNorm * bounds.height);
			float xOff = s.jitterNorm * bounds.width;

			// Dark backing band so the offset accent slice above it reads as a
			// tear rather than a translucent smear over whatever's underneath.
			ofSetColor(scaledAlpha(theme.colors.background, motion.opacity * a * 2.2f));
			ofDrawRectangle(bounds.x, y, bounds.width, h);

			ofSetColor(scaledAlpha(theme.colors.accent, motion.opacity * a));
			ofDrawRectangle(bounds.x + xOff, y, bounds.width, h);

			ofSetColor(scaledAlpha(theme.colors.secondary, motion.opacity * a * 0.8f));
			ofSetLineWidth(1.0f);
			ofDrawLine(bounds.x + xOff, y, bounds.x + xOff + bounds.width, y);
			ofDrawLine(bounds.x + xOff, y + h, bounds.x + xOff + bounds.width, y + h);
		} else {
			float x = bounds.x + s.posNorm * bounds.width;
			float w = std::max(1.0f, s.sizeNorm * bounds.width);
			float yOff = s.jitterNorm * bounds.height;

			ofSetColor(scaledAlpha(theme.colors.background, motion.opacity * a * 2.2f));
			ofDrawRectangle(x, bounds.y, w, bounds.height);

			ofSetColor(scaledAlpha(theme.colors.accent, motion.opacity * a));
			ofDrawRectangle(x, bounds.y + yOff, w, bounds.height);

			ofSetColor(scaledAlpha(theme.colors.secondary, motion.opacity * a * 0.8f));
			ofSetLineWidth(1.0f);
			ofDrawLine(x, bounds.y + yOff, x, bounds.y + yOff + bounds.height);
			ofDrawLine(x + w, bounds.y + yOff, x + w, bounds.y + yOff + bounds.height);
		}
	}

	ofDisableBlendMode();
	ofPopStyle();
}

} // namespace hud
