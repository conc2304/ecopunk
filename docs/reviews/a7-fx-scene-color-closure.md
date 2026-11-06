# A7 — FX Scene Color Sampling Closure

**Status:** Phases 0–2 done (code, unit + desktop integration validation). **Phase 3 (ROCK Pi) not executed** — this agent may not SSH/deploy; a supervised protocol is below. Phase 4 can therefore only be evaluated for the targets that do not need the Pi.
**Recommendation (Phase 5): option 3 — FX remains disabled pending a specific blocker: the matched ROCK Pi qualification (Phase 3) has not been run.** No FX enablement is proposed on desktop evidence.

## 1. Repository provenance (Phase 0)

- HEAD `03b3bb2a06bc4e3cef03b6057e8caff55c085ec4`. Large pre-existing dirty tree (≈100 modified/untracked paths: HUD compositor, Orbit Console, video-playback, fragment-trail, experience_runtime scenes, OC-003/004 docs). The sampler files (`SceneColorSampler.*`, `SceneColorSeries.h`, `SceneColorSamplerDiag.cpp`, `SceneActivityProbe.*`) are **untracked**, i.e. they have no committed baseline; "before" below is the code read at the start of this session.
- Unrelated concurrent edits left untouched. Separately, earlier in this session (different task) I touched `RadarStationWidget.*`, `HudOverlayLayer.*`, `fragment-trail/FragmentSceneCore.cpp` (radar wedge colour); not part of A7.
- Benchmark infrastructure found: `docs/reviews/oc-004-evidence/scripts/` (`global_frame_stats.py`, `run6.sh`, `wsa.sh`, `oc3.sh`), `oc-004-pi-test-plan.md`, `ExperienceRuntime-RockPi-Validation-Runbook.md`. Existing fence evidence: OC-003 desktop 12-run matrix + OC-004 WSA (desktop only, fence invisible in the global distribution, latency p50 33.3 ms = one frame, `forced=0`). **No Pi fence evidence exists.**

## 2. Deferred-readback lifecycle: before / after

| Step | Before | After |
|---|---|---|
| Create | `glFenceSync` after the reduction; may return null (unchecked) | same; a null fence is treated as a failed request |
| Poll | `glClientWaitSync(…,0)`; **anything other than SIGNALED was "not ready"**, including `GL_WAIT_FAILED` | Mapped: SIGNALED/CONDITION_SATISFIED → ready; `GL_TIMEOUT_EXPIRED` → not ready; **`GL_WAIT_FAILED`/unknown → failed** |
| Not ready | return (non-blocking) | same |
| Timeout (250 ms) | **forced a blocking `glGetTexImage`** (`forcedBlocking++`) | **request discarded, fence released, explicit gap recorded; no read** |
| WAIT_FAILED | looped as not-ready until 250 ms, then forced blocking read | discarded immediately, fence released, gap recorded |
| Pending bound | one `pending_` + 2 slots | unchanged, now enforced by the gate (`tryIssue`/`issued`) |
| Generation change | `beginGeneration` released fence, cleared pending; poll re-checked | same, plus the gate rejects a mismatched-generation request before it looks at the fence |
| Result validity | empty → gap; NaN/inf not checked | additionally non-finite → gap (`invalidSamples`) |
| Repeated failure | none (every sample could stall) | 3 consecutive failures disable scene-colour telemetry only; scene rendering is never touched |
| Recovery | n/a | after 30 s one probe request; success re-enables, failure re-arms the cooldown; `reenableTelemetry()` is the explicit path |
| Shutdown | fences leaked | `shutdown()` (called from `ExperienceRuntime::exit()` while GL is alive) |

Series semantics are unchanged: a failure is a **gap** (`valid=false`, no value), never a zero and never a copy of the previous sample; stale is still decided by the age of the last valid sample; valid-black is still a valid zero. `SceneColorSnapshot` is untouched.

## 3. Files changed

