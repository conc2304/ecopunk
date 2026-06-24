# QUADRANT CROSSHAIR

## openFrameworks Implementation Handoff

**Raspberry Pi 3B / oF 0.12.x / 1280×720 @ 24fps / v1.0**

| Field           | Value                                                        |
| --------------- | ------------------------------------------------------------ |
| Document type   | Implementation Handoff — build directly from this document   |
| Target          | openFrameworks code generation agent                         |
| Platform        | Raspberry Pi 3B, Raspberry Pi OS 64-bit                      |
| OF version      | openFrameworks 0.12.x                                        |
| Render target   | 1280×720 @ 24fps, HDMI output, no UI                         |
| Input media     | `/home/pi/blueprint/media/*.mp4` (H.264, scanned at startup) |
| Language        | C++17                                                        |
| Shader language | GLSL ES 1.0 (VideoCore IV — no exceptions)                   |
| Build system    | Standard oF project Makefile                                 |

---

## 01 — PROJECT FILE STRUCTURE

Create the following files. Every class listed here must be implemented. Do not merge classes.

| File                             | Class / Content      | Notes                                    |
| -------------------------------- | -------------------- | ---------------------------------------- |
| `src/ofApp.h`                    | `ofApp`              | Main app. Owns all subsystems.           |
| `src/ofApp.cpp`                  | `ofApp`              |                                          |
| `src/CrosshairSystem.h`          | `CrosshairSystem`    | Perlin motion + preset management.       |
| `src/CrosshairSystem.cpp`        | `CrosshairSystem`    |                                          |
| `src/TriggerBus.h`               | `TriggerBus`         | Evaluates crosshair state, fires events. |
| `src/TriggerBus.cpp`             | `TriggerBus`         |                                          |
| `src/Quadrant.h`                 | `Quadrant`           | Single quadrant: region, effect, blend.  |
| `src/Quadrant.cpp`               | `Quadrant`           |                                          |
| `src/QuadrantManager.h`          | `QuadrantManager`    | Owns 4 Quadrant instances.               |
| `src/QuadrantManager.cpp`        | `QuadrantManager`    |                                          |
| `src/VideoSystem.h`              | `VideoSystem`        | Single ofVideoPlayer wrapper.            |
| `src/VideoSystem.cpp`            | `VideoSystem`        |                                          |
| `src/ShaderLibrary.h`            | `ShaderLibrary`      | Loads + owns all ofShader instances.     |
| `src/ShaderLibrary.cpp`          | `ShaderLibrary`      |                                          |
| `data/shaders/vert.glsl`         | shared vertex shader | One vertex shader used by all effects.   |
| `data/shaders/desaturate.glsl`   | effect               |                                          |
| `data/shaders/invert.glsl`       | effect               |                                          |
| `data/shaders/recolor.glsl`      | effect               |                                          |
| `data/shaders/threshold.glsl`    | effect               |                                          |
| `data/shaders/dither.glsl`       | effect               |                                          |
| `data/shaders/solarize.glsl`     | effect               |                                          |
| `data/shaders/scanlines.glsl`    | effect               |                                          |
| `data/shaders/channelshift.glsl` | effect               |                                          |

---

## 02 — CROSSHAIR SYSTEM

### 2.1 CrosshairSystem.h

```cpp
#pragma once
#include "ofMain.h"

struct CrosshairPreset {
    float freqA, freqB;    // noise frequencies for octave A and B
    float ampA, ampB;      // amplitude weights, should sum to ~1.0
    float margin;          // px — keeps crosshair off literal canvas edge
    std::string name;
};

struct CrosshairState {
    float cx, cy;          // current center position (pixels)
    float vx, vy;          // velocity this frame (pixels/frame)
    float speed;           // magnitude of velocity
};

class CrosshairSystem {
public:
    void setup();
    void update(float dt);
    void draw();           // draws crosshair lines + tick marks
    void setPreset(int index);
    void nextPreset();

    CrosshairState getState() const { return state; }
    int getCurrentPreset()    const { return presetIndex; }

private:
    std::vector<CrosshairPreset> presets;
    int presetIndex = 0;

    float noiseT  = 0.0f;
    float seedXA  = 0.0f,   seedXB = 100.0f;
    float seedYA  = 200.0f, seedYB = 300.0f;

    CrosshairState state;
    CrosshairState prevState;
};
```

### 2.2 CrosshairSystem.cpp — setup()

Populate the presets vector with exactly these five entries. Values are final — do not alter without design approval.

