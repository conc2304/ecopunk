#include "HudOverlayLayer.h"
#include "HudOverlayTheme.h"
#include <cmath>

namespace hudoverlay {

void HudOverlayLayer::setup(float width, float height) {
	canvasW = width;
	canvasH = height;

	radarStation.setBounds(canvasW - 200.0f, 20.0f, 180.0f, 180.0f);
	breathingTicks.setup();
	breathingTicks.setBounds(0.0f, 0.0f, canvasW, canvasH);
	accentBurst.setBounds(0.0f, 0.0f, canvasW, canvasH);

	hud::TelemetryReadoutOptions fc;
	fc.mode = hud::TelemetryMode::FrameCounter;
	frameCounter.setOptions(fc);
	frameCounter.setBounds(0.0f, 0.0f, canvasW, canvasH);

	hud::TelemetryReadoutOptions cw;
	cw.mode = hud::TelemetryMode::CoordinateWalk;
	coordinateReadout.setOptions(cw);
	coordinateReadout.setBounds(0.0f, 0.0f, canvasW, canvasH);

	lockSequence.setup();
	lockSequence.setBounds(0.0f, 0.0f, canvasW, canvasH);
	faultCascade.setup();
	faultCascade.setBounds(0.0f, 0.0f, canvasW, canvasH);
	radarPing.setup();
	radarPing.setBounds(0.0f, 0.0f, canvasW, canvasH);
	handshake.setup();
	handshake.setBounds(0.0f, 0.0f, canvasW, canvasH);

	applyThemeIfChanged(Palette::Mono);

	scheduler.setup();
	scheduler.onLockSequence = [this]() {
		ofVec2f p = pickPlacementPoint(currentDials.discipline);
		static const hud::CalloutAnchor kCorners[] = { hud::CalloutAnchor::TopLeft, hud::CalloutAnchor::TopRight,
			hud::CalloutAnchor::BottomLeft, hud::CalloutAnchor::BottomRight };
		lockSequence.trigger(p.x, p.y, kCorners[static_cast<int>(ofRandom(4))]);
	};
	scheduler.onFaultCascade = [this]() {
		ofVec2f p = pickPlacementPoint(currentDials.discipline);
		faultCascade.trigger(p.x, p.y);
	};
	scheduler.onRadarPing = [this]() {
		// Near the radar ring's edge — see RadarPingOrganism.h's comment on
		// why this doesn't read the wedge's live angle in this phase.
		float angle = ofRandom(TWO_PI);
		ofVec2f ringCenterPx = { canvasW - 110.0f, 110.0f };
		float ringRadiusPx = 180.0f * 0.42f;
		ofVec2f p = { (ringCenterPx.x + std::cos(angle) * ringRadiusPx) / canvasW,
			(ringCenterPx.y + std::sin(angle) * ringRadiusPx) / canvasH };
		radarPing.trigger(ofClamp(p.x, 0.02f, 0.98f), ofClamp(p.y, 0.02f, 0.98f));
	};
	scheduler.onHandshake = [this]() {
		ofVec2f a = pickPlacementPoint(currentDials.discipline);
		ofVec2f b = a;
		for (int i = 0; i < 8; i++) {
			float angle = ofRandom(TWO_PI);
			float dist  = ofRandom(0.18f, 0.4f);
			b = { ofClamp(a.x + std::cos(angle) * dist, 0.05f, 0.95f), ofClamp(a.y + std::sin(angle) * dist, 0.08f, 0.92f) };
			if (a.distance(b) >= 0.15f) break;
		}
		handshake.trigger(a.x, a.y, b.x, b.y, canvasW, canvasH);
	};
}

void HudOverlayLayer::windowResized(float width, float height) {
	canvasW = width;
	canvasH = height;
	radarStation.setBounds(canvasW - 200.0f, 20.0f, 180.0f, 180.0f);
	breathingTicks.setBounds(0.0f, 0.0f, canvasW, canvasH);
	accentBurst.setBounds(0.0f, 0.0f, canvasW, canvasH);
	frameCounter.setBounds(0.0f, 0.0f, canvasW, canvasH);
	coordinateReadout.setBounds(0.0f, 0.0f, canvasW, canvasH);
	lockSequence.setBounds(0.0f, 0.0f, canvasW, canvasH);
	faultCascade.setBounds(0.0f, 0.0f, canvasW, canvasH);
	radarPing.setBounds(0.0f, 0.0f, canvasW, canvasH);
	handshake.setBounds(0.0f, 0.0f, canvasW, canvasH);
}

void HudOverlayLayer::applyThemeIfChanged(Palette palette) {
	if (palette == appliedPalette) return;
	appliedPalette = palette;
	hud::HudTheme theme = themeForPalette(palette);

	radarStation.setTheme(theme);
	breathingTicks.setTheme(theme);
	accentBurst.setTheme(theme);
	frameCounter.setTheme(theme);
	coordinateReadout.setTheme(theme);
	lockSequence.setTheme(theme);
	faultCascade.setTheme(theme);
	radarPing.setTheme(theme);
	handshake.setTheme(theme);
}

ofVec2f HudOverlayLayer::pickPlacementPoint(float discipline) const {
	// Virtual, undrawn placement grid (design doc Section 05's Discipline
	// dial) — never rendered, exists purely as a reference for how tightly
	// organism positions snap to it.
	const int cols = 8, rows = 6;
	int cx = static_cast<int>(ofRandom(cols));
	int cy = static_cast<int>(ofRandom(rows));
	float cellW = 1.0f / cols, cellH = 1.0f / rows;
	float centerX = (cx + 0.5f) * cellW;
	float centerY = (cy + 0.5f) * cellH;

	float jitterAmount = 1.0f - ofClamp(discipline, 0.0f, 1.0f);
	float jx = ofRandom(-0.4f, 0.4f) * cellW * jitterAmount;
	float jy = ofRandom(-0.4f, 0.4f) * cellH * jitterAmount;

	return { ofClamp(centerX + jx, 0.04f, 0.96f), ofClamp(centerY + jy, 0.06f, 0.94f) };
}

void HudOverlayLayer::update(float dt, const HudOverlayDialState& dials) {
	currentDials = dials; // scheduler callbacks (fired below) and draw() read this
	applyThemeIfChanged(dials.palette);
	scanlineT += dt;

	hud::RadarStationOptions rso;
	rso.rotationPeriod = ofLerp(9.0f, 2.5f, ofClamp(dials.intensity, 0.0f, 1.0f));
	radarStation.setOptions(rso);
	radarStation.update(dt);
	breathingTicks.update(dt);
	accentBurst.update(dt);
	frameCounter.update(dt);
	coordinateReadout.update(dt);

	if (schedulerEnabled) scheduler.update(dt, dials);

	lockSequence.update(dt);
	faultCascade.update(dt);
	radarPing.update(dt);
	handshake.update(dt);
}

void HudOverlayLayer::drawScanline(float intensity) {
	float loopDuration = ofLerp(10.0f, 4.0f, ofClamp(intensity, 0.0f, 1.0f));
	float y = std::fmod(scanlineT, loopDuration) / loopDuration * canvasH;

	ofPushStyle();
	ofEnableAlphaBlending();
	ofSetColor(255, 255, 255, static_cast<int>(0.16f * 255.0f));
	ofDrawLine(0, y, canvasW, y);
	ofPopStyle();
}

void HudOverlayLayer::drawAmbient() {
	drawScanline(currentDials.intensity);
	radarStation.draw();
	breathingTicks.draw();
	frameCounter.draw();
	coordinateReadout.draw();
}

void HudOverlayLayer::drawOrganisms() {
	accentBurst.draw();
	lockSequence.draw();
	faultCascade.draw();
	radarPing.draw();
	handshake.draw();
}

} // namespace hudoverlay
