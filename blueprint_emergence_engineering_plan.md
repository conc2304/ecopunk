# Blueprint Emergence — Engineering Plan
## `pi_sketches` Multi-Sketch Repository

> **Document type:** LLM-directed engineering handoff
> **For:** Implementation planning, phase-by-phase execution, and lower-level handoff generation
> **Source design doc:** Blueprint Emergence Design Handoff v1.0
> **Updated:** Repo structure revised to support multiple sketches with shared library architecture
> **Platform:** macOS (dev/validation) → Raspberry Pi 3B+ (deploy)
> **Framework:** openFrameworks 0.12.x · C++17 · GLSL ES 1.0

---

## How to Use This Document

This document is written to be handed directly to an LLM to:
- Generate a lower-level implementation handoff for any single phase
- Answer implementation questions about any system
- Scaffold boilerplate code for any class described here

When spinning off a phase handoff, provide this document plus the instruction:
> *"Generate a detailed implementation handoff for [Phase N — Name] using the context in this engineering plan."*

Each phase section is self-contained enough to do this without additional context.

---

## 00 — Repository Structure

All sketches live in a single monorepo. Shared systems compile to a static library that every sketch links against. Each sketch is an independent OF project with its own `bin/data/` and Makefile.

```
pi_sketches/
├── shared/                              # Compiled to libpisketches.a
│   ├── src/
│   │   ├── VideoSampler.h/.cpp          # seek-and-freeze, file scanning, player lifecycle
│   │   ├── Fragment.h/.cpp              # base class: state machine, ofTexture, opacity, drift
│   │   ├── GridSystem.h/.cpp            # grid math, occupancy tracking, cell utilities
│   │   ├── AnnotationRenderer.h/.cpp    # measurement lines, code text, grid draw, divider
│   │   ├── CompositionBase.h/.cpp       # base cycle lifecycle (blank→placement→density→dissolve)
│   │   ├── Settings.h                   # color tokens, platform flags, base constants
│   │   └── shaders/
│   │       ├── desaturate.vert
│   │       ├── desaturate.frag          # GLSL ES 1.0 / 1.20 dual-version via #ifdef PLATFORM_PI
│   │       └── scanreveal.frag
│   └── Makefile                         # builds libpisketches.a
│
├── sketches/
│   └── blueprint_emergence/             # First sketch — all others follow this pattern
│       ├── src/
│       │   ├── main.cpp                 # OF entry point, window setup, 24fps target
│       │   ├── ofApp.h/.cpp             # owns BEComposition, drives update/draw
│       │   ├── BEFragment.h/.cpp        # extends Fragment: scan reveal, iris, slide-in animations
│       │   ├── BEComposition.h/.cpp     # extends CompositionBase: BE placement rules, circle event
│       │   └── BESettings.h             # includes shared/Settings.h, overrides BE-specific values
│       ├── bin/
│       │   └── data/
│       │       ├── fonts/
│       │       │   └── VeraMono.ttf
│       │       ├── codefragments.txt    # 50+ code text strings, one per line
│       │       └── media/               # local test footage on Mac; /home/pi/blueprint/media/ on Pi
│       ├── addons.make
│       ├── config.make                  # injects PLATFORM_PI define, links libpisketches.a
│       └── Makefile
│
├── tools/
│   └── deploy.sh                        # usage: ./deploy.sh blueprint_emergence
│                                        # rsyncs binary + data to Pi, restarts systemd service
└── README.md
```

### Shared vs Sketch Boundary Rules

| Belongs in `shared/` | Belongs in `sketches/<name>/` |
|---|---|
| VideoSampler (seek-and-freeze) | `ofApp` (sketch entry point) |
| `Fragment` base class | Sketch-specific Fragment subclass (e.g. `BEFragment`) |
| `GridSystem` (math, occupancy) | Sketch-specific composition subclass |
| `CompositionBase` (cycle arc) | `BESettings.h` (timing/color overrides) |
| `AnnotationRenderer` | `codefragments.txt` and other data files |
| Color token definitions in `Settings.h` | Any sketch-unique shaders |
| `desaturate` and `scanreveal` shaders | Sketch-specific `bin/data/` assets |

### Adding a New Sketch

1. Copy `sketches/blueprint_emergence/` → `sketches/<new_name>/`
2. Rename `BE*` classes to sketch prefix
3. Gut sketch-specific logic from `BEComposition` and `BEFragment`, keep the structure
4. The new sketch inherits video sampling, fragment management, grid math, and cycle arc for free

---

## 01 — Concept & Intent