```cpp
void CrosshairSystem::setup() {
    presets = {
        { 0.04f,  0.009f, 0.60f, 0.35f, 120.f, "DRIFT"   },
        { 0.07f,  0.020f, 0.75f, 0.22f,  80.f, "SCAN"    },
        { 0.12f,  0.035f, 0.85f, 0.15f,  60.f, "HUNT"    },
        { 0.18f,  0.004f, 0.45f, 0.55f,  80.f, "NERVOUS" },
        { 0.05f,  0.051f, 0.70f, 0.30f, 100.f, "ORBIT"   },
    };
    presetIndex = 0;
    noiseT = ofRandom(0, 1000);  // random start position in noise space
}
```

### 2.3 CrosshairSystem.cpp — update()

Two independent noise fields drive X and Y. Each is the sum of two octaves. Velocity is derived from the frame delta.

```cpp
void CrosshairSystem::update(float dt) {
    const CrosshairPreset& p = presets[presetIndex];
    noiseT += dt;

    float nx = ofNoise(noiseT * p.freqA + seedXA) * p.ampA
             + ofNoise(noiseT * p.freqB + seedXB) * p.ampB;
    float ny = ofNoise(noiseT * p.freqA + seedYA) * p.ampA
             + ofNoise(noiseT * p.freqB + seedYB) * p.ampB;

    prevState = state;
    float W = ofGetWidth(), H = ofGetHeight();
    state.cx    = ofMap(nx, 0, 1, p.margin, W - p.margin);
    state.cy    = ofMap(ny, 0, 1, p.margin, H - p.margin);
    state.vx    = state.cx - prevState.cx;
    state.vy    = state.cy - prevState.cy;
    state.speed = sqrtf(state.vx * state.vx + state.vy * state.vy);
}
```

### 2.4 CrosshairSystem.cpp — draw()

```cpp
void CrosshairSystem::draw() {
    float W = ofGetWidth(), H = ofGetHeight();
    float cx = state.cx, cy = state.cy;
    const float TICK = 8.f;

    ofPushStyle();
    ofSetLineWidth(1.0f);
    ofSetColor(255, 255, 255, 90);  // white at ~35% opacity

    ofDrawLine(0, cy, W, cy);       // horizontal rule
    ofDrawLine(cx, 0, cx, H);       // vertical rule

    // Tick marks at canvas edges
    ofDrawLine(0,        cy, TICK,        cy);  // left
    ofDrawLine(W - TICK, cy, W,           cy);  // right
    ofDrawLine(cx,       0,  cx,        TICK);  // top
    ofDrawLine(cx, H - TICK, cx,           H);  // bottom

    // Registration mark at intersection
    ofNoFill();
    ofDrawCircle(cx, cy, 4.f);

    ofPopStyle();
}
```

### 2.5 Preset switching

```cpp
void CrosshairSystem::setPreset(int index) {
    presetIndex = ofClamp(index, 0, (int)presets.size() - 1);
    // NO lerp — preset changes are instant by design
}

void CrosshairSystem::nextPreset() {
    setPreset((presetIndex + 1) % presets.size());
}
```

---

## 03 — TRIGGER BUS

The TriggerBus evaluates `CrosshairState` every frame and fires named events. Consumers register callbacks. All evaluation is O(1) — no loops, no spatial lookups.

### 3.1 TriggerBus.h

```cpp
#pragma once
#include "ofMain.h"
#include "CrosshairSystem.h"
#include <functional>
#include <map>

enum class TriggerID {
    EDGE_PROXIMITY,
    VELOCITY_HIGH,
    VELOCITY_LOW,
    QUADRANT_CENTER,
    CORNER_NEAR,
    DWELL,
};

struct TriggerEvent {
    TriggerID id;
    bool      active;        // true = fired, false = resolved
    float     intensity;     // 0.0–1.0, trigger-specific meaning
    int       quadrantHint;  // -1 = all quadrants, 0–3 = specific quadrant
};

using TriggerCallback = std::function<void(const TriggerEvent&)>;

class TriggerBus {
public:
    void setup();
    void update(const CrosshairState& state, float dt);
    void addListener(TriggerCallback cb);

private:
    void fire(TriggerEvent e);
    void checkEdgeProximity(const CrosshairState& s);
    void checkVelocity(const CrosshairState& s);
    void checkQuadrantCenter(const CrosshairState& s);
    void checkCornerNear(const CrosshairState& s);
    void checkDwell(const CrosshairState& s, float dt);

    std::vector<TriggerCallback> listeners;

    // Per-trigger state
    bool edgeActive    = false;
    bool velHighActive = false;
    bool velLowActive  = false;
    bool centerActive  = false;
    bool cornerActive  = false;
    bool dwellActive   = false;

    float dwellAccum     = 0.f;
    float velLowAccum    = 0.f;
    float dwellTotalMove = 0.f;

    std::map<TriggerID, float> cooldowns;

    // Constants
    static constexpr float EDGE_ZONE   = 100.f;
    static constexpr float HIGH_THRESH =   4.0f;
    static constexpr float LOW_THRESH  =   0.8f;
    static constexpr float LOW_SECS    =   3.0f;
    static constexpr float CENTER_ZONE =  80.f;
    static constexpr float CORNER_ZONE = 150.f;
    static constexpr float DWELL_MOVE  =   5.f;
    static constexpr float DWELL_SECS  =   8.0f;
    static constexpr float COOLDOWN_SEC =  1.5f;
};
```

