# Blob Experiment 2 — Multi-Effect Capacity Harness

**Date:** 2026-10-16
**Status:** IMPLEMENTED — PI CAPACITY HARNESS READY. **Pi results: NOT RUN — OPERATOR REQUIRED.**
**Operator guide:** [blob-effect-capacity-pi-runbook.md](blob-effect-capacity-pi-runbook.md)

## 1. Architecture

**Enable.** `EXPERIENCE_RUNTIME_BLOB_EFFECT_CAPACITY=<A..G|1>`. With it unset nothing is constructed: the normal media-change Original/Recolor policy runs, and the regression logs contain no capacity lines.

**Configuration.**
- `BlobCapacityPlan` (OF-free, unit-tested) holds presets A–G, named effect sets, the shader-slot count, the distinct count, and the assignment mode: single, round robin, or weighted.
- `BlobEffectCapacityBench` (Blob-local, owned by `BlobProductionScene`) handles operator keys, the unattended script, measurement windows, the TSV, and captures.
- `ofApp` offers keys to the bench before normal runtime routing, and serves capture requests (scene-only FBO plus the full window).

**Assignment.** A new fragment's treatment is `plan.pattern()[trackId mod 10]`. It is chosen once, at creation, through the new `VideoRegionController::effectAssigner`, which fills the existing `assignEffectForId()` seam.
- A fragment therefore never changes treatment.
- Any configuration change resets fragments, so each window measures only the new configuration.
- Shaded slots are spread evenly over the 10 slots. Round robin cycles the active effects in order; weighted mode uses smooth weighted round robin. Nothing is random.

**Load.**
- **Synthetic (default):** ten fixed IDs (1–10) feed `VideoRegionController` through a new `BlobSceneCore::setRegionOverride()`. Regions drift slowly on a 5×2 grid (~230×173 px each) and occasionally overlap. Detection and tracking still run on the real video, so their cost is real.
- **Tracked (`g` key):** real motion. On desktop this proved unusable for capacity measurement. Fern is a 25.9 s loop with bursty motion, and its windows swung between 0 and 10 fragments.

**Frame cap.** Runs stay at the production 30 fps cap. An uncapped mode was tried and removed: `BlobDetector` differences consecutive *app* frames, so uncapped repeats of the same video frame collapse the detected motion and with it the fragment load.

**Instrumentation.** `VideoRegionEffectRenderer::StageTimings` gains `unshadedDraws`, `shaderBinds`, `shaderSwitches` (program changes between consecutive shaded draws) and `distinctPrograms`. They are counted per frame only when the bench or the profile resets them. Render routing is unchanged.

**Windows.** 5 s windows, as in BLOB-002 profiling. Frames over 250 ms count as stalls and are excluded from the averages. Each window samples temperature and throttle state from sysfs. Script steps start with a 10 s warmup and end with a capture.

## 2. Effects

| Effect | Visual status | BLOB-002 status | Benchmark-ready | Sets |
|---|---|---|---|---|
| Original (unshaded) | valid | direct path at every alpha | yes (baseline) | all presets |
| recolor | valid | compatible | yes — **candidate** | q w |
| desaturate | valid (invisible on desaturated background) | compatible | yes — **candidate** | q w |
| heatmap_recolor | valid | outside tolerance (fade pop) | diagnostic | w |
| solarize | valid | outside tolerance | diagnostic | w |
| threshold | valid | outside tolerance | diagnostic | w |
| invert | valid | outside tolerance | diagnostic | w |
| channelshift | INCOMPATIBLE (UV-dependent offset, edge fringe) | within tolerance | diagnostic | e |
| chromatic_aberration, bioluminescence, ink_outlines, edge_glow | INCOMPATIBLE (neighbor/UV sampling) | outside tolerance | diagnostic | e |
| water_refraction, pixel_drift, scanlines | INCOMPATIBLE (UV/time displacement) | outside tolerance | diagnostic | r |
| hue_rotate | no-op at catalog defaults | within tolerance | diagnostic (cost only) | r |
| pixel_sorting | INCOMPATIBLE (UV) | outside tolerance | diagnostic | t |
| dither | no-op at effectAmount 1 | within tolerance | diagnostic (cost only) | t |
| ascii_solarpunk | INCOMPATIBLE (fractional alpha) | outside tolerance | diagnostic | t |

These statuses come from the BLOB-002 A/B harness and the 2026-10-15/16 reviews. Pi data will add cost classes (cheap / moderate / expensive). It doesn't change these statuses.

## 3. Cases