- `sketches/experience_runtime/src/SceneColorDeferredGate.h` **(new, OF-free)** — pure decision core (single-pending, timeout→discard, WAIT_FAILED→discard, generation→discard, failure threshold, cooldown/probe, manual re-enable).
- `sketches/experience_runtime/src/SceneColorSampler.h/.cpp` — GL half delegates to the gate; `forcedBlocking` removed; `discardedTimeout`, `discardedWaitFailed`, `invalidSamples`, `telemetryDisabledSkips` added; `shutdown()`, `telemetryDisabled()`, `reenableTelemetry()`, `gateStats()`; diagnostic fault injection `EXPERIENCE_RUNTIME_SCENE_COLOR_FAULT=waitfailed|stuck` (validation only, inert when unset).
- `sketches/experience_runtime/src/ExperienceRuntime.cpp` — stats line fields (`discardTimeout`, `discardWaitFailed`, `invalid`, `telemetryDisabled`, `disabledSkips` replace `forcedBlocking`); `exit()` calls `shutdown()` on both samplers.
- `sketches/experience_runtime/test/scene_color_deferred_gate_tests.cpp` **(new)** + `Makefile.tests` target `scene-color-gate` (added to `test`).
- This report.

Not changed: `SceneColorSnapshot`, Orbit Console, HUD-004/006, Shared Video selection, Temporal/Fragment playback, `SceneActivityProbe` (diagnostic only; it still has its own 250 ms forced blocking read, see risks).

## 4. Tests executed

| Suite | Result |
|---|---|
| `scene_color_deferred_gate_tests` (new) | **85/85** — signaled; unsignaled never blocks / nothing published; timeout → discard, no collect; `GL_WAIT_FAILED`; capacity (one pending, refusals counted); resource cleanup (4 requests/4 releases across collect, wait-failed, timeout, generation; shutdown); generation replacement; invalid publication (valid-black vs gap vs stale, no copy-forward, snapshot flags); failure threshold; streak reset; telemetry-disabled (no fences created); recovery (cooldown, single probe, failed probe re-arms, success re-enables, manual re-enable, generation drop during probe) |
| `scene_color_series_tests` | 47/47 |
| `make test` (experience_runtime/test, all suites incl. snapshot/semantic/settings) | all pass |
| `hud-compositor-test test-all` (orbit, real-frame, viewport, typography, vitals) | all pass |
| `make Release` experience_runtime | passes |
| Desktop smoke, deferred+fence, 45 s | 2 Hz (≈20 issued/10 s), `done == issued`, `discard*=0`, `invalid=0`; wait p50 0.26 ms, submit p50 0.04 ms, total p95 0.38 ms; latency p50 33.3 ms (one frame) |
| Desktop fault injection, 40 s each | `waitfailed`: 3 discards → 3 gaps → telemetry DISABLED (log), then 56 skips, one probe at +30 s fails and re-arms; `stuck`: 3 timeouts at 250 ms (not collected), DISABLED, 55 skips; app kept rendering in both |

Not run: the GL-side `poll()` mapping for a *naturally* occurring `GL_WAIT_FAILED` (cannot be provoked on desktop); it is a 3-line mapping reviewed by reading, and exercised via the injection hook.

## 5. Phase 3 — matched ROCK Pi protocol (operator-run; not executed)

Same binary, media ID, scene/mode, output resolution, 2 Hz cadence, diagnostics (frame-phase probe + `SCENE_COLOR_STATS=1`) in every condition; 30 s warm-up discarded; ≥3 interleaved repeats × 120 s; nothing else running; no builds.

| Cond. | Env |
|---|---|
| A disabled | `EXPERIENCE_RUNTIME_SCENE_COLOR=0` |
| B sync | `…SCENE_COLOR=1 …SCENE_COLOR_MODE=sync` |
| C deferred-fence | `…SCENE_COLOR_MODE=deferred` (auto → fence; confirm the log line `strategy=fence`) |

