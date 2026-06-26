QUADRANT CROSSHAIR

openFrameworks Implementation Handoff — v2.0

Raspberry Pi 3B / oF 0.12.x / 1280×720 @ 24fps

FieldValueDocument typeImplementation Handoff — build directly from this documentTargetopenFrameworks code generation agentPlatformRaspberry Pi 3B, Raspberry Pi OS 64-bitOF versionopenFrameworks 0.12.xRender target1280×720 @ 24fps, HDMI output, no UIInput media/home/pi/blueprint/media/\*.mp4 (H.264, scanned at startup)LanguageC++17Shader languageGLSL ES 1.0 (VideoCore IV — no exceptions)Build systemStandard oF project MakefilePrevious versionv1.0 — superseded by this document

Frame budget (41.67ms at 24fps)

All estimates from agent feasibility report. Do not exceed this budget.

SystemEstimated costVideo decode (VPU hardware)~2ms4 quadrant shader passes at 1280×720~16–24ms6 FBO switches (4 erosion + 2 RD)~5–6msGray-Scott GLSL ping-pong pass~1–2msCrosshair + CPU logic + LFOs~3msVideo brightness sampling~0.3msEstimated total28–36msHeadroom5–13ms

⚠ Quadrant fragment shaders must remain single-texture reads. No multi-tap in quadrant shaders. Multi-tap is only permitted in the Gray-Scott and erosion shaders which run at 160×90 and 640×360 respectively.

01 — PROJECT FILE STRUCTURE

Every file listed here must be created. Do not merge classes.

FileClassNotessrc/ofApp.h/.cppofAppOwns all subsystems.src/CrosshairSystem.h/.cppCrosshairSystemPerlin motion, presets, crosshair draw.src/TriggerBus.h/.cppTriggerBusEvaluates crosshair state, fires events.src/Quadrant.h/.cppQuadrantRegion, effect, blend, erosion FBO.src/QuadrantManager.h/.cppQuadrantManagerOwns 4 Quadrant instances, handles triggers.src/VideoSystem.h/.cppVideoSystemSingle ofVideoPlayer + getPixels() access.src/ShaderLibrary.h/.cppShaderLibraryLoads + owns all ofShader instances.src/ReactionDiffusion.h/.cppReactionDiffusionGray-Scott GLSL ping-pong, 160×90.src/LFOBank.h/.cppLFOBank12 independent sin/cos oscillator lanes.src/GridState.h/.cppGridState24×18 CPU float array, uploads as texture.data/shaders/vert.glslshared vertexUsed by all effect shaders.data/shaders/desaturate.glsleffectdata/shaders/invert.glsleffectdata/shaders/recolor.glsleffectdata/shaders/threshold.glsleffectdata/shaders/dither.glsleffectdata/shaders/solarize.glsleffectdata/shaders/scanlines.glsleffectdata/shaders/channelshift.glsleffectdata/shaders/rd_step.glslGray-Scott stepPing-pong compute pass.data/shaders/erosion.glslerosion/residuePer-quadrant decay accumulation.

02 — CROSSHAIR SYSTEM

2.1 Motion — unchanged from v1.0

Two-octave layered Perlin noise drives (cx, cy). Five named presets. See v1.0 for full CrosshairSystem.h, setup(), update(), and preset switching code — no changes to motion architecture.

Preset table for reference:

PresetfreqAfreqBampAampBmarginDRIFT0.040.0090.600.35120pxSCAN0.070.0200.750.2280pxHUNT0.120.0350.850.1560pxNERVOUS0.180.0040.450.5580pxORBIT0.050.0510.700.30100px

2.2 Crosshair color — sampled from video

The crosshair color is not fixed. It is sampled from a blurred cross-section of the video frame centered on (cx, cy). Computed in update() using ofVideoPlayer::getPixels() — no GPU stall on Pi 3B (agent confirmed: GStreamer backend keeps a CPU-side pixel buffer).

