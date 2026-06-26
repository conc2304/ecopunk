#include "CrosshairSystem.h"
#include <algorithm>

void CrosshairSystem::setup() {
	presets = {
		{ 0.04f, 0.009f, 0.60f, 0.35f, 120.f, "DRIFT" },
		{ 0.07f, 0.020f, 0.75f, 0.22f, 80.f, "SCAN" },
		{ 0.12f, 0.035f, 0.85f, 0.15f, 60.f, "HUNT" },
		{ 0.18f, 0.004f, 0.45f, 0.55f, 80.f, "NERVOUS" },
		{ 0.05f, 0.051f, 0.70f, 0.30f, 100.f, "ORBIT" },
	};
	presetIndex = 0;
	noiseT = ofRandom(0, 1000);

	posHistory.fill({ 640.f, 360.f });
	haloPos = { 640.f, 360.f };
	ghostPos = { 640.f, 360.f };
}

void CrosshairSystem::update(float dt, const LFOBank & lfo) {
	const CrosshairPreset & p = presets[presetIndex];
	noiseT += dt;

	float nx = ofNoise(noiseT * p.freqA + seedXA) * p.ampA
		+ ofNoise(noiseT * p.freqB + seedXB) * p.ampB;
	float ny = ofNoise(noiseT * p.freqA + seedYA) * p.ampA
		+ ofNoise(noiseT * p.freqB + seedYB) * p.ampB;

	prevState = state;
	float W = ofGetWidth(), H = ofGetHeight();
	state.cx = ofMap(nx, 0, 1, p.margin, W - p.margin);
	state.cy = ofMap(ny, 0, 1, p.margin, H - p.margin);
	state.vx = state.cx - prevState.cx;
	state.vy = state.cy - prevState.cy;
	state.speed = sqrtf(state.vx * state.vx + state.vy * state.vy);
	state.chWidth = lineWidth;

	// Ghost ring buffer
	posHistory[historyHead % HISTORY_SIZE] = { state.cx, state.cy };
	historyHead++;
	ghostPos = posHistory[(historyHead - GHOST_LAG + HISTORY_SIZE) % HISTORY_SIZE];

	// Halo lags behind crosshair
	haloPos = haloPos + (glm::vec2(state.cx, state.cy) - haloPos) * 0.04f;

	// Line width: speed + LFO pulse
	float speedNorm = ofMap(state.speed, 0, HIGH_THRESH, 0.f, 1.f, true);
	float pulse = ofMap(lfo.get(LFO_CROSSHAIR_PULSE), -1, 1, 0.f, 1.f);
	lineWidth = ofLerp(6.0f, 12.0f, speedNorm * 0.7f + pulse * 0.3f);

	// Motion attractor: biases position toward active region, noise remains dominant
	if (attractForce > 0.001f) {
		state.cx = ofLerp(state.cx, attractX, attractForce * ATTRACT_LERP);
		state.cy = ofLerp(state.cy, attractY, attractForce * ATTRACT_LERP);
	}

	// Bloom animation
	bloomRadius = ofLerp(bloomRadius, bloomTarget, 0.15f);
}

void CrosshairSystem::sampleColor(const ofPixels & px, float cx, float cy) {
	int W = px.getWidth(), H = px.getHeight();
	float r = 0, g = 0, b = 0;
	int count = 0;
	const int HALF = 40;
	const int STEP = 4;

	int sy = ofClamp((int)cy, 0, H - 1);
	for (int x = ofClamp((int)cx - HALF, 0, W - 1);
		x < ofClamp((int)cx + HALF, 0, W - 1); x += STEP) {
		auto c = px.getColor(x, sy);
		r += c.r;
		g += c.g;
		b += c.b;
		count++;
	}
	int sx = ofClamp((int)cx, 0, W - 1);
	for (int y = ofClamp((int)cy - HALF, 0, H - 1);
		y < ofClamp((int)cy + HALF, 0, H - 1); y += STEP) {
		auto c = px.getColor(sx, y);
		r += c.r;
		g += c.g;
		b += c.b;
		count++;
	}
	if (count > 0)
		sampledColor.set(r / count, g / count, b / count);

	crosshairColor = crosshairColor.getLerped(sampledColor, 0.08f);
}

void CrosshairSystem::triggerBloom(float targetRadius, float /*durationSecs*/) {
	bloomTarget = targetRadius;
}

void CrosshairSystem::setBloomFill(float target, float /*rampSecs*/) {
	bloomFill = target;
}

// ─── Draw helpers ────────────────────────────────────────────────────────────

void CrosshairSystem::addArmQuad(ofMesh & mesh, glm::vec2 a, glm::vec2 b,
	float T, ofColor ca, ofColor cb) {
	glm::vec2 perp = glm::normalize(b - a);
	glm::vec2 side = { -perp.y * T, perp.x * T };

	int base = mesh.getNumVertices();
	mesh.addColor(ca);
	mesh.addVertex({ a.x - side.x, a.y - side.y, 0.f });
	mesh.addColor(ca);
	mesh.addVertex({ a.x + side.x, a.y + side.y, 0.f });
	mesh.addColor(cb);
	mesh.addVertex({ b.x + side.x, b.y + side.y, 0.f });
	mesh.addColor(cb);
	mesh.addVertex({ b.x - side.x, b.y - side.y, 0.f });
	mesh.addTriangle(base, base + 1, base + 2);
	mesh.addTriangle(base, base + 2, base + 3);
}

