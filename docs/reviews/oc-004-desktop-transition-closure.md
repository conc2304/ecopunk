# OC-004 — Desktop Transition Closure

**Date:** 2026-11-05 · macOS desktop · **Disposition: CONDITIONAL PASS** (desktop only; two evidence gaps listed in §6; no lifecycle defect reproduced, no production code correction made).
**Provenance:** HEAD `03b3bb2a06bc4e3cef03b6057e8caff55c085ec4`, branch `blueprint-emergence-updates`, 167 dirty paths (concurrent work preserved; nothing staged/committed/reset/stashed).
Evidence: [oc-004-transition-evidence/](oc-004-transition-evidence/). Builds on [oc-004-visual-activity-production-integration.md](oc-004-visual-activity-production-integration.md).

## 1. Where a transition invalidates measurement state (code, `ExperienceRuntime::draw` + `SceneActivityProbe`)
1. Static frames (`!activeSceneLiveThisFrame`) → `probe.onStaticFrame`: `tracker.onStatic()` (state Transition, observation dropped) on every static frame; then, rate-limited to the sampling period, `clearStream()` (previous-grid reference invalidated, any pending readback dropped).
2. Activation (`activationSucceededThisFrame`) → generation++ → `probe.beginGeneration`: `tracker.beginGeneration()`, `clearStream()`, normaliser reset, `scenechange` row.
3. Deferred completion: `poll()` completes a pending readback only if `p.gen == d.gen`.
4. HUD: `HudVisualActivityBallistics` resets on any non-Valid snapshot or generation change.

## 2. Real runtime transitions (not synthetic)
Actual ExperienceRuntime, sampling opted in (`EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY=1`), real installation scene auto-cycle at the 10 s minimum via a scratch settings file. Order observed: Blob → Temporal → Blueprint → Fragment → Quadrant → Blob → Temporal → Blueprint → Fragment → Quadrant (generations 1–10). Four runs (≈40 transitions): r1 (screenshots every 2 s, diagnostic only — its screenshot stalls caused stale flicker), r2 (default deferral, sparse screenshots), r3 (readback deferral stretched to 30 frames), r4 (2-frame frames-strategy deferral). Per-frame log `VisualActivityHud` records scene, generation, state, raw, level, the **region #6 gauge input** (placeholder / value) and the HUD-smoothed gauge value.

## 3. Results (runtime observations)
- **Generation invalidation / no cross-scene difference.** In every run the probe CSV shows, at every generation boundary, `static … static → scenechange → first` where `first` carries no raw value; the first difference in the new generation is between two new-generation grids (dt ≈ 0.1 s, i.e. one sampling period; larger only after frame stalls). Asserted over all four runs: `scenechange` is always followed immediately by `first` with empty raw.
- **First frame is a baseline.** Snapshot state is `unavailable` (gauge placeholder) on the first frame of each new generation, then `valid`.
- **Static/Transition.** All 58 logged frames with `live=0` across the four runs reported `transition` with the gauge as placeholder; **0** frames reported Valid during a static run, and no valid/wide CSV row appears between a `static` row and the next `scenechange` (all 4 runs).
- **State ↔ gauge consistency.** Across 1241 logged frames (4 runs): `valid`→gauge value 846; `stale`→placeholder 249; `unavailable`→placeholder 88; `transition`→placeholder 58. **Zero** violations (no non-Valid state ever shown as a value; no Valid shown as placeholder). Generations are monotonic.
- **HUD smoothing reset.** For all 23 generations checked, the gauge value on the first Valid frame equals the unsmoothed level to <0.01 (no ramp from the previous scene's value), e.g. Temporal gen 2: level 0.952, gauge 0.952 immediately after a Blob gauge near 0.
- **Resume.** Every generation reached Valid within a few sampling periods.
- **Screenshots (region #6):** `s12.png` — mid-transition ("TRANSITIONING IN 33%"): `ACTIVITY --`, empty bar; `s34.png` — Blueprint, `45%` with 6 of 12 segments; `s09/s23/s56.png`; `default_startup.png` — normal startup `--`.

## 4. Delayed-readback safety
- **Real-runtime evidence:** with the stretched deferral (r3), a pending readback straddled all 7 transitions that occurred and was discarded each time: `DISCARD pending readback issued gen=N … (current gen=N) reason=static-frame`; r2 (default settings) also logged one natural discard. After each discard the next measurement was a new-generation `first` then a new-generation difference (CSV, §3). Submission generation is retained (`Pending::gen`) and printed.
- **Not demonstrated on the real path:** (a) the `completion-gen-mismatch` branch in `poll()` (never reached: `beginGeneration`/static `clearStream` drop the pending slot first, so it is defence in depth); (b) the `generation-change` discard reason (static frames always precede activation, so the pending slot was already dropped). Both would need a GL-level test seam that does not exist; I did not add one. This is the exact remaining evidence gap for the completion-time generation check.
- **Residual hazard (code inspection, NOT reproduced):** `onStaticFrame` calls `tracker.onStatic()` before the sampling-period rate limit but `clearStream()` after it, so a readback issued ≤2 frames before the first static frame could complete during the static run and flip the snapshot to Valid for a frame. Not observed in ~40 real transitions (including deterministic 2-frame deferral, r4), so per scope no change was made. Recommend Architecture decide whether to harden it speculatively.

## 5. Defects
No lifecycle defect was reproduced; no production behaviour was changed. Added **verification-only logging** (opt-in): probe `DISCARD`/in-flight messages (when the probe is enabled) and an `EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY_LOG=1` line in `ExperienceRuntime::draw` after HUD update showing the gauge input. `widgetInputFor` (already added) is the seam. I also fixed my own earlier mistake in `sketches/experience_runtime/test/Makefile.tests` (a stray `X_BIN)` in the `test:` prerequisites from a previous edit), which had broken the aggregate target.

## 6. Tests and build
| | Result |
|---|---|
| `visual_activity_tracker_tests` | passed |
| `scene_activity_core_tests` | passed |
| sketch aggregate `make -f Makefile.tests test` (ExperienceRuntime unit tests) | exit 0, all suites pass |
| HUD: presentation 1127, real-frame 323, vitals 258, segmented-bar 144, orbit 57, viewport 357, typography 14 | all pass |
| `make Release` (experience_runtime) | success |
Unit-test evidence = the above; runtime evidence = §3–§4 only. The previously reported unrelated full-ring stale-status failure was not run and is kept separate.

## 7. Default-off
Plain launch (no opt-in env): 0 `SceneActivity` log lines, null snapshot on all 68 logged frames, gauge placeholder on all 68; screenshot `default_startup.png` shows `ACTIVITY --`.

## 8. Remaining ROCK Pi blockers (unchanged)
Pi acceptance thresholds, Pi cadence, Pi stale-period value, GLES float-target path decision, FX-colour cumulative budget (Architecture); supervised hardware session (operator). Hardware-qualified: NO. Production-enabled: NO.
