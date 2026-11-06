# OC-004 — Static-Boundary Lifecycle Correction (preventative)

**Date:** 2026-11-05 · **Status:** IMPLEMENTED; unit-verified and desktop-verified on the real runtime path (post-fix transition runs, below). Hardware-qualified: NO. Production-enabled: NO. **Uncommitted.**
**Provenance:** HEAD `8088ec28c1e1138cfc3d3dbe3e522170b80f84d3`, branch `blueprint-emergence-updates`; only compiled test binaries were dirty at start.

## Root cause
`SceneActivityProbe::onStaticFrame` ran `tracker.onStatic()` on every static frame, but the pending-readback drop and comparison-grid reset (`clearStream`) ran only after the sampling-period rate limit (`now - lastIssueT < period` → return). A readback issued ≤ one period before the first static frame therefore stayed pending, and `poll()` (which also runs in the static branch) could complete it and call `tracker.onSample`, flipping Transition → Valid mid-transition. Eligibility window: from the first static frame until the first rate-limit-passing static frame (up to ~3 frames at 30 fps). Not reproduced in ~40 earlier real transitions; closed preventatively.

## Change (production)
- `SceneActivityCore.h`: new OF-free `StaticBoundary` (`enter()` true once per static interval; `exitOnLive()`; `resetForGeneration()`; epoch; `eligible(epoch)`).
- `SceneActivityProbe.cpp`: first static frame of an interval now immediately does `tracker.onStatic()` + drops the pending readback + forgets the comparison grid, independent of the rate limit; later static frames do no stream/GPU work (only the rate-limited `static` CSV row). A live frame ends the interval (next grid is a fresh baseline). A pending carries `(gen, epoch)`; `poll()` publishes only if the generation matches (existing guard, unchanged) **and** `StaticBoundary::eligible(epoch)` (new, defence in depth). Counters `staticFrames` / `staticEntries` added to the 10 s stats line.
- Unchanged: snapshot, `scene.visual.activity`, 64×36 grid, fixed-log mapping, HUD smoothing, region #6, DEC-020, generation guard, existing verification logging.

## Regression test
`visual_activity_tracker_tests.cpp::staticBoundaryRegression` (deterministic, GL-free): active generation + pending readback → static entry → pending dropped and grid ineligible immediately; a stale copy completing late is discarded and the tracker stays Transition; 20 further static frames cause 0 additional resets; new generation → first frame is baseline (no Valid), later frames Valid; a generation-2 pending is rejected after generation 3 (guard intact); a live frame re-arms exactly one reset per new interval. Negative control: with the old behaviour substituted, the test fails (pending still active, discard 0, snapshot not Transition).

## Results
| | |
|---|---|
| `visual_activity_tracker_tests` | all passed |
| `scene_activity_core_tests` | all passed |
| sketch aggregate `make -f Makefile.tests test` | exit 0, 22 suite lines pass, 0 failures |
| HUD: 57 + 1127 + 323 + 357 + 14 + 258 + 144 | all pass |
| `make Release` (experience_runtime) | success |

## Real desktop confirmation (post-fix, real ExperienceRuntime)
A first attempt was blocked (every launch failed with `ofAppGLFWWindow: 65537: The GLFW library is not initialized` — no window-server access from the shell); a later launch worked and the runs below were taken on the Release build that contains the fix. Setup: real scene auto-cycle at the 10 s minimum (`EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS`), `EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY=1`, `EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY_LOG=1`, probe CSV directory. Evidence: [oc-004-static-boundary-evidence/](oc-004-static-boundary-evidence/). Single run each.

| Run | Readback deferral | Scenes reached | `NextScene` accepted | `scenechange` rows | `DISCARD … reason=static-entry` |
|---|---|---|---|---|---|
| r5 | default (fence) | 7 generations | 6 | 7 | 0 (no readback was pending at any transition) |
| r6 | 2 frames | 10 generations | 9 | 10 | 7 |
| r7 | 30 frames (stress) | 10 generations | 9 | 10 | 9 |

Invariant checks over the per-frame log (`VisualActivityHud`) and probe CSV:
- **No Valid during static:** frames with `live=0` reported `transition` only (r5 9, r6 18, r7 18); **0** with state Valid. No valid/wide CSV row between a `static` row and the next `scenechange` in any run.
- **Immediate invalidation + pending discard:** where a readback was pending at static entry it was discarded once, at entry (`static-entry`, 7 of 9 transitions in r6, 9 of 9 in r7). The older `static-frame` reason no longer appears.
- **One reset per interval:** the last periodic stats line (printed every 10 s, so it lags the end of the run) shows `staticFrames`/`staticEntries` = 78/6 (r5), 104/8 (r6), 104/8 (r7): many static frames, one entry per transition. Repeated static frames do no GPU/stream work.
- **First-frame baseline, no cross-generation measurement:** `scenechange` is always followed by `first` with no raw value in all three runs.
- **HUD smoothing reset / region #6:** 0 state↔gauge disagreements (Valid shows a value; Stale/Unavailable/Transition show the placeholder). In r5 and r6 the gauge on the first Valid frame of each new generation equals the unsmoothed level (6/6, 9/9): no ramp from the previous scene. r7 never reached Valid (30-frame deferral exceeds the stale limit), so it adds discard/transition evidence but no smoothing-seed evidence. Screenshots `s12.png` (mid-transition) and `s34.png` in the evidence folder.
- **Generation protection intact:** unchanged code path; covered by the unit test. The `completion-gen-mismatch` and `completion-static-epoch` branches remain unreachable on the real path (the pending slot is already dropped at static entry / generation change), so they are covered by the unit-level driver only.

Limits: desktop only; single run each; r5 exercised no pending-at-transition case; the stats counters are read from the last 10 s print. The unit-level driver models the probe's boundary logic; the GL probe itself is exercised only through these runs.

## Remaining hardware blockers (unchanged)
Pi acceptance thresholds, cadence, stale-period, GLES float-target decision, FX-colour budget, supervised Pi session. Hardware-qualified: NO. Production-enabled: NO.
