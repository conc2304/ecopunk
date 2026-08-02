#pragma once
#include "HudOverlayDialState.h"
#include "HudOverlayScheduler.h"
#include "HudElements.h"
#include "organisms/LockSequenceOrganism.h"
#include "organisms/FaultCascadeOrganism.h"
#include "organisms/RadarPingOrganism.h"
#include "organisms/HandshakeOrganism.h"

namespace hudoverlay {

// Top-level owner (design doc Section 06). In this phase draws directly to
// the full framebuffer over a solid near-black background — no knowledge of
// Fragment/VideoSampler/Composition Manager, and none should be added here;
// see shared/src/hud_overlay/README.md's isolation boundary.
class HudOverlayLayer {
public:
	void setup(float width, float height);
	void update(float dt, const HudOverlayDialState& dials);
	// Split so a live-compositing consumer (e.g. fragment-trail) can
	// interleave these with its own draw calls instead of drawing the
	// overlay as one opaque block. draw() is a thin wrapper calling both,
	// in order — unchanged behavior for the standalone-toggle sketches.
	void drawAmbient();   // the always-on ambient atoms — "the room"
	void drawOrganisms(); // the accent burst + the 4 composite organisms
	void draw() {
		drawAmbient();
		drawOrganisms();
	}
	void windowResized(float width, float height);

	// When false, skips the internal random-interval scheduler entirely —
	// for a consumer that wants organisms triggered only by its own real
	// events (trigger*() below), not ambient random timing. Defaults to
	// true (unchanged behavior for the standalone-toggle sketches).
	void setSchedulerEnabled(bool enabled) { schedulerEnabled = enabled; }

	// True while any of the 4 organisms is mid-sequence — lets an external
	// caller enforce "at most one organism active at a time" before firing
	// a new one via the trigger*() passthroughs below.
	bool anyOrganismActive() const {
		return lockSequence.isActive() || faultCascade.isActive() || radarPing.isActive() || handshake.isActive();
	}

	// Thin passthroughs to the otherwise-private organisms/accent atom, for
	// a consumer driving them off real events instead of the scheduler.
	void triggerLockSequence(float nx, float ny, hud::CalloutAnchor corner) { lockSequence.trigger(nx, ny, corner); }
	void triggerFaultCascade(float nx, float ny) { faultCascade.trigger(nx, ny); }
	void triggerRadarPing(float nx, float ny) { radarPing.trigger(nx, ny); }
	void triggerHandshake(float nxA, float nyA, float nxB, float nyB) {
		handshake.trigger(nxA, nyA, nxB, nyB, canvasW, canvasH);
	}
	// Standalone one-shot burst — not an HudOverlayOrganism, so it's exempt
	// from the anyOrganismActive() mutex: cheap enough to accent a real
	// event (e.g. a spawn) without competing with the rarer bold gestures.
	void triggerTickBurstAccent(float nx, float ny) { accentBurst.triggerAt(nx, ny); }

	// Gates FaultCascadeOrganism's halftone-patch component (default true,
	// matching the standalone-toggle sketches' existing look). A consumer
	// wanting a calmer tear+flash-only eviction gesture sets this false.
	void setFaultCascadeHalftoneEnabled(bool enabled) { faultCascade.setHalftoneEnabled(enabled); }

private:
	bool schedulerEnabled = true;
	float canvasW = 1280.0f, canvasH = 720.0f;
	Palette appliedPalette = Palette::Mono; // tracks last-applied theme so setTheme() isn't called every frame
	HudOverlayDialState currentDials; // latest dial values, set at the top of update(); read by scheduler callbacks and draw()

	// Continuous ambient atoms (Section 03).
	float scanlineT = 0.0f;
	hud::RadarStationWidget radarStation;
	hud::BreathingTickClusterWidget breathingTicks;
	hud::TelemetryReadoutWidget frameCounter;
	hud::TelemetryReadoutWidget coordinateReadout;
	hud::TickBurstWidget accentBurst; // standalone accent atom — see triggerTickBurstAccent()

	// Composite organisms (Section 04) + their scheduler (Section 05).
	HudOverlayScheduler scheduler;
	LockSequenceOrganism lockSequence;
	FaultCascadeOrganism faultCascade;
	RadarPingOrganism radarPing;
	HandshakeOrganism handshake;

	void applyThemeIfChanged(Palette palette);
	void drawScanline(float intensity);
	ofVec2f pickPlacementPoint(float discipline) const;
};

} // namespace hudoverlay