Blueprint Emergence is a generative animation system that treats nature footage as raw scientific specimen material. The system simulates a computational filing apparatus — as if a machine is receiving video of the natural world, analyzing it, categorizing it, and placing it into an archival document structure in real time.

**Central metaphor:** observation as bureaucracy. Nature does not flow freely — it is cropped, framed, measured, labeled, and filed. The grid is the institution. The footage is the specimen. The code fragments are the machine's notes to itself.

This is not a screensaver or ambient loop. It has a procedural narrative arc: the canvas begins empty and accumulates over time, building toward density, then periodically clearing to begin again. Each cycle is unique.

---

## 02 — Visual Language

### Color Tokens (`shared/Settings.h`)

```cpp
// All sketches inherit these. BESettings.h may override.
static const ofColor GROUND_DARK  = ofColor::fromHex(0x0D0D0D); // primary canvas bg
static const ofColor GROUND_LIGHT = ofColor::fromHex(0xE8E6E0); // warm off-white, zone variant
static const ofColor RULE_WHITE   = ofColor(255, 255, 255, 77); // 30% opacity grid lines
static const ofColor RULE_ORANGE  = ofColor::fromHex(0xC87941); // vertical divider accent
static const ofColor TEXT_CODE    = ofColor(255, 255, 255, 153); // 60% — code fragment overlays
static const ofColor TEXT_DIM     = ofColor::fromHex(0x888888); // secondary labels, coords
```

`FOOTAGE_TINT`: none by default. Desaturation (0–40%) applied per-fragment via shader in Phase 3.

### Typography

All text is rendered as visual texture, never interactive. Purpose: make the canvas feel like a document being generated by the machine.

| Constant | Value | Usage |
|---|---|---|
| `CODE_FONT` | VeraMono.ttf (bundled) | All text overlays |
| `SIZE_LABEL` | 9–11px | Coordinate readouts, frame counters, zone IDs |
| `SIZE_CODE` | 10–13px | Code snippet fragments |
| `SIZE_ANNOTATION` | 8–9px | Measurement line labels |
| `CASE` | lowercase (code) / ALL CAPS (labels) | Enforced at content level |
| `RENDERING` | ofDrawBitmapString for labels; ofTrueTypeFont for code overlays | Anti-aliasing off for labels |

> **Open question (resolve in Phase 2 Pi validation):** ofTrueTypeFont at 9px on Pi — if anti-aliasing is unacceptable at that size, switch coordinate labels to `ofDrawBitmapString`.

### Grid System (`shared/GridSystem`)

- **6 columns × 8 rows** of equal cells at 720p → `floor(1280/6)` × `floor(720/8)` px per cell (integer math, no sub-pixel drift)
- Grid lines: 1px, RULE_WHITE, drawn every frame
- Grid does not animate — fixed reference frame
- All fragment placement snaps to grid intersections
- **Orange vertical divider:** 2px, `ofSetLineWidth(2)`, sits at column 3→4 boundary, full canvas height, always visible
- GridSystem exposes: `cellRect(col, row)`, `isOccupied(col, row)`, `reserve(col, row, w, h)`, `clear()`

### Crop Geometries (`BEFragment` subclass)

| Type | Description | Size |
|---|---|---|
| `RECT` | Axis-aligned rectangle, most common | 1–4 cells wide, 1–3 cells tall |
| `CIRCLE` | Perfect circle mask via FBO or radial shader | 3.5–4.5 cell diameter |
| `SLIVER` | Very thin rectangle, accent use | 1 cell × 4–6 cells, or inverse |
| `SQUARE` | 1:1 aspect | Exactly 2×2 cells |

---

## 03 — Composition States & Scene Arc

### Cycle Lifecycle (`CompositionBase`)

`CompositionBase` owns the four-phase arc. `BEComposition` extends it with BE-specific placement rules. Total cycle duration: 90–180 seconds (randomized per cycle via seed).

**Phase 1 — BLANK GROUND (0–8s)**
- Canvas is GROUND_DARK
- Orange divider draws top→bottom over ~3s at constant speed
- Grid lines fade in over 2s after divider completes
- Single coordinate label `[0,0]` appears bottom-right in TEXT_DIM

**Phase 2 — PLACEMENT (8s – ~120s)**
- Video fragments arrive on stochastic timer: 4–12s interval (uniform random)
- Each placement is a discrete event with arrival animation
- System tracks occupied grid cells, prefers unoccupied zones
- After each placement: measurement annotation draws to nearest existing fragment
- Code text fragments appear autonomously between placements
- Circle geometry appears once at mid-cycle (50–65% through placements)