### 3.2 TriggerBus.cpp — trigger evaluation

Each check method fires only on state transitions (off→on or on→off). Do not fire every frame while active.

```cpp
void TriggerBus::checkEdgeProximity(const CrosshairState& s) {
    float W = ofGetWidth(), H = ofGetHeight();
    float minDist = std::min({ s.cx, s.cy, W - s.cx, H - s.cy });
    bool near = minDist < EDGE_ZONE;
    if (near != edgeActive) {
        edgeActive = near;
        float d[4] = { s.cx, s.cy, W - s.cx, H - s.cy };
        int edge = (int)(std::min_element(d, d + 4) - d);
        float intensity = near ? ofMap(minDist, EDGE_ZONE, 0, 0, 1, true) : 0.f;
        fire({ TriggerID::EDGE_PROXIMITY, near, intensity, near ? edge : -1 });
    }
}

void TriggerBus::checkVelocity(const CrosshairState& s) {
    bool high = s.speed > HIGH_THRESH;
    if (high != velHighActive) {
        velHighActive = high;
        fire({ TriggerID::VELOCITY_HIGH, high, s.speed / HIGH_THRESH, -1 });
    }

    if (s.speed < LOW_THRESH) velLowAccum += ofGetLastFrameTime();
    else                      velLowAccum  = 0.f;
    bool low = velLowAccum > LOW_SECS;
    if (low != velLowActive) {
        velLowActive = low;
        fire({ TriggerID::VELOCITY_LOW, low, 1.f, 3 });  // hint: quadrant BR
    }
}

void TriggerBus::checkDwell(const CrosshairState& s, float dt) {
    dwellTotalMove += s.speed;
    dwellAccum     += dt;
    if (dwellAccum >= DWELL_SECS) {
        bool still = dwellTotalMove < DWELL_MOVE;
        if (still != dwellActive) {
            dwellActive = still;
            fire({ TriggerID::DWELL, still, 1.f, -1 });
        }
        dwellAccum     = 0.f;
        dwellTotalMove = 0.f;
    }
}

void TriggerBus::checkQuadrantCenter(const CrosshairState& s) {
    float W = ofGetWidth(), H = ofGetHeight();
    bool near = fabsf(s.cx - W * 0.5f) < CENTER_ZONE
             && fabsf(s.cy - H * 0.5f) < CENTER_ZONE;
    if (near != centerActive) {
        centerActive = near;
        fire({ TriggerID::QUADRANT_CENTER, near, 1.f, -1 });
    }
}

void TriggerBus::checkCornerNear(const CrosshairState& s) {
    float W = ofGetWidth(), H = ofGetHeight();
    bool tl = s.cx < CORNER_ZONE && s.cy < CORNER_ZONE;
    bool tr = s.cx > W - CORNER_ZONE && s.cy < CORNER_ZONE;
    bool bl = s.cx < CORNER_ZONE && s.cy > H - CORNER_ZONE;
    bool br = s.cx > W - CORNER_ZONE && s.cy > H - CORNER_ZONE;
    bool near = tl || tr || bl || br;
    int hint = tl ? 0 : tr ? 1 : bl ? 2 : br ? 3 : -1;
    if (near != cornerActive) {
        cornerActive = near;
        fire({ TriggerID::CORNER_NEAR, near, 1.f, hint });
    }
}

void TriggerBus::update(const CrosshairState& state, float dt) {
    checkEdgeProximity(state);
    checkVelocity(state);
    checkQuadrantCenter(state);
    checkCornerNear(state);
    checkDwell(state, dt);
}

void TriggerBus::fire(TriggerEvent e) {
    for (auto& cb : listeners) cb(e);
}

void TriggerBus::addListener(TriggerCallback cb) {
    listeners.push_back(cb);
}
```

---

## 04 — VIDEO SYSTEM

> **⚠ ONE `ofVideoPlayer` instance at all times. Hard Pi 3B constraint. Never instantiate a second player.**

### 4.1 VideoSystem.h

