#pragma once

#include "BECycleMode.h"
#include "BECompositionState.h"
#include "BEFragment.h"
#include "CompositionBase.h"
#include "GridState.h"
#include "LFOBank.h"
#include "ShaderLibrary.h"
#include "TriggerBus.h"
#include "VideoPlaybackService.h"
#include "glm/vec2.hpp"
#include <array>
#include <memory>
#include <vector>

namespace hud {
	class HudWidget;
	class ScannerWidget;
	class DataCardWidget;
	class GaugeWidget;
	class ContourWidget;
	class FlowFieldWidget;
	class HexGridWidget;
	class NodeNetworkWidget;
	class ReticleWidget;
	class StatusLightWidget;
	class LogScrollWidget;
	class GlitchTearWidget;
}
class MotionExtraction;

// Blueprint Emergence's placement algorithm (§06): geometry/size selection,
// random candidate sampling, scoring, and zone bookkeeping.
class BEComposition : public CompositionBase {
	public:
		// Both declared here (not defaulted inline) so their bodies are
		// generated in the .cpp, after hud::HudWidget's full definition is
		// included — this header only forward-declares it, to avoid leaking
		// HUD widget headers into every consumer of BEComposition.h. The
		// constructor needs this too, not just the destructor: generating an
		// implicit default constructor still requires knowing how to unwind
		// (destroy) already-constructed members if a later member's
		// construction throws, which needs hudWidget's unique_ptr<T> to see
		// a complete T.
		BEComposition();
		~BEComposition() override;

		void setupBE(GridSystem* grid, VideoPlaybackService* videoPlayback, int canvasW, int canvasH);

		// Non-owning vitality-system pointers, wired once by ofApp after
		// setupBE(). Any of these may be left null to opt out of that system.
		void setLFOBank(LFOBank* lfo) { lfoBank = lfo; }
		void setTriggerBus(TriggerBus* bus);
		void setGridState(GridState* gs) { gridState = gs; }
		void setShaderLibrary(ShaderLibrary* lib) { shaderLib = lib; }
		void setMotionExtraction(MotionExtraction* m) { motionEx = m; }

		bool isZoneALight() const { return zoneALight; }
		BECompositionState getState() const;
		float getGridDimAmount() const { return gridDimCurrent; }
		CycleMode getCurrentMode() const { return currentMode; }
		float getDividerX() const { return divider.pivot.x; }

		// Canvas-edge endpoints of the divider at its current angle/pivot —
		// valid at any angle (0°=vertical, 90°=horizontal, mid-rotation).
		std::pair<glm::vec2, glm::vec2> getDividerEndpoints() const;

		// Orientation helpers for the zone-background split in ofApp.
		bool  isDividerHorizontal() const { return divider.orientation == DividerOrientation::HORIZONTAL; }
		float getDividerY()         const { return divider.pivot.y; }

		// Force an axis-flip rotation immediately (keyboard shortcut / debug).
		// No-op if a rotation is already in progress.
		void forceAxisFlip();

		// Non-null while a HUD widget is occupying a grid slot (max 1 at a
		// time, system-wide). Lifecycle owned/driven entirely by BEComposition.
		hud::HudWidget* getActiveHudWidget() const { return hudWidget.get(); }

		// Non-null while the hero circle fragment is alive and holds its scanner
		// overlay. Created in placeCircleFragment(), destroyed when the circle
		// begins dissolving. Drawn by ofApp on top of the fragment layer.
		hud::HudWidget* getCircleScannerWidget() const;

	protected:
		bool attemptPlacement() override; // unused: see usesAutomaticPlacementTimer()
		void onCycleStart() override;
		void onUpdate(float dt) override;
		bool usesAutomaticPlacementTimer() const override { return false; }
		bool usesAutomaticPhaseTimer() const override { return currentMode != CycleMode::PERPETUAL; }
		bool onDissolveComplete() override;
		float getDissolveFloor() const override;