**Phase 3 — DENSITY (120s – 150s)**
- Canvas 60–80% covered
- Placement rate slows: 15–20s interval
- Existing fragments begin slow drift: ±2px over 10s via sine
- Desaturation on existing fragments increases toward 40% — older material fades toward grayscale

**Phase 4 — DISSOLVE & RESET (150s – 180s)**
- Fragments erode: opacity decreases over 8–12s each, staggered
- Grid lines persist until last fragment gone
- Orange divider fades last
- Canvas holds black for 2s, then Phase 1 begins

### Composition Seed System

Each cycle uses a random seed (logged to console) that determines: placement order, fragment sizes, timing jitter, which code snippets appear, whether Zone A uses GROUND_LIGHT. No two cycles identical, all share the same structural grammar. `srand(cycleSeed)` called at cycle start in `CompositionBase::startCycle()`.

---

## 04 — Motion System

### Fragment Arrival Animations (`BEFragment::onArrival()`)

**RECT — Scan Reveal** (0.8s, linear)
- Horizontal scan line sweeps top→bottom across fragment bounding box
- Footage revealed beneath scanline; above is ground color
- After reveal: thin border rule draws clockwise around perimeter (~0.3s)

**CIRCLE — Iris Open** (1.0s, ease-out cubic: `t = 1 - pow(1-t, 3)`)
- Radius animates 0 → target
- Outline circle draws simultaneously, slightly ahead of fill
- Faint ghost ring at 102% radius persists permanently for remainder of cycle

**SLIVER — Slide In** (0.6s, ease-out quad: `t = 1 - pow(1-t, 2)`)
- Fragment slides in from nearest canvas edge, traveling its own width/height
- 1px afterimage trail at starting position fades over 2s

### Measurement Line Animation (`AnnotationRenderer`)

After each fragment arrives:
- Dashed line grows from new fragment edge → nearest existing fragment edge
- Draw speed: 400px/second
- At midpoint: pixel-distance label appears
- After 4s: fades to 20% opacity, persists permanently
- Style: RULE_WHITE, 4px on / 4px off dash (implemented as alternating `ofDrawLine()` segments)

### Ambient Drift (Phase 3 only)

Applied in `Fragment::draw()` as a position offset — stored grid position never mutates:

```cpp
float dx = sin(ofGetElapsedTimef() * 0.08f + phaseOffset) * 2.0f;
float dy = cos(ofGetElapsedTimef() * 0.06f + phaseOffset) * 1.5f;
// draw at (gridPos.x + dx, gridPos.y + dy)
```

Each fragment receives a unique `phaseOffset` at placement time. Grid lines are unaffected.

---

## 05 — Media System

### Video Input Spec

| Property | Requirement |
|---|---|
| Container | MP4 (.mp4) |
| Codec | H.264, Baseline or Main profile |
| Resolution | 720p max; 540p preferred for multi-video |
| Frame rate | 24 or 30fps source |
| Duration | Minimum 30s |
| Audio | Stripped or muted |
| Color space | sRGB, no HDR/Log |
| Pi path | `/home/pi/blueprint/media/` |
| Mac path | `bin/data/media/` |

### VideoSampler Architecture (`shared/VideoSampler`)

**Critical constraint:** ONE `ofVideoPlayer` instance active at all times (VideoCore IV limit).

`VideoSampler` exposes a single method: `seekAndCapture(float normalizedOffset)` → queues a seek request. The result (populated `ofPixels`) is available the following `update()` frame. Caller (Fragment) uploads pixels to its `ofTexture` on receipt.

```
update() frame N:   CompositionManager calls VideoSampler::requestCapture(offset, &targetFragment)
update() frame N+1: VideoSampler checks player state, calls getPixels(), signals targetFragment
update() frame N+2: Fragment uploads pixels to ofTexture, triggers arrival animation
```

Fragment arrival animation (0.6–1.0s) is designed to mask this 1–2 frame capture latency.

**Maximum 2 live (non-frozen) fragments** at any time. All others are static `ofTexture` snapshots.

**Cycle-to-cycle:** if multiple MP4s exist in `/media/`, rotate one per cycle.

### Frozen Texture Management

| Property | Value |
|---|---|
| Max fragments | 12 per cycle (triggers early dissolve if reached) |
| Texture size | Cropped to display size, not full-frame |
| Pixel format | `GL_RGB` for rect/sliver; `GL_RGBA` for circle |
| Memory estimate | ~10MB total for 12 fragments at avg size |
| Release policy | All textures destroyed at end of dissolve phase |

> **⚠ Seek latency risk:** GStreamer seek on Pi 3B+ may be 200–800ms. Prototype `seekAndCapture()` in isolation during Phase 3 before committing to timing. If P95 latency > 400ms, implement pre-capture: seek during idle intervals between placements and cache `ofPixels` until a fragment requests it.

