# Orbit Console dual-source UI + scene-colour sampler optimisation — report

Nothing staged, committed or pushed. Desktop evidence only; the Raspberry Pi / Rock Pi was NOT measured (no SSH/deploy by agents).

## 1. FX sampler optimisation (desktop attribution, Pi pending)
Instrumented stages (stats log, `EXPERIENCE_RUNTIME_SCENE_COLOR_STATS=1`), p50/p95/max in ms, macOS desktop, Blob, ~116 samples, machine under load (load average 3-5, other sessions):

| stage | sync | deferred |
|---|---|---|
| (1) reduction submit | 0.058 / 0.091 / 24.0 | 0.053 / 0.114 / 25.2 |
| (2)+(3a) readback wait + 16x9 transfer | 1.99 / 3.83 / 6.65 | 0.22 / 0.46 / 2.02 |
| (3b) CPU average | 0.003 / 0.006 / 0.025 | 0.002 / 0.005 / 0.037 |
| (4) series push | 0.0003 / 0.0008 / 0.0012 | 0.0003 / 0.0008 / 0.0014 |
| snapshot rebuild | 0.005 avg, 0.036 max | 0.005 avg, 0.045 max |
| whole sample | 2.05 / 4.44 / 27.8 | 0.22 / 0.46 / 2.02 |

- The 24 ms "submit" max is the first sample (shader compile / FBO first use), once per run, same in both modes.
- The cost is the readback wait: the CPU blocks until the GPU finishes the reduction (and everything queued before it). On a tile-based GPU such as the RK3399 Mali this is likely most of the Pi's 32.9 ms mean; that is a hypothesis until measured on the Pi.
- New `EXPERIENCE_RUNTIME_SCENE_COLOR_MODE=deferred` (default unchanged: sync): the reduction draws into one of two 16x9 RGBA16F targets and is read two frames later when the GPU is done. No PBO/fence/GL 3.2 needed, so it is also valid on GLES drivers. At most one sample in flight (`busy` counts overruns); the sample keeps its issue timestamp and generation; a scene change drops it (`droppedGeneration`); readback buffer is reused (no per-sample allocation).
- `snapshotRebuilds` = number of immutable snapshots built by `SnapshotPublisher`. It rebuilds when series revision, Valid/Stale state, generation or scene id changes; every completed sample (or gap) bumps the revision, so one rebuild per sample is the expected rate (116 samples = 116/117 rebuilds). Each rebuild costs about 5 us (one 120-sample copy), so it is not a cost driver.
- Not done: smaller reduction targets (total texel fetches are constant, so the first pass is not shrunk by a smaller target), lower cadence (not needed on desktop), matched Pi enabled/disabled comparison, accuracy re-test of deferred (same shader path; the self-test compares the newest sample with a same-frame read, so it does not apply to deferred), Pi lifecycle checks.
- Draw CPU (mean us per frame, 3 windows each): off 4398/3916/3359, sync 3679/3831/3653, deferred 3980/3778/3314. The machine was loaded, so the enabled/disabled difference is within noise and no causal delta is claimed.
- Recommendation (pending Pi): keep default-on; switch the default to deferred only after the Pi run shows the wait collapsing; keep 2 Hz.

## 2. RAW source status
No decoded-video colour measurement exists. The seam is `VideoPlaybackService::currentTexture()` (single active media, pre-effects). A new public snapshot / `HudFrameData` field / source ID is required, so work stopped at a Tier C proposal: `orbit-console-raw-video-tier-c-proposal.md`. Caveat: Temporal-style adapters play the file at a time offset, so the service texture is the live head, not necessarily the displayed frame. Production RAW is BLOCKED on Architecture; the HUD shows NO RAW VIDEO.

## 3. HUD visuals (Dark Moss, 1280x720)
- Upper section (#7) = FX OUTPUT, lower (#8) = RAW VIDEO. Left column (84 px): source-labelled swatch, R/G/B bars with values, `Y` relative luminance. One shared R/G/B radar (FX green, RAW blue, same [0,1] scale, polygons only for sources with data); lower section holds its legend. Right group (239.6 px): colour ribbon (actual average RGB per bin) with a white luminance line on a dark halo, and a compact R/G/B trace directly below, sharing x with the ribbon.
- No header/footer, REL LUMA row and standalone luminance strip removed.
- Time axis: oldest left, newest right (anchored to the right edge); both sources anchor on the FX newest timestamp; bins = floor((width + gap)/(segmentWidthPx + gap)) capped at 120; each bin averages its samples in linear light (no interpolation); empty bin = Missing, all-invalid bin = Gap, valid black stays painted.
- Constants: `kOrbitHistoryTuning` in `shared/src/hud-compositor/OrbitConsoleModel.h` (`segmentWidthPx`=2.6, `segmentGapPx`=0.75). Non-positive width falls back to the default, gap clamped to [0, `kOrbitMaxGapPx`=8]. Verification-only env override `EXPERIENCE_RUNTIME_ORBIT_SEG_WIDTH_PX/_GAP_PX`. No setting UI or persistence.
- Derived slot counts at the 239.6 px ribbon: dense 1.2/0.4 -> 120 (sampler cap), default 2.6/0.75 -> 71, sparse 6/2 -> 30.
- Screenshots: `docs/reviews/orbit-placement/dual-source/`: `real_none_*` (real FX, RAW unavailable), `real_default|dense|wide_*` (real FX with SYNTHETIC RAW fixture: valid / gap / stale), `fixture_*_SYNTHETIC.png` (full 60 s synthetic history for all three tunings). Fixtures: `EXPERIENCE_RUNTIME_ORBIT_FIXTURE`, `EXPERIENCE_RUNTIME_ORBIT_RAW_FIXTURE=valid|gap|stale|black`, off by default.

## 4. Tests
Orbit model 57/57; presentation 301, real-frame 357, vitals 227, typography 14, 1119-check suite all pass; scene-colour series 47/47. Four five-scene Dark Moss ring runs: 102 passed / 1 failed (the separately owned Fragment `maxFragments` cap) for the none/default/dense runs, 0 GL errors; the wide run had 100/3 (two extra timing failures: Blueprint cycle freshness and a media-change check) while the machine was also compiling and running other jobs; not re-run. Fullscreen and a captured Loading frame were not re-verified in the new layout.

## 5. Performance separation
Sampling (GPU/readback) and snapshot publication are separated above; HUD drawing cost with the new visual enabled vs disabled was not isolated (loaded machine). Pi: none.

## 6. Remaining
Pi matched measurement (sync vs deferred), Architecture ruling on RAW, Loading/fullscreen capture in the new layout, HUD draw-cost isolation, re-run of the wide ring. `Makefile.tests.rej` in `sketches/experience_runtime/test/` is another session's leftover.