	private:
		// Quadrant-style slot lifecycle: each of the (up to) NUM_SLOTS slots
		// independently cycles EMPTY (spawn attempt) -> ARRIVING -> HOLD
		// (guaranteed-visible window) -> DISSOLVING -> EMPTY (silence wait)
		// -> repeat. Mirrors quadrant-crosshair's Quadrant PLAYING/SILENCING/
		// READY loop, mapped onto BEFragment's existing Fragment::State.
		enum class SlotPhase { EMPTY, ARRIVING, HOLD, DISSOLVING };

		struct SlotState {
			SlotPhase phase = SlotPhase::EMPTY;
			float timer = 0.0f;
			float timerTarget = 0.0f; // hold duration, or silence/retry backoff while EMPTY
			// Index into `fragments` this slot owns, claimed on its first
			// successful spawn and reused on every respawn thereafter. Slots
			// don't necessarily claim indices in slot-number order (a slot can
			// fail its first attempt and retry while another succeeds first),
			// so this must not be assumed equal to the slot's own index.
			int fragmentIndex = -1;
		};

		static constexpr int NUM_SLOTS = 4;

		struct Candidate {
			ofRectangle bounds;
			float score;
			Fragment* nearest;
		};

		void updateSlots(float dt);
		bool spawnFragmentInSlot(int slotIndex);

		// ── Continuous cycle mode ─────────────────────────────────────────────
		// Divider geometry — replaces the old float dividerX.
		enum class DividerOrientation { VERTICAL, HORIZONTAL };
		enum class Zone { A, B };

		struct DividerLine {
			glm::vec2         pivot;
			float             angle       = 0.0f; // degrees: 0=vertical, 90=horizontal
			DividerOrientation orientation = DividerOrientation::VERTICAL;
		};

		// Axis-flip rotation animation state machine.
		enum class RotationPhase { INACTIVE, ROTATING_AROUND_A, ROTATING_AROUND_B };

		struct DividerRotationAnim {
			RotationPhase phase            = RotationPhase::INACTIVE;
			glm::vec2     pivotA;
			glm::vec2     pivotB;
			bool          pivotBChosen     = false;
			float         startAngle       = 0.0f;
			float         targetAngle      = 90.0f;
			float         currentAngle     = 0.0f;
			float         totalDuration    = 0.0f;
			float         elapsed          = 0.0f;
			int           candidatesFound  = 0;
			int           targetCandidateIndex = 0;
			float         minTravelAngle   = 0.0f;
			void reset() {
				phase = RotationPhase::INACTIVE;
				pivotBChosen = false;
				candidatesFound = 0;
				elapsed = 0.0f;
			}
		};

		void enterMode(CycleMode mode);
		void updateGhostDecay(float dt);
		void updateDividerAnimation(float dt);
		void updatePerpetualMode(float dt);
		void pruneDeadFragments();
		float selectNewDividerX() const;

		// Axis-flip rotation
		void triggerDividerRelocation();
		bool shouldAxisFlip() const;
		void startAxisFlipAnimation();
		void updateRotationAnim(float dt);
		void updatePivotBDetection();
		std::vector<glm::vec2> getLineGridIntersections(const glm::vec2& pivot, float angleDeg) const;
		bool isNewIntersection(const glm::vec2& pt);
		void handOffToPivotB();
		void completeRotationAnimation();
		void onDividerRelocationComplete();
		glm::vec2 nearestGridIntersection(const glm::vec2& pos) const;
		Zone getZoneForPoint(const glm::vec2& pt) const;
		Zone getZoneForCell(int col, int row) const;

		// HUD widget — independent of the 4 video slots; occasionally claims
		// unoccupied grid space instead of a video fragment.
		enum class HudPhase { SILENCE, ACTIVE };
		void updateHudWidget(float dt);
		bool trySpawnHudWidget();