### Code Text Fragment Library (`shared/AnnotationRenderer`)

50–60 pre-authored strings in `bin/data/codefragments.txt`, one per line. Selected randomly per cycle seed. Feel: plausible machine-generated analysis of video content.

Examples:
```
void particleManager()
float seed = perlinNoise(x,y)
{ render_options: { noise: true, resolution: 48 } }
[50000, 0-10, Y-08]
[SPECIFS, DAM.IA.SUPERBA]
[GENUS: RUPESTRIS]
frame: 1024
t=00:03:42.18
#include <geometry.h>
int width=1024, height=1024
Math.pow(10, 0);
```

Placement rules: within a zone (never crossing orange divider), near a recently placed fragment, in a dark/ground area (never over footage), opacity 50–70%.

---

## 06 — Spatial & Placement Logic

### Zone System

The orange divider splits the canvas into Zone A (left) and Zone B (right). Ground color set at cycle start, does not change mid-cycle.

| Zone | Default | Variant (30% of cycles) |
|---|---|---|
| Zone A (left of divider) | GROUND_DARK | GROUND_LIGHT |
| Zone B (right of divider) | GROUND_DARK | GROUND_DARK (independent) |

### Placement Algorithm (`BEComposition::onPlacement()`)

```
1. Select geometry type (weighted): RECT 50%, SLIVER 20%, SQUARE 20%, CIRCLE 10%
2. Select size within type constraints (in grid cells)
3. Find candidate positions: grid-aligned, fits without fully occluding existing fragment
   (partial overlap ≤30% acceptable)
4. Score candidates by:
     - Distance from canvas center (prefer off-center)
     - Proximity to existing fragments (prefer near-but-not-touching)
     - Zone balance (prefer underrepresented zone)
5. Add jitter to scores (ofRandom noise) to prevent rigid patterns
6. Select highest-scoring candidate
7. Assign random video time offset for this fragment's texture
8. Execute arrival animation → draw measurement line to nearest neighbor
9. Schedule next placement: uniform random 4–12s

MAX_ATTEMPTS guard: if 50 candidate positions all fail overlap check → trigger early dissolve
```

### Circle Exception

The CIRCLE does not go through the standard placement flow:

- Appears **exactly once** per cycle
- Triggered at 50–65% of total placement events complete
- Always placed near canvas center-left (never corner); center within ~200×200px region around `(320, 360)`
- Diameter: 3.5–4.5 grid cells
- Interrupts normal placement timer — placed as a special event in `BEComposition`
- Texture sampled from a different time offset than surrounding rectangles
- Ghost ring persists for remainder of cycle

---

## 07 — Systems Architecture

### Class Hierarchy

```
shared/
  Fragment           (base: state machine, ofTexture, opacity, drift)
  └── BEFragment     (BE-specific: arrival animations, geometry type)

  CompositionBase    (base: cycle arc, phase timing, fragment list, seed)
  └── BEComposition  (BE-specific: placement algorithm, circle event, zone logic)

  VideoSampler       (standalone: player wrapper, seekAndCapture queue)
  AnnotationRenderer (standalone: grid, divider, measurement lines, code text)
  GridSystem         (standalone: cell math, occupancy grid)
  Settings.h         (constants only, no class)

sketches/blueprint_emergence/
  ofApp              (owns BEComposition, VideoSampler, AnnotationRenderer)
  BESettings.h       (#include "../../shared/src/Settings.h" + overrides)
```

### Fragment State Machine

```
ARRIVING → STABLE → DRIFTING → DISSOLVING → DEAD

ARRIVING:   playing arrival animation (duration depends on geometry type)
STABLE:     static texture, no motion, measurement line drawing
DRIFTING:   STABLE + sin/cos position offset active (Phase 3 only)
DISSOLVING: opacity decreasing on staggered timer
DEAD:       flagged for removal; CompositionBase removes from list and destroys texture
```

### System Communication

- `ofApp` owns all systems. Systems do not hold references to each other.
- `CompositionBase` calls `VideoSampler::requestCapture()` and receives result via callback
- `CompositionBase` fires `onFragmentPlaced(newFrag, nearestFrag)` event → `AnnotationRenderer` receives it
- `AnnotationRenderer` calls `GridSystem` for cell coordinate queries
- No global state. All timing via `ofGetElapsedTimef()` deltas stored per-object.

---

## 08 — Performance Constraints (Pi 3B+)

