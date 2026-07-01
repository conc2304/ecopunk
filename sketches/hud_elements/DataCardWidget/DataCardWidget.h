#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"

namespace hud {

struct DataCardOptions {
    std::string title = "FUNGAL MESH";
    std::string subtitle = "SYMBIOTIC LINK ONLINE";
    std::string value = "ACTIVE";
    float meter = 0.68f;
    bool showMeter = true;
    bool showSparkline = true;
};

class DataCardWidget : public HudWidget {
public:
    void setup() override;
    void setOptions(const DataCardOptions& next) { options = next; }
    void setMeter(float m) { options.meter = ofClamp(m, 0.0f, 1.0f); }
    void setValueText(const std::string& v) { options.value = v; }
    void update(float dt) override;
    void draw() override;
private:
    DataCardOptions options;
    HudFrameRenderer frame;
    std::vector<float> spark;
};

} // namespace hud
