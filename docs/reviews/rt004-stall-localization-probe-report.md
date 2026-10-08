# RT-004 — 4–5 Second Stall Localization Probe: Report

**Work item:** RT-004 (diagnostic probe; no production behavior changed)
**Recipient:** ExperienceRuntime & SceneManager domain manager
**Date:** 2026-10-08
**Verdict:** RT-004 PROBE INCONCLUSIVE — see §1 and §6

## 1. TL;DR and reproducibility verdict

- **The 4–5 s stall did not reproduce on the desktop.** Across six full or partial runs and one acceptance-harness run (§4), no frame on the normal visible path exceeded 2.7 s.
- **One multi-second stall reproduces every time: the first Blob → Temporal activation in a process.**
  - It lasted 2.2–2.7 s on the normal path (6 of 6 occurrences, including one run with the probe off) and 3.26 s under the RT-002 harness.
  - It is a single blocked frame on the main thread, inside `SceneManager::beginFrame()` → `setupEntry()` → `TemporalProductionScene::setup()` → `TemporalSceneCore::setup()` → **`TFAmbientTextureLayer::setup()`**.
  - That function synchronously loads 38 background PNGs (1536×1024 or 1672×941, 32 MB on disk).
  - It accounts for 2.10–2.58 s of each stall. File read plus PNG decode is about 1.5 s of that; GL upload is about 0.1 s.
  - The video work in the same frame is small: about 20 ms for `loadExplicit` and about 47 ms for shader compilation.
- **No other path stalls for multiple seconds on desktop.** That includes Temporal reactivation, canonical media changes (manual or automatic), media changes during a Blob dwell, Temporal history refill, and pattern transitions. Video loads were at most 230 ms (cold startup) and at most 92 ms afterwards. The canonical decoder's per-frame `ofVideoPlayer::update()` occasionally spikes to 100–305 ms.
- **Why this is inconclusive:** the reported 4–5 s magnitude, and earlier notes of multi-second "media-load stalls", most likely come from the Raspberry Pi. No Pi measurement exists. Desktop evidence can't confirm whether the Pi stall is this same first-activation image load (likely to scale up on a Pi 3B) or a video load or decoder path that is fast here.
- **Next step:** run the same probe and driver on the Pi (§9). The instrumentation is already portable.

## 2. Environment

| Item | Value |
|---|---|
| Hardware | MacBook Pro, Intel i7-8850H, 16 GB RAM; Intel UHD 630 + Radeon Pro 560X; internal SSD |
| OS | macOS 15.7.7 |
| Build | `experience_runtime`, `make Release`, branch `blueprint-emergence-updates` @ `61a4acf`; worktree also has the uncommitted BLOB-002 change |
| Frame rate | 30 fps cap; steady-state p50 33.3 ms |
| Window | 1280×720, normal visible rendering (except the RT-002 run) |
| Media | `assets/shared/media/`: 26 files, all H.264 Main 960×540 at 24/29.97/30 fps, 0.5–20 Mbps, 2–79 MB, local SSD |
| Background images | `bin/data/backgrounds/`: 38 PNG files (19 tint/mask pairs), 32 MB on disk |
| Cache state | Warm OS file cache only. A cold cache needs `sudo purge`, which this agent may not run. |
| Pi | **Not measured** |

## 3. Instrumentation and overhead

**Probe:** `shared/src/Rt004StallProbe.h`, header only.
- Enabled only by `EXPERIENCE_RUNTIME_STALL_PROBE`; when off, each scope costs one branch.
- Timing uses `steady_clock` entry and exit around each scope.
- Each frame is closed at the top of `ofApp::update()`.
- Events are held in a bounded in-memory buffer (4,096 per frame, 20,000 samples per label). Nothing is written for normal frames.
- A frame longer than the threshold (`..._PROBE_MS`, default 200) writes:
  - its full scope tree;
  - the time not covered by any top-level scope (shown in logs as `uncovered`; covers swap, vsync and OS work);
  - the previous 8 frames;
  - a correlation context: switch phase, owner scene, Temporal epoch, canonical media ID and selection origin.
- **Watchdog thread:** logs the open scope stack once the main thread is more than 1 s into a frame.
- **macOS native stack sampling** (`..._PROBE_SAMPLE`, runs `/usr/bin/sample`): tried in run 1, but it started too late to catch the 2.4 s stall. Its output (Temporal drawing after the stall) was not kept; the scope breakdown localized the stall instead.