cpp// In CrosshairSystem::update(), after receiving video pixels:
void CrosshairSystem::sampleColor(const ofPixels& px, float cx, float cy) {
int W = px.getWidth(), H = px.getHeight();
float r = 0, g = 0, b = 0;
int count = 0;
const int HALF = 40; // sample 80px strip centered on cx/cy
const int STEP = 4; // every 4th pixel — enough for a blur average

    // Horizontal strip at cy
    int sy = ofClamp((int)cy, 0, H - 1);
    for (int x = ofClamp((int)cx - HALF, 0, W-1);
             x < ofClamp((int)cx + HALF, 0, W-1); x += STEP) {
        auto c = px.getColor(x, sy);
        r += c.r; g += c.g; b += c.b; count++;
    }
    // Vertical strip at cx
    int sx = ofClamp((int)cx, 0, W - 1);
    for (int y = ofClamp((int)cy - HALF, 0, H-1);
             y < ofClamp((int)cy + HALF, 0, H-1); y += STEP) {
        auto c = px.getColor(sx, y);
        r += c.r; g += c.g; b += c.b; count++;
    }
    if (count > 0) {
        sampledColor.set(r / count, g / count, b / count);
    }
    // Smooth toward new value — prevents flicker
    crosshairColor = crosshairColor.getLerped(sampledColor, 0.08f);

}

crosshairColor is an ofColor member updated each frame. Skip sampling if !video.isFrameNew() — reuse previous value.

2.3 Crosshair draw — full specification

All crosshair elements drawn in CrosshairSystem::draw(). Draw order: gradient arms → ghost crosshair → halo → intersection circle. Crosshair is always drawn last in ofApp::draw(), on top of all quadrant content.

Gradient arms

The crosshair color is brightest at (cx, cy) and fades to zero opacity at canvas edges. Implemented with ofMesh (two quads, one per arm) with per-vertex color alpha.

cppvoid CrosshairSystem::drawGradientArms(float cx, float cy) {
float W = ofGetWidth(), H = ofGetHeight();
ofColor col = crosshairColor;
ofColor zero = ofColor(col.r, col.g, col.b, 0);
ofColor peak = ofColor(col.r, col.g, col.b, 200);

    ofMesh mesh;
    mesh.setMode(OF_PRIMITIVE_TRIANGLES);
    const float T = lineWidth * 0.5f;  // half-thickness

    // Horizontal arm: left segment (edge → cx)
    mesh.addColor(zero); mesh.addVertex({ 0,      cy - T, 0 });
    mesh.addColor(zero); mesh.addVertex({ 0,      cy + T, 0 });
    mesh.addColor(peak); mesh.addVertex({ cx,     cy + T, 0 });
    mesh.addColor(peak); mesh.addVertex({ cx,     cy - T, 0 });
    mesh.addTriangle(0,1,2); mesh.addTriangle(0,2,3);

    // Horizontal arm: right segment (cx → edge)
    mesh.addColor(peak); mesh.addVertex({ cx,     cy - T, 0 });
    mesh.addColor(peak); mesh.addVertex({ cx,     cy + T, 0 });
    mesh.addColor(zero); mesh.addVertex({ W,      cy + T, 0 });
    mesh.addColor(zero); mesh.addVertex({ W,      cy - T, 0 });
    mesh.addTriangle(4,5,6); mesh.addTriangle(4,6,7);

    // Vertical arm: top segment (edge → cy)
    mesh.addColor(zero); mesh.addVertex({ cx - T, 0,  0 });
    mesh.addColor(zero); mesh.addVertex({ cx + T, 0,  0 });
    mesh.addColor(peak); mesh.addVertex({ cx + T, cy, 0 });
    mesh.addColor(peak); mesh.addVertex({ cx - T, cy, 0 });
    mesh.addTriangle(8,9,10); mesh.addTriangle(8,10,11);

    // Vertical arm: bottom segment (cy → edge)
    mesh.addColor(peak); mesh.addVertex({ cx - T, cy, 0 });
    mesh.addColor(peak); mesh.addVertex({ cx + T, cy, 0 });
    mesh.addColor(zero); mesh.addVertex({ cx + T, H,  0 });
    mesh.addColor(zero); mesh.addVertex({ cx - T, H,  0 });
    mesh.addTriangle(12,13,14); mesh.addTriangle(12,14,15);

    ofEnableAlphaBlending();
    mesh.draw();
    ofDisableAlphaBlending();

}

Variable line thickness

lineWidth is a float member driven by two sources added together:

cpp// In update():
float speedNorm = ofMap(state.speed, 0, HIGH_THRESH, 0, 1, true);
float pulse = lfoBank.get(LFO_CROSSHAIR_PULSE); // slow sine, 0–1
lineWidth = ofLerp(1.0f, 4.0f, speedNorm _ 0.7f + pulse _ 0.3f);

⚠ Agent to confirm: does ofSetLineWidth() on VideoCore IV accept float values, or is it clamped to integers? If clamped, use the ofMesh quad approach above exclusively and drive width through T only — do not call ofSetLineWidth() for gradient arms since they are already mesh-based.