		GeometryType pickGeometryType() const;
		void pickSizeRange(GeometryType type, glm::vec2& wRange, glm::vec2& hRange) const;
		int countZoneFragments(bool zoneA) const;
		ofColor pickPlaceholderColor() const;
		void requestVideoTexture(BEFragment* fragment, int w, int h) const;
		bool placeCircleFragment(int slotIndex); // rare hero event; bypasses scored-candidate flow
		bool findSnapSpan(const std::vector<float>& positions, float lo, float hi, float& outStart, float& outEnd) const;
		std::vector<ofRectangle> otherFragmentBounds(const Fragment* exclude) const;

		float lfoDesatNudgeForGroup(int group) const;
		void evaluateStateTriggers();

		VideoPlaybackService* videoPlayback = nullptr;
		int canvasW                  = 0;
		int canvasH                  = 0;
		int nextFragmentId           = 0;
		bool zoneALight              = false;

		std::array<SlotState, NUM_SLOTS> slots{};
		bool circleSpawnedThisCycle  = false; // hero circle: at most once per cycle

		// HUD widget state — sentinel id keeps its GridSystem occupancy
		// reservation distinct from real fragment ids (which start at 0).
		static constexpr int HUD_FRAGMENT_ID = -2;
		std::unique_ptr<hud::HudWidget> hudWidget;
		// Non-owning typed aliases into hudWidget — set alongside hudWidget,
		// cleared whenever hudWidget is destroyed. Used for live per-frame updates.
		hud::DataCardWidget*    hudDataCard    = nullptr;
		hud::GaugeWidget*       hudGauge       = nullptr;
		hud::NodeNetworkWidget* hudNodeNetwork = nullptr;
		hud::ReticleWidget*     hudReticle     = nullptr;
		hud::HexGridWidget*     hudHexGrid     = nullptr;
		hud::StatusLightWidget* hudStatusLight = nullptr;
		HudPhase hudPhase = HudPhase::SILENCE;
		float hudTimer = 0.0f;
		float hudTimerTarget = 0.0f;

		// Circle scanner overlay — lives for the duration of the hero circle.
		std::unique_ptr<hud::ScannerWidget> circleScanner;
		int circleScannerSlot = -1;

		// Vitality systems — non-owning, set by ofApp after setupBE()
		LFOBank* lfoBank             = nullptr;
		TriggerBus* triggerBus       = nullptr;
		GridState* gridState         = nullptr;
		ShaderLibrary* shaderLib     = nullptr;
		MotionExtraction* motionEx   = nullptr;

		float secondsSinceLastPlacement = 0.0f;
		bool densityHighActive          = false;
		bool densityCriticalActive      = false;
		int  zoneScoreBoostRemaining    = 0;
		float gridDimCurrent             = 0.0f;
		float gridDimTarget              = 0.0f;

		// ── Continuous cycle mode ─────────────────────────────────────────────
		CycleMode currentMode              = CycleMode::GHOST_LAYERS;
		DividerLine divider;                         // position + angle + orientation
		float dividerTargetX               = 0.0f;  // PERPETUAL vertical drift target
		float dividerTargetY               = 0.0f;  // PERPETUAL horizontal drift target
		bool  dividerAnimating             = false;  // GHOST_LAYERS position-jump in progress
		float dividerAnimFrom              = 0.0f;
		float dividerAnimTo                = 0.0f;
		float dividerAnimT                 = 0.0f;  // 0..1 normalised progress
		float dividerAnimDur               = 0.0f;
		DividerRotationAnim rotAnim;
		std::vector<glm::vec2> seenIntersections;   // intersections counted this rotation
		float timeInMode                   = 0.0f;
		float modeTransitionTarget         = 0.0f;  // PERPETUAL: seconds until transition armed
		bool  perpetualTransitionArmed     = false;
		int   placementsSinceSeedRefresh   = 0;
};
