#pragma once

#include "ofMain.h"
#include "FTFragment.h"
#include "CrosshairSystem.h"
#include "TimeOffsetVideoBuffer.h"
#include "ShaderLibrary.h"
#include <vector>
#include <string>
#include <functional>

// Owns the whole population of fragments: spawn throttle (Section 1), age
// decay + hard-cap eviction, and Mode-A playhead acquire/release against
// TimeOffsetVideoBuffer's small playhead pool (Section 9's resolved
// playhead-pool-vs-maxFragments question — see spawnOne()).
class FTFragmentPool {
public:
	void setup(int maxFragments, float spawnMinDistance, float spawnMinInterval,
		bool idlePulseEnabled, float idlePulseInterval,
		float sustainSeconds, float decaySeconds, float minSize, float maxSize,
		int numPlayheads);

	// Re-applies the ofxGui panel's current values — called every frame so
	// the panel's sliders stay actually live, not just startup defaults.
	// spawnMinDistance/spawnMinInterval aren't included here: those are
	// passed into update() already speedResponse-scaled for the frame.
	void applyLiveSettings(int maxFragments_, float sustainSeconds_, float decaySeconds_,
		float minSize_, float maxSize_, bool idlePulseEnabled_, float idlePulseInterval_) {
		maxFragments = maxFragments_;
		sustainSeconds = sustainSeconds_;
		decaySeconds = decaySeconds_;
		minSize = minSize_;
		maxSize = maxSize_;
		idlePulseEnabled = idlePulseEnabled_;
		idlePulseInterval = idlePulseInterval_;
	}

	// effectiveMinDistance/effectiveMinInterval are the speedResponse-scaled
	// thresholds for this frame — computed by the caller (ofApp), not here;
	// this class only consumes them (see FTParameterPanel/CrosshairSystem's
	// HIGH_THRESH/LOW_THRESH for the normalization itself).
	void update(float dt, const CrosshairState& crosshairState, TimeOffsetVideoBuffer& videoBuffer,
		FTContentMode activeMode, const std::string& modeCEffectName,
		float effectiveMinDistance, float effectiveMinInterval);

	void draw(TimeOffsetVideoBuffer& videoBuffer, ShaderLibrary& shaderLib);

	int getFragmentCount() const { return static_cast<int>(fragments.size()); }

	// Read-only access to the live population, oldest-first (see fragments'
	// own comment below) — e.g. for an external HUD overlay director that
	// anchors effects to real fragment positions/ages. Callers must not
	// retain a reference/pointer past the current frame: push_back()/erase()
	// elsewhere in this class can reallocate or invalidate it.
	const std::vector<FTFragment>& getFragments() const { return fragments; }

	// Fired synchronously from spawnOne()/the hard-cap eviction loop below —
	// same "let an external consumer react to a real event" pattern as
	// BEComposition::setOnFragmentPlaced()/TFComposition::setOnPatternChanged().
	// Callback bodies must copy out what they need synchronously; the
	// FTFragment reference is not valid past the callback's return.
	void setOnFragmentSpawned(std::function<void(const FTFragment&)> cb) { onFragmentSpawned = std::move(cb); }
	void setOnFragmentEvicted(std::function<void(const FTFragment&)> cb) { onFragmentEvicted = std::move(cb); }

private:
	std::vector<FTFragment> fragments; // front = oldest; hard-cap evicts from the front
	std::vector<bool> playheadInUse;

	int maxFragments = 18;
	float spawnMinDistance = 70.f;
	float spawnMinInterval = 0.10f;
	bool idlePulseEnabled = false;
	float idlePulseInterval = 1.2f;
	float sustainSeconds = 1.6f;
	float decaySeconds = 1.6f;
	float minSize = 90.f, maxSize = 220.f;

	glm::vec2 lastSpawnPos{ -9999.f, -9999.f };
	float sinceLastSpawn = 0.f;
	int spawnCounter = 0;

	hud::HudTheme theme;

	std::function<void(const FTFragment&)> onFragmentSpawned;
	std::function<void(const FTFragment&)> onFragmentEvicted;

	int acquirePlayhead();
	void releasePlayhead(int idx);
	void spawnOne(glm::vec2 pos, FTContentMode mode, const std::string& modeCEffectName, TimeOffsetVideoBuffer& videoBuffer);
	std::string makeHexLabel();
};
