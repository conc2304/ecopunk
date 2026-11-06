# OC-003A — Predominant scene-colour investigation (diagnostic)

Status: diagnostic only. The production HUD and the ratified `scenecolor::SceneColorSnapshot` / `runtime.scene_color` contract are unchanged. Nothing staged or committed. Desktop evidence only (macOS, other agents' apps also running); the Pi was not measured.

## What was built
- `sketches/experience_runtime/src/PredominantColor.h` — OF-free, allocation-free algorithms (unit tests: `test/predominant_color_tests.cpp`, 22/22).
  - **Average**: linear-light spatial mean (the ratified reference).
  - **Dominant**: most populated 8x8x8 cluster; pixels are binned in the display-encoded domain, the representative colour is the **linear-light mean of the pixels assigned to the winning bin** (not the bin centre).
  - **Vibrant**: encoded-domain saturation s=(max-min)/max and value v=max; a pixel is eligible only if v>=0.15 and s>=0.20; eligible pixels vote w=s*v into the same bins; a bin qualifies only with >= max(2, 1% of the pixels) eligible pixels (a vibrant feature must cover ~1% of the frame; isolated saturated pixels cannot win); the winner has the largest summed weight, representative = weight-averaged linear colour. No qualifying bin => the vibrant value is explicitly **invalid** (never a substitute colour).
  - **Temporal variants** (dominant only): EMA alpha=0.4 on the representative colour; cluster-winner hysteresis (challenger must exceed the held bin's current population by 30%; a held bin below 0.5% of the pixels is released). Both reset on a scene-generation change and on any gap.
- `SceneColorSamplerDiag.cpp` (+ small additions to `SceneColorSampler.h`, two hook lines in `ExperienceRuntime.cpp`) — inert unless `EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM=<dir>`. It reduces the same SceneFBO through the existing two GPU passes (1280x720 -> 160x90 -> 16x9, RGBA16F), reads **both** targets and logs every measure at both resolutions plus the EMA/hysteresis variants, stage costs, and a "mixed 16x9 cell" fraction (cells whose 10x10 source pixels disagree, encoded std-dev > 0.12), to `<dir>/predominant.csv`, with a SceneFBO frame saved every 10 s. It never touches the series or the published snapshot. No full-resolution readback.
- `docs/reviews/orbit-placement/predominant/analyze_predominant.py` (stdlib only) composes the boards and `results.md`.

## Evidence (all real SceneFBO measurements; no synthetic data)
One 90 s run per scene (2 Hz, ~150-160 steady samples each, first 4 s dropped for stats): `docs/reviews/orbit-placement/predominant/board_<scene>.png` (actual FX frame, average / dominant / vibrant swatches, three synchronised ribbons) and `compare_<scene>.png` (160x90 vs 16x9 and the temporal variants on one axis). Tables: `results.md` there.

### Usefulness (observed)
- **Average** is stable but often a colour that is barely present: Blob's average is olive while the frame is orange/teal/grey; Blueprint's is a muddy green-brown.
- **Dominant** mostly reports the *background*: near-black in Blueprint, Fragment and Quadrant (the scene fields are dark/black), grey in Blob. It is very stable there, but carries little colour information.
- **Vibrant** is the most expressive (Blob: orange/green; Blueprint: khaki; Quadrant: teal) and tracks content, but is the least stable in busy scenes (Blob 40% winner-bin switches, mean successive-sample dE 16.7) and is explicitly invalid for 11% of Blob's steady samples.

### Stability (mean CIE76 dE between successive samples / share of samples whose winning bin changed)
| scene | average | dom160 | dom160 EMA | dom160 hysteresis | dom16 | vib160 |
|---|---|---|---|---|---|---|
| blob | 3.56 | 3.87 / 14% | 1.60 | 2.68 / 8% | 3.91 / 12% | 16.66 / 40% |
| temporal | 2.62 | 2.72 / 11% | 1.67 | 2.54 / 10% | 4.46 / 22% | 3.24 / 22% |
| blueprint | 0.63 | 0.11 / 0% | 0.07 | 0.11 / 0% | 0.13 / 0% | 0.91 / 3% |
| fragment | 2.71 | 0.22 / 0% | 0.09 | 0.22 / 0% | 4.22 / 31% | 5.01 / 26% |
| quadrant | 2.66 | 5.05 / 17% | 3.06 | 4.43 / 12% | 10.90 / 31% | 4.45 / 26% |

EMA roughly halves dominant jitter in the busy scenes; hysteresis lowers winner switching modestly (Blob 14% -> 8%, Quadrant 17% -> 12%) but not jitter. Vibrant was not smoothed in this study (it would need it in Blob/Quadrant).

### 16x9 vs 160x90
- **Average is identical** at both resolutions (mean dE 0.006-0.008; the 160x90 -> 16x9 pass is an exact 10x10 box).
- **Dominant/vibrant are resolution-sensitive.** 16x9 cells are 80x80 scene px; 5-49% of cells (Blob 29% mean, 49% in the shown frame) mix sub-colours. Mean dominant dE between 16x9 and 160x90: Blueprint 0.5, Blob 3.2, Temporal 2.9, Fragment 7.7, **Quadrant 19.0**; vibrant: Blob 18.6, others 2-6.5. 16x9 is acceptable for dominant in some scenes and misleading in others (Quadrant, Fragment, Blob vibrant).

### Incremental cost (desktop, p50 / p95; Pi not measured)
- 16x9 histogram analysis: 7-12 us / 10-34 us per sample. No extra readback (the series already reads 16x9).
- 160x90 histogram analysis: ~300-510 us / 460-860 us per sample, **plus** an extra 160x90 float readback (the GPU work is already finished): ~0.20-0.32 ms p50, up to 2.7 ms p95 in Blob under load. At 2 Hz this is small on desktop but roughly 40x the 16x9 CPU cost and adds a 230 KB transfer; on the Pi it must be measured before it is assumed cheap.

### Missing-data semantics (verified end to end)
A five-scene ring run with the diagnostic: 373 rows, 24 explicit gap rows from static transition frames, 51 generation changes, and in all 51 the first sample's EMA equalled the raw dominant (state reset). Smoothing never bridges a gap. The ring itself finished 102 passed / 1 failed (the known Fragment `maxFragments` cap).

## Recommendation (for the next layout decision)
1. Keep the ratified **average** as the reference/audit measurement; it is exact and resolution-independent.
2. If the Orbit ribbon should show "what colour is this scene", **vibrant** is the most informative candidate, but it needs temporal stabilisation and an explicit "no vibrant colour" state.
3. **Dominant as built is not recommended** for the ribbon: it mostly returns the dark background. A variant that excludes near-black pixels is an untested follow-up.
4. Prefer **160x90** for dominant/vibrant if they are adopted; 16x9 is too coarse in Quadrant/Fragment/Blob. Measure the extra readback and analysis on the Pi first.
5. Any adoption is a Tier C contract change (new measurement kinds in the snapshot / `runtime.scene_color` semantics) and goes through Architecture.

## Limitations
One run per scene, random media, so per-scene numbers are not repeat-averaged; dE is CIE76 on the linear-light values; thresholds were chosen up front and not tuned; no human preference test; desktop only and not under a quiet machine (another application was running); vibrant smoothing and a combined EMA+hysteresis variant were not evaluated; the diagnostic's own cost is excluded from production estimates (it also re-draws the two reduction passes).
