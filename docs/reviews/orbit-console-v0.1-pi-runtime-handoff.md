# Orbit Console v0.1 — handoff to Pi Runtime manager

Status: desktop-verified, NOT qualified on Raspberry Pi 3B. Nothing is staged, committed or pushed. Base revision `03b3bb2` plus about 90 uncommitted paths from several sessions.
Full detail: `orbit-console-v0.1-real-scene-color-integration.md`.

## 1. What you are receiving
- Live scene-colour measurement: SceneFBO -> `scenecolor::SceneColorSnapshot` -> `HudFrameData::sceneColor` -> Orbit Console in regions #7/#8. Source ID `runtime.scene_color`.
- On by default (`sceneColorEnabled_ = true`); `EXPERIENCE_RUNTIME_SCENE_COLOR=0` disables.
- Latest change (not yet in the running desktop build): the RGB time series shares the segmented history's px/second (chart shows ratio x 60 s, right edges aligned). Tests: orbit 37/37, vitals 227/227.

## 2. Files to carry (the tree is uncommitted; a pull will not bring them)
- New: `shared/src/hud-runtime/SceneColorSnapshot.h`; `sketches/experience_runtime/src/SceneColorSeries.h`, `SceneColorSampler.h/.cpp`; `shared/src/hud-compositor/OrbitConsoleModel.*`, `OrbitDraw.h`; tests under `sketches/experience_runtime/test/` and `shared/src/hud-compositor-test/`.
- Edited: `HudFrameData.h`, `ExperienceRuntime.h/.cpp`, HUD renderer, resolver, presentation profile, region catalog, widgets (see the integration doc, section 3).
- Other sessions' uncommitted work is mixed in (fragment-trail pool, ofApp, OrbitInjectHarness). Review before shipping a tree to the Pi.

## 3. Pi-specific risks (all unmeasured)
1. GLES2 cannot use the half-float GPU linear path, so the Pi takes the 8-bit halving chain plus CPU LUT fallback. Desktop fallback error was about 0.0018 on Blob and worse on high-contrast scenes (Quadrant dither).
2. Readback is synchronous. Desktop cost was about 3.3-3.4 ms per sample at 2 Hz (first sample up to about 18 ms), against the original 0.3 ms/frame budget. The Architecture decision on this budget is still open.
3. Async PBO/fence readback needs GL 3.2 and is not usable on the Pi.
4. Fullscreen with live Orbit was not re-verified (RT-007 harness crashed on a Cocoa display fault, also on the previous build).

## 4. Qualification procedure
Run each mode on the Pi and record results:
- `EXPERIENCE_RUNTIME_SCENE_COLOR_STATS=1` (path, `unsupported`, `snapshotRebuilds`, `DrawCost` per 600 frames)
- `EXPERIENCE_RUNTIME_SCENE_COLOR_SELFTEST=1` (probe vs full-res reference error)
- `EXPERIENCE_RUNTIME_SCENE_COLOR=0` (disabled baseline)
Record: enabled vs disabled draw CPU; per-sample time and frame-time spike at each 2 Hz sample; fallback accuracy per scene; Orbit screenshots in all five scenes, one transition and one Loading frame; stale/gap behaviour; generation resets across scene switches.
Acceptance thresholds are not defined yet; Architecture must set the per-sample cost budget.

## 5. Open items
- Architecture: per-sample cost budget; accept or reject fallback accuracy on Pi; add `runtime.scene_color` and catalog edits to the decision log.
- Fragment-trail owner: `maxFragments` cap mismatch (cap 18 vs pool 36) still fails the five-scene ring. Unrelated to Orbit.
- Possible mitigations if Pi cost is too high (not implemented): lower sample rate on Pi, smaller probe via `EXPERIENCE_RUNTIME_SCENE_COLOR_PROBE`, or disable by default on Pi.
- `OrbitInjectHarness` (other session) should treat `luma` as linear relative luminance.

## 6. Constraints that still apply
No staging/commits/pushes by agents; no deploy/SSH from Claude Code sessions, so the Pi transfer and build are manual; no canonical geometry, HUD-004 manifest or scene/media/input authority changes; no readback inside HUD Runtime.

```text
WORK ITEM: Orbit Console v0.1 real scene-colour integration
PREVIOUS STATUS: desktop-verified
NEW STATUS: handed to Pi Runtime manager for hardware qualification
WHAT WAS NOT PROVEN: any Raspberry Pi behaviour; fullscreen with live Orbit
NEXT ACTION: transfer tree, build, run section 4 procedure
NEXT GATE: Architecture budget ruling + Pi results
```