| Constraint | Value | Notes |
|---|---|---|
| Target FPS | 24 | Set via `ofSetFrameRate(24)` in `main.cpp` |
| Render resolution | 1280×720 | Fallback: 960×540 + HDMI upscale if 24fps not sustained |
| Video backend | `ofGstVideoPlayer` | GStreamer + VideoCore IV hardware H.264 decode |
| Active players | 1 max | Hard constraint |
| Active textures | 12 max + 1 FBO | Fragment textures + circle mask FBO |
| GLSL | ES 1.0 on Pi / 1.20 on Mac | Dual-version via `#ifdef PLATFORM_PI` in shader source |
| Fragment shader instructions | ≤20 | ES 1.0 limit |
| FBO count | 1 | Circle mask only. No ping-pong FBOs. |
| CPU rule | Placement algorithm in `update()` only | `draw()` contains GL calls only |
| Texture uploads | Never in `draw()` | Queue in `update()`, upload on non-render frame |
| GPU memory | `gpu_mem=128` in `/boot/firmware/config.txt` | Required for VideoCore IV |

### GLSL Shader Strategy

Shader files carry both versions in one file:

```glsl
#ifdef PLATFORM_PI
  #version 100
  precision mediump float;
#else
  #version 120
#endif
```

`PLATFORM_PI` injected by `config.make` at build time. No `#ifdef` anywhere above the shader level.

---

## 09 — Settings Reference (`BESettings.h`)

All tunable values here. Primary knob surface for Pi performance tuning without recompiling logic.

```cpp
#pragma once
#include "../../shared/src/Settings.h"

// Canvas
constexpr int   CANVAS_W                  = 1280;
constexpr int   CANVAS_H                  = 720;
constexpr int   TARGET_FPS                = 24;

// Grid
constexpr int   GRID_COLS                 = 6;
constexpr int   GRID_ROWS                 = 8;
constexpr int   DIVIDER_COL               = 3;   // orange divider at col 3→4 boundary (0-indexed)

// Cycle timing (seconds)
constexpr float CYCLE_DURATION_MIN        = 90.0f;
constexpr float CYCLE_DURATION_MAX        = 180.0f;
constexpr float PLACEMENT_INTERVAL_MIN    = 4.0f;
constexpr float PLACEMENT_INTERVAL_MAX    = 12.0f;
constexpr float PLACEMENT_INTERVAL_DENSE  = 17.5f; // midpoint of 15–20s density range
constexpr float DISSOLVE_FADE_MIN         = 8.0f;
constexpr float DISSOLVE_FADE_MAX         = 12.0f;
constexpr float DIVIDER_DRAW_DURATION     = 3.0f;
constexpr float GRID_FADEIN_DURATION      = 2.0f;

// Fragments
constexpr int   MAX_FRAGMENTS             = 12;
constexpr int   PLACEMENT_MAX_ATTEMPTS    = 50;  // guard against infinite loop when canvas full
constexpr float DESATURATE_MAX            = 0.40f;

// Drift (Phase 3)
constexpr float DRIFT_AMP_X               = 2.0f;   // pixels
constexpr float DRIFT_AMP_Y               = 1.5f;
constexpr float DRIFT_FREQ_X              = 0.08f;  // radians/second
constexpr float DRIFT_FREQ_Y              = 0.06f;

// Arrival animations
constexpr float SCAN_REVEAL_DURATION      = 0.8f;
constexpr float IRIS_OPEN_DURATION        = 1.0f;
constexpr float SLIDE_IN_DURATION         = 0.6f;

// Measurement lines
constexpr float MLINE_DRAW_SPEED          = 400.0f; // px/second
constexpr float MLINE_FADE_OPACITY        = 0.20f;
constexpr float MLINE_FADE_DELAY          = 4.0f;   // seconds before fade to 20%

// Media path (override for Pi via #ifdef PLATFORM_PI in Settings.h)
// #ifdef PLATFORM_PI
//   constexpr char MEDIA_PATH[] = "/home/pi/blueprint/media/";
// #else
//   constexpr char MEDIA_PATH[] = "data/media/";
// #endif
```

---

## 10 — Phase Plan

Six sequential phases. Mac validation + Pi hardware gate required before proceeding to next phase.

---

### Phase 0 — Project Scaffold & Canvas Foundation
**Estimate:** 2–3 days · **Risk:** Low

**Goal:** Repo structure established, shared library builds, blueprint_emergence sketch links against it, canvas renders correctly on Mac and Pi boots without crash.

