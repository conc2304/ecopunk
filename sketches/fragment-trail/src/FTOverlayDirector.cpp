#include "FTOverlayDirector.h"
#include "ofMain.h"
#include <algorithm>
#include <cmath>

namespace {
// Canvas-corner anchors used by the expiry->LockSequence gesture — paired
// so the reticle position and its status-phrase callout land at the same
// corner.
struct CornerAnchor {
	float nx, ny;
	hud::CalloutAnchor callout;
};
const CornerAnchor kCanvasCorners[] = {
	{ 0.06f, 0.08f, hud::CalloutAnchor::TopLeft },
	{ 0.94f, 0.08f, hud::CalloutAnchor::TopRight },
	{ 0.06f, 0.92f, hud::CalloutAnchor::BottomLeft },
	{ 0.94f, 0.92f, hud::CalloutAnchor::BottomRight },
};
} // namespace

void FTOverlayDirector::setup(FTFragmentPool* poolPtr, hudoverlay::HudOverlayLayer* overlayPtr) {
	pool = poolPtr;
	overlay = overlayPtr;

	pool->setOnFragmentSpawned([this](const FTFragment& f) { onSpawned(f); });
	pool->setOnFragmentEvicted([this](const FTFragment& f) { onEvicted(f); });
}

float FTOverlayDirector::speedScale() const {
	// Matches HudOverlayScheduler::rearm()'s exact formula, so the Intensity
	// slider stays meaningfully live even with the internal scheduler
	// disabled (see ofApp::setup()'s setSchedulerEnabled(false)).
	return ofLerp(0.4f, 2.2f, ofClamp(currentDials.intensity, 0.0f, 1.0f));
}

bool FTOverlayDirector::tryAccent(float nx, float ny) {
	if (accentCooldown > 0.0f) return false;
	overlay->triggerTickBurstAccent(nx, ny);
	accentCooldown = ofRandom(2.0f, 5.0f) / speedScale();
	return true;
}

bool FTOverlayDirector::tryOrganism(const std::function<void()>& fire, bool isHandshake) {
	if (overlay->anyOrganismActive()) return false;
	if (organismCooldown > 0.0f) return false;
	if (isHandshake && handshakeFloor > 0.0f) return false;
	fire();
	organismCooldown = ofRandom(10.0f, 20.0f) / speedScale();
	if (isHandshake) handshakeFloor = ofRandom(20.0f, 40.0f) / speedScale();
	return true;
}

void FTOverlayDirector::onSpawned(const FTFragment& f) {
	ofRectangle b = f.getBounds();
	static const ofVec2f kCornerOffsets[] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
	ofVec2f corner = kCornerOffsets[static_cast<int>(ofRandom(4))];
	float px = b.x + corner.x * b.width;
	float py = b.y + corner.y * b.height;
	tryAccent(ofClamp(px / ofGetWidth(), 0.0f, 1.0f), ofClamp(py / ofGetHeight(), 0.0f, 1.0f));
}

void FTOverlayDirector::onEvicted(const FTFragment& f) {
	ofVec2f center = f.getBounds().getCenter();
	float nx = ofClamp(center.x / ofGetWidth(), 0.0f, 1.0f);
	float ny = ofClamp(center.y / ofGetHeight(), 0.0f, 1.0f);
	tryOrganism([this, nx, ny]() { overlay->triggerFaultCascade(nx, ny); }, false);
}

void FTOverlayDirector::pollDecayThreshold() {
	if (pool->getFragmentCount() == 0) return;
	const FTFragment& oldest = pool->getFragments().front();
	if (oldest.getAlpha() < 1.0f && lastDecayAccentSpawnIndex != oldest.getSpawnIndex()) {
		ofVec2f center = oldest.getBounds().getCenter();
		tryAccent(ofClamp(center.x / ofGetWidth(), 0.0f, 1.0f), ofClamp(center.y / ofGetHeight(), 0.0f, 1.0f));
		lastDecayAccentSpawnIndex = oldest.getSpawnIndex();
	}
}

void FTOverlayDirector::pollExpiry() {
	if (pool->getFragmentCount() == 0) return;
	const FTFragment& oldest = pool->getFragments().front();
	if (oldest.getAlpha() < 0.05f && lastExpirySpawnIndex != oldest.getSpawnIndex()) {
		const CornerAnchor& c = kCanvasCorners[static_cast<int>(ofRandom(4))];
		tryOrganism([this, c]() { overlay->triggerLockSequence(c.nx, c.ny, c.callout); }, false);
		lastExpirySpawnIndex = oldest.getSpawnIndex();
	}
}

void FTOverlayDirector::pollHandshake() {
	if (pool->getFragmentCount() < 2) return;
	const auto& fragments = pool->getFragments();
	const FTFragment& a = fragments.front();
	const FTFragment& b = fragments.back();

	float canvasW = ofGetWidth(), canvasH = ofGetHeight();
	ofVec2f aC = a.getBounds().getCenter();
	ofVec2f bC = b.getBounds().getCenter();
	float anx = ofClamp(aC.x / canvasW, 0.0f, 1.0f), any = ofClamp(aC.y / canvasH, 0.0f, 1.0f);
	float bnx = ofClamp(bC.x / canvasW, 0.0f, 1.0f), bny = ofClamp(bC.y / canvasH, 0.0f, 1.0f);

	float dx = anx - bnx, dy = any - bny;
	if (std::sqrt(dx * dx + dy * dy) < 0.15f) return;

	tryOrganism([this, anx, any, bnx, bny]() { overlay->triggerHandshake(anx, any, bnx, bny); }, true);
}

void FTOverlayDirector::update(float dt, const hudoverlay::HudOverlayDialState& dials) {
	currentDials = dials;
	accentCooldown = std::max(0.0f, accentCooldown - dt);
	organismCooldown = std::max(0.0f, organismCooldown - dt);
	handshakeFloor = std::max(0.0f, handshakeFloor - dt);

	pollDecayThreshold();
	pollExpiry();
	pollHandshake();
}
