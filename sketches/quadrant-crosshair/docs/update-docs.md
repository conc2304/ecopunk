QUADRANT CROSSHAIR

openFrameworks system reference — current implementation

Target platform: Raspberry Pi 3B (Raspberry Pi OS 64-bit, VideoCore IV, GLSL ES 1.0/GLSL 120), with a macOS development path gated by `PLATFORM_PI` conditionals throughout the source. Render target 1280×720 @ 24fps, HDMI output, no UI chrome. Input media is a folder of `.mp4` files (H.264) scanned at startup. Build system: standard oF project Makefile, C++17. No addons.make — core openFrameworks only, plus the self-contained `hud_elements/` widget library checked into `src/`.

This document describes the sketch as it exists in source today. The source of truth is `src/` and `data/shaders/`; keep this file in sync with that code, not the other way around.

01 — PROJECT FILE STRUCTURE

| File | Class | Notes |
|---|---|---|
| src/ofApp.h/.cpp | ofApp | Owns every subsystem, drives the frame loop. |
| src/CrosshairSystem.h/.cpp | CrosshairSystem | Perlin-noise cursor motion, presets, color sampling, crosshair draw. |
| src/TriggerBus.h/.cpp | TriggerBus | Evaluates crosshair state each frame, fires spatial/velocity/dwell events. |
| src/Quadrant.h/.cpp | Quadrant | One screen region: erosion/residue FBO, two-slot shader crossfade, scale/morph wobble. |
| src/QuadrantManager.h/.cpp | QuadrantManager | Owns the 4 Quadrant instances, runs the per-quadrant shader-cycling state machine, routes triggers. |
| src/VideoSystem.h/.cpp | VideoSystem | Single ofVideoPlayer, playlist sequencing, brightness/contrast/saturation adjustment pass. |
| src/ShaderLibrary.h/.cpp | ShaderLibrary | Loads and owns all named ofShader instances. |
| src/LFOBank.h/.cpp | LFOBank | 12 independent sin oscillator lanes. |
| src/GridState.h/.cpp | GridState | 24×18 CPU float array of crosshair-proximity history, uploaded as a texture. |
| src/MotionExtraction.h/.cpp | MotionExtraction | GPU motion-difference extraction (accumulation-based and delayed-frame), drives crosshair attraction and a screen overlay. |
| src/ExpansionDirector.h/.cpp | ExpansionDirector | Periodic finite-state sequence that sends the crosshair into a corner and holds one quadrant fullscreen. |
| src/DebugMode.h/.cpp | DebugMode | Fullscreen shader preview/tuning mode with live parameter adjustment and trigger simulation. |
| src/HudManager.h/.cpp | HudManager | Telemetry/visualization overlay built from the hud_elements widget set. |
| src/NatureCopy.h | — | Classifies a video filename into a nature category and returns matching HUD copy (card titles, gauge labels, reticle labels). |
| src/hud_elements/ | hud:: namespace | Self-contained, reusable vector-based HUD widget library (Scanner, NodeNetwork, FlowField, Contour, Gauge, DataCard, HexGrid, Reticle). See its own README.md. |
| data/shaders/vert.glsl | shared vertex | Used by every effect shader. |
| data/shaders/desaturate.glsl, invert.glsl, recolor.glsl, threshold.glsl, dither.glsl, solarize.glsl, scanlines.glsl, channelshift.glsl | effect shaders | Core per-quadrant effect pool. |
| data/shaders/erosion.glsl | residue/decay | Per-quadrant accumulation-and-decay pass. |
| data/shaders/motion_extract.glsl, motion_accum.glsl, motion_effect.glsl | motion pipeline | Used by MotionExtraction and by the `motion_effect` quadrant shader. |
| data/shaders/ascii_threshold_solarpunk.glsl | effect shader | ASCII-style threshold effect, loaded as `ascii_solarpunk`. |
| data/shaders/video_adjust.glsl | post-process | Brightness/contrast/saturation pass applied to the video texture (desktop dev path only). |
| data/shaders/of_nature_shader_pack_glsl/*.glsl | effect pack | bioluminescence, chromatic_aberration, edge_glow, ink_outlines, pixel_drift, pixel_sorting, temporal_trails, water_refraction — loaded into the shader library; `temporal_trails` is excluded from the quadrant shader pool (would need its own per-quadrant FBO). |

02 — CROSSHAIR SYSTEM

2.1 Motion and presets

Two-octave layered Perlin noise drives `(cx, cy)`. Five named presets:

| Preset | freqA | freqB | ampA | ampB | margin |
|---|---|---|---|---|---|
| DRIFT | 0.04 | 0.009 | 0.60 | 0.35 | 120px |
| SCAN | 0.07 | 0.020 | 0.75 | 0.22 | 80px |
| HUNT | 0.12 | 0.035 | 0.85 | 0.15 | 60px |
| NERVOUS | 0.18 | 0.004 | 0.45 | 0.55 | 80px |
| ORBIT | 0.05 | 0.051 | 0.70 | 0.30 | 100px |

Cycle presets with number keys 1–5 or TAB (see §14).

Two things can override the noise-driven position:
- **Motion attractor** — `setMotionAttractor(x, y, energy)` biases the noise position toward the video's motion centroid (from MotionExtraction), capped so noise always dominates (bias force maps `energy` in `[0.02, 0.25]` to `[0, 0.35]`).
- **Expansion control** — while an `ExpansionDirector` sequence is active, `setExpansionControl(true, pos)` hands position control entirely to the director; `beginResume()` blends back into noise-driven motion over ~2 seconds once the sequence ends.

`CrosshairState` (`cx, cy, vx, vy, speed, chWidth`) is recomputed every frame in `update(dt, lfo)` and read by `TriggerBus`, `QuadrantManager`, and `HudManager`.

2.2 Crosshair color — sampled from video

The crosshair color is sampled from a blurred horizontal+vertical strip of the video frame centered on `(cx, cy)`, then smoothed toward that target:

```cpp
void CrosshairSystem::sampleColor(const ofPixels& px, float cx, float cy) {
    int W = px.getWidth(), H = px.getHeight();
    float r = 0, g = 0, b = 0;
    int count = 0;
    const int HALF = 40;  // 80px strip centered on cx/cy
    const int STEP = 4;   // every 4th pixel

    int sy = ofClamp((int)cy, 0, H - 1);
    for (int x = ofClamp((int)cx - HALF, 0, W-1); x < ofClamp((int)cx + HALF, 0, W-1); x += STEP) {
        auto c = px.getColor(x, sy);
        r += c.r; g += c.g; b += c.b; count++;
    }
    int sx = ofClamp((int)cx, 0, W - 1);
    for (int y = ofClamp((int)cy - HALF, 0, H-1); y < ofClamp((int)cy + HALF, 0, H-1); y += STEP) {
        auto c = px.getColor(sx, y);
        r += c.r; g += c.g; b += c.b; count++;
    }
    if (count > 0) sampledColor.set(r / count, g / count, b / count);
    crosshairColor = crosshairColor.getLerped(sampledColor, 0.08f);
}
```

Called from `ofApp::update()` only when `video.isFrameNew()` — no GPU stall, reuses the previous color otherwise.

2.3 Crosshair draw

Draw order in `CrosshairSystem::draw(uiFadeAlpha)`: arms (gradient or dashed) → halo → intersection bloom. The crosshair is drawn last in `ofApp::draw()`, on top of quadrants and HUD. `uiFadeAlpha` (driven by `ExpansionDirector`) scales every element's opacity and fully skips the draw below `0.01`.

**Gradient arms** — brightest at `(cx, cy)`, fading to zero at the canvas edges. Built as triangle-strip quads via a shared `addArmQuad()` helper (4 calls: left-horizontal, right-horizontal, top-vertical, bottom-vertical):

```cpp
void CrosshairSystem::addArmQuad(ofMesh& mesh, glm::vec2 a, glm::vec2 b, float T, ofColor ca, ofColor cb) {
    glm::vec2 perp = glm::normalize(b - a);
    glm::vec2 side = { -perp.y * T, perp.x * T };
    int base = mesh.getNumVertices();
    mesh.addColor(ca); mesh.addVertex({ a.x - side.x, a.y - side.y, 0.f });
    mesh.addColor(ca); mesh.addVertex({ a.x + side.x, a.y + side.y, 0.f });
    mesh.addColor(cb); mesh.addVertex({ b.x + side.x, b.y + side.y, 0.f });
    mesh.addColor(cb); mesh.addVertex({ b.x - side.x, b.y - side.y, 0.f });
    mesh.addTriangle(base, base+1, base+2);
    mesh.addTriangle(base, base+2, base+3);
}
```

Thickness is driven entirely through the mesh half-thickness `T = lineWidth * 0.5f` — there is no `ofSetLineWidth()` call for the arms. `lineWidth` is updated each frame as a blend of crosshair speed and the `LFO_CROSSHAIR_PULSE` lane: `lineWidth = ofLerp(4.0f, 12.0f, speedNorm*0.7f + pulse*0.3f)`.

**Dashed arms** — at `state.speed > HIGH_THRESH` (1.5 px/frame), the gradient arms are replaced by `drawDashArms()`, which builds short mesh quad segments with a gap that widens with speed (`dash = 12px`, `gap` mapped from `[4, 20]px` over `speed ∈ [HIGH_THRESH, 4×HIGH_THRESH]`). No `ofSetLineStipple()` is used anywhere in the file.

**Halo** — a faint circle of radius 200px at `haloPos`, which lags the crosshair via `haloPos += (cursorPos - haloPos) * 0.04f`. Drawn with `ofNoFill()`, alpha `20 * uiFadeAlpha`, line width 3.

**Intersection bloom** — a circle at `(cx, cy)` whose radius eases toward `bloomTarget` (`bloomRadius = ofLerp(bloomRadius, bloomTarget, 0.15f)`) and whose target resets to a resting `4.0f` at the end of every `draw()` call, so each call to `triggerBloom(targetRadius, ...)` produces one outward pulse. `setBloomFill(target, ...)` sets fill opacity directly (the fill ramps visually only insofar as `bloomFill` itself is changed gradually by the caller — `TriggerBus`/`ofApp` currently set it immediately on DWELL activate/deactivate). Fill alpha is `(int)((125 + bloomFill*165) * uiFadeAlpha)`.

Independent horizontal/vertical arm opacity (separate LFO-driven multipliers per axis, as in earlier designs) is not present in the current `draw()` — both arms currently share one opacity value derived from a fixed map, not per-axis LFO lanes (`LFO_ARM_H`/`LFO_ARM_V` exist in `LFOBank` but are not read by `CrosshairSystem` today).

03 — LFO BANK

12 independent oscillator lanes, computed once per frame in `update(dt)`.

```cpp
enum LFOIndex {
    LFO_CROSSHAIR_PULSE = 0, LFO_ARM_H, LFO_ARM_V,
    LFO_THRESH_Q0, LFO_THRESH_Q1, LFO_SHIFT_Q2,
    LFO_TINT_HUE, LFO_DITHER_SCALE, LFO_SCAN_DARK,
    LFO_RD_FEED, LFO_RD_KILL, LFO_GRID_DECAY,
    LFO_COUNT = 12
};
```

```cpp
void LFOBank::setup() {
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
    for (auto& lane : lanes) lane.value = sinf(timeAccum * lane.freq * TWO_PI + lane.phase);
}
float LFOBank::get(int index) const { return lanes[index].value; }
```

`get(index)` returns `[-1, 1]`. In active use today: `LFO_CROSSHAIR_PULSE` (crosshair thickness), `LFO_THRESH_Q0`/`LFO_THRESH_Q1` (per-quadrant threshold uniform), `LFO_SHIFT_Q2` (channel-shift amount), `LFO_TINT_HUE` (recolor tint rotation), `LFO_GRID_DECAY` (both `GridState` decay and per-quadrant erosion decay). `LFO_ARM_H`/`LFO_ARM_V`/`LFO_DITHER_SCALE`/`LFO_SCAN_DARK`/`LFO_RD_FEED`/`LFO_RD_KILL` are computed every frame but not currently read by any consumer.

04 — HIDDEN GRID STATE

A 24×18 grid of float values on the CPU tracks how often the crosshair has passed through each region; it decays over time and uploads as a small luminance texture that quadrant shaders can sample for spatial modulation.

```cpp
class GridState {
public:
    static constexpr int COLS = 24, ROWS = 18;
    void setup();
    void update(float cx, float cy, float dt, float decayRate);
    void uploadTexture();
    ofTexture& getTexture();
private:
    std::array<float, COLS*ROWS> grid;
    ofTexture gridTex;
};
```

```cpp
void GridState::update(float cx, float cy, float dt, float decayRate) {
    int col = (int)ofMap(cx, 0, ofGetWidth(),  0, COLS-1, true);
    int row = (int)ofMap(cy, 0, ofGetHeight(), 0, ROWS-1, true);
    grid[row*COLS+col] = ofClamp(grid[row*COLS+col] + dt*0.5f, 0.f, 1.f);
    for (auto& v : grid) v *= (1.f - dt * (1.f - decayRate));
}
```

`decayRate` is mapped from `LFO_GRID_DECAY` to `[0.92, 0.98]` in `QuadrantManager::update()` and to `[0.96, 0.995]` in `ofApp::update()` for the grid's own decay — two independent decay-rate ranges feeding the same LFO lane. The texture is uploaded once per frame and bound to quadrant shaders as `gridState` (texture unit 3) when the shader's uniform block includes it.

05 — TRIGGER BUS

A generic listener/event-bus pattern over `CrosshairState`. Six trigger types:

```cpp
enum class TriggerID { EDGE_PROXIMITY, VELOCITY_HIGH, VELOCITY_LOW, QUADRANT_CENTER, CORNER_NEAR, DWELL };
```

| Trigger | Condition | Key constants |
|---|---|---|
| EDGE_PROXIMITY | crosshair within EDGE_ZONE of any screen edge | EDGE_ZONE = 100px |
| VELOCITY_HIGH | speed > HIGH_THRESH | HIGH_THRESH = 4.0 px/frame |
| VELOCITY_LOW | speed < LOW_THRESH sustained for LOW_SECS | LOW_THRESH = 0.8, LOW_SECS = 3.0s, cooldown 45s between fires |
| QUADRANT_CENTER | within CENTER_ZONE of screen center on both axes | CENTER_ZONE = 80px |
| CORNER_NEAR | within CORNER_ZONE of any corner | CORNER_ZONE = 150px |
| DWELL | total movement over a DWELL_SECS window stays below DWELL_MOVE | DWELL_SECS = 6.0s, DWELL_MOVE = 25px |

Each check fires only on activation/deactivation edges, with a `quadrantHint`/`intensity` payload. `clearAll()` force-deactivates everything (called by `ofApp` around `ExpansionDirector` transitions so QuadrantManager state doesn't go stale while the director owns the crosshair). Note `CrosshairSystem::HIGH_THRESH` (1.5, used for arm-thickness mapping) and `TriggerBus::HIGH_THRESH` (4.0, used for the VELOCITY_HIGH trigger) are separate constants in separate classes.

06 — VIDEO SYSTEM

Single `ofVideoPlayer` instance, always — a hard Pi 3B constraint, never relaxed.

- Scans `.mp4` files from the media path at `setup()`, builds a shuffled playlist (`buildPlaylist()`), and avoids repeating the seam file between playlist cycles.
- Each file loops `loopMin`–`loopMax` times (random target via `pickTargetLoops()`) before `nextFile()` advances the playlist.
- `getPixels()`/`isFrameNew()` exposed for CPU-side sampling (crosshair color, brightness steering, motion extraction source).
- On desktop builds (`#ifndef PLATFORM_PI`) a brightness/contrast/saturation correction pass runs through `video_adjust.glsl` into an `ofFbo`, and `getTexture()` returns that adjusted texture; on Pi, `getTexture()` returns the raw player texture directly (no adjustment pass).
- Media path: `/home/pi/blueprint/media` on Pi, `ofToDataPath("media", true)` elsewhere.

`ofApp::sampleBrightness()` computes overall scene brightness via a strided CPU scan (every 64px on Pi, every 16px on desktop) and feeds it to `QuadrantManager::setVideoBrightness()`, which factors into dither-parameter risk scoring (see §08).

07 — SHADER LIBRARY

A name → `ofShader` registry; every shader pairs with the same `vert.glsl`.

```cpp
void ShaderLibrary::setup() {
    load("desaturate", "shaders/desaturate.glsl");
    load("invert", "shaders/invert.glsl");
    load("recolor", "shaders/recolor.glsl");
    load("threshold", "shaders/threshold.glsl");
    load("dither", "shaders/dither.glsl");
    load("solarize", "shaders/solarize.glsl");
    load("scanlines", "shaders/scanlines.glsl");
    load("channelshift", "shaders/channelshift.glsl");
    load("motion_effect", "shaders/motion_effect.glsl");
    load("ascii_solarpunk", "shaders/ascii_threshold_solarpunk.glsl");
    // nature pack
    load("bioluminescence", "of_nature_shader_pack_glsl/bioluminescence.glsl");
    load("chromatic_aberration", "of_nature_shader_pack_glsl/chromatic_aberration.glsl");
    load("edge_glow", "of_nature_shader_pack_glsl/edge_glow.glsl");
    load("ink_outlines", "of_nature_shader_pack_glsl/ink_outlines.glsl");
    load("pixel_drift", "of_nature_shader_pack_glsl/pixel_drift.glsl");
    load("pixel_sorting", "of_nature_shader_pack_glsl/pixel_sorting.glsl");
    load("temporal_trails", "of_nature_shader_pack_glsl/temporal_trails.glsl");
    load("water_refraction", "of_nature_shader_pack_glsl/water_refraction.glsl");
}
```

`get(name)` returns a reference; `has(name)` guards optional lookups. `erosion.glsl` is loaded directly by `Quadrant::setup()` via the same library instance.

08 — QUADRANT + QUADRANT MANAGER

**Dynamic regions.** The four quadrant rectangles are *not* static screen halves — every frame, `QuadrantManager::update()` redefines all four regions by splitting the canvas at the live crosshair position:

```cpp
quads[0].region = { 0,  0,  cx,     cy     };  // TL
quads[1].region = { cx, 0,  W - cx, cy     };  // TR
quads[2].region = { 0,  cy, cx,     H - cy };  // BL
quads[3].region = { cx, cy, W - cx, H - cy };  // BR
```

This is the source of the sketch's name: the crosshair *is* the quadrant divider.

**Erosion / residue layer.** Each `Quadrant` owns a ping-ponged pair of full-canvas-sized FBOs (`fbo_read`/`fbo_write`). Every frame, the previous accumulated state is decayed and blended with a low-opacity copy of the current video frame, then the FBOs are swapped (`std::swap(fbo_read, fbo_write)`):

```glsl
uniform sampler2D accumulated;  // previous frame FBO
uniform sampler2D videoFrame;   // current video texture
uniform float decayRate;        // ~0.97 nominal, LFO_GRID_DECAY-driven
uniform float videoAlpha;       // ~0.03 nominal
varying vec2 vTexCoord;
void main() {
    vec4 history = texture2D(accumulated, vTexCoord);
    vec4 current = texture2D(videoFrame,  vTexCoord);
    gl_FragColor = clamp(history * decayRate + current * videoAlpha, 0.0, 1.0);
}
```

Draw order per quadrant, inside a `glScissor` clip to the quadrant's exact (un-morphed) region: erosion FBO (ghost/accumulation base) → raw video (same scale/translate transform) → active shader slot(s) alpha-blended on top.

**Per-quadrant shader cycling.** Each `Quadrant` has two `ShaderSlot`s (IDLE/FADE_IN/ACTIVE/FADE_OUT) so one effect can crossfade into the next. `QuadrantManager` drives each quadrant through an independent `PLAYING → SILENCING → READY` cycle: on READY it pops a shader name from a per-quadrant shuffled "deck" (refilled from the shared 17-entry `shaderPool` when empty), skipping any shader currently playing in another quadrant so effects stay visually distributed. Dwell time per shader is randomized (54–72s), fade time randomized (15–30s), and silence gaps are normally 9–15s with a 20% chance of a long 60–120s gap (suppressed if 2+ other quadrants are already silent, so the composition never goes fully dark).

**Scale personality.** Each quadrant has its own baseline zoom range (`Q0: 0.80–1.00`, `Q1: 1.20–1.60`, `Q2: 0.50–0.70`, `Q3: 1.80–2.40`), which slowly drifts toward a freshly-randomized target range every 90–180s.

**Morph wobble.** Per-quadrant Perlin noise also offsets draw position/size/crop by a few pixels (`morphOffX/Y`, `morphW/H`, `cropOffX/Y`) for a subtle living-frame effect; the scissor clip itself stays locked to the un-morphed region so quadrant boundaries stay flush with the crosshair lines.

**Trigger → effect mapping** (`applyTriggerToQuad`): VELOCITY_HIGH forces `threshold` with an indefinite dwell (held until the trigger deactivates); EDGE_PROXIMITY → `invert` (intensity-scaled channel shift) for 3s; VELOCITY_LOW → `dither` for 5s; QUADRANT_CENTER → brief `invert` (0.5s); CORNER_NEAR → `recolor` tinted orange for 4s; DWELL → `solarize` for 8s. Before overriding, the manager snapshots whatever shader was actively playing so it can resume after the trigger effect clears. `QuadrantManager::onTrigger()` decides which quadrants a trigger affects — usually the hinted quadrant plus each other quadrant with 33% independent probability, staggered with a small random delay per quadrant (`scheduleEffect`/`pending`).

**Expansion contraction crossfade.** When `ExpansionDirector` returns from HOLD, `QuadrantManager::beginContraction(quadrantID)` starts a 3-second crossfade where a fullscreen copy of the expanded quadrant's content fades out while its normal scissored quarter-view fades back in underneath.

**Shader uniforms.** `Quadrant::drawWithEffect()` binds `tex`/`tex0` (video), `resolution`, `alpha`/`opacity`, optionally `gridState` (unit 3), `motionTex`/`motionDelayedTex` (units 4/5, `motionGamma`, `blendMode`, `motionSourceMode`), plus generic `tint`/`threshold`/`shift` via `bindUniforms()`. Nature-pack and `ascii_solarpunk` shaders additionally receive their own hardcoded parameter sets (e.g. `bioluminescence` gets `time`/`threshold`/`intensity`/`glowColor`).

09 — MOTION EXTRACTION

GPU motion-difference pipeline running off a 160×90 accumulation buffer, independent of the quadrant erosion system. Two reference modes feed `motion_extract.glsl`:

- **Accumulation-based** (`fboMotion`) — compares the current frame against a slow exponential-decay accumulation (`motion_accum.glsl`, ping-ponged), used for the fullscreen motion overlay.
- **Delayed-frame** (`fboMotionDelayed`) — compares the current frame against a ring buffer of recent low-res frames (`MAX_HISTORY_FRAMES`: 15 on Pi, 60 on desktop), used as quadrant shader variety via `motion_effect.glsl`.

CPU-side control values (`motionEnergy`, `motionCentroidX/Y`) are sampled every frame (every other frame on Pi) from a `readToPixels()` snapshot diff of the accumulation buffer, throttled and re-snapshotted every 8 samples to survive 8-bit rounding. `ofApp::update()` uses `motionEnergy` to: bias the crosshair attractor, switch `MotionExtraction`'s output mode (LUMA_GLOW / CHROMA_PRESERVE / SIGNED_FIELD) based on energy thresholds, and ramp the fullscreen motion-overlay alpha on motion onset.

10 — EXPANSION DIRECTOR

A periodic finite-state sequence (`IDLE → TRAVEL_OUT → HOLD → TRAVEL_BACK → IDLE`) that sends the crosshair to a corner and holds one quadrant essentially fullscreen for a while, then returns. Auto-fires every 120–300s in IDLE, or can be fired manually (`E` = random quadrant, `F1`–`F4` = specific quadrant). Travel duration paces itself off the crosshair's last known speed, clamped to 6–20s; hold duration is randomized 60–90s. UI elements (crosshair, HUD) fade out over the last 10% of travel-out and fade back in over the first 40% of travel-back, via `getUIFadeAlpha()`. `ofApp` wires state transitions to `TriggerBus::clearAll()` (on entering/leaving a sequence) and `QuadrantManager::beginContraction()` (on HOLD → TRAVEL_BACK).

11 — DEBUG MODE

Toggled with `D`. Fullscreen single-shader preview with a live parameter panel: cycle through every shader in the library, adjust shader-specific float parameters (coarse/fine step), and simulate any of the six `TriggerBus` events on demand without needing the crosshair to actually produce them. Owns its own per-shader parameter storage so values persist across shader switches, plus a private ping-pong FBO pair for previewing `temporal_trails` in isolation (the only place that shader currently runs, since it's excluded from the live quadrant pool).

12 — HUD + NATURE COPY

`HudManager` runs a rotating two-slot system (same FADE_IN/ACTIVE/FADE_OUT pattern as quadrant shader slots) that cycles a subset of `hud_elements` widgets (Contours, HexGrid, NodeNetwork, Reticles, FlowField) on screen, while a `ScannerWidget`, two `GaugeWidget`s (motion energy, dwell), and four `DataCardWidget`s (one per quadrant) stay always-visible. `NatureCopy.h` classifies the current video's filename (keyword match against six categories — HYDRO, TERRA, FLORAL, IGNIS, FAUNA, AETHER, with a NATURE fallback) and supplies matching card titles/gauge labels/reticle labels, swapped in via `HudManager::onVideoFileChanged()` whenever `VideoSystem` advances to a new file. The whole HUD fades with `ExpansionDirector`'s UI alpha and can be toggled with `H`.

13 — ofApp WIRING

13.1 Members

```cpp
ShaderLibrary    shaders;
VideoSystem      video;
LFOBank          lfo;
GridState        grid;
CrosshairSystem  crosshair;
TriggerBus       triggerBus;
QuadrantManager  quadrants;
MotionExtraction motionEx;
ExpansionDirector expansionDirector;
DebugMode        debug;
HudManager       hud;
```

13.2 setup()

Loads shaders, opens the video player, sets up every subsystem, wires three `TriggerBus` listeners (→ QuadrantManager, → CrosshairSystem bloom, and implicitly → DebugMode's own trigger simulation), and hands HudManager references to the systems it visualizes.

13.3 update() — order matters

1. `video.update()`; on file change, reset every quadrant's erosion FBOs and notify the HUD.
2. `lfo.update(dt)`.
3. If a new video frame arrived: sample crosshair color, sample scene brightness, run `MotionExtraction::update()`, feed the motion centroid to the crosshair attractor, and switch motion output mode / ramp overlay alpha based on motion energy.
4. `crosshair.update(dt, lfo)`.
5. `expansionDirector.update(...)`, then react to its state transitions (hand crosshair control to/from the director, clear triggers, begin quadrant contraction).
6. `triggerBus.update(...)` — suppressed while an expansion sequence is active.
7. `grid.update(...)` + `grid.uploadTexture()`.
8. `quadrants.update(dt, state, lfo, gridTex, motionTex, motionDelayedTex)`.
9. `hud.update(dt)`.

13.4 draw()

`debug.draw()` short-circuits everything else when debug mode is active; `hudOverlay`'s standalone system (toggled by `o`/`O`) short-circuits it a second, separate way — see the key-map note below. Otherwise: background clear → motion overlay (always drawn behind the quadrants — the old behind/on-top toggle was removed, see key-map note) → quadrants (erosion + effects + contraction crossfade) → HUD (if shown) → crosshair (always last, on top of everything).

14 — KEY MAP

| Key | Action |
|---|---|
| 1–5 | Set crosshair preset (1=DRIFT, 2=SCAN, 3=HUNT, 4=NERVOUS, 5=ORBIT) |
| TAB | Cycle to next crosshair preset |
| N | Load next video file |
| F | Toggle fullscreen |
| [ / ] | Decrease / increase motion overlay alpha |
| O | Toggle the standalone `shared/src/hud_overlay` system: fully replaces the scene with its ambient/organism HUD over a solid near-black background |
| M | Cycle motion extraction output mode |
| H | Toggle `HudManager`'s always-on telemetry HUD (distinct from `hud_overlay` above — the two are separate systems that happen to share the word "HUD") |
| D | Toggle debug mode |
| E | Trigger an expansion sequence to a random quadrant |
| F1–F4 | Trigger an expansion sequence to a specific quadrant |
| ESC | Quit |

> **Note (2026-08 correction):** `O` previously toggled a `motionOverlayBehind` bool (behind-quadrants ↔ on-top-of-quadrants for the motion overlay). That feature was removed — not just left undocumented — in the same commit that added `hudoverlay::HudOverlayLayer`/`HudOverlayDialPanel`/`HudOverlayDialState` and repurposed the `O` key for it (members `hudOverlay`, `hudOverlayPanel`, `hudOverlayDials`, `bool hudOverlayActive` are not listed in §13.1's member list above, which also predates that change). The motion overlay now draws behind the quadrants unconditionally, with no depth toggle. This doc previously described the removed behavior; corrected here to match current `ofApp.cpp`.

Debug mode has its own key layer (shader navigation, parameter adjustment, trigger simulation) — see `docs/keybindings.md`.