Ghost crosshair

A ring buffer stores the last 96 (cx, cy) positions (4 seconds at 24fps). The ghost is drawn at the position from GHOST_LAG frames ago at 18% opacity using the same crosshairColor but dimmed.

cpp// In CrosshairSystem.h:
static constexpr int GHOST_LAG = 72; // 3 seconds
static constexpr int HISTORY_SIZE = 96; // 4 seconds
std::array<glm::vec2, HISTORY_SIZE> posHistory;
int historyHead = 0;

// In update():
posHistory[historyHead % HISTORY_SIZE] = { state.cx, state.cy };
historyHead++;

// Ghost position:
glm::vec2 ghostPos = posHistory[(historyHead - GHOST_LAG + HISTORY_SIZE) % HISTORY_SIZE];

// In draw():
ofColor ghostCol = ofColor(crosshairColor.r, crosshairColor.g,
crosshairColor.b, 46); // ~18% of 255
// Draw ghost arms at ghostPos using same gradient mesh, scaled to ghostCol

Halo

A faint circle at radius 200px centered on (cx, cy), following with a slight positional lag via lerp. Drawn at 8% opacity.

cpp// In update():
haloPos = haloPos + (glm::vec2(state.cx, state.cy) - haloPos) \* 0.04f;

// In draw():
ofPushStyle();
ofNoFill();
ofSetColor(crosshairColor.r, crosshairColor.g, crosshairColor.b, 20);
ofSetLineWidth(1.0f);
ofDrawCircle(haloPos.x, haloPos.y, 200.f);
ofPopStyle();

Intersection bloom

The registration circle at (cx, cy) has two states driven by TriggerBus:

VELOCITY_HIGH active: radius pulses outward to 24px then contracts back to 4px over 0.4s
DWELL active: circle fills solid (opacity ramps from 0 to 180 over 2s), then dissolves when dwell resolves

cpp// In CrosshairSystem.h:
float bloomRadius = 4.f;
float bloomFill = 0.f; // 0 = outline only, 1 = solid fill
float bloomTarget = 4.f;

// In update():
bloomRadius = ofLerp(bloomRadius, bloomTarget, 0.15f);

// In draw():
ofPushStyle();
ofSetColor(crosshairColor.r, crosshairColor.g, crosshairColor.b,
(int)(90 + bloomFill \* 165));
if (bloomFill > 0.05f) ofFill(); else ofNoFill();
ofDrawCircle(state.cx, state.cy, bloomRadius);
ofPopStyle();

Signal-break dashes at high velocity

At VELOCITY_HIGH, the gradient arms are replaced with dashed segments. Gap length scales with speed.

⚠ Agent to confirm: is ofSetLineStipple() available in oF 0.12.x on Pi 3B VideoCore IV? If not, implement as an ofMesh of short quad segments with computed gaps. Gap count and length driven by state.speed. If ofSetLineStipple() is available, prefer it over mesh for simplicity.

Independent arm opacity

Horizontal and vertical arms have separate opacity multipliers driven by independent LFO lanes, never reaching zero.

cppfloat opacH = ofMap(lfoBank.get(LFO_ARM_H), -1, 1, 0.35f, 1.0f);
float opacV = ofMap(lfoBank.get(LFO_ARM_V), -1, 1, 0.35f, 1.0f);
// Apply as alpha scale to peak vertex color when building gradient mesh

03 — LFO BANK

12 independent oscillator lanes. All computed in update(). Negligible cost: ~0.6ms (agent confirmed).

3.1 LFOBank.h

cpp#pragma once
#include "ofMain.h"
#include <array>

enum LFOIndex {
LFO_CROSSHAIR_PULSE = 0, // crosshair line thickness pulse
LFO_ARM_H, // horizontal arm opacity
LFO_ARM_V, // vertical arm opacity
LFO_THRESH_Q0, // threshold uniform, quadrant TL
LFO_THRESH_Q1, // threshold uniform, quadrant TR
LFO_SHIFT_Q2, // channel shift amount, quadrant BL
LFO_TINT_HUE, // recolor tint hue rotation
LFO_DITHER_SCALE, // dither density modulation
LFO_SCAN_DARK, // scanline darkness modulation
LFO_RD_FEED, // Gray-Scott feed rate drift
LFO_RD_KILL, // Gray-Scott kill rate drift
LFO_GRID_DECAY, // hidden grid decay rate
LFO_COUNT = 12
};