```cpp
#pragma once
#include "ofMain.h"
#include <vector>
#include <string>

class VideoSystem {
public:
    void setup(const std::string& mediaPath);
    void update();
    ofTexture&  getTexture();
    glm::vec2   getVideoSize() const;
    void nextFile();

private:
    ofVideoPlayer            player;
    std::vector<std::string> files;
    int                      fileIndex = 0;
    void loadFile(int index);
};
```

### 4.2 VideoSystem.cpp

```cpp
void VideoSystem::setup(const std::string& mediaPath) {
    ofDirectory dir(mediaPath);
    dir.allowExt("mp4");
    dir.listDir();
    for (auto& f : dir.getFiles())
        files.push_back(f.getAbsolutePath());
    if (files.empty()) {
        ofLogError("VideoSystem") << "No MP4 files found in " << mediaPath;
        return;
    }
    loadFile(0);
}

void VideoSystem::loadFile(int index) {
    player.stop();
    player.close();
    player.load(files[index]);
    player.setLoopState(OF_LOOP_NORMAL);
    player.setVolume(0);
    player.play();
    fileIndex = index;
}

void VideoSystem::update()              { player.update(); }
ofTexture& VideoSystem::getTexture()    { return player.getTexture(); }
glm::vec2  VideoSystem::getVideoSize() const {
    return { (float)player.getWidth(), (float)player.getHeight() };
}
void VideoSystem::nextFile() {
    loadFile((fileIndex + 1) % files.size());
}
```

---

## 05 — SHADER LIBRARY

All shaders compile at startup. `ShaderLibrary` owns all `ofShader` instances. No runtime compilation.

### 5.1 ShaderLibrary.h

```cpp
#pragma once
#include "ofMain.h"
#include <map>
#include <string>

class ShaderLibrary {
public:
    void      setup();
    ofShader& get(const std::string& name);
    bool      has(const std::string& name) const;

private:
    std::map<std::string, ofShader> shaders;
    void load(const std::string& name, const std::string& fragPath);
};
```

### 5.2 ShaderLibrary.cpp

```cpp
void ShaderLibrary::setup() {
    load("desaturate",   "shaders/desaturate.glsl");
    load("invert",       "shaders/invert.glsl");
    load("recolor",      "shaders/recolor.glsl");
    load("threshold",    "shaders/threshold.glsl");
    load("dither",       "shaders/dither.glsl");
    load("solarize",     "shaders/solarize.glsl");
    load("scanlines",    "shaders/scanlines.glsl");
    load("channelshift", "shaders/channelshift.glsl");
}

void ShaderLibrary::load(const std::string& name, const std::string& fragPath) {
    shaders[name].load("shaders/vert.glsl", fragPath);
    ofLogNotice("ShaderLibrary") << "Loaded: " << name;
}

ofShader& ShaderLibrary::get(const std::string& name) { return shaders.at(name); }
bool ShaderLibrary::has(const std::string& name) const { return shaders.count(name) > 0; }
```

---

## 06 — GLSL SHADER SOURCE

> **⚠ GLSL ES 1.0 only. No `#version` beyond `#version 120`. No `gl_FragData`. No `texture2DLod`. Validate every shader on Pi hardware before integrating.**

### 6.1 `data/shaders/vert.glsl` — shared by all effects

```glsl
#version 120
varying vec2 vTexCoord;
void main() {
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    vTexCoord   = gl_MultiTexCoord0.xy;
}
```

### 6.2 `desaturate.glsl`

```glsl
uniform sampler2D tex;
varying vec2 vTexCoord;
void main() {
    vec4  c    = texture2D(tex, vTexCoord);
    float luma = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    gl_FragColor = vec4(vec3(luma), c.a);
}
```

### 6.3 `invert.glsl`

```glsl
uniform sampler2D tex;
varying vec2 vTexCoord;
void main() {
    vec4 c = texture2D(tex, vTexCoord);
    gl_FragColor = vec4(1.0 - c.rgb, c.a);
}
```

### 6.4 `recolor.glsl`

```glsl
uniform sampler2D tex;
uniform vec3      tint;   // set per trigger event
varying vec2 vTexCoord;
void main() {
    vec4  c    = texture2D(tex, vTexCoord);
    float luma = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    gl_FragColor = vec4(luma * tint, c.a);
}
```

### 6.5 `threshold.glsl`

```glsl
uniform sampler2D tex;
uniform float     threshold;  // animated 0.3–0.7 via noise in update()
varying vec2 vTexCoord;
void main() {
    vec4  c    = texture2D(tex, vTexCoord);
    float luma = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    gl_FragColor = vec4(vec3(step(threshold, luma)), c.a);
}
```

### 6.6 `dither.glsl` — 8×8 Bayer ordered dither

