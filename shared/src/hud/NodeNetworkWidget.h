#pragma once
#include "HudWidget.h"
#include "HudFrameRenderer.h"

namespace hud {

struct NodeNetworkOptions {
    int nodeCount = 34;
    float connectionDistance = 0.24f; // normalized to min dimension
    bool wrap = true;
    bool showPackets = true;
};

class NodeNetworkWidget : public HudWidget {
public:
    void setup() override;
    void randomize(int seed = -1) override;
    void setOptions(const NodeNetworkOptions& next) { options = next; rebuild(); }
    void update(float dt) override;
    void draw() override;
    ofVec2f getMinSize() const override { return { 150.0f, 100.0f }; }

private:
    struct Node { ofVec2f p, v; float phase = 0.0f; float energy = 1.0f; };
    std::vector<Node> nodes;
    NodeNetworkOptions options;
    HudFrameRenderer frame;
    void rebuild();
    ofVec2f toScreen(const ofVec2f& normalized) const;
};

} // namespace hud
