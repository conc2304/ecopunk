#include "HalftonePatchWidget.h"
#include "../shared/HudUtils.h"
#include <algorithm>

namespace hud {

void HalftonePatchWidget::triggerAt(float nx, float ny, float wNorm, float hNorm) {
	patches.push_back({ nx, ny, wNorm, hNorm, 0.0f });
}

void HalftonePatchWidget::update(float dt) {
	HudWidget::update(dt);
	for (auto& p : patches) p.age += dt;
	patches.erase(std::remove_if(patches.begin(), patches.end(),
	                  [this](const Patch& p) { return p.age >= options.duration; }),
	    patches.end());
}

void HalftonePatchWidget::draw() {
	if (patches.empty()) return;
	ofPushStyle();
	ofEnableAlphaBlending();
	ofFill();

	float cell = std::max(1.0f, su(bounds, options.cellSizeNorm * bounds.minDim()));

	for (const auto& p : patches) {
		float u = ofClamp(p.age / options.duration, 0.0f, 1.0f);
		float alpha = motion.opacity * (1.0f - u);
		ofVec2f origin = pointInBounds(bounds, p.nx, p.ny);
		float w = p.w * bounds.width;
		float h = p.h * bounds.height;

		int cols = std::max(1, static_cast<int>(w / cell));
		int rows = std::max(1, static_cast<int>(h / cell));
		for (int cy = 0; cy < rows; cy++) {
			for (int cx = 0; cx < cols; cx++) {
				bool checker = ((cx + cy) % 2) == 0;
				float flicker = ofNoise(cx * 0.7f, cy * 0.7f, time * 22.0f) > 0.5f ? 1.0f : 0.0f;
				if (checker == (flicker > 0.5f)) {
					ofSetColor(scaledAlpha(theme.colors.accent, alpha * 0.5f));
					ofDrawRectangle(origin.x + cx * cell, origin.y + cy * cell, cell, cell);
				}
			}
		}
	}

	ofDisableBlendMode();
	ofPopStyle();
}

} // namespace hud