**Driver:** `Rt004StallDriver.{h,cpp}`, enabled by `EXPERIENCE_RUNTIME_STALL_DRIVER`.
- Runs on the normal visible path and only injects operator key presses through `ExperienceRuntime::keyPressed()`: `]` for next scene and `n` for next media.
- It logs its own 5 s frame-time windows, independent of the probe.
- Scripts:
  - **full matrix** (default): three cycles, about 5.5 minutes;
  - **`first-activation`**: one switch, then exit;
  - **`ambient-split`**: times decode and upload of each background file on a copy; production code is not called.

**Scope coverage:**
- ofApp update and draw;
- every `ExperienceRuntime::update` and `draw` step: scene-FBO pass, HUD update and draw;
- SceneManager begin-frame, setup, activate, deactivate and capability query;
- VideoPlaybackService setup and directory listing, decoder update, candidate load, automatic advance and next;
- `OfVideoDecoder` close, load and play;
- Temporal `synchronizeSelectedMedia`, activate and adapter update;
- `TimeOffsetVideoBuffer` load, reset, close, player update, history push and playhead upload;
- `TemporalSceneCore` setup sub-steps, update and draw;
- Blob core update and draw.

**Overhead:**
- About 130 ns per scope (measured in process), roughly 8 µs per frame.
- Same script with the probe on (run 1) vs off (run 2): mean frame 33.16 vs 33.19 ms. No measurable difference at the 30 fps cap.
- The first-activation stall also appears with the probe off (2,245 ms), so it is not a probe artifact.

**Not instrumented:** file `stat`/`open`/`read` calls inside `ofImage::load` (only the whole load is timed), GPU timer queries, and other threads' work.

## 4. Reproduction matrix and per-occurrence timings

**Runs:**

| Run | Path | Script | Probe |
|---|---|---|---|
| run1 | normal | full matrix | on, 200 ms |
| run2 | normal | full matrix | **off** (driver windows only) |
| run3 | normal | full matrix | on, 40 ms |
| first1–3 | normal | first-activation | on |
| rt002 | RT-002 acceptance harness | 20 cycles | on |

**First Blob → Temporal activation (once per process):**

| Run | Stall frame ms | `setupEntry` ms | `ambientTextures` ms | `loadExplicit` ms | `shaderLib` ms | Canonical decoder catch-up next |
|---|---|---|---|---|---|---|
| run1 | 2,476 | 2,342 | not split yet | 19 | not split yet | 102 |
| first1 | 2,331 | 2,208 | 2,114 | 20 | 48 | 122 |
| first2 | 2,305 | 2,182 | 2,096 | 21 | 47 | 121 |
| first3 | 2,363 | 2,257 | 2,167 | 21 | 47 | 105 |
| run3 | 2,677 | — | 2,437 | — | — | — |
| run2 (probe off) | 2,245 (driver max) | — | — | — | — | — |
| rt002 harness | 3,261 | 2,669 | 2,581 | — | — | — |

**`ambient-split`** (same 38 files, timed step by step): file read plus decode totals 1,498 ms (max 76 ms per file); allocate plus upload (with `glFinish`) totals 99 ms (max 5.7 ms).

**Other events in the matrix:** three cycles each.

| Event | Worst desktop frame (ms) | Dominant stage |
|---|---|---|
| Temporal reactivation, media unchanged | 82 | `TimeOffsetVideoBuffer.player.load` 37 |
| Temporal reactivation after a media change during Blob dwell | no frame over 40 | — |
| `n` while Temporal active (canonical load + Temporal `loadExplicit`) | 92 | `TimeOffsetVideoBuffer.player.load` 67 (canonical load 43) |
| `n` during Blob dwell | 207 (follow-on frame) | canonical `ofVideoPlayer.update` 204 |
| 30 s automatic advance | 112 | `OfVideoDecoder.ofVideoPlayer.load` 86 |
| Startup canonical load | 230 | `ofVideoPlayer.load` 230 |
| Temporal → Blob | under 70 | — |

**Steady state** (run1, by owner scene): Temporal p50/p95/p99 is 33.3/34.1/34.7 ms; Blob is 33.3/33.8/34.1 ms. Isolated spikes up to 248–305 ms come from the canonical `ofVideoPlayer::update()`.

**Normal path vs harness:**
- The harness shows ten more frames of 1.0–1.5 s. In each, runtime scopes total about 10 ms; the rest is the harness's own `grabScreen`/PNG save and readback hashing inside `ofApp::update`.
- Those frames are harness artifacts, not runtime stalls. The harness's first activation is longer (3.26 s) because the same work runs alongside harness I/O.

Logs: `docs/reviews/logs/rt004-*-2026-10-08.log`.