class LFOBank {
public:
void setup();
void update(float dt);
float get(int index) const; // returns -1 to 1

private:
struct Lane {
float freq; // Hz
float phase; // radians
float value; // current output
};
std::array<Lane, LFO_COUNT> lanes;
float timeAccum = 0.f;
};

3.2 LFOBank.cpp

cppvoid LFOBank::setup() {
// Frequencies chosen to avoid harmonic relationships — prevents sync
lanes = {{
        { 0.031f, 0.00f, 0 },  // LFO_CROSSHAIR_PULSE  ~32s cycle
        { 0.047f, 1.10f, 0 },  // LFO_ARM_H            ~21s
        { 0.053f, 2.30f, 0 },  // LFO_ARM_V            ~19s
        { 0.019f, 0.70f, 0 },  // LFO_THRESH_Q0        ~53s
        { 0.023f, 3.50f, 0 },  // LFO_THRESH_Q1        ~43s
        { 0.061f, 1.80f, 0 },  // LFO_SHIFT_Q2         ~16s
        { 0.013f, 0.40f, 0 },  // LFO_TINT_HUE         ~77s
        { 0.071f, 2.90f, 0 },  // LFO_DITHER_SCALE     ~14s
        { 0.041f, 4.20f, 0 },  // LFO_SCAN_DARK        ~24s
        { 0.007f, 1.60f, 0 },  // LFO_RD_FEED          ~143s
        { 0.009f, 0.90f, 0 },  // LFO_RD_KILL          ~111s
        { 0.017f, 3.10f, 0 },  // LFO_GRID_DECAY       ~59s
    }};
}

void LFOBank::update(float dt) {
timeAccum += dt;
for (auto& lane : lanes) {
lane.value = sinf(timeAccum _ lane.freq _ TWO_PI + lane.phase);
}
}

float LFOBank::get(int index) const { return lanes[index].value; }

04 — REACTION-DIFFUSION SYSTEM

Gray-Scott model running at 160×90 via GLSL ES 1.0 FBO ping-pong. Agent confirmed cost: ~1–2ms/frame. Result used as a control surface — it drives effect parameters, it is not rendered directly.

4.1 ReactionDiffusion.h

cpp#pragma once
#include "ofMain.h"

class ReactionDiffusion {
public:
void setup();
void update(float feedRate, float killRate); // rates driven by LFOs
ofTexture& getTexture(); // returns current RD state as texture

    static constexpr int RD_W = 160;
    static constexpr int RD_H = 90;

private:
ofFbo fboA, fboB;
bool pingPong = false;
ofShader rdShader;
ofMesh fullscreenQuad;

    void buildQuad();

};

4.2 ReactionDiffusion.cpp

cppvoid ReactionDiffusion::setup() {
ofFbo::Settings s;
s.width = RD_W;
s.height = RD_H;
s.internalformat = GL_RGBA;
s.useDepth = false;
fboA.allocate(s);
fboB.allocate(s);

    // Seed fboA with initial state: U=1 everywhere, V=0 except center patch
    fboA.begin();
    ofClear(255, 0, 0, 255);  // R=U=1, G=V=0
    ofSetColor(0, 255, 0);
    ofDrawRectangle(RD_W/2 - 8, RD_H/2 - 8, 16, 16);  // V seed patch
    fboA.end();

    rdShader.load("shaders/vert.glsl", "shaders/rd_step.glsl");
    buildQuad();

}

void ReactionDiffusion::update(float feedRate, float killRate) {
ofFbo& src = pingPong ? fboB : fboA;
ofFbo& dst = pingPong ? fboA : fboB;

    dst.begin();
    rdShader.begin();
    rdShader.setUniformTexture("rdState", src.getTexture(), 0);
    rdShader.setUniform2f("resolution", RD_W, RD_H);
    rdShader.setUniform1f("feedRate", feedRate);
    rdShader.setUniform1f("killRate", killRate);
    fullscreenQuad.draw();
    rdShader.end();
    dst.end();

    pingPong = !pingPong;

}

ofTexture& ReactionDiffusion::getTexture() {
return pingPong ? fboA.getTexture() : fboB.getTexture();
}

4.3 data/shaders/rd_step.glsl

glsluniform sampler2D rdState;
uniform vec2 resolution;
uniform float feedRate; // nominal 0.055, drifts ±0.008 via LFO
uniform float killRate; // nominal 0.062, drifts ±0.006 via LFO

