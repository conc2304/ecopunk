#pragma once

// ============================================================================
// BECollageTuning — the single developer-only tuning surface for the evolving
// Editorial Collage (scene-local; nothing here is a runtime/HUD control).
//
// Three curated profiles — QUIET, BALANCED (default), KINETIC — change event
// frequency and scope, never "motion on/off": every profile keeps evolving.
// Select with BECollage::setProfile(); the standalone sketch exposes it as
// BE_COLLAGE_PROFILE=<name> and key 'p'.
// ============================================================================

#include <cstring>
#include <strings.h>

struct CollageRange {
	float lo = 1.0f, hi = 2.0f;
};

enum class CollageEvent {
	// ── micro (graphics, overlays, measurement) ──
	OverprintFade,
	FrameEdge,
	GridReveal,
	TickExtend,
	MarkPulse,
	MarkMove,
	DataUpdate,
	AddMicroGraphic,
	RemoveMicroGraphic,
	// ── local (imagery / composition) ──
	CropSwap,
	MoveResize,
	ReplaceFragment,
	TransformChange,
	Split,
	Merge,
	BlockSlide,
	StripReplace,
	Montage,
	// ── internal choreography (never drawn from the weighted table) ──
	SwellMass,
	Recompose,
	Settle,
	DissolveClear,
	COUNT
};

constexpr int COLLAGE_EVENT_COUNT = static_cast<int>(CollageEvent::COUNT);

inline bool collageEventIsMicro(CollageEvent e) { return static_cast<int>(e) <= static_cast<int>(CollageEvent::RemoveMicroGraphic); }
inline bool collageEventIsLocal(CollageEvent e) {
	return static_cast<int>(e) > static_cast<int>(CollageEvent::RemoveMicroGraphic) && static_cast<int>(e) < static_cast<int>(CollageEvent::SwellMass);
}

inline const char * collageEventName(CollageEvent e) {
	static const char * names[] = { "OverprintFade", "FrameEdge", "GridReveal", "TickExtend", "MarkPulse", "MarkMove", "DataUpdate",
		"AddMicroGraphic", "RemoveMicroGraphic", "CropSwap", "MoveResize", "ReplaceFragment", "TransformChange", "Split", "Merge",
		"BlockSlide", "StripReplace", "Montage", "SwellMass", "Recompose", "Settle", "DissolveClear" };
	const int i = static_cast<int>(e);
	return (i >= 0 && i < COLLAGE_EVENT_COUNT) ? names[i] : "?";
}

struct CollageTuning {
	const char * name = "BALANCED";

	// Seeded-jitter interval ranges, seconds (at activity level 1.0).
	CollageRange micro { 1.0f, 3.0f };
	CollageRange local { 4.0f, 8.0f };
	CollageRange swell { 18.0f, 35.0f };
	CollageRange recompose { 60.0f, 100.0f };

	// Activity envelope: event-rate multipliers per state and the dwell time in
	// each. Rise is fast, decay slow (asymmetric); never a fixed sine.
	float quietLevel = 0.55f, activeLevel = 1.0f, swellLevel = 2.2f, settleLevel = 0.7f;
	float riseSeconds = 1.0f, decaySeconds = 3.5f;
	CollageRange activeDwell { 14.0f, 30.0f };
	CollageRange quietDwell { 6.0f, 12.0f };
	CollageRange swellLength { 5.0f, 8.0f };
	CollageRange settleLength { 5.0f, 8.0f };
	float quietChance = 0.3f; // chance that an active dwell ends in QUIET rather than staying ACTIVE

	// Bounds.
	int maxFragments = 8;
	int minFragments = 3;
	int maxGraphics = 34;
	int maxConcurrent = 4; // elements simultaneously mid-transition (scaled by activity)
	int maxQueue = 24;
	float coverLo = 0.20f, coverHi = 0.46f; // video area fraction: sparse below / crowded above

	// Transition durations, seconds.
	CollageRange enterDur { 0.5f, 1.1f };
	CollageRange exitDur { 0.4f, 0.9f };
	CollageRange moveDur { 0.8f, 1.6f };
	CollageRange cropDur { 0.7f, 1.3f };
	float holdVideoMin = 3.0f; // minimum readable hold before an element may be mutated/replaced
	float holdGraphicMin = 2.0f;
	CollageRange anchorRotate { 25.0f, 45.0f };

	// Mutation weights, indexed by CollageEvent.
	float weights[COLLAGE_EVENT_COUNT] = {
		1.0f, // OverprintFade
		0.8f, // FrameEdge
		0.9f, // GridReveal
		0.8f, // TickExtend
		0.9f, // MarkPulse
		0.5f, // MarkMove
		0.8f, // DataUpdate
		1.0f, // AddMicroGraphic
		0.8f, // RemoveMicroGraphic
		1.4f, // CropSwap
		1.1f, // MoveResize
		1.1f, // ReplaceFragment
		0.8f, // TransformChange
		0.7f, // Split
		0.6f, // Merge
		1.0f, // BlockSlide
		0.5f, // StripReplace
		0.6f, // Montage
		0.0f, // SwellMass (internal)
		0.0f, // Recompose (internal)
		0.0f, // Settle (internal)
		0.0f // DissolveClear (internal)
	};

	// Performance. lowQuality = Raspberry Pi 3B fallback: fewer elements and less
	// concurrency, cheaper circles. Evolution never stops.
	bool lowQuality = false;

	void applyLowQuality() {
		lowQuality = true;
		maxFragments = 6;
		maxGraphics = 22;
		maxConcurrent = 2;
	}

	static CollageTuning balanced() { return CollageTuning(); }

	static CollageTuning quiet() {
		CollageTuning t;
		t.name = "QUIET";
		t.micro = { 2.5f, 5.0f };
		t.local = { 7.0f, 12.0f };
		t.swell = { 30.0f, 50.0f };
		t.recompose = { 90.0f, 140.0f };
		t.quietLevel = 0.5f;
		t.swellLevel = 1.7f;
		t.quietChance = 0.45f;
		t.maxFragments = 6;
		t.maxGraphics = 26;
		t.maxConcurrent = 3;
		t.moveDur = { 1.2f, 2.2f };
		t.cropDur = { 1.0f, 1.8f };
		return t;
	}

	static CollageTuning kinetic() {
		CollageTuning t;
		t.name = "KINETIC";
		t.micro = { 0.7f, 1.6f };
		t.local = { 2.5f, 5.0f };
		t.swell = { 12.0f, 22.0f };
		t.recompose = { 40.0f, 70.0f };
		t.quietLevel = 0.7f;
		t.swellLevel = 2.6f;
		t.quietChance = 0.15f;
		t.activeDwell = { 10.0f, 20.0f };
		t.maxFragments = 9;
		t.maxGraphics = 40;
		t.maxConcurrent = 6;
		t.moveDur = { 0.6f, 1.2f };
		t.cropDur = { 0.5f, 1.0f };
		t.holdVideoMin = 2.0f;
		return t;
	}

	static CollageTuning byName(const char * n) {
		if (n && (!strcasecmp(n, "quiet"))) return quiet();
		if (n && (!strcasecmp(n, "kinetic"))) return kinetic();
		return balanced();
	}
};