**Deliverables:**
- `pi_sketches/` repo initialized with `shared/` and `sketches/blueprint_emergence/` structure as specified in §00
- `shared/Makefile` produces `libpisketches.a`
- `sketches/blueprint_emergence/config.make` links against it
- 720p canvas at 24fps on Mac: GROUND_DARK background, 6×8 grid, orange divider, `[0,0]` label
- Zone A / Zone B color variant functional
- Divider grow animation (top→bottom, 3s) and grid fade-in (2s) working
- Pi cross-compile: binary runs to blank canvas on Pi 3B+

**Tech notes:**
- Use OF project generator for initial sketch scaffold to avoid linker issues, then retrofit shared lib linkage into `config.make`
- `ofSetTargetFrameRate(24)` in `main.cpp` from day one
- Grid cell size: `floor(CANVAS_W / GRID_COLS)` × `floor(CANVAS_H / GRID_ROWS)` — integer math
- Orange divider: `ofSetLineWidth(2)` + `ofDrawLine()`
- Pi cross-compile setup deserves a full dedicated day — it's the highest setup risk in the project

**Pi gate:** Binary cross-compiles, boots on Pi to black canvas, no crash.

---

### Phase 1 — Fragment System & Placement Logic
**Estimate:** 4–6 days · **Risk:** Medium

**Goal:** Full `CompositionBase` cycle arc and `BEFragment` state machine running with placeholder (solid color) textures. No video yet.

**Deliverables:**
- `shared/Fragment` base class with state machine (`ARRIVING → STABLE → DRIFTING → DISSOLVING → DEAD`)
- `BEFragment` subclass with `RECT`, `SLIVER`, `SQUARE` geometries (CIRCLE deferred to Phase 4)
- `shared/GridSystem`: `cellRect()`, `isOccupied()`, `reserve()`, `clear()`
- `CompositionBase`: full four-phase arc, timer, fragment list, seed system
- `BEComposition`: placement algorithm (§06), zone logic, `MAX_ATTEMPTS` guard
- Arrival animations for RECT (scan reveal) and SLIVER (slide in) — placeholder texture only
- Ambient drift active in Phase 3
- Staggered dissolve in Phase 4
- Debug overlay: grid cell occupancy visualization (toggleable, strip from release)
- Cycle seed logged to console

**Tech notes:**
- `Fragment` drift: apply sin offset in `draw()`, not to stored grid position — position stays grid-snapped
- Placement algorithm runs entirely in `update()`, never `draw()`; cap to ~2ms/frame
- CIRCLE deferred intentionally — its FBO dependency is a separate risk surface
- All timing in `BESettings.h` — no magic numbers in logic
- `srand(cycleSeed)` called at cycle start in `CompositionBase::startCycle()`

**Pi gate:** Placeholder fragment animations run at 24fps on Pi. Measure `update()` time budget with timing brackets.

---

### Phase 2 — Annotation Renderer & Typography
**Estimate:** 3–4 days · **Risk:** Low–Medium

**Goal:** `AnnotationRenderer` fully implemented: grid, divider animation, measurement lines, code text system.

**Deliverables:**
- `shared/AnnotationRenderer` class
- `codefragments.txt` with 50+ entries (see §05 for examples and rules)
- Code text placement: near most-recent fragment, within correct zone, never crossing divider, opacity 50–70%
- Measurement line system: dashed line at 400px/s, midpoint distance label, fades to 20% after 4s, persists
- Dashed line implemented as alternating `ofDrawLine()` segments (4px on / 4px off along line vector)
- All text rendering verified at 720p
- Font question resolved on Pi hardware (see open question below)

**Tech notes:**
- `ofDrawBitmapString` for coordinate/frame labels (fastest on Pi, pixel-perfect at small sizes)
- `ofTrueTypeFont` (VeraMono.ttf) for code fragments — load once at startup via `ofToDataPath()`
- Always check `ofTrueTypeFont::isLoaded()` — fails silently on Pi if path is wrong
- Measurement lines stored as structs `{p1, p2, label, opacity, age}`, updated each frame in `update()`
- `AnnotationRenderer` receives `onFragmentPlaced(newFrag, nearestFrag)` event from `CompositionBase`

**Open question — resolve this phase:** ofTrueTypeFont at 9–11px on Pi display. If anti-aliasing is unacceptable, switch coordinate labels to `ofDrawBitmapString`. Code fragment overlays may retain TrueType at 11–13px.

**Pi gate:** Font rendering verified on Pi display. Text draws correctly at 720p. 24fps sustained with annotation rendering active.

---

### Phase 3 — Video Sampler & Seek-and-Freeze
**Estimate:** 4–6 days · **Risk:** HIGH ⚠

**Goal:** Real H.264 video textures in RECT and SLIVER fragments. Seek latency measured and architecture confirmed on Pi hardware.