varying vec2 vTexCoord;

void main() {
vec2 texel = 1.0 / resolution;

    vec4 center = texture2D(rdState, vTexCoord);
    float U = center.r;
    float V = center.g;

    // 5-tap Laplacian
    float lapU =
        texture2D(rdState, vTexCoord + vec2( texel.x, 0)).r +
        texture2D(rdState, vTexCoord + vec2(-texel.x, 0)).r +
        texture2D(rdState, vTexCoord + vec2(0,  texel.y)).r +
        texture2D(rdState, vTexCoord + vec2(0, -texel.y)).r -
        4.0 * U;
    float lapV =
        texture2D(rdState, vTexCoord + vec2( texel.x, 0)).g +
        texture2D(rdState, vTexCoord + vec2(-texel.x, 0)).g +
        texture2D(rdState, vTexCoord + vec2(0,  texel.y)).g +
        texture2D(rdState, vTexCoord + vec2(0, -texel.y)).g -
        4.0 * V;

    float Du = 1.0;
    float Dv = 0.5;
    float reaction = U * V * V;

    float newU = U + (Du * lapU - reaction + feedRate * (1.0 - U)) * 0.5;
    float newV = V + (Dv * lapV + reaction - (killRate + feedRate) * V) * 0.5;

    newU = clamp(newU, 0.0, 1.0);
    newV = clamp(newV, 0.0, 1.0);

    gl_FragColor = vec4(newU, newV, 0.0, 1.0);

}

4.4 How RD drives the scene

The RD texture is sampled in update() — not in shaders — to extract scalar control values. Sample the RD CPU-side once per frame by reading getTexture() via a tiny ofFbo blit to a 1×1 FBO (average), or sample a fixed set of 4–6 known pixel positions as proxy values.

RD sample pointDrivesCenter pixel V channelOverall threshold uniform for quadrant TLTop-left V channelDither density scaleTop-right V channelRecolor tint hue offsetBottom-left V channelChannel shift amountBottom-right V channelScanline darknessCenter U channelGrid decay rate

⚠ Agent to determine cheapest RD readback method at 160×90. Options: (a) 1×1 blit FBO average, (b) sparse glReadPixels on the 160×90 FBO, (c) CPU-side pass on the raw FBO pixel data. Pick whichever avoids a full pipeline stall on VideoCore IV.

05 — QUADRANT EROSION / RESIDUE LAYER

Each quadrant accumulates a ghost of its recent video content in a persistent FBO. Older content decays slowly. The video is never fully replaced — it layers. Agent confirmed FBO content persists across frames on Pi 3B with no CPU copy needed.

5.1 Architecture

Each Quadrant owns two ofFbo instances at half resolution (640×360):

fbo_read — previous frame's accumulated state (input)
fbo_write — current frame's output (draw into, then swap)

Each frame:

Bind fbo_write
Draw fbo_read content through the erosion shader (applies decay)
Draw current video frame on top at low opacity (~0.15)
Swap fbo_read / fbo_write
Render fbo_read to screen as the quadrant's base layer, then apply effect shader on top

5.2 data/shaders/erosion.glsl

glsluniform sampler2D accumulated; // previous frame FBO
uniform sampler2D videoFrame; // current video texture
uniform float decayRate; // 0.92–0.98, driven by LFO_GRID_DECAY
uniform float videoAlpha; // 0.10–0.20, how strongly new frame writes in

varying vec2 vTexCoord;

void main() {
vec4 history = texture2D(accumulated, vTexCoord);
vec4 current = texture2D(videoFrame, vTexCoord);

    // Decay history, add new frame
    vec4 result = history * decayRate + current * videoAlpha;
    gl_FragColor = clamp(result, 0.0, 1.0);

}

decayRate nominal value: 0.95. LFO modulation range: 0.92–0.98. At 0.95 a given frame's contribution halves in ~14 frames (~0.6 seconds at 24fps). At 0.98 it halves in ~35 frames (~1.4 seconds). This gives the composition geological layering — the image builds history.

videoAlpha nominal: 0.15. Range: 0.10–0.20. Driven by LFO on a long cycle.

06 — HIDDEN GRID STATE

A 24×18 grid (432 cells) of float values lives on the CPU. Each cell tracks accumulated local crosshair proximity — how often the crosshair has passed through that region. Values feed back into rendering as a subtle spatial modulation. Agent confirmed this is the right approach: CPU array + small texture upload, ~0.2ms total.

