# hud_elements

Reusable openFrameworks HUD widgets for an eco-punk / cyberpunk / sci-fi nature overlay system.

Each widget:

- Accepts `x`, `y`, `width`, and `height` through `setBounds(...)`.
- Responsively scales internal geometry using the provided bounds.
- Accepts a shared `HudTheme`, direct `WidgetColors`, and `FrameOptions`.
- Favors composition: widgets use shared frame rendering, utility scaling, and base widget contracts.
- Draws mostly vector primitives for Raspberry Pi friendliness.

## Directory structure

```text
hud_elements/
  HudElements.h
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
```

## Add to an openFrameworks app

Copy `hud_elements` into your app `src/` directory or another include path.

Include:

```cpp
#include "hud_elements/HudElements.h"
```

Make sure the `.cpp` files are included in your project build. With the openFrameworks project generator, regenerate the project after copying the folder. If you use a Makefile-based workflow, the standard OF make system usually discovers `.cpp` files under `src/`.

## Example usage

```cpp
// ofApp.h
#include "hud_elements/HudElements.h"

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

## Widget list

### ScannerWidget
Hero circular ecological scanner. Good for center or corner overlays.

### NodeNetworkWidget
Mycelium/root communication mesh. Good for large horizontal areas.

### FlowFieldWidget
Curved stream lines with moving particles. Good for wind, pollen, water, nutrients.

### ContourWidget
Topographic/noise contour lines. Good subtle background layer.

### GaugeWidget
Radial, segmented, or semicircle value display.

### DataCardWidget
Small ecological status card with title, value, meter, and sparkline.

### HexGridWidget
Honeycomb/cybernetic ecology grid with animated active cells.

### ReticleWidget
Soft scan brackets and labels. Use sparingly over video.

## Raspberry Pi notes

These widgets avoid heavy shaders and textures. For best performance:

- Use `OF_BLENDMODE_ADD` sparingly if overdraw becomes expensive.
- Keep node count and particles moderate.
- Prefer fewer large transparent fills over many full-screen glow passes.
- Avoid running every widget at full-screen size at once.
- Use `setBounds` to limit each widget to its actual visual area.
