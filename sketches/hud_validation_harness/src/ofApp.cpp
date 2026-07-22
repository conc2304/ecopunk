#include "ofApp.h"

hud::HudTheme ofApp::referenceTheme() const {
    hud::HudTheme t;
    t.colors.primary = ofColor(124, 232, 230, 220);
    t.colors.secondary = ofColor(103, 255, 142, 200);
    t.colors.accent = ofColor(244, 255, 106, 220);
    t.colors.muted = ofColor(124, 232, 230, 70);
    t.colors.background = ofColor(0, 20, 16, 18);
    t.frame.style = hud::FrameStyle::Corners;
    t.frame.showTicks = true;
    t.additive = true;
    return t;
}

hud::HudTheme ofApp::themeWithFrame(hud::FrameStyle style) const {
    hud::HudTheme t = referenceTheme();
    t.frame.style = style;
    return t;
}

hud::MotionSettings ofApp::neutralMotion() const {
    return hud::MotionSettings{1.0f, 1.0f, 1.0f, 1.0f};
}

hud::MotionSettings ofApp::activeMotion() const {
    return hud::MotionSettings{1.3f, 1.0f, 1.2f, 1.2f};
}

void ofApp::buildScenarios() {
    hud::HudTheme theme = referenceTheme();
    hud::MotionSettings neutral = neutralMotion();

    struct BoundsSpec { std::string tag; float w, h; };
    std::vector<BoundsSpec> boundsList = {
        {"200x120", 200, 120}, {"400x240", 400, 240}, {"100x60", 100, 60}, {"600x80", 600, 80}
    };

    struct WidgetSpec { std::string name; std::function<std::unique_ptr<hud::HudWidget>()> make; };
    std::vector<WidgetSpec> widgets = {
        {"Scanner", []() { auto w = std::make_unique<hud::ScannerWidget>(); w->setup(); return w; }},
        {"Contour", []() { auto w = std::make_unique<hud::ContourWidget>(); w->setup(); return w; }},
        {"HexGrid", []() { auto w = std::make_unique<hud::HexGridWidget>(); w->setup(); return w; }},
        {"Gauge", []() { auto w = std::make_unique<hud::GaugeWidget>(); w->setup(); return w; }},
        {"DataCard", []() { auto w = std::make_unique<hud::DataCardWidget>(); w->setup(); return w; }},
        {"FlowField", []() { auto w = std::make_unique<hud::FlowFieldWidget>(); w->setup(); return w; }},
        {"NodeNetwork", []() { auto w = std::make_unique<hud::NodeNetworkWidget>(); w->setup(); return w; }},
        {"Reticle", []() { auto w = std::make_unique<hud::ReticleWidget>(); w->setup(); return w; }},
    };

    // Base matrix: every widget x every required bounds size, default options.
    for (auto& ws : widgets) {
        for (auto& bs : boundsList) {
            Scenario s;
            s.name = ws.name + "_" + bs.tag;
            s.build = ws.make;
            s.width = bs.w;
            s.height = bs.h;
            s.theme = theme;
            s.motion = neutral;
            scenarios.push_back(s);
        }
    }

    // Gauge styles.
    for (auto style : {hud::GaugeStyle::Ring, hud::GaugeStyle::Segmented, hud::GaugeStyle::SemiCircle}) {
        std::string styleName = style == hud::GaugeStyle::Ring ? "Ring" : style == hud::GaugeStyle::Segmented ? "Segmented" : "SemiCircle";
        Scenario s;
        s.name = "Gauge_style_" + styleName;
        s.width = 300; s.height = 200; s.theme = theme; s.motion = neutral;
        s.build = [style]() {
            auto w = std::make_unique<hud::GaugeWidget>();
            hud::GaugeOptions o; o.style = style; o.value = 0.65f;
            w->setOptions(o);
            return w;
        };
        scenarios.push_back(s);
    }

    // Scanner: showBackground on/off, at two fixed times (different sweep/pulse phase).
    for (bool bg : {true, false}) {
        for (int steps : {12, 180}) { // ~0.2s and ~3.0s at 60fps
            Scenario s;
            s.name = "Scanner_bg" + std::string(bg ? "On" : "Off") + "_steps" + ofToString(steps);
            s.width = 300; s.height = 200; s.theme = theme; s.motion = neutral; s.steps = steps;
            s.build = [bg]() {
                auto w = std::make_unique<hud::ScannerWidget>();
                hud::ScannerOptions o; o.showBackground = bg;
                w->setOptions(o);
                return w;
            };
            scenarios.push_back(s);
        }
    }

    // Reticle: Standard/Tracking preset x default/override labels (including a
    // short override list, to exercise the modulo-safe label indexing).
    {
        Scenario s1; s1.name = "Reticle_Standard_defaultLabels";
        s1.width = 300; s1.height = 200; s1.theme = theme; s1.motion = neutral;
        s1.build = []() {
            auto w = std::make_unique<hud::ReticleWidget>();
            hud::ReticleOptions o; o.preset = hud::ReticlePreset::Standard; o.targetCount = 5;
            w->setOptions(o);
            return w;
        };
        scenarios.push_back(s1);

        Scenario s2; s2.name = "Reticle_Tracking_defaultLabels";
        s2.width = 300; s2.height = 200; s2.theme = theme; s2.motion = neutral;
        s2.build = []() {
            auto w = std::make_unique<hud::ReticleWidget>();
            hud::ReticleOptions o; o.preset = hud::ReticlePreset::Tracking; o.targetCount = 5;
            w->setOptions(o);
            return w;
        };
        scenarios.push_back(s2);

        Scenario s3; s3.name = "Reticle_Tracking_overrideLabels_shortList";
        s3.width = 300; s3.height = 200; s3.theme = theme; s3.motion = neutral;
        s3.build = []() {
            auto w = std::make_unique<hud::ReticleWidget>();
            hud::ReticleOptions o;
            o.preset = hud::ReticlePreset::Tracking;
            o.targetCount = 5;
            o.labelOverride = {"ALPHA"}; // fewer entries than targetCount — must not crash
            w->setOptions(o);
            return w;
        };
        scenarios.push_back(s3);
    }

    // FlowField: density 1.0 vs 0.5.
    for (float density : {1.0f, 0.5f}) {
        Scenario s;
        s.name = "FlowField_density" + ofToString(density, 1);
        s.width = 400; s.height = 200; s.theme = theme; s.motion = neutral;
        s.build = [density]() {
            auto w = std::make_unique<hud::FlowFieldWidget>();
            hud::FlowFieldOptions o; o.density = density;
            w->setOptions(o);
            return w;
        };
        scenarios.push_back(s);
    }

    // DataCard: default vs after setMeter/setValueText.
    {
        Scenario s1; s1.name = "DataCard_default";
        s1.width = 260; s1.height = 140; s1.theme = theme; s1.motion = neutral;
        s1.build = []() { auto w = std::make_unique<hud::DataCardWidget>(); w->setup(); return w; };
        scenarios.push_back(s1);

        Scenario s2; s2.name = "DataCard_afterSetMeterAndValueText";
        s2.width = 260; s2.height = 140; s2.theme = theme; s2.motion = neutral;
        s2.build = []() {
            auto w = std::make_unique<hud::DataCardWidget>();
            w->setup();
            w->setMeter(0.91f);
            w->setValueText("42/50");
            return w;
        };
        scenarios.push_back(s2);
    }

    // Frames: every FrameStyle at small and large bounds (uses GaugeWidget as
    // a representative interior so the frame's relationship to content is visible).
    for (auto style : {hud::FrameStyle::None, hud::FrameStyle::Corners, hud::FrameStyle::Box, hud::FrameStyle::Brackets, hud::FrameStyle::Organic}) {
        std::string styleName;
        switch (style) {
            case hud::FrameStyle::None: styleName = "None"; break;
            case hud::FrameStyle::Corners: styleName = "Corners"; break;
            case hud::FrameStyle::Box: styleName = "Box"; break;
            case hud::FrameStyle::Brackets: styleName = "Brackets"; break;
            case hud::FrameStyle::Organic: styleName = "Organic"; break;
        }
        for (auto& bs : std::vector<BoundsSpec>{{"small", 120, 80}, {"large", 500, 300}}) {
            Scenario s;
            s.name = "Frame_" + styleName + "_" + bs.tag;
            s.width = bs.w; s.height = bs.h; s.motion = neutral;
            s.theme = themeWithFrame(style);
            s.build = []() { auto w = std::make_unique<hud::GaugeWidget>(); w->setup(); return w; };
            scenarios.push_back(s);
        }
    }

    // One non-neutral-motion case per widget family that has a motion-reactive
    // path, to confirm nothing breaks away from the default MotionSettings.
    hud::MotionSettings active = activeMotion();
    for (auto& ws : widgets) {
        Scenario s;
        s.name = ws.name + "_activeMotion";
        s.width = 300; s.height = 200; s.theme = theme; s.motion = active;
        s.build = ws.make;
        scenarios.push_back(s);
    }
}