> **⚠ If the Pi GLSL ES 1.0 driver rejects the 64-element local array, fall back to the 4×4 Bayer matrix (16 values). Provide the 4×4 fallback in a comment block.**

```glsl
uniform sampler2D tex;
uniform vec2      resolution;
varying vec2 vTexCoord;

float bayer8(vec2 p) {
    int x = int(mod(p.x, 8.0));
    int y = int(mod(p.y, 8.0));
    float m[64];
    m[ 0]= 0.0/63.0; m[ 1]=32.0/63.0; m[ 2]= 8.0/63.0; m[ 3]=40.0/63.0;
    m[ 4]= 2.0/63.0; m[ 5]=34.0/63.0; m[ 6]=10.0/63.0; m[ 7]=42.0/63.0;
    m[ 8]=48.0/63.0; m[ 9]=16.0/63.0; m[10]=56.0/63.0; m[11]=24.0/63.0;
    m[12]=50.0/63.0; m[13]=18.0/63.0; m[14]=58.0/63.0; m[15]=26.0/63.0;
    m[16]=12.0/63.0; m[17]=44.0/63.0; m[18]= 4.0/63.0; m[19]=36.0/63.0;
    m[20]=14.0/63.0; m[21]=46.0/63.0; m[22]= 6.0/63.0; m[23]=38.0/63.0;
    m[24]=60.0/63.0; m[25]=28.0/63.0; m[26]=52.0/63.0; m[27]=20.0/63.0;
    m[28]=62.0/63.0; m[29]=30.0/63.0; m[30]=54.0/63.0; m[31]=22.0/63.0;
    m[32]= 3.0/63.0; m[33]=35.0/63.0; m[34]=11.0/63.0; m[35]=43.0/63.0;
    m[36]= 1.0/63.0; m[37]=33.0/63.0; m[38]= 9.0/63.0; m[39]=41.0/63.0;
    m[40]=51.0/63.0; m[41]=19.0/63.0; m[42]=59.0/63.0; m[43]=27.0/63.0;
    m[44]=49.0/63.0; m[45]=17.0/63.0; m[46]=57.0/63.0; m[47]=25.0/63.0;
    m[48]=15.0/63.0; m[49]=47.0/63.0; m[50]= 7.0/63.0; m[51]=39.0/63.0;
    m[52]=13.0/63.0; m[53]=45.0/63.0; m[54]= 5.0/63.0; m[55]=37.0/63.0;
    m[56]=63.0/63.0; m[57]=31.0/63.0; m[58]=55.0/63.0; m[59]=23.0/63.0;
    m[60]=61.0/63.0; m[61]=29.0/63.0; m[62]=53.0/63.0; m[63]=21.0/63.0;
    return m[y * 8 + x];
}

void main() {
    vec2  px   = vTexCoord * resolution;
    vec4  c    = texture2D(tex, vTexCoord);
    float luma = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    gl_FragColor = vec4(vec3(step(bayer8(px), luma)), c.a);
}
```

### 6.7 `solarize.glsl`

```glsl
uniform sampler2D tex;
varying vec2 vTexCoord;
void main() {
    vec4 c    = texture2D(tex, vTexCoord);
    float luma = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    vec3  out_c = (luma > 0.5) ? (1.0 - c.rgb) : c.rgb;
    gl_FragColor = vec4(out_c, c.a);
}
```

### 6.8 `scanlines.glsl`

```glsl
uniform sampler2D tex;
uniform vec2      resolution;
varying vec2 vTexCoord;
void main() {
    vec4  c    = texture2D(tex, vTexCoord);
    float line = mod(floor(vTexCoord.y * resolution.y), 2.0);
    gl_FragColor = vec4(c.rgb * (line < 1.0 ? 0.35 : 1.0), c.a);
}
```

### 6.9 `channelshift.glsl`

```glsl
uniform sampler2D tex;
uniform float     shift;   // 0.002–0.008, driven by noise in update()
varying vec2 vTexCoord;
void main() {
    float r = texture2D(tex, vTexCoord + vec2( shift, 0.0)).r;
    float g = texture2D(tex, vTexCoord               ).g;
    float b = texture2D(tex, vTexCoord - vec2( shift, 0.0)).b;
    gl_FragColor = vec4(r, g, b, 1.0);
}
```

---

## 07 — QUADRANT

Each `Quadrant` owns its clip region, current/next effect, blend state, and scale noise. It receives the video texture as a parameter at draw time — it does not own it.

### 7.1 Quadrant.h

