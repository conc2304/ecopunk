#pragma once

#include "CompositionBase.h"
#include "BECompositionState.h"
#include "BEFragment.h"
#include "GridState.h"
#include "LFOBank.h"
#include "ShaderLibrary.h"
#include "TriggerBus.h"
#include "VideoSampler.h"
#include "glm/vec2.hpp"
#include <array>
#include <memory>
#include <vector>

namespace hud {
	class HudWidget;
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

		void setupBE(GridSystem* grid, VideoSampler* videoSampler, int canvasW, int canvasH);

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

		// Non-null while a HUD widget is occupying a grid slot (max 1 at a
		// time, system-wide). Lifecycle owned/driven entirely by BEComposition.
		hud::HudWidget* getActiveHudWidget() const { return hudWidget.get(); }

	protected:
		bool attemptPlacement() override; // unused: see usesAutomaticPlacementTimer()
		void onCycleStart() override;
		void onUpdate(float dt) override;
		bool usesAutomaticPlacementTimer() const override { return false; }

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

		VideoSampler* videoSampler   = nullptr;
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
		HudPhase hudPhase = HudPhase::SILENCE;
		float hudTimer = 0.0f;
		float hudTimerTarget = 0.0f;

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
};