6.1 GridState.h

cpp#pragma once
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
std::array<float, COLS \* ROWS> grid;
ofTexture gridTex;
};

6.2 GridState.cpp

cppvoid GridState::setup() {
grid.fill(0.f);
gridTex.allocate(COLS, ROWS, GL_LUMINANCE);
}

void GridState::update(float cx, float cy, float dt, float decayRate) {
float W = ofGetWidth(), H = ofGetHeight();
int col = (int)ofMap(cx, 0, W, 0, COLS - 1, true);
int row = (int)ofMap(cy, 0, H, 0, ROWS - 1, true);

    // Increment cell under crosshair
    grid[row * COLS + col] = ofClamp(grid[row * COLS + col] + dt * 0.5f, 0, 1);

    // Decay all cells
    for (auto& v : grid) v *= (1.f - dt * (1.f - decayRate));

}

void GridState::uploadTexture() {
// Convert float [0,1] to uint8 for GL_LUMINANCE upload
std::array<uint8_t, COLS _ ROWS> bytes;
for (int i = 0; i < (int)grid.size(); i++)
bytes[i] = (uint8_t)(grid[i] _ 255.f);
gridTex.loadData(bytes.data(), COLS, ROWS, GL_LUMINANCE);
}

GridState::update() is called each frame with decayRate driven by LFO_GRID_DECAY (mapped to 0.96–0.995). uploadTexture() called once per frame in update(). The grid texture is passed as a uniform to quadrant shaders that want spatial modulation — currently threshold.glsl and recolor.glsl sample it to modulate their effect intensity per screen region.

07 — VIDEO SYSTEM

No structural changes from v1.0. One addition: getPixels() access for crosshair color sampling and brightness-based steering.

⚠ ONE ofVideoPlayer instance at all times. Hard Pi 3B constraint. Never instantiate a second player.

cpp// Added to VideoSystem.h:
const ofPixels& getPixels() const { return player.getPixels(); }
bool isFrameNew() const { return player.isFrameNew(); }

Caller pattern:

cpp// In ofApp::update(), after video.update():
if (video.isFrameNew()) {
crosshair.sampleColor(video.getPixels(), state.cx, state.cy);
steeringBrightness = sampleBrightness(video.getPixels());
}

Brightness sampling for crosshair steering (agent confirmed ~0.3ms, no GPU stall):

cppfloat sampleBrightness(const ofPixels& px) {
int W = px.getWidth(), H = px.getHeight();
float brightness = 0;
int count = 0;
const int STEP = 16;
for (int y = 0; y < H; y += STEP)
for (int x = 0; x < W; x += STEP) {
auto c = px.getColor(x, y);
brightness += 0.299f*c.r + 0.587f*c.g + 0.114f*c.b;
count++;
}
return brightness / (count * 255.f); // 0–1
}

08 — SHADER LIBRARY

Add two new shaders to ShaderLibrary::setup() alongside the existing eight:

cppload("rd_step", "shaders/rd_step.glsl");
load("erosion", "shaders/erosion.glsl");

All other shader source unchanged from v1.0. Full GLSL source for all eight effect shaders is in v1.0 — do not re-implement, copy verbatim.

09 — TRIGGER BUS

No changes to trigger detection logic from v1.0. Three additions:

Crosshair bloom reactions — CrosshairSystem registers directly as a TriggerBus listener:

cpptriggerBus.addListener([this](const TriggerEvent& e) {
if (e.id == TriggerID::VELOCITY_HIGH && e.active) {
crosshair.triggerBloom(24.f, 0.4f); // radius, duration
}
if (e.id == TriggerID::DWELL) {
crosshair.setBloomFill(e.active ? 1.f : 0.f, 2.0f);
}
});

Signal-break dash state — CrosshairSystem reads velHighActive directly from CrosshairState::speed — no new trigger needed. Dash rendering activates when state.speed > HIGH_THRESH.

RD feed/kill rate perturbation on CORNER_NEAR — when CORNER_NEAR fires, briefly push feedRate up by 0.012 for 3 seconds, creating a visible RD reaction surge that slowly settles. Implemented in ofApp::onTrigger().

10 — ofApp — FULL WIRING

10.1 ofApp.h

cpp#pragma once
#include "ofMain.h"
#include "CrosshairSystem.h"
#include "TriggerBus.h"
#include "QuadrantManager.h"
#include "VideoSystem.h"
#include "ShaderLibrary.h"
#include "ReactionDiffusion.h"
#include "LFOBank.h"
#include "GridState.h"

