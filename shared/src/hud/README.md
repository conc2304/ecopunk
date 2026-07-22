# hud_elements (canonical)

Reusable openFrameworks HUD widgets for an eco-punk / cyberpunk / sci-fi nature overlay system.

**This is the only copy of this library in the repository.** Do not copy the
HUD library into a sketch. Add the canonical shared source path to the
sketch build configuration instead — see "Add to an openFrameworks app"
below. `scripts/check-hud-library-uniqueness.sh` (run from the repo root)
fails the build if a second copy of any widget `.cpp` file appears anywhere
in the tree.

Each widget:

- Accepts `x`, `y`, `width`, and `height` through `setBounds(...)`.
- Responsively scales internal geometry and typography using the provided
  bounds (`sx`/`sy`/`su` in `shared/HudUtils.h`), including frame stroke
  width and tick length (`HudFrameRenderer`).
- Accepts a shared `HudTheme`, direct `WidgetColors`, and `FrameOptions`.
- Favors composition: widgets own a `HudFrameRenderer` member rather than
  inheriting frame-drawing behavior, and all widgets derive from the single
  `HudWidget` base with no further inheritance.
- Draws only immediate-mode vector primitives (no shaders, FBOs, or
  textures) for Raspberry Pi friendliness.

## Directory structure

```text
shared/src/hud/
  HudElements.h
  README.md
  shared/
    HudTypes.h
    HudUtils.h
    HudWidget.h
    HudFrameRenderer.h/.cpp
  ScannerWidget/
  NodeNetworkWidget/
  FlowFieldWidget/
  ContourWidget/
  GaugeWidget/
  DataCardWidget/
  HexGridWidget/
  ReticleWidget/
  GlitchTearWidget/
  StatusLightWidget/
  LogScrollWidget/
```

## Add to an openFrameworks app

In the sketch's `config.make`, add this directory (and any other
`shared/src/` subdirectories the sketch needs) to
`PROJECT_EXTERNAL_SOURCE_PATHS`, relative to the sketch directory, e.g.:

```make
PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src/hud
```