**Deliverables:**
- `shared/VideoSampler`: single `ofVideoPlayer` wrapper, non-blocking seek queue, `requestCapture(offset, &fragment)` API
- Pixel copy: `ofVideoPlayer` → `ofPixels` → caller's `ofTexture`, uploaded in `update()`
- File scanner: reads `MEDIA_PATH` at startup, graceful error if empty
- Cycle-to-cycle video rotation: one file per cycle
- Seek latency measured and logged: P50, P95 over 20 seeks
- Maximum 2 live (non-frozen) fragments enforced
- Desaturation shader (`shared/shaders/desaturate.frag`) active: `mix(texture, grayscale, amount)` per fragment
- RECT and SLIVER fragments rendering real video

**Tech notes:**
- `seekAndCapture()` is NEVER called in `draw()`. Queue in `update()`, result available next `update()`.
- Use `ofVideoPlayer::setPosition(float 0.0–1.0)` for seeking — more reliable than `setFrame()` on GStreamer
- After seek: call `update()` once; may need 2–3 frame delay on Pi before `getPixels()` is valid
- Fragment arrival animation (0.6–1.0s) designed to mask this capture latency
- Mac seek latency (AVFoundation) will be <1 frame — Pi behavior will differ significantly; test on Pi early
- Desaturation shader must stay within GLSL ES 1.0 20-instruction budget; `ofPixels` CPU fallback if needed
- Test with real user-supplied nature footage at 540p preferred source resolution

**Pi gate — CRITICAL:** `seekAndCapture()` functional at 24fps on Pi with H.264 source. Seek latency P95 < 400ms OR pre-capture queuing strategy implemented and tested. This gate must pass before Phase 4.

**Pre-capture fallback:** If seek latency > 400ms — seek during idle intervals between placements, cache `ofPixels` in a queue, Fragment pulls from queue at arrival time rather than triggering a live seek.

---

### Phase 4 — Circle Geometry & FBO
**Estimate:** 2–3 days · **Risk:** Medium

**Goal:** CIRCLE fragment type implemented. FBO or shader approach chosen and confirmed on Pi.

**Deliverables:**
- `BEFragment` extended with CIRCLE geometry
- Iris open animation (1.0s, ease-out cubic)
- Ghost ring: `ofNoFill` + `ofDrawCircle()` at 102% radius, drawn every frame, persists until cycle end
- Circle placement: mid-cycle trigger, center-left region (~200×200px around `(320, 360)`), 3.5–4.5 cell diameter
- Circle appears exactly once per cycle (special event path in `BEComposition`)
- Circle texture sampled from different time offset than surrounding rectangles
- FBO memory confirmed within budget

**Tech notes:**
- Two candidate approaches — implement and measure both on Pi before committing:
  - **FBO + alpha mask:** render circular clip to `ofFbo` using `ofPath`, composite when drawing video texture. `ofFbo::allocate()` once at cycle start.
  - **Radial discard shader:** `if (distance(gl_FragCoord.xy, center) > radius) discard;` — lighter GPU cost, verify GLSL ES 1.0 compatibility (`distance()` and `sqrt()` both available in ES 1.0)
- Ghost ring is cheap (`ofNoFill` + `ofDrawCircle()`) — no FBO needed for it
- Ease-out cubic: `float t_eased = 1.0f - pow(1.0f - t, 3.0f);` where `t` is normalized 0→1

**Pi gate:** Circle geometry at 24fps on Pi. Chosen approach (FBO or shader) documented in deployment notes.

---

### Phase 5 — Integration, Polish & Pi Deployment
**Estimate:** 3–4 days · **Risk:** Low–Medium

**Goal:** All systems integrated. Full composition loop running stably on Pi. Autostart configured.

**Deliverables:**
- 10 full cycles run on Pi without crash or memory leak
- 24fps confirmed at 720p on Pi (or 960×540 fallback documented)
- HDMI output mode resolved and documented (X11 vs framebuffer/EGL — see open question)
- `tools/deploy.sh` functional: `./deploy.sh blueprint_emergence` rsyncs binary + data, restarts service
- systemd service unit file: autostart on boot, `Restart=on-failure`
- Headless operation: no keyboard/mouse required post-boot
- Release build flags: `-O2`, strip debug symbols, no console output
- Peak VRAM logged via `vcgencmd get_mem gpu` at composition density peak
- All five open questions from §11 resolved and answers recorded

