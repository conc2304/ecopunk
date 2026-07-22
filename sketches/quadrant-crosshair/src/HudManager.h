#pragma once
#include "ofMain.h"
#include "HudElements.h"
#include "TriggerBus.h"
#include "CrosshairSystem.h"
#include "LFOBank.h"
#include "QuadrantManager.h"
#include "MotionExtraction.h"
#include "NatureCopy.h"
#include <vector>

class HudManager {
public:
    void setup(TriggerBus&, CrosshairSystem&, LFOBank&,
               QuadrantManager&, MotionExtraction&);
    void update(float dt);
    void draw(float expansionFade = 1.f);

    // ── In-code dials ──────────────────────────────────────────────────
    bool showBioGauge   = false;   // motion energy ring (top-center left)
    bool showDwellGauge = false;   // dwell semi-circle (top-center right)

    void onVideoFileChanged(const std::string& basename);

private:
    // Pool index → widget mapping
    // 0:Contours  1:HexGrid  2:Network  3:Reticles  4:FlowField
    // -1 = silence (no element)
    // Scanner, DataCards, MotionGauge, DwellGauge are always on — not in pool
    hud::ContourWidget     contours;
    hud::HexGridWidget     hexGrid;
    hud::NodeNetworkWidget network;
    hud::ScannerWidget     scanner;
    hud::GaugeWidget       motionGauge;
    hud::GaugeWidget       dwellGauge;
    hud::DataCardWidget    quadCards[4];   // always visible, not in pool
    hud::ReticleWidget     reticles;
    hud::FlowFieldWidget   flowField;

    // Always-on, event-driven — not in the rotation pool.
    hud::StatusLightWidget mediaStatus;
    hud::LogScrollWidget   log;
    hud::GlitchTearWidget  glitch;
    float logPushTimer = 0.f;

    // System pointers (non-owning)
    TriggerBus*       triggerBus   = nullptr;
    CrosshairSystem*  crosshairSys = nullptr;
    LFOBank*          lfo          = nullptr;
    QuadrantManager*  quadMgr      = nullptr;
    MotionExtraction* motionEx     = nullptr;

    hud::HudTheme    baseTheme;
    float            time = 0.f;

    // Stored so label can be swapped on file change without losing style/units
    hud::GaugeOptions motionGaugeOpts_;
    hud::GaugeOptions dwellGaugeOpts_;
    // Stored so labelOverride can be swapped without losing other reticle settings
    hud::ReticleOptions reticleOpts_;

    NatureCopy currentCopy_;

    // ── Rotating slot system ──────────────────────────────────────────────
    struct HudSlot {
        enum class State { IDLE, FADE_IN, ACTIVE, FADE_OUT };
        State state      = State::IDLE;
        int   elemIdx    = -1;     // pool index; -1 = silence period
        float alpha      = 0.f;
        float drawnAlpha = 0.f;
        float fadeDur    = 7.f;
        float dwellDur   = 45.f;
        float dwellAcc   = 0.f;
        bool  isIdle() const { return state == State::IDLE; }
    };
    HudSlot          slots[2];
    std::vector<int> deck;

    void refillDeck();
    void popNextToSlot(int slotIdx);
    void startSlot(HudSlot& slot, int idx);
    void updateSlot(int slotIdx, float dt);
    void drawElement(int elemIdx, float alpha);

    hud::HudTheme themedAt(float alpha01) const;
    void          updateScannerBounds(const CrosshairState& cs);
    void          updateTelemetryCards();
    ofVec2f       panelBasePos(int q) const;
};
