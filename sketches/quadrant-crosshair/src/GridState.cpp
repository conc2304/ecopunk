#include "GridState.h"

void GridState::setup() {
    grid.fill(0.f);
    gridTex.allocate(COLS, ROWS, GL_LUMINANCE);
}

void GridState::update(float cx, float cy, float dt, float decayRate) {
    float W   = ofGetWidth(),  H  = ofGetHeight();
    int   col = (int)ofMap(cx, 0, W, 0, COLS - 1, true);
    int   row = (int)ofMap(cy, 0, H, 0, ROWS - 1, true);

    grid[row * COLS + col] = ofClamp(grid[row * COLS + col] + dt * 0.5f, 0.f, 1.f);

    for (auto& v : grid)
        v *= (1.f - dt * (1.f - decayRate));
}

void GridState::uploadTexture() {
    std::array<uint8_t, COLS * ROWS> bytes;
    for (int i = 0; i < (int)grid.size(); i++)
        bytes[i] = (uint8_t)(grid[i] * 255.f);
    gridTex.loadData(bytes.data(), COLS, ROWS, GL_LUMINANCE);
}