void ofApp::setup() {
    ofSetFrameRate(0);
    ofSetVerticalSync(false);
    ofSetLogLevel(OF_LOG_NOTICE);
    ofBackground(0);
    buildScenarios();
    ofLogNotice("hud_validation_harness") << scenarios.size() << " scenarios queued.";
    startScenario(0);
}

void ofApp::startScenario(size_t idx) {
    currentIndex = idx;
    const Scenario& s = scenarios[currentIndex];
    currentWidget = s.build();
    currentWidget->setTheme(s.theme);
    currentWidget->setMotion(s.motion);
    currentWidget->setBounds(20, 20, s.width, s.height);
    currentWidget->randomize(s.seed);
    stepsRemaining = s.steps;
}

void ofApp::update() {
    if (currentIndex >= scenarios.size()) return;
    const Scenario& s = scenarios[currentIndex];
    if (stepsRemaining > 0) {
        currentWidget->update(kFixedDt);
        stepsRemaining--;
    }
}

void ofApp::captureCurrentScenario() {
    const Scenario& s = scenarios[currentIndex];

    ofBlendMode blendBefore = ofGetStyle().blendingMode;
    ofPushStyle();
    currentWidget->draw();
    ofPopStyle();
    ofBlendMode blendAfter = ofGetStyle().blendingMode;

    if (blendBefore != blendAfter) {
        ofLogError("hud_validation_harness") << s.name << ": blend mode leaked ("
            << blendBefore << " -> " << blendAfter << ") — ofPushStyle/ofPopStyle imbalance in this widget's draw().";
    }

    ofSaveScreen("captures/" + s.name + ".png");
    ofLogNotice("hud_validation_harness") << "captured " << (currentIndex + 1) << "/" << scenarios.size() << ": " << s.name;
}

void ofApp::draw() {
    if (currentIndex >= scenarios.size()) {
        static bool announced = false;
        if (!announced) {
            ofLogNotice("hud_validation_harness") << "All " << scenarios.size() << " captures complete. Exiting.";
            announced = true;
            ofExit();
        }
        return;
    }

    ofBackground(0);
    if (stepsRemaining == 0) {
        captureCurrentScenario();
        if (currentIndex + 1 < scenarios.size()) {
            startScenario(currentIndex + 1);
        } else {
            currentIndex = scenarios.size(); // signal completion next frame
        }
    }
}