class ofApp : public ofBaseApp {
public:
void setup();
void update();
void draw();
void keyPressed(int key);

private:
ShaderLibrary shaders;
VideoSystem video;
LFOBank lfo;
ReactionDiffusion rd;
GridState grid;
CrosshairSystem crosshair;
TriggerBus triggerBus;
QuadrantManager quadrants;

    float steeringBrightness = 0.5f;
    float rdFeedBump = 0.f;   // additional feed rate from CORNER_NEAR trigger

};

10.2 ofApp.cpp — setup()

cppvoid ofApp::setup() {
ofSetFrameRate(24);
ofSetVerticalSync(true);
ofBackground(13, 13, 13);
ofHideCursor();

    shaders.setup();
    video.setup("/home/pi/blueprint/media");
    lfo.setup();
    rd.setup();
    grid.setup();
    crosshair.setup();
    triggerBus.setup();
    quadrants.setup(&shaders);

    // Wire TriggerBus → QuadrantManager
    triggerBus.addListener([this](const TriggerEvent& e) {
        quadrants.onTrigger(e);
    });

    // Wire TriggerBus → CrosshairSystem (bloom)
    triggerBus.addListener([this](const TriggerEvent& e) {
        if (e.id == TriggerID::VELOCITY_HIGH && e.active)
            crosshair.triggerBloom(24.f, 0.4f);
        if (e.id == TriggerID::DWELL)
            crosshair.setBloomFill(e.active ? 1.f : 0.f, 2.0f);
    });

    // Wire TriggerBus → RD feed bump
    triggerBus.addListener([this](const TriggerEvent& e) {
        if (e.id == TriggerID::CORNER_NEAR && e.active)
            rdFeedBump = 0.012f;
        if (e.id == TriggerID::CORNER_NEAR && !e.active)
            rdFeedBump = 0.f;
    });

}

10.3 ofApp.cpp — update()

cppvoid ofApp::update() {
float dt = ofGetLastFrameTime();

    // 1. Source systems
    video.update();
    lfo.update(dt);

    // 2. Sample video CPU-side (no GPU stall — GStreamer keeps CPU buffer)
    if (video.isFrameNew()) {
        crosshair.sampleColor(video.getPixels(), crosshair.getState().cx,
                                                  crosshair.getState().cy);
        steeringBrightness = sampleBrightness(video.getPixels());
    }

    // 3. Crosshair motion
    crosshair.update(dt, steeringBrightness);

    // 4. Triggers
    triggerBus.update(crosshair.getState(), dt);

    // 5. RD — feed/kill driven by LFOs + trigger bumps
    float feed = 0.055f + ofMap(lfo.get(LFO_RD_FEED), -1, 1, -0.008f, 0.008f) + rdFeedBump;
    float kill = 0.062f + ofMap(lfo.get(LFO_RD_KILL), -1, 1, -0.006f, 0.006f);
    rd.update(feed, kill);

    // 6. Grid
    float decayRate = ofMap(lfo.get(LFO_GRID_DECAY), -1, 1, 0.96f, 0.995f);
    grid.update(crosshair.getState().cx, crosshair.getState().cy, dt, decayRate);
    grid.uploadTexture();

    // 7. Quadrants — pass LFO values for per-parameter modulation
    quadrants.update(dt, crosshair.getState(), lfo, rd.getTexture(), grid.getTexture());

}

10.4 ofApp.cpp — draw()

cppvoid ofApp::draw() {
ofBackground(13, 13, 13);
// Quadrants draw their erosion FBOs + effect shaders internally
quadrants.draw(video.getTexture(), video.getVideoSize());
// Crosshair always on top
crosshair.draw();
}

10.5 Key map

KeyAction1–5Set crosshair preset (1=DRIFT, 2=SCAN, 3=HUNT, 4=NERVOUS, 5=ORBIT)TABCycle to next crosshair presetRRe-seed reaction-diffusion (reset RD FBOs to initial state)NLoad next video fileFToggle fullscreenESCQuit

11 — QUADRANT MANAGER — LFO + RD integration

QuadrantManager::update() now receives the LFOBank, RD texture, and grid texture and distributes LFO values to each quadrant's effect uniforms each frame. This replaces the static uniform values from v1.0.

