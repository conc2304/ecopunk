# OC-004 — Visual Activity Production Integration (HUD region #6)

**Date:** 2026-11-05 · **Platform:** macOS desktop (GL, 30 fps cap). **ROCK Pi / Pi 3B: not tested.**
**Baseline:** HEAD `03b3bb2`, branch `blueprint-emergence-updates`, 156+ dirty paths from concurrent work (preserved; nothing staged, committed, reset or stashed).

**Contract record.** The implementation baseline is [oc-004-scene-visual-activity-probe.md](oc-004-scene-visual-activity-probe.md) §6. Architecture confirmed it for this increment in the directive "OC-004 Contract Confirmation and Resume Authorization", delivered in the implementing session. That directive is **not a filed repository document**; no earlier ratification record existed in the repo. Registry: OC-004 `IMPLEMENTED`; Decision Log: DEC-020 `Proposed` (Architecture to confirm/file).

State vocabulary used below: **implemented** (code present) · **desktop-verified** · **hardware-qualified** (none) · **production-enabled** (none; default OFF).

## 1. Inspection matrix (before → after)

| # | Component | Before | After |
|---|---|---|---|
| 1 | `SceneVisualActivitySnapshot` | Missing | Implemented: `shared/src/hud-runtime/SceneVisualActivitySnapshot.h` (§6 fields; `history` omitted — optional in §6) |
| 2 | Optional `HudFrameData` publication | Missing | Implemented: `sceneVisualActivity`, null unless sampling opted in |
| 3 | `scene.visual.activity` binding | Missing | Implemented: source table + real resolver (missing unless `Valid`) |
| 4 | 64×36 frame-difference sampling | Implemented, disconnected (CSV only) | Connected: probe feeds the tracker; sampling unchanged |
| 5 | Fixed-log normalization | Core had sqrt diagnostic; log mapping only in report | `fixedLogLevel()` 3e-4…0.30 in `SceneVisualActivityTracker.h` (CSV diagnostics untouched) |
| 6 | HUD-owned smoothing | Runtime-side only | `HudVisualActivityBallistics.h`, applied in the renderer (attack 0.08 s / decay 0.9 s) |
| 7 | Segmented gauge, region #6 | Implemented, **wrong source** | Reads `scene.visual.activity`, Placeholder when missing |

**Gauge data source.** Before: `scene.activity.overall` (scene-semantic, not pixel-derived, not synthetic; DimLastValid policy). After: `scene.visual.activity` (measured). Disabled / Unavailable / Stale / Transition render `--` with an empty bar. There is no fallback source and no retained value.

Data path (now complete): SceneFBO → `SceneActivityProbe` → `VisualActivityTracker` → `VisualActivityPublisher` → `SceneVisualActivitySnapshot` → `HudFrameData::sceneVisualActivity` → `scene.visual.activity` → `HudVisualActivityBallistics` → segmented gauge.

## 2. Files changed