openFrameworks' Makefile system recursively adds every subdirectory under
an external source path to both the compile source list and the include
search path, so no additional `PROJECT_CFLAGS` entry is needed — a bare
`#include "HudElements.h"` (or `#include "hud/HudElements.h"` if the
sketch's external source path is the broader `shared/src`) will resolve.

Include:

```cpp
#include "HudElements.h" // or "hud/HudElements.h", depending on the sketch's source path
```

## Example usage

```cpp
// ofApp.h
#include "HudElements.h"

hud::ScannerWidget scanner;
hud::NodeNetworkWidget network;
hud::DataCardWidget card;
```

```cpp
// ofApp.cpp
void ofApp::setup() {
    hud::HudTheme theme;
    theme.colors.primary = ofColor(124, 232, 230, 220);
    theme.colors.secondary = ofColor(103, 255, 142, 200);
    theme.colors.accent = ofColor(244, 255, 106, 220);
    theme.colors.muted = ofColor(124, 232, 230, 70);
    theme.colors.background = ofColor(0, 20, 16, 22);
    theme.frame.style = hud::FrameStyle::Corners;
    theme.frame.showTicks = true;
    theme.additive = true;

    scanner.setTheme(theme);
    scanner.setBounds(60, 60, 320, 320);
    scanner.setup();

    network.setTheme(theme);
    network.setBounds(420, 80, 520, 280);
    network.setup();

    card.setTheme(theme);
    card.setBounds(70, 430, 340, 150);
    card.setup();
}

void ofApp::update() {
    float dt = ofGetLastFrameTime();
    scanner.update(dt);
    network.update(dt);
    card.update(dt);
}

void ofApp::draw() {
    // Draw video/nature content first.
    scanner.draw();
    network.draw();
    card.draw();
}
```

## Responsive resizing

Call `setBounds(...)` any time the output size changes.

```cpp
void ofApp::windowResized(int w, int h) {
    scanner.setBounds(w * 0.05f, h * 0.08f, w * 0.22f, w * 0.22f);
    network.setBounds(w * 0.35f, h * 0.10f, w * 0.42f, h * 0.25f);
}
```

## Per-widget colors

```cpp
hud::WidgetColors colors;
colors.primary = ofColor(120, 255, 220, 220);
colors.secondary = ofColor(90, 255, 130, 190);
colors.accent = ofColor(255, 245, 90, 220);
scanner.setColors(colors);
```

## Frame options

```cpp
hud::FrameOptions frame;
frame.style = hud::FrameStyle::Organic;
frame.showFrame = true;
frame.showTicks = true;
frame.showScanLines = true;
frame.padding = 12;
scanner.setFrameOptions(frame);
```

## Seeded procedural variation

`FlowFieldWidget`, `NodeNetworkWidget`, and `ReticleWidget` persist randomized
geometry/state and support `randomize(seed)`:

```cpp
network.randomize(42); // reseeds OF's global RNG and rebuilds node layout
```

The remaining widgets (`ScannerWidget`, `ContourWidget`, `HexGridWidget`,
`GaugeWidget`, `DataCardWidget`, `GlitchTearWidget`, `StatusLightWidget`,
`LogScrollWidget`) render purely from `time`/`ofNoise(...)`/short-lived
internal timers each frame and have no persisted random layout to rebuild,
so their `randomize()` override is the `HudWidget` default (reseeds the
global RNG only).

## Widget list

### ScannerWidget
Hero circular ecological scanner. Good for center or corner overlays.
Carries a set of public in-code tuning dials (`scaleMultiplier`,
`globalOpacity`, `lineWidthScale`, `hueShift`) inherited from
quadrant-crosshair's production tuning; no consumer currently sets these
away from their defaults.

### NodeNetworkWidget
Good for large horizontal areas. `NodeNetworkOptions::edgeStyle` selects
`Straight` (default — straight edges, reads as a tech/circuit mesh) or
`Organic` (edges bow into a quadratic-bezier curve via `organicBulge`,
reading closer to mycelium/root/vine growth). Node drift and packet timing
are identical in both modes — only edge rendering (and packet position,
which follows the curve in `Organic`) changes.

### FlowFieldWidget
Curved stream lines with moving particles. Good for wind, pollen, water,
nutrients. `FlowFieldOptions::density` (0-1, clamped) scales down line and
particle counts for a lighter-weight instance.

### ContourWidget
Topographic/noise contour lines. Good subtle background layer.

### GaugeWidget
Radial, segmented, or semicircle value display. Draws arcs via a hand-rolled
polyline helper — `ofDrawArc` does not exist in this project's linked
openFrameworks version; do not reintroduce it.

### DataCardWidget
Small ecological status card with title, value, meter, and sparkline.
`setMeter(float)` / `setValueText(string)` update a single field without
reconstructing the whole options struct.

### HexGridWidget
Honeycomb/cybernetic ecology grid with animated active cells, driven by
`ofNoise(x, y, time)` by default. `pulseAt(nx, ny)` (normalized, relative to
the widget's own bounds) triggers a ring of boosted activation expanding
outward from that point and decaying over `HexGridOptions::rippleLifetime`
— ties the grid's activation to a real external event instead of only its
own ambient clock.

### ReticleWidget
Soft scan brackets and labels. Use sparingly over video.
`ReticleOptions::preset` selects `Standard` (static targets) or `Tracking`
(spawn/track/die lifecycle with position lag and jitter). `labelOverride`,
when non-empty, replaces the built-in label table for both presets.

### GlitchTearWidget
A handful of thin horizontal bands that briefly appear with a horizontal
offset, then vanish within a couple of frames — signal-glitch chrome, not a
true pixel-displacement of whatever's underneath (this library draws only
vector primitives; see Raspberry Pi notes below). Fires ambiently on a
random interval; call `trigger()` to fire one on demand (e.g. off a real
event like a pattern switch).

### StatusLightWidget
A single on/off/state indicator: a small dot plus a label. `Idle` /
`Active` (gentle breathing pulse) / `Alert` (blinks by default). Distinct
from every other widget here, all of which are dials, cards, or full-field
effects rather than a simple discrete-state light. `setState(...)` updates
just the state without touching the rest of `StatusLightOptions`.

### LogScrollWidget
Continuous vertical scroll of monospace-style text, terminal-log style.
`pushLine(...)` appends one line, entering at the bottom and drifting
upward; oldest retained lines are dropped past `LogScrollOptions::maxLines`.
Distinct from `DataCardWidget`, which only ever shows one static value line.

## Raspberry Pi notes

These widgets avoid heavy shaders and textures. For best performance:

- Use `OF_BLENDMODE_ADD` sparingly if overdraw becomes expensive.
- Keep node count and particles moderate.
- Prefer fewer large transparent fills over many full-screen glow passes.
- Avoid running every widget at full-screen size at once.
- Use `setBounds` to limit each widget to its actual visual area.
- Pi runtime validation (FPS on real Pi hardware) is currently outstanding
  for this library — no Pi-specific build target exists in this repo yet.

## Drift prevention

Run `scripts/check-hud-library-uniqueness.sh` from the repo root before
committing changes to this library. It fails (non-zero exit, printing the
duplicate paths) if a second copy of any widget `.cpp` file exists anywhere
in the repository outside this directory.