Collect per condition: global p50/p95/p99/worst (`global_frame_stats.py` over `frames.csv`, not mean-of-window-p95); mean draw CPU and delta vs A; stage stats from the 10 s line (`submit`, `wait`, `total`, `latencyMs`); `done`, `invalid`, `discardTimeout`, `discardWaitFailed`, `telemetryDisabled`; GL errors; RSS/PSS. Correlate long frames with sample timestamps. Stop on any crash, new GL error or frame > 1 s outside a scene change.

## 6. Phase 4 — targets (status)

| Target | Status |
|---|---|
| Sampler p95 ≤ 5 ms / worst ≤ 10 ms (main thread) | **Unmeasured on Pi.** Desktop: p95 0.38 ms, worst 0.40 ms **except one startup sample: submit 15.5 ms (lazy shader compile on first use) and 330 ms completion latency**. On the Pi this first-sample cost will likely breach "worst ≤ 10 ms" unless excluded by definition or pre-warmed; a decision is needed from Architecture/HUD |
| Mean draw CPU increase ≤ 1 ms | Unmeasured on Pi |
| No recurring stalls | Unmeasured on Pi; desktop shows none |
| 2 Hz | Met (desktop) |
| Invalid/stale/generation correct | Met (unit + injection) |
| Render-thread blocking fallback: none | **Met for the fence strategy.** Not met for the legacy frame-count strategy (still a blocking `glGetTexImage` after N frames), which is used only when sync objects are unavailable or `…DEFER=frames` is forced |

## 7. Remaining risks

1. **No Pi evidence for fence** (the root-cause question from the A5 decision rule — GPU lag vs. driver blocking in `glGetTexImage` — is still open).
2. `strategy=frames` still blocks. `chooseDeferStrategy` reports sync objects unavailable under `TARGET_OPENGLES` regardless of the driver, so a GLES build silently gets the blocking strategy. If the Rock Pi build is GLES, this is a blocker, and the safe answer there would be "telemetry off", not "frames".
3. First-sample warm-up (15 ms submit, 330 ms latency; the latter is fine, the former may hit the worst-case target).
4. `videoColor_` (RAW) shares `SceneColorSampler`, so it inherits this behaviour; it is **not** qualified by this work.
5. `SceneActivityProbe` keeps its own forced blocking read (diagnostic-only, unpublished, default off).
6. Disabled telemetry just goes Stale/Unavailable; the HUD shows it through the existing state, with no new "disabled" indication (no contract change made).
7. The 30 s cooldown/3-failure threshold are engineering choices, not specified numbers.

## Completion report

1. **Summary** — Blocking fallback removed; WAIT_FAILED/timeout/generation discard with explicit gaps; bounded pending; telemetry-only disable with explicit recovery; unit + desktop validation. Pi qualification outstanding.
2. **Files inspected** — `SceneColorSampler.h/.cpp`, `SceneColorSamplerDiag.cpp`, `SceneColorSeries.h`, `ExperienceRuntime.cpp` (hooks/stats/exit), test Makefile, OC-003/OC-004 reports and scripts.
3. **Files changed** — §3.
4. **Tests/builds** — §4.
5. **Results** — §4, §6.
6. **Deviations** — Phase 3 not executed (no SSH/deploy permitted); fault-injection env hook added (diagnostic, not requested); `ExperienceRuntime::exit()` and the stats line touched.
7. **New risks** — §7.
8. **Contract changes requested** — None. Stats-line field names changed (log only).
9. **Recommended next step** — Operator-run §5; Architecture to decide the first-sample warm-up treatment and the GLES policy (item 2) before any enablement decision.

```text
WORK ITEM: A7 FX Scene Color Sampling Closure
NEW STATUS: Phases 0–2 complete; Phase 3 outstanding (Pi). Proposed: IN PROGRESS (not ACCEPTED/CLOSED)
RECOMMENDATION: FX remains disabled pending a specific blocker (Pi qualification not run)
REGISTRY UPDATE REQUIRED: YES
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: YES (GLES policy; first-sample warm-up treatment)
```
