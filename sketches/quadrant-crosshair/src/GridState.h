#pragma once
#include "ofMain.h"
#include <array>

class GridState {
public:
    static constexpr int COLS = 24;
    static constexpr int ROWS = 18;

    void setup();
    void update(float cx, float cy, float dt, float decayRate);
    void uploadTexture();
    ofTexture& getTexture() { return gridTex; }

    float get(int col, int row) const { return grid[row * COLS + col]; }

private:
    std::array<float, COLS * ROWS> grid;
    ofTexture gridTex;
};
