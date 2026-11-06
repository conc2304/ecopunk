> UPDATE: ratified and implemented (see `orbit-console-v0.1-real-scene-color-integration.md`); section 4 numbers are superseded (linear light, ~160x90 probe).

# Orbit Console v0.1 — placement finding and additive contract proposal

Status: PROPOSAL to Architecture. No contract, region, skin or scene file has been changed.

## 1. Placement finding (from inspection)

- "System Performance" is the System Telemetry v1 panel: five vitals sparklines in two
  adjacent frozen regions, `universal.activity_trace` (0.66, 0.43, 0.32 x 0.10 = 410 x 72 px,
  3 slots) and `universal.secondary_trace` (0.66, 0.54, 0.32 x 0.10 = 410 x 72 px, 2 slots),
  on the 1280x720 canvas. Together they span y = 309.6..460.8 px (about 410 x 151 px).
- Proposed Orbit host: the same two regions, drawn as one 410 x 151 px instrument view that
  alternates with the five sparklines. No new region, no geometry change, no skin change.
  `flexible.primary` (410 x 115 px) is rejected: too short for orbit + two history strips.
- Retained everywhere: scene/media titles, mode line, clock, activity/effects rows, controls,
  `media_viewport`. Alternated: only the five vitals sparklines <-> Orbit Console.
- View switching: today there is NO view-selection mechanism and no free control among the
  seven (`controls.*`). Proposed: a runtime-owned `instrumentView` enum (System | Orbit),
  toggled by a keyboard key on desktop, plus an "Instrumentation view" row on the existing
  MENU settings page for D-pad hardware. Focus ownership (HudFocusNavigator) is unchanged.

## 2. Why existing contracts cannot carry the data

`HudFrameData` exposes `sceneFrame.texture` (a non-owning GL texture pointer) but no CPU-side
measurement. HUD drawing must not read back (brief section 2), so the HUD has no legal way to
get RGB or luminance. `systemVitals` is the wrong carrier (different metrics and cadences).

## 3. Proposed additive field (the only boundary change)

```cpp
// shared/src/hud-runtime/SceneColorSnapshot.h  (OF-free, std only)
namespace scenecolor {
struct Sample { double t; float r, g, b; float luma; bool valid; }; // r,g,b,luma in 0..1
constexpr size_t kCapacity = 120;          // 60 s @ 2 Hz
constexpr double kPeriod = 0.5;            // seconds
struct SceneColorSnapshot {
  uint64_t revision;                       // bumps per new sample/gap/scene change
  uint64_t sceneGeneration;                // changes on every scene switch; history cleared
  double capturedAtSeconds;                // monotonic
  State state;                             // Unavailable | Valid | Stale (stale after 3 periods)
  uint32_t count;
  std::array<Sample, kCapacity> history;   // oldest -> newest
};}
// HudFrameData: std::shared_ptr<const scenecolor::SceneColorSnapshot> sceneColor; // null = none yet
```

Source id for the HUD resolver: `runtime.scene_color`.

## 4. Sampling definition (ExperienceRuntime-owned)

- Rect: the full SceneFBO, GPU-downsampled (blit) to a fixed 4x4 RGBA8 FBO, then averaged on
  the CPU: 16 texels, one tiny readback.
- Transparency/background: composite over opaque black, matching what the viewer sees.
- Encoding/space: sRGB-encoded 8-bit as stored in the FBO, normalised to 0..1, no
  linearisation. Luminance = Rec.709 weights (0.2126 R + 0.7152 G + 0.0722 B) on those values.
- Cadence 2 Hz, taken once per N frames after the scene FBO pass, never in HUD draw. Samples carry
  the monotonic timestamp. Both history strips read this one series, so timing, 60 s window and
  oldest-to-newest direction are identical.
- Invalid: readback failure or static-frame transition produces `valid=false` (a gap), never 0.
  Stale after 3 missed periods. A scene switch bumps `sceneGeneration` and clears history.
- Hue stability: below a saturation floor (about 0.04) the orbit holds its last hue and only
  the radius moves. Hue is computed in the HUD, not in the contract.
- Budget: <= 0.3 ms desktop; the Rock Pi stall must be measured before any acceptance claim.

## 5. Next step

On Architecture ratification: add the header and field, the ExperienceRuntime sampler, the
`runtime.scene_color` resolver source, an `orbit` presentation variant over the two trace regions,
the `instrumentView` toggle, and tests. Then desktop verification across all five scenes.
