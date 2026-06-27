#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"

namespace hud {

struct ReticleOptions {
    int targetCount = 5;
    bool showLabels = true;
    bool randomTargets = true;
};

class ReticleWidget : public HudWidget {
public:
    void setup() override;
    void randomize(int seed = -1) override;
    void setOptions(const ReticleOptions& next) { options = next; rebuild(); }
    void update(float dt) override;
    void draw() override;
private:
    struct Target { ofVec2f p; float phase; float size; std::string label; };
    std::vector<Target> targets;
    ReticleOptions options;
    HudFrameRenderer frame;
    void rebuild();
};

} // namespace hud
