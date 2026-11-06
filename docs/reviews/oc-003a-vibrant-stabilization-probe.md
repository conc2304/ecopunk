# OC-003A follow-up — Vibrant colour stabilisation probe

Status: diagnostic only; no public contract, snapshot, semantic source or HUD change. The ratified 16x9 average path is untouched. Nothing staged or committed. Desktop only (macOS, other agents' work running); the Pi was not measured. Builds on `oc-003a-predominant-color-investigation.md`.

## Method
- Same 160x90 vibrant measurement and thresholds as before (eligible: v>=0.15 and s>=0.20; weight s*v; qualifying cluster >= max(2, 1%) of the pixels). 
- New diagnostic variants at 160x90 (`PredominantColor.h`, 28/28 unit tests, `SceneColorSamplerDiag.cpp`): **EMA** (alpha 0.4 on the representative colour), **winner-bin hysteresis** (a held bin stays while it still qualifies unless a challenger's summed weight exceeds the held bin's current weight by 30%), and **EMA + hysteresis**. All reset on a scene-generation change and on any gap; an invalid vibrant observation stays invalid in every variant (no average/dominant substitution, nothing carried across it).
- **Hold-last-valid** was evaluated offline from the same CSV (it is a display policy, not an algorithm).
- Five real scenes, 120 s each at 2 Hz (~190-215 steady samples), media selected by the app (random). Analysis: `analyze_vibrant.py`; tables: `docs/reviews/orbit-placement/predominant/results_vibrant.md`; boards: `board_vibrant_<scene>.png` (actual FX frame, average as reference only, raw / EMA / hysteresis / combined / hold-last ribbons on one time axis, winner-change ticks).

## Results (steady state, first 4 s dropped)
Valid coverage and winner switching:

| scene | valid | invalid runs (count / mean s / max s) | raw winner switches | hysteresis winner switches |
|---|---|---|---|---|
| blob | 82% | 22 / 0.9 / 2.0 | 39% | 34% |
| temporal | 100% | 0 | 19% | 10% |
| blueprint | 49% | 6 / 8.8 / 36.5 | 54% | 39% |
| fragment | 100% | 0 | 1% | 1% |
| quadrant | 96% | 3 / 1.2 / 2.0 | 34% | 23% |

Successive-sample colour change (CIE76 dE, mean / p95):

| scene | raw | EMA | hysteresis | EMA + hysteresis |
|---|---|---|---|---|
| blob | 14.3 / 68.0 | 6.8 / 26.7 | 12.9 / 70.7 | 5.9 / 26.7 |
| temporal | 2.6 / 12.9 | 1.4 / 6.0 | 1.8 / 12.7 | 1.2 / 5.8 |
| blueprint | 9.1 / 32.9 | 4.9 / 13.3 | 7.2 / 21.1 | 4.6 / 13.0 |
| fragment | 1.0 / 1.8 | 0.5 / 0.8 | 1.0 / 1.8 | 0.6 / 1.5 |
| quadrant | 10.0 / 43.5 | 5.2 / 14.9 | 7.8 / 38.5 | 4.6 / 14.3 |

Responsiveness (persistent steps of >= 20 dE in the raw series; latency = time until the variant is within 10 dE of the new level; 1-12 steps per scene, so these are indicative only): raw 0 s by definition; **EMA median 1.0-1.5 s** (reaches the new level in 100% of steps); hysteresis adds no delay but failed to reach the new level in 11% of Blob steps; combined median 1.0-3.0 s and 67-100% reached.

### Reading the boards
- EMA removes most flicker (jitter about halved in every scene) at a cost of about 1-1.5 s of lag. Raw vibrant in Blob and Quadrant is a flickering strip; EMA is a readable colour history.
- **Hysteresis adds little on top of EMA** (winner switches drop only a few points; successive-change improves slightly) and occasionally holds a stale cluster.
- Vibrant colours track real content (Blob: orange foliage/blue accents while the average is neutral grey; Blueprint: blue water/ice while the average is grey-brown), which the average cannot show.

### Unavailability and "hold last valid"
Vibrant is legitimately unavailable when the frame is grey/dark: Blob 18%, Blueprint 51% (one 36.5 s run), Quadrant 4% of the time in these runs; the same scene can differ by media (Blueprint was 100% valid in the previous probe).
Holding the last valid colour would be **misleading**: it would be shown 18% of the time in Blob and 50% in Blueprint, and the held colour disagreed with the next valid one by more than 20 dE in 9 of 21 Blob holds and 3 of 4 Blueprint holds (mean 20-31 dE). In Blueprint the board shows a 36 s hold of a pale blue while the frame was grey/black. Recommendation: show invalid explicitly (dark/"no vibrant colour"), never hold.

### Cost (desktop; p50 / p95)
- 160x90 float readback (extra, the series already pays the 16x9 read): 0.21-0.31 ms / 0.34-4.9 ms (spikes under load); about 230 KB per sample at 2 Hz.
- 160x90 histogram: 0.39-0.68 ms / 0.63-0.99 ms CPU per sample; the vibrant raw + EMA + hysteresis logic itself is 1-2.4 us (negligible).
- **GPU synchronisation:** in the diagnostic ordering the 16x9 read waits for the GPU (p50 1.2-3.1 ms) and the 160x90 read follows with no further stall, so the 160x90 read adds transfer only. A 160x90-only design would absorb that wait into the larger read; this was inferred, not measured (alternating-order runs would measure it; not done because another session is editing the sampler files).
- Production would not need the extra reduction submit (0.07-0.14 ms in the diagnostic): it would reuse the series' own passes.
- Pi: not measured. The CPU histogram is the dominant cost and would scale with the slower cores; assume it must be re-measured, and consider a cheaper analysis (e.g. subsampled 80x45) before the Pi test.

## Recommendation: CONTINUE INVESTIGATION (do not adopt yet)
- If adopted, the form that the evidence supports is **EMA-smoothed (alpha 0.4) vibrant at 160x90 with an explicit "no vibrant colour" state and no hold-last**, as an optional secondary measurement that can be switched off independently. Hysteresis is not worth its complexity on top of EMA.
- Blockers before adoption: (1) Rock Pi measurement of the 160x90 readback and histogram; (2) a cheaper analysis path if the Pi cost is material; (3) a coverage decision (up to ~50% invalid in some media) and how the HUD presents "unavailable"; (4) Architecture authorisation for any snapshot field / semantic source.

## Limitations
One run per scene with random media (coverage and switch rates vary by media), thresholds not tuned, CIE76 dE as the colour-difference proxy, step detection definitions are mine and based on few events, desktop only on a shared machine, no human preference test, hold-last evaluated offline, the diagnostic binary predates another session's concurrent edits to the same sampler files.