## 5. Critical-path trace and hypothesis classification

**First-activation stall** (first1, frame 310):
1. Last normal frame: frame 309, 33 ms, Blob fading out (phase 1).
2. Trigger: `]` → controller enters the Loading phase. The incoming scene was never set up, so `setupEntry()` runs.
3. Blocking stages, all on the main thread:
   - `syncVideoAdapter` → `loadExplicit`: 20 ms;
   - `ShaderLibrary::setup`: 48 ms;
   - **`TFAmbientTextureLayer::setup`: 2,114 ms**;
   - `TFBackgroundLayer::setup`: 26 ms.
4. `RuntimeServices.update`: the canonical decoder catches up after the blocked frame, 122 ms.
5. First recovered frame: frame 311, which activates Temporal.

| Hypothesis | Classification | Evidence / coverage |
|---|---|---|
| Decoder init or per-frame decode | **Ruled out** for multi-second on desktop. Per-frame `ofVideoPlayer::update` spikes of 100–305 ms are confirmed but small. | All loads and updates scoped |
| Filesystem I/O / media discovery | Media discovery ruled out (listing 5 ms, startup only). Image file reads are **part of the confirmed stall** but not split from decode. | `ambient-split` |
| Canonical media switching / adapter sync | **Ruled out** on desktop (≤ 112 ms) | 9+ manual and 6+ automatic changes |
| Blocking media load/open | **Confirmed synchronous, but ≤ 230 ms on desktop.** Unknown on the Pi. | Scoped |
| GPU texture allocation/upload/readback or driver sync | **Ruled out** as dominant (upload about 0.1 s of the 2.1 s) | `ambient-split` with `glFinish` |
| Scene activation/setup and FBO work | **Confirmed:** first-activation `TemporalProductionScene::setup()`. FBO reallocation is negligible. | 7 occurrences |
| Temporal transition/effects work | **Ruled out** (composition update and draw ≤ 32 ms) | Scoped |
| HUD or other runtime work | **Ruled out** (HUD draw max 12.7 ms) | Scoped |
| OS scheduling / resource contention | **Unknown.** Many 40–90 ms frames have no dominant scope, which looks like vsync or scheduling misses. Never seen at multi-second size. | Uncovered time logged |

## 6. Root cause

