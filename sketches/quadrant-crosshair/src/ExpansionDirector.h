#pragma once
#include "ofMain.h"

enum class ExpansionState {
    IDLE,
    TRAVEL_OUT,
    HOLD,
    TRAVEL_BACK,
};

// Corner IDs match quadrant IDs:
// 0 = TL (top-left),  1 = TR (top-right)
// 2 = BL (bottom-left), 3 = BR (bottom-right)

struct ExpansionSequence {
    int       targetQuadrant = 0;
    glm::vec2 cornerPos;
    glm::vec2 originPos;
    float     travelDuration = 4.f;
    float     holdDuration   = 75.f;
    float     elapsed        = 0.f;
};

class ExpansionDirector {
public:
    void setup();
    // crosshairSpeed: px/frame from CrosshairState::speed, used to pace travel naturally
    void update(float dt, glm::vec2 currentCrosshairPos, float crosshairSpeed);

    void trigger(glm::vec2 currentCrosshairPos);
    void triggerQuadrant(int quadrantID, glm::vec2 currentCrosshairPos);

    ExpansionState getState()           const { return state; }
    glm::vec2      getCrosshairTarget() const { return travelledPos; }
    float          getExpansionT()      const { return expansionT; }
    int            getTargetQuadrant()  const { return seq.targetQuadrant; }
    float          getUIFadeAlpha()     const { return uiFadeAlpha; }
    bool           isActive()           const { return state != ExpansionState::IDLE; }

    float nextTriggerTime = 0.f;
    float timerAccum      = 0.f;

private:
    ExpansionState    state = ExpansionState::IDLE;
    ExpansionSequence seq;

    float     expansionT     = 0.f;
    float     uiFadeAlpha    = 1.f;
    glm::vec2 travelledPos   = { 0.f, 0.f };
    float     lastKnownSpeed = 1.5f;  // px/frame, updated each update() call

    void enterTravelOut(glm::vec2 currentPos);
    void enterHold();
    void enterTravelBack();
    void enterIdle();

    void updateTravelOut(float dt);
    void updateHold(float dt);
    void updateTravelBack(float dt);

    glm::vec2 cornerPosition(int quadrantID);
    int       randomQuadrant();
    float     easeInOut(float t);
};