void CrosshairSystem::drawGradientArms(float cx, float cy, float opacH, float opacV) {
	float W = ofGetWidth(), H = ofGetHeight();
	float T = lineWidth * 0.5f;

	ofColor zero(crosshairColor.r, crosshairColor.g, crosshairColor.b, 0);
	ofColor peakH(crosshairColor.r, crosshairColor.g, crosshairColor.b, (int)(250 * opacH));
	ofColor peakV(crosshairColor.r, crosshairColor.g, crosshairColor.b, (int)(250 * opacV));

	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_TRIANGLES);

	addArmQuad(mesh, { 0.f, cy }, { cx, cy }, T, zero, peakH);
	addArmQuad(mesh, { cx, cy }, { W, cy }, T, peakH, zero);
	addArmQuad(mesh, { cx, 0.f }, { cx, cy }, T, zero, peakV);
	addArmQuad(mesh, { cx, cy }, { cx, H }, T, peakV, zero);

	ofEnableAlphaBlending();
	mesh.draw();
	ofDisableAlphaBlending();
}

void CrosshairSystem::drawDashArms(float cx, float cy, float speed) {
	float W = ofGetWidth(), H = ofGetHeight();
	float T = lineWidth * 0.5f;
	float dash = 12.f;
	float gap = ofMap(speed, HIGH_THRESH, HIGH_THRESH * 4.f, 4.f, 20.f, true);
	float step = dash + gap;

	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_TRIANGLES);
	ofColor col(crosshairColor.r, crosshairColor.g, crosshairColor.b, 200);

	// Horizontal: left then right of cx
	for (float x = 0.f; x < cx; x += step) {
		float x1 = std::min(x + dash, cx);
		addArmQuad(mesh, { x, cy }, { x1, cy }, T, col, col);
	}
	for (float x = cx; x < W; x += step) {
		float x1 = std::min(x + dash, W);
		addArmQuad(mesh, { x, cy }, { x1, cy }, T, col, col);
	}
	// Vertical: top then bottom of cy
	for (float y = 0.f; y < cy; y += step) {
		float y1 = std::min(y + dash, cy);
		addArmQuad(mesh, { cx, y }, { cx, y1 }, T, col, col);
	}
	for (float y = cy; y < H; y += step) {
		float y1 = std::min(y + dash, H);
		addArmQuad(mesh, { cx, y }, { cx, y1 }, T, col, col);
	}

	ofEnableAlphaBlending();
	mesh.draw();
	ofDisableAlphaBlending();
}

// ─── draw() ──────────────────────────────────────────────────────────────────

void CrosshairSystem::draw() {
	float cx = state.cx, cy = state.cy;

	ofEnableAlphaBlending();

	// 1. Arms (gradient or dashed)
	if (state.speed > HIGH_THRESH) {
		drawDashArms(cx, cy, state.speed);
	} else {
		float opacH = ofMap(1.f, -1.f, 1.f, 0.35f, 1.0f); // neutral until LFO wired per-draw
		float opacV = opacH;
		drawGradientArms(cx, cy, opacH, opacV);
	}

	// 2. Ghost arms — same mesh at ghostPos, 18% opacity
	if (showGhost) {
		float T = std::max(lineWidth * 0.1f, 1.0f);  // 20% of main width, min 2px total
		float W = ofGetWidth(), H = ofGetHeight();
		ofColor zero(crosshairColor.r, crosshairColor.g, crosshairColor.b, 0);
		ofColor ghost(crosshairColor.r, crosshairColor.g, crosshairColor.b, 46);
		ofMesh gm;
		gm.setMode(OF_PRIMITIVE_TRIANGLES);
		addArmQuad(gm, { 0.f, ghostPos.y }, { ghostPos.x, ghostPos.y }, T, zero, ghost);
		addArmQuad(gm, { ghostPos.x, ghostPos.y }, { W, ghostPos.y }, T, ghost, zero);
		addArmQuad(gm, { ghostPos.x, 0.f }, { ghostPos.x, ghostPos.y }, T, zero, ghost);
		addArmQuad(gm, { ghostPos.x, ghostPos.y }, { ghostPos.x, H }, T, ghost, zero);
		gm.draw();
	}

	// 3. Halo — lagged circle, 8% opacity
	ofPushStyle();
	ofNoFill();
	ofSetColor(crosshairColor.r, crosshairColor.g, crosshairColor.b, 20);
	ofSetLineWidth(1.f);
	ofDrawCircle(haloPos.x, haloPos.y, 200.f);
	ofPopStyle();

	// 4. Intersection bloom
	ofPushStyle();
	ofColor bloomCol(crosshairColor.r, crosshairColor.g, crosshairColor.b,
		(int)(125 + bloomFill * 165));
	ofSetColor(bloomCol);
	if (bloomFill > 0.05f)
		ofFill();
	else
		ofNoFill();
	ofSetLineWidth(3.f);
	ofDrawCircle(cx, cy, bloomRadius);
	ofPopStyle();

	// Reset bloom target to resting state each frame; callers must re-set each frame if sustained
	bloomTarget = 4.f;

	ofDisableAlphaBlending();
}

void CrosshairSystem::setMotionAttractor(float x, float y, float energy) {
	attractX = x;
	attractY = y;
	// Cap at 0.35 so noise always dominates; attractor only biases
	attractForce = ofMap(energy, 0.02f, 0.25f, 0.f, 0.35f, true);
}

void CrosshairSystem::setPreset(int index) {
	presetIndex = ofClamp(index, 0, (int)presets.size() - 1);
}

void CrosshairSystem::nextPreset() {
	setPreset((presetIndex + 1) % (int)presets.size());
}