**Confirmed on desktop:** the multi-second stall is Temporal's one-time asset load, run synchronously inside the first scene switch.
- `SceneManager::setupEntry()` ([SceneManager.cpp](../../sketches/experience_runtime/src/SceneManager.cpp)) calls `TemporalProductionScene::setup()` ([TemporalProductionScene.cpp](../../sketches/experience_runtime/src/TemporalProductionScene.cpp)).
- That calls `TemporalSceneCore::setup()` ([TemporalSceneCore.cpp](../../sketches/temporal-fields/src/TemporalSceneCore.cpp)), which calls `TFAmbientTextureLayer::setup()` ([TFAmbientTextureLayer.cpp:9-80](../../sketches/temporal-fields/src/TFAmbientTextureLayer.cpp#L9-L80)).
- That function runs `ofImage::load()` for 38 PNG files.
- RT-003's lazy setup means this happens in the switch frame, not at startup.

**Root cause of the reported 4–5 s stall: UNKNOWN.** It hasn't been measured where it was observed. On the Pi it could be:
1. This same image load. A Pi 3B decodes PNG several times slower, and 38 × 4.7 MB of RGB textures also strain its memory.
2. A synchronous `ofVideoPlayer::load()`/`close()` on the Pi's GStreamer backend.
3. Something else entirely.

The probe distinguishes these directly (§9).

## 7. Smallest proposed correction (not implemented)

Only if the Pi run confirms that the stall is this asset load. The fix should stay inside the Temporal domain, with no contract or switching-policy change:
- `TFAmbientTextureLayer` should stop decoding all 38 images inside `setup()`.
- Smallest version: load only the texture pairs the current cycle actually uses, on first use, or spread loading over frames (for example one pair per frame) while Temporal is active.
- Alternative, an asset-pipeline change (TEMP-002 domain): ship backgrounds pre-scaled to the canvas, and store masks as single-channel images.

**Not recommended without an Architecture decision:** setting up every registered scene at startup. That changes SceneManager switching policy and keeps about 180 MB of decoded images resident on a 1 GB Pi.

If the Pi run instead points at video load/close, the correction belongs to Shared Video Playback (VIDEO-004) and needs its own proposal. Async loading is out of scope here.

## 8. Owning domain and cross-domain handoff

- **Confirmed desktop cause:** Temporal Fields Scene Migration (`TFAmbientTextureLayer`), with ExperienceRuntime/SceneManager informed because the cost lands in the switch frame.
- **If the Pi shows a video-load stall:** Shared Video Playback (VIDEO-004) plus Raspberry Pi Runtime & Performance.
- **No handoff yet;** the Pi run decides which domain owns it.

## 9. Pi verification needed (next diagnostic step) and post-patch regression

**Pi probe run.** Sync the files listed in §10 first. Then, from `sketches/experience_runtime`, run:

```bash
make Release -j2
EXPERIENCE_RUNTIME_STALL_PROBE=1 EXPERIENCE_RUNTIME_STALL_PROBE_LOG=$HOME/rt004-pi-probe.log \
EXPERIENCE_RUNTIME_STALL_DRIVER=1 EXPERIENCE_RUNTIME_STALL_DRIVER_LOG=$HOME/rt004-pi-driver.log \
./bin/experience_runtime
EXPERIENCE_RUNTIME_STALL_DRIVER=1 EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=ambient-split \
EXPERIENCE_RUNTIME_STALL_DRIVER_LOG=$HOME/rt004-pi-ambient.log ./bin/experience_runtime
```

- Native stack sampling is macOS-only. On the Pi, the watchdog's open-scope stack identifies the blocking stage.
- Read the `LONG FRAME` and `WATCHDOG` blocks for each 4–5 s frame. The innermost long scope names the stage.

**After any future patch:**
- RT-002 20-cycle acceptance;
- RT-003 switch harness;
- Temporal and Blob lifecycle harnesses;
- a Pi probe run showing the first-activation frame under the agreed budget, with steady-state p99 unchanged.

## 10. Files, commands, tests, limitations, contract impact

**Inspected:**
- `VideoPlaybackService.cpp`, `OfVideoDecoder.cpp`, `IVideoDecoder.h`, `TimeOffsetPlaybackAdapter.{h,cpp}`, `TimeOffsetVideoBuffer.{h,cpp}`
- `TemporalProductionScene.cpp`, `TemporalSceneCore.cpp`, `TFAmbientTextureLayer.cpp`, `TFSettings.h`
- `ExperienceRuntime.cpp`, `SceneManager.{h,cpp}`, `RuntimeServices.h`, `InputRouter.cpp`, `ofApp.{h,cpp}`, harness sources
- Media and background assets

**Changed (diagnostic only; no effect with the env vars unset):**
- New:
  - `shared/src/Rt004StallProbe.h`
  - `sketches/experience_runtime/src/Rt004StallDriver.{h,cpp}`
  - this report
  - `docs/reviews/logs/rt004-*-2026-10-08.log`
- Scope and `note` hooks:
  - `shared/src/video-playback/VideoPlaybackService.cpp`
  - `shared/src/video-playback/OfVideoDecoder.cpp`
  - `shared/src/TimeOffsetVideoBuffer.cpp`
  - `sketches/experience_runtime/src/ExperienceRuntime.cpp`
  - `sketches/experience_runtime/src/SceneManager.cpp`
  - `sketches/experience_runtime/src/TemporalProductionScene.cpp`
  - `sketches/experience_runtime/src/BlobProductionScene.cpp`
  - `sketches/experience_runtime/src/ofApp.{h,cpp}`
  - `sketches/temporal-fields/src/TemporalSceneCore.cpp`
- **Note:** `BlobProductionScene.{h,cpp}` and `ofApp.cpp` also carry the uncommitted BLOB-002 changes.

**Builds:** `make Release` exits 0 for `experience_runtime`, `temporal-fields` and `blob-region-prototype`.

**Tests:**
- With the probe off:
  - RT-003 switch: 5,351 checks, 0 failures;
  - Temporal lifecycle: 295 checks, 0 failures;
  - Blob lifecycle: 895/895.
- With the probe on: RT-002 acceptance, 80,600 checks, 0 failures, PASS.

**Limitations:**
- Desktop only; warm cache only.
- No GPU timer queries.
- Other threads (AVFoundation decoding) not instrumented.
- The native stack sample missed the stall.

**Contract impact:** none. No change to IEcopunkScene, SceneFrame, HudFrameData, switching policy, VideoPlaybackService authority, Temporal history semantics, Shared Effects or HUD ownership. Nothing committed or pushed.

**RT-004 PROBE INCONCLUSIVE — the 4–5 s stall was not reproduced on desktop (largest: 2.2–2.7 s first-activation `TFAmbientTextureLayer::setup()`, localized); missing evidence is a Pi measurement; next step is the §9 Pi probe run.**