- **Presets A–G.** See the runbook §4.
- **Built-in scripts:**
  - `matrix`: A–G, 40 s each.
  - `distinct`: `w#1/2/4/6@10`, then `y#18@10`, 30 s each.
  - `scaling`: recolor, heatmap_recolor, edge_glow, water_refraction and pixel_sorting at 2/4/6/8/10 shaded fragments, 30 s each.
- **Arbitrary steps:** `fx@N:sec` or `set#D@N:sec`.

## 4. Example window line

```
[BlobCapacity] WINDOW ts=2026-10-16T11:10:34 step=1 case=A mode=single set=curated shaderSlots=0 distinctTarget=1 effects=- load=synthetic:10@x1.00 media=media.auto.3838009d fragments=10.00 shader=0.00 original=10.00 distinctUsed=0.00 effectsUsed=- frames=151 fps=30.00 frameMs=33.333 sceneDrawMs=0.163 regionRenderMs=0.050 backgroundMs=0.086 detectorMs=1.431 trackerMs=0.004 directDraws=10.00 legacyDraws=0.00 unshadedDraws=10.00 shaderBinds=0.00 shaderSwitches=0.00 stalls=0 maxStallMs=0 tempC=NA throttled=NA
```

The TSV carries the same fields: [desktop example](blob-effect-capacity-desktop/).

## 5. Desktop verification (macOS, 30 fps cap — not a capacity result)

The script ran 12 steps on the Fern clip with synthetic load ([log](logs/blob-effect-capacity-desktop-script-2026-10-16.log)). Every step held 30.00 fps with 0 stalls. The load and the mix were exact.

| Step | fragments | shader | distinctUsed | shaderSwitches/frame | regionRenderMs | backgroundMs |
|---|---|---|---|---|---|---|
| A | 10 | 0 | 0 | 0 | 0.049 | 0.084 |
| B | 10 | 10 | 1 | 0 | 0.255 | 0.073 |
| C | 10 | 5 | 1 | 0 | 0.165 | 0.074 |
| D | 10 | 5 | 2 | 4 | 0.172 | 0.074 |
| E | 10 | 8 | 4 | 7 | 0.228 | 0.073 |
| F | 10 | 10 | 2 | 9 | 0.253 | 0.073 |
| G | 10 | 10 | 10 | 9 | 0.273 | 0.073 |
| edge_glow@2/6/10 | 10 | 2/6/10 | 1 | 0 | 0.102 / 0.181 / 0.283 | 0.073 |
| w#1@10 vs w#6@10 | 10 | 10 | 1 vs 6 | 0 vs 9 | 0.255 vs 0.253 | 0.073 |

These are CPU submission times on a desktop GPU, so they say nothing about Pi capacity. One thing they do show: switching across six pointwise programs (9 switches per frame) costs no measurable CPU time on desktop. GPU-side switch cost is a Pi question.

Captures (`blob-effect-capacity-desktop/step*_{scene_only,hud}.png`) show the correct effect per fragment, the desaturated background intact, the HUD intact, and no black fragments. Threshold renders as intended black and white.

## 6. Regressions (2026-10-16)

| Check | Result |
|---|---|
| ExperienceRuntime / standalone Blob Release builds | PASS |
| Unit suite (incl. blob_effect_policy 1214/1214, blob_capacity_plan 934/934) | PASS |
| Blob lifecycle (bench off) | 3351/3351 |
| BLOB-002 A/B | 1514/1514 |
| GL restoration | ALL PASSED |
| RT-003 Blob→Temporal→Blob, bench off | 5351/0 |
| RT-003 Blob→Temporal→Blob, bench on (preset E) | 5351/0 |

## 7. Contract changes

None. The shared-file changes are Blob-only classes:
- `VideoRegionController`: optional assigner, plus a diagnostic effect-name list;
- `VideoRegionEffectRenderer`: counters;
- `BlobSceneCore`: optional region override, null by default.

## 8. Limitations

- Synthetic load is steady state, so fades (the legacy path for shaders) aren't in the synthetic numbers. The tracked-load matrix in runbook step 5 covers realistic churn.
- Synthetic region size is fixed (~230×173 px; scale with `EXPERIENCE_RUNTIME_BLOB_CAPACITY_REGION_SCALE`). Fill cost scales with area.
- In synthetic mode the HUD region count shows real tracking, not the synthetic fragments.
- Because of the 30 fps cap, headroom above 30 isn't measured. A case that holds 30 is "fits", not "free".
- No production policy is derived from any of this.

## 9. Next step

**RUN CONTROLLED PI CAPACITY MATRIX** (runbook §3).