```cpp
#pragma once
#include "ofMain.h"
#include "ShaderLibrary.h"
#include <string>

class Quadrant {
public:
    int          id;      // 0=TL, 1=TR, 2=BL, 3=BR
    ofRectangle  region;  // pixel bounds on canvas

    void setup(int id, ofRectangle region, ShaderLibrary* lib);
    void update(float dt, float cx, float cy);
    void draw(ofTexture& videoTex, glm::vec2 videoSize);

    void setEffect(const std::string& effectName, float transitionSecs = 0.8f);

    void setTint(glm::vec3 t)  { tint      = t; }
    void setThreshold(float t) { threshold = t; }
    void setShift(float s)     { shift     = s; }

private:
    ShaderLibrary* shaderLib = nullptr;

    std::string currentEffect = "passthrough";
    std::string nextEffect    = "";
    float blendT   = 1.0f;   // 1.0 = fully on current; animates toward 0 during transition
    float blendDur = 0.8f;

    float scaleNoiseSeed;
    float currentScale = 1.0f;
    float scaleMin, scaleMax;

    float     lastCx = 640.f, lastCy = 360.f;
    glm::vec3 tint      = { 1.f, 0.78f, 0.25f };
    float     threshold = 0.5f;
    float     shift     = 0.004f;

    void drawWithEffect(ofTexture& tex, glm::vec2 videoSize,
                        const std::string& effect, float alpha,
                        float cx, float cy);
    void bindUniforms(ofShader& sh);
};
```

### 7.2 Per-quadrant scale configuration

Set these values in `QuadrantManager::setup()` when constructing each `Quadrant`.

| ID  | Name | scaleMin | scaleMax | scaleNoiseSeed |
| --- | ---- | -------- | -------- | -------------- |
| 0   | TL   | 0.80     | 1.00     | 0.0            |
| 1   | TR   | 1.20     | 1.60     | 50.0           |
| 2   | BL   | 0.50     | 0.70     | 100.0          |
| 3   | BR   | 1.80     | 2.40     | 150.0          |

Scale is driven by `ofNoise(time * 0.03 + scaleNoiseSeed)` mapped to `[scaleMin, scaleMax]`. At 0.03 frequency one noise cycle ≈ 33 seconds — imperceptible frame to frame, apparent only over 10–20 seconds.

### 7.3 Quadrant.cpp — update()

```cpp
void Quadrant::update(float dt, float cx, float cy) {
    lastCx = cx;
    lastCy = cy;

    static float timeAccum = 0.f;
    timeAccum += dt;
    currentScale = ofMap(
        ofNoise(timeAccum * 0.03f + scaleNoiseSeed),
        0, 1, scaleMin, scaleMax
    );

    // Advance blend transition
    if (blendT < 1.0f) {
        blendT += dt / blendDur;
        if (blendT >= 1.0f) {
            blendT         = 1.0f;
            currentEffect  = nextEffect;
            nextEffect     = "";
        }
    }
}
```

### 7.4 Quadrant.cpp — draw()

The video is always centered on `(cx, cy)`. `glScissor` clips to the quadrant region without an FBO.

> **⚠ `glScissor` Y origin is bottom-left (OpenGL convention). oF draws top-left. Use `ofGetHeight() - region.y - region.height` for the scissor Y value.**

```cpp
void Quadrant::draw(ofTexture& videoTex, glm::vec2 videoSize) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(
        (GLint)region.x,
        (GLint)(ofGetHeight() - region.y - region.height),
        (GLint)region.width,
        (GLint)region.height
    );

    if (blendT < 1.0f && !nextEffect.empty()) {
        drawWithEffect(videoTex, videoSize, currentEffect, 1.0f,        lastCx, lastCy);
        drawWithEffect(videoTex, videoSize, nextEffect,    1.0f - blendT, lastCx, lastCy);
    } else {
        drawWithEffect(videoTex, videoSize, currentEffect, 1.0f, lastCx, lastCy);
    }

    glDisable(GL_SCISSOR_TEST);
}

void Quadrant::drawWithEffect(ofTexture& tex, glm::vec2 videoSize,
                               const std::string& effect, float alpha,
                               float cx, float cy) {
    bool useShader = (effect != "passthrough" && shaderLib->has(effect));

    if (useShader) {
        ofShader& sh = shaderLib->get(effect);
        sh.begin();
        sh.setUniformTexture("tex", tex, 0);
        sh.setUniform2f("resolution", ofGetWidth(), ofGetHeight());
        bindUniforms(sh);
    }

    ofPushMatrix();
    ofTranslate(cx, cy);
    ofScale(currentScale);
    ofTranslate(-cx, -cy);
    ofSetColor(255, 255, 255, (int)(alpha * 255));
    tex.draw(0, 0, videoSize.x, videoSize.y);
    ofPopMatrix();

    if (useShader) shaderLib->get(effect).end();
}

void Quadrant::bindUniforms(ofShader& sh) {
    sh.setUniform3f("tint",      tint);
    sh.setUniform1f("threshold", threshold);
    sh.setUniform1f("shift",     shift);
}

void Quadrant::setEffect(const std::string& effectName, float transitionSecs) {
    if (effectName == currentEffect) return;
    nextEffect = effectName;
    blendT     = 0.0f;
    blendDur   = transitionSecs;
}
```