cppvoid QuadrantManager::update(float dt, const CrosshairState& state,
const LFOBank& lfo,
ofTexture& rdTex, ofTexture& gridTex) {
// Distribute LFO values to quadrant effect parameters
quads[0].setThreshold(ofMap(lfo.get(LFO_THRESH_Q0), -1, 1, 0.3f, 0.7f));
quads[1].setThreshold(ofMap(lfo.get(LFO_THRESH_Q1), -1, 1, 0.3f, 0.7f));
quads[2].setShift(ofMap(lfo.get(LFO_SHIFT_Q2), -1, 1, 0.002f, 0.008f));

    // Tint hue rotation via LFO — convert hue offset to RGB tint
    float hue = ofMap(lfo.get(LFO_TINT_HUE), -1, 1, 0.f, 360.f);
    ofColor tintColor = ofColor::fromHsb(hue, 200, 255);
    quads[2].setTint({ tintColor.r/255.f, tintColor.g/255.f, tintColor.b/255.f });

    // Pass RD and grid textures to quadrants for use in shaders
    for (auto& q : quads) {
        q.setRDTexture(rdTex);
        q.setGridTexture(gridTex);
        q.update(dt, state.cx, state.cy);
    }

}

12 — PERFORMANCE RULES

All rules from v1.0 remain in force. Additions and amendments for v2.0:

RuleRationaledraw() contains GL calls onlyUnchanged.One ofVideoPlayer at all timesUnchanged.Shaders compiled at startup onlyUnchanged.glScissor for quadrant clipUnchanged.Quadrant shaders: single-texture reads onlyMulti-tap reserved for RD (160×90) and erosion (640×360) only.FBO switches: max 6 per frameEach switch costs ~0.5–1ms on VideoCore IV. Do not add FBOs without cutting elsewhere.getPixels() only when isFrameNew()GStreamer buffer is valid and no-cost, but only call when new frame is available.RD runs every frameAt 1–2ms per pass this is affordable. Do not skip frames — the simulation loses continuity.Grid upload: once per frame in update()loadData() on a 24×18 texture is ~0.1ms. No batching needed.ofSwap(fbo_read, fbo_write) not pointer swapUse oF's swap utility to avoid reallocating FBO objects.

12.1 Benchmark checklist

Run each scenario on Pi 3B. Record fps. All must sustain ≥ 24fps for 60 seconds.

Baseline: four quadrants, passthrough, no new systems → expect ≥ 30fps
Add LFOBank + GridState: should be negligible change
Add ReactionDiffusion ping-pong: expect ~2ms addition, still ≥ 24fps
Add four erosion FBOs: expect ~5–6ms addition — this is the critical test
Full system, all effects active, all LFO modulation live → must sustain 24fps
Full system + VELOCITY_HIGH trigger active (most expensive crosshair state) → must sustain 24fps
Full system running 5 minutes continuous → no memory growth, no fps degradation

12.2 Fallback strategy if FBO budget is exceeded

If four erosion FBOs + two RD FBOs cannot sustain 24fps:

First cut: reduce erosion FBOs from 640×360 to 320×180 — halves VRAM cost and switch overhead
Second cut: share one erosion FBO across all four quadrants (single full-canvas residue layer instead of per-quadrant)
Third cut: run RD every other frame (step the simulation at 12fps, render at 24fps) — RD continuity is maintained, cost halved

Do not cut the RD system entirely — it is the primary procedural complexity driver.

13 — OPEN QUESTIONS FOR AGENT (resolve during build)

#QuestionDecisionQ1Does ofSetLineWidth() accept float values on Pi 3B VideoCore IV, or is it clamped to integers?If clamped: drive all crosshair thickness through ofMesh quad T value only.Q2Is ofSetLineStipple() available in oF 0.12.x on Pi?If yes: use for signal-break dashes. If no: implement as ofMesh short quad segments.Q3Cheapest RD readback method — 1×1 blit FBO, sparse glReadPixels on 160×90 FBO, or CPU pass on raw pixel data?Pick whichever avoids full pipeline stall. Document measured cost.Q4Does ofVideoPlayer::getPixels() return a valid buffer before the first isFrameNew() fires?If not: guard all getPixels() calls behind a hasFirstFrame bool.Q5Does ofSwap(ofFbo, ofFbo) work correctly in oF 0.12.x or must we use pointer/reference swap manually?Document which pattern is used.

— QUADRANT CROSSHAIR / IMPLEMENTATION HANDOFF v2.0 / SUPERSEDES v1.0 — BUILD FROM THIS DOCUMENT —