New: `shared/src/hud-runtime/SceneVisualActivitySnapshot.h`; `sketches/experience_runtime/src/SceneVisualActivityTracker.h`; `shared/src/hud-compositor/HudVisualActivityBallistics.h`; `sketches/experience_runtime/test/visual_activity_tracker_tests.cpp`; this report + `oc-004-integration-evidence/`.
Edited: `HudFrameData.h` (+1 field/include); `SceneActivityProbe.h/.cpp` (publish-only opt-in, tracker hooks); `ExperienceRuntime.h/.cpp` (publisher member, publication line, opt-in verification log); `HudSourceResolver.cpp`, `HudRealFrameResolver.cpp` (new source); `HudPresentationProfile.cpp` (region #6 binding source/policy only); `HudWireframeRenderer.h/.cpp` (ballistics member, substitution, `widgetInputFor` test seam); tests `hud_presentation_tests.cpp`, `hud_real_frame_tests.cpp`, `hud_vitals_tests.cpp`, `test/Makefile.tests`; governance `03-work-registry.md`, `Decision-Log-v3.md`.
Not touched: HUD-006 geometry, HUD-004 manifests, `scene.activity.overall`, scene/media authority, specialized playback, the segmented-bar drawing.

## 3. Tests (exact)

| Suite | Result |
|---|---|
| `visual_activity_tracker_tests` (new) | all passed |
| `hud_presentation_tests` | 1127/1127 (includes new region #6 binding assertions) |
| `hud_real_frame_tests` | 323/323 (new: resolution incl. valid zero, null/Unavailable/Stale/Transition missing, `scene.activity.overall` unchanged; ballistics reset rules) |
| `hud_vitals_tests` | 258/258 (new: renderer-level gauge test: disabled → placeholder not 0.77, valid zero, attack, stale/transition, generation reset) |
| `hud_segmented_bar_tests` / `hud_orbit_tests` / `hud_media_viewport_tests` / `hud_typography_tests` | 144 / 57 / 357 / 14 passed |
| `make Release` (experience_runtime) | builds |

Not run: the full sketch `make -f Makefile.tests test` aggregate (added the new binary to it; not executed end to end).

## 4. Five-scene desktop evidence (real ExperienceRuntime path)

Opt-in: `EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY=1` (documented cadence 10 Hz, deferred readback, CPU diff). 50 s per scene, single instance. Snapshot logs, frames and screenshots are in [oc-004-integration-evidence/](oc-004-integration-evidence/).

| Scene | States seen | raw min / p50 / max | level p10 / p50 / p90 | Gauge (screenshot) |
|---|---|---|---|---|
| Blob | unavail 1, valid 168, stale 1 | 0.0067 / 0.0089 / 0.0231 | 0.46 / 0.49 / 0.53 | lit (`blob_enabled_b.png`) |
| Temporal | unavail 1, valid 170, stale 2 | 0.0000 / 0.0084 / 0.0604 | 0.39 / 0.48 / 0.56 | lit |
| Blueprint | unavail 1, valid 174 | 0.0001 / 0.0031 / 0.0133 | 0.25 / 0.34 / 0.43 | 43 %, 6 of 12 segments (viewed) |
| Fragment | unavail 1, valid 170, stale 2 | 0.0028 / 0.0064 / 0.1016 | 0.37 / 0.44 / 0.56 | lit |
| Quadrant | unavail 2, valid 168, stale 1 | 0.0031 / 0.0120 / 0.3816 | 0.48 / 0.53 / 0.63 | 58 %, 7 of 12 segments (viewed) |

I viewed the Blueprint and Quadrant screenshots and the default-run screenshot; Blob/Temporal/Fragment screenshots were saved but only their snapshot logs were analysed. I did not count segments by pixel.

Observations: a genuine raw 0 occurred (Temporal, 1 sample) and was published as Valid; Blueprint stillness reads lowest (as in the probe report); `Stale` appears only as brief gaps during frame stalls and renders as unavailable. Every run began `Unavailable` then became `Valid` (the first grid has nothing to difference).

Frame time, enabled vs matched baseline (same build/scene/media; baseline = probe present, `MODE=off`), after dropping 30 startup frames, ms p50 / p95 / p99:

| Scene | enabled | baseline |
|---|---|---|
| Blob | 33.34 / 35.97 / 41.22 | 33.33 / 40.11 / 41.77 |
| Temporal | 33.22 / 42.91 / 46.79 | 33.32 / 42.37 / 44.26 |
| Blueprint | 33.33 / 39.69 / 41.73 | 33.33 / 39.13 / 40.50 |
| Fragment | 33.28 / 37.68 / 42.77 | 33.32 / 39.57 / 42.17 |
| Quadrant | 33.26 / 39.99 / 42.69 | 33.33 / 39.49 / 42.35 |

No measurable desktop difference. The enabled arm's worst frames (500–930 ms) fall at ≈10.5/20.5/30.5/40.5 s: these are the screenshot saves, not sampling; the baseline arm took none. One run each; desktop, not Pi.

**Not exercised on the real path:** scene-to-scene transitions and generation changes (each run was a single scene, generation 1). Those behaviours (Transition state, generation reset, invalid → no smoothing carry-over) are covered by the unit/renderer tests only. A real transition run is still owed.

**Evidence-quality note.** Two earlier batches were discarded: one ran from a copy without the media root (empty scenes), one had leaked background instances inflating frame times. The reported runs are the clean re-run.

## 5. Default-disabled verification

A normal launch (no opt-in env) produced zero `SceneActivity` log lines (no GL objects), a null `sceneVisualActivity` on every logged frame (0 non-null snapshots in 82 logged frames), and the gauge shows `ACTIVITY --` with an empty bar (`oc-004-integration-evidence/default_blob_a.png`) — not the `scene.activity.overall` value. Tests also assert null → unavailable.

## 6. Remaining ROCK Pi / Pi 3B qualification (hardware-qualified: NO)

Unresolved parameters Architecture must supply (none are in the repository; I did not invent them):
1. OC-004 performance acceptance thresholds (frame-time p50/p95/p99/worst deltas, incremental draw CPU, readback/sync cost, result latency).
2. Pi sampling cadence (desktop diagnostic 10 Hz only).
3. Pi `stalePeriods` / nominal period (snapshot currently carries the desktop 0.1 s / 3).
4. Whether the `TARGET_OPENGLES` path (float targets unavailable on GLES2; currently reports Unavailable) must be implemented for qualification.
5. Allowed cumulative budget with FX colour sampling.

Supervised, bounded tests to run once those exist (plan: [oc-004-pi-test-plan.md](oc-004-pi-test-plan.md)): matched sampling-off/on runs (same media, scene, resolution, config); 64×36 at the approved cadence; frame-time p50/p95/p99/worst; incremental draw CPU; GPU sync/readback cost; sampling latency; long-frame correlation; GL errors; resource bounds/cleanup; generation changes, invalid observations and recovery; combined with FX colour sampling.

## 7. Risks / decisions needed

- Optional `history` (§6) not implemented; Architecture to say whether it is required for region #6 (the gauge does not need it).
- Gauge text shows the level as a percentage of the log scale (e.g. "58%"), per the existing format; confirm that is the intended label.
- Level constants remain provisional (re-check when media pool changes).
- Registry/Decision-Log entries are proposals for Architecture; nothing is marked ACCEPTED or CLOSED.

```text
WORK ITEM: OC-004 Visual activity production integration
PREVIOUS STATUS: probe only; contract proposal (not implemented)
NEW STATUS: IMPLEMENTED (desktop-verified for sampling, publication, binding, gauge, default-off; transitions unit-tested only; hardware not qualified; production sampling OFF)
NEXT ACTION: Architecture supplies Pi criteria; real desktop transition run; supervised Pi session
REGISTRY UPDATE REQUIRED: DONE (proposed)   DECISION LOG UPDATE REQUIRED: DONE (proposed)   ARCHITECTURE HANDOFF REQUIRED: YES
```