---

## 08 — QUADRANT MANAGER

`QuadrantManager` owns the four `Quadrant` instances, sets default effects, and handles `TriggerEvent` callbacks.

### 8.1 Default effect assignments

| Quadrant          | ID  | Default effect |
| ----------------- | --- | -------------- |
| TL (top-left)     | 0   | `passthrough`  |
| TR (top-right)    | 1   | `desaturate`   |
| BL (bottom-left)  | 2   | `recolor`      |
| BR (bottom-right) | 3   | `scanlines`    |

### 8.2 Trigger → effect mapping

| Trigger           | `active=true`                                                                                                      | `active=false`                                    |
| ----------------- | ------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------- |
| `EDGE_PROXIMITY`  | Quadrant nearest the edge → `invert`. Use `intensity` to drive `shift` uniform on that quadrant.                   | Nearest-edge quadrant returns to default.         |
| `VELOCITY_HIGH`   | All 4 quadrants → `threshold`. Set `threshold` uniform = 0.5. **Highest priority — overrides all other triggers.** | All quadrants return to defaults.                 |
| `VELOCITY_LOW`    | Quadrant BR (hint=3) → `dither`.                                                                                   | BR returns to `scanlines`.                        |
| `QUADRANT_CENTER` | All quadrants → `invert` for 0.3s, then return. Fire once; do not repeat until resolved.                           | No action.                                        |
| `CORNER_NEAR`     | Corner quadrant (from `quadrantHint`) → `recolor`. Set tint to `(1.0, 0.5, 0.1)`.                                  | Corner quadrant returns to default.               |
| `DWELL`           | All quadrants → `solarize`. Crossfade 2.0s.                                                                        | All quadrants return to defaults. Crossfade 2.0s. |

**Priority rule:** if `VELOCITY_HIGH` is active, its assignment wins for all quadrants regardless of other active triggers.

**Transition duration:** 0.8s for all trigger-driven changes unless the table specifies otherwise.

**Corner → quadrant mapping:** hint 0 → quadrant 0 (TL), hint 1 → quadrant 1 (TR), hint 2 → quadrant 2 (BL), hint 3 → quadrant 3 (BR).

### 8.3 QuadrantManager.h

```cpp
#pragma once
#include "ofMain.h"
#include "Quadrant.h"
#include "ShaderLibrary.h"
#include "TriggerBus.h"
#include <array>

class QuadrantManager {
public:
    void setup(ShaderLibrary* lib);
    void update(float dt, const CrosshairState& state);
    void draw(ofTexture& videoTex, glm::vec2 videoSize);
    void onTrigger(const TriggerEvent& e);

private:
    std::array<Quadrant, 4> quads;
    std::array<std::string, 4> defaultEffects = {
        "passthrough", "desaturate", "recolor", "scanlines"
    };
    void returnToDefaults(float transitionSecs = 0.8f);
};
```

---

## 09 — ofApp

### 9.1 ofApp.h

```cpp
#pragma once
#include "ofMain.h"
#include "CrosshairSystem.h"
#include "TriggerBus.h"
#include "QuadrantManager.h"
#include "VideoSystem.h"
#include "ShaderLibrary.h"

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void keyPressed(int key);

private:
    CrosshairSystem crosshair;
    TriggerBus      triggerBus;
    QuadrantManager quadrants;
    VideoSystem     video;
    ShaderLibrary   shaders;
};
```

### 9.2 ofApp.cpp — setup()

```cpp
void ofApp::setup() {
    ofSetFrameRate(24);
    ofSetVerticalSync(true);
    ofBackground(13, 13, 13);  // #0D0D0D
    ofHideCursor();

    shaders.setup();
    video.setup("/home/pi/blueprint/media");
    crosshair.setup();
    triggerBus.setup();
    quadrants.setup(&shaders);

    triggerBus.addListener([this](const TriggerEvent& e) {
        quadrants.onTrigger(e);
    });
}
```

### 9.3 ofApp.cpp — update() and draw()

```cpp
void ofApp::update() {
    float dt = ofGetLastFrameTime();
    video.update();
    crosshair.update(dt);
    triggerBus.update(crosshair.getState(), dt);
    quadrants.update(dt, crosshair.getState());
}

void ofApp::draw() {
    ofBackground(13, 13, 13);
    quadrants.draw(video.getTexture(), video.getVideoSize());
    crosshair.draw();
}
```