**Tech notes:**
- systemd service: `Type=simple`, `Restart=on-failure`, `Environment=DISPLAY=:0` if X11
- If X11: `xset s off; xset -dpms` to disable screensaver in service `ExecStartPre`
- If framebuffer/EGL: configure OF with EGL backend in `main.cpp`
- Font paths must be absolute on Pi — use `ofToDataPath("fonts/VeraMono.ttf")`
- Profile `update()` and `draw()` separately with `ofGetElapsedTimeMicros()` brackets; log to console in debug build
- Test with 3 different source videos to confirm cycle rotation

**Pi gate:** 10 full cycles stable. Peak VRAM within budget. Autostart confirmed from cold boot.

---

## 11 — Open Design Questions

These must be resolved before the relevant phase can finalize. Record answers in this document when resolved.

| # | Question | Resolve In | Engineering Action | Answer |
|---|---|---|---|---|
| 1 | Seek latency on Pi 3B+ with ofGstVideoPlayer | Phase 3 | Prototype `seekAndCapture()` in isolation. Measure P50/P95 over 20 seeks. If P95 > 400ms, implement pre-capture queuing. | _pending_ |
| 2 | Circle geometry: FBO alpha mask vs radial discard shader | Phase 4 | Implement both on Pi. Measure frame time delta. Prefer shader if within ES 1.0 budget. | _pending_ |
| 3 | HDMI output mode: X11 kiosk vs framebuffer/EGL | Phase 5 | Test both paths. X11 simplifies vsync + font rendering. Framebuffer saves ~10–15MB RAM. Document chosen approach. | _pending_ |
| 4 | Font rendering at 9–11px: ofTrueTypeFont vs ofDrawBitmapString | Phase 2 Pi validation | Test VeraMono.ttf at 9px on Pi display. Switch coordinate labels to `ofDrawBitmapString` if anti-aliasing unacceptable. | _pending_ |
| 5 | Audio: in scope? | Pre-Phase 0 | Confirm with project owner. If yes: ofSoundPlayer ambient loop as Phase 6, independent of visual system. | _pending_ |

---

## 12 — Validation Checkpoints

| Gate | Phase | Pass Criteria |
|---|---|---|
| Mac-0 | Scaffold | 720p at 24fps, grid + divider visible, shared lib links cleanly, no frame drops over 60s |
| Pi-0 | Scaffold | Binary boots on Pi to blank canvas, no crash |
| Mac-1 | Fragment System | Full cycle arc with placeholder textures, all animations play, 24fps sustained |
| Pi-1 | Fragment System | Placeholder animations at 24fps. `update()` time budget measured and logged. |
| Mac-2 | Annotation Renderer | Measurement lines, code text, divider animation correct at 720p |
| Pi-2 | Annotation Renderer | Font rendering verified on Pi display. 24fps with text active. Font question resolved. |
| Mac-3 | Video Sampler | Real video textures in RECT + SLIVER. Seek latency logged on Mac. |
| Pi-3 | Video Sampler | **CRITICAL.** seekAndCapture() at 24fps on Pi with H.264 source. P95 latency < 400ms OR pre-capture in place. |
| Mac-4 | Circle / FBO | Circle geometry correct, iris animation smooth, ghost ring persists |
| Pi-4 | Circle / FBO | Circle at 24fps on Pi. FBO vs shader decision made and documented. |
| Mac-5 | Integration | 10 full cycles without crash. Instruments profiler: no memory leak. |
| Pi-5 | Integration | 10 full cycles at 24fps. Peak VRAM logged. Autostart from cold boot confirmed. |

---

## 13 — Glossary

| Term | Definition |
|---|---|
| Fragment | A single video texture displayed through one of the four crop geometries |
| Composition | The full set of fragments and annotations on canvas at any moment |
| Cycle | One complete arc from blank canvas through accumulation to dissolve and reset |
| Ground | The background color of a canvas zone (DARK or LIGHT) |
| Grid | The fixed 6×8 rule-line structure that all placement snaps to |
| Divider | The orange vertical line splitting the canvas into Zone A and Zone B |
| Measurement line | The dashed annotation drawn between newly placed and nearest existing fragment |
| Ghost ring | The persistent circle outline at 102% of the circle fragment's radius |
| Phase offset | Per-fragment random value that staggers ambient drift timing |
| Seek-and-freeze | Seeking the video player to a time offset, capturing a texture frame, then holding it as a static image |
| `libpisketches.a` | Static library compiled from `shared/` — linked by all sketches |
| `CompositionBase` | Abstract base class in shared/ owning the four-phase cycle arc |
| `BEComposition` | Blueprint Emergence subclass implementing BE-specific placement logic |

---

*BLUEPRINT EMERGENCE / ENGINEERING PLAN v1.1 — Updated for pi_sketches multi-sketch repo structure*
*Each phase section above contains sufficient context to generate a standalone lower-level implementation handoff.*
