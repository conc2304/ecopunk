#pragma once
#include "ofMain.h"
#include "HudElements.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Deterministic visual + GL-state capture harness for shared/src/hud/.
// See main.cpp for intent. Run once before a HUD change and once after;
// diff bin/data/captures/*.png between the two runs.
class ofApp : public ofBaseApp {
public:
    void setup() override;
    void update() override;
    void draw() override;

private:
    struct Scenario {
        std::string name;
        std::function<std::unique_ptr<hud::HudWidget>()> build;
        float width = 200.0f;
        float height = 120.0f;
        int seed = 42;
        int steps = 60;              // fixed-dt update() calls before capture
        hud::HudTheme theme;
        hud::MotionSettings motion;
    };

    std::vector<Scenario> scenarios;
    size_t currentIndex = 0;
    std::unique_ptr<hud::HudWidget> currentWidget;
    int stepsRemaining = 0;
    static constexpr float kFixedDt = 1.0f / 60.0f;

    hud::HudTheme referenceTheme() const;
    hud::HudTheme themeWithFrame(hud::FrameStyle style) const;
    hud::MotionSettings neutralMotion() const;
    hud::MotionSettings activeMotion() const;

    void buildScenarios();
    void startScenario(size_t idx);
    void captureCurrentScenario();
};