### 9.4 keyPressed() — keymap

| Key     | Action                                                             |
| ------- | ------------------------------------------------------------------ |
| `1`–`5` | Set crosshair preset (1=DRIFT, 2=SCAN, 3=HUNT, 4=NERVOUS, 5=ORBIT) |
| `TAB`   | Cycle to next crosshair preset                                     |
| `N`     | Load next video file                                               |
| `F`     | Toggle fullscreen                                                  |
| `ESC`   | Quit                                                               |

```cpp
void ofApp::keyPressed(int key) {
    if (key >= '1' && key <= '5') crosshair.setPreset(key - '1');
    if (key == OF_KEY_TAB)        crosshair.nextPreset();
    if (key == 'n' || key == 'N') video.nextFile();
    if (key == 'f' || key == 'F') ofToggleFullscreen();
    if (key == OF_KEY_ESC)        ofExit();
}
```

---

## 10 — PERFORMANCE RULES

> **⚠ These rules are non-negotiable on Pi 3B. Violating any of them risks dropping below 24fps.**

| Rule                                                                              | Rationale                                                 |
| --------------------------------------------------------------------------------- | --------------------------------------------------------- |
| `draw()` contains GL calls only — no logic, no math, no allocation                | Pi GPU cannot tolerate CPU stalls mid-frame.              |
| All `ofNoise()` calls run in `update()`                                           | Noise is CPU-bound — keep it off the render path.         |
| One `ofVideoPlayer` at all times                                                  | VideoCore IV hardware decode supports one H.264 stream.   |
| Shaders compiled at startup only                                                  | Runtime `glCompileShader` causes multi-frame stalls.      |
| `glScissor` for quadrant clipping — not stencil, not FBO                          | `glScissor` is a rasterizer state change, near-zero cost. |
| No texture allocation in `draw()`                                                 | `ofTexture::allocate()` syncs the GPU.                    |
| Max 8 texture samples per fragment shader                                         | VideoCore IV fragment throughput limit.                   |
| `ofSetFrameRate(24)` + `ofSetVerticalSync(true)`                                  | Cap the loop; vsync prevents HDMI tearing.                |
| `channelshift.glsl` uses 3 samples — benchmark before enabling on all 4 quadrants | 3 samples × 4 quadrants = 12 total. Validate on Pi.       |

### 10.1 Benchmark checklist

Run each scenario on Pi 3B hardware. Record fps. All must sustain ≥ 24fps.

- [ ] Single quadrant, `passthrough` → baseline, expect ~60fps
- [ ] Four quadrants, all `passthrough` → expect ≥ 30fps
- [ ] Four quadrants, four distinct shaders (`desaturate` / `invert` / `recolor` / `scanlines`) → must hit 24fps
- [ ] Four quadrants, `dither` on all four → must hit 24fps. If not: restrict dither to one quadrant at a time.
- [ ] Four quadrants, `channelshift` on all four → must hit 24fps. If not: restrict to one quadrant.
- [ ] Full system running (crosshair + triggers + mixed effects) → must sustain 24fps for 60 seconds.

> **⚠ If four simultaneous distinct shaders cannot hit 24fps: fall back to a single multi-branch shader with a `uniform int effectID[4]` array. This is the known Pi 3B mitigation strategy.**

---

## 11 — OPEN QUESTIONS

Document your finding for each before closing the build.

| #   | Question                                                                                  | Decision required                                                                                        |
| --- | ----------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| Q1  | Can four distinct GLSL ES 1.0 shaders run simultaneously at 720p @ 24fps on VideoCore IV? | If no: implement single multi-branch shader with `uniform int effectID[4]`.                              |
| Q2  | Does the Bayer 8×8 `float m[64]` local array compile on the Pi GLSL ES 1.0 driver?        | If no: fall back to 4×4 Bayer (16 values). Include 4×4 fallback in a comment block inside `dither.glsl`. |
| Q3  | Does `glScissor` in oF 0.12.x on Pi correctly handle the Y-axis flip?                     | Verify with a test rect. Adjust the `glScissor` Y calculation if needed.                                 |
| Q4  | Does `ofVideoPlayer` on Pi 3B use the GStreamer backend (`ofGstVideoPlayer`)?             | Confirm. If not: add `OF_USE_GST=1` to the Makefile.                                                     |
| Q5  | `channelshift.glsl` at 3 samples × 4 quadrants: does it hit 24fps?                        | If not: limit `channelshift` to a single quadrant at a time via trigger assignment.                      |

---

_— QUADRANT CROSSHAIR / IMPLEMENTATION HANDOFF v1.0 / BUILD FROM THIS DOCUMENT —_
