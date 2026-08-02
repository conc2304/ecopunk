#pragma once
#include "FTFragmentPool.h"
#include "HudOverlayLayer.h"
#include "HudOverlayDialState.h"
#include <functional>

// Pure decision/glue layer between FTFragmentPool's real lifecycle events
// and HudOverlayLayer's organism/accent triggers — owns no rendering.
// Registers itself with the pool's spawn/evict callbacks in setup(); polls
// the pool each update() for decay-threshold crossings and Handshake
// opportunities (both are level checks against the live population, not
// discrete events the pool itself fires).
//
// Restraint rules (see the design brief this implements): at most one
// HudOverlayOrganism active at a time (gated via
// HudOverlayLayer::anyOrganismActive()), organism-tier events paced to
// roughly one per 10-20s, tick-burst accents on a much shorter independent
// cooldown since they're not organisms and don't compete for the mutex.
//
// Every trigger here is anchored to a real fragment position, not
// HudOverlayLayer's internal virtual-grid Discipline snap
// (pickPlacementPoint()) — that dial has close to no practical effect in
// this integration by design; event-anchored placement is more meaningful
// here than a virtual grid snap.
class FTOverlayDirector {
public:
	void setup(FTFragmentPool* pool, hudoverlay::HudOverlayLayer* overlay);
	void update(float dt, const hudoverlay::HudOverlayDialState& dials);

private:
	FTFragmentPool* pool = nullptr;
	hudoverlay::HudOverlayLayer* overlay = nullptr;
	hudoverlay::HudOverlayDialState currentDials; // latest dials; read by callbacks fired earlier in the same frame (one-frame-stale is fine)

	float accentCooldown = 0.0f;   // tier 1: tick-burst accent, no organism mutex
	float organismCooldown = 0.0f; // tier 2: Lock Sequence / Fault Cascade / Handshake, gated by overlay->anyOrganismActive()
	float handshakeFloor = 0.0f;   // extra floor so Handshake (checked every frame) doesn't starve the rarer eviction/expiry gestures

	// Guards against re-firing the decay-threshold/expiry accents for the
	// same still-fading oldest fragment across consecutive frames.
	int lastDecayAccentSpawnIndex = -1;
	int lastExpirySpawnIndex = -1;

	void onSpawned(const FTFragment& f);
	void onEvicted(const FTFragment& f);
	void pollDecayThreshold();
	void pollExpiry();
	void pollHandshake();

	bool tryAccent(float nx, float ny);
	bool tryOrganism(const std::function<void()>& fire, bool isHandshake);
	float speedScale() const;
};
