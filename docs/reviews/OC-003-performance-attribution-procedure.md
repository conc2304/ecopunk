# OC-003 — Performance attribution: instrumentation and P0/P1/P2 procedure

Diagnostic only. No production default, contract, HUD or skin change. Hardware runs are NOT started by this work.

## 1. Instrumentation (all env-gated, inert unless the frame-phase probe is on)

| Item | Where | Notes |
|---|---|---|
| Per-frame CSV stream | `EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES=<path>` (requires `EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE=1`) in `Rt004FramePhaseProbe` | One row per frame, buffered `FILE*` (64 KiB, flush ~1 s). Nothing is retained in memory. |
| Columns | `frame, mono, total_ms`, then per segment (`update, preDraw, draw, glFinish, postDraw`) `_wall,_cpu,_vcsw,_ivcsw`, then `scene, phase, media, ex_startup, ex_media, ex_scene, ex_transition, exclude` | `draw_cpu` is main-thread CPU (CLOCK_THREAD_CPUTIME_ID); `vcsw/ivcsw` (Linux only, -1 on macOS) are the blocking / preemption indicators. `glFinish_*` is non-zero only with `..._GLFINISH=1`. |
| Exclusion markers | `..._WARMUP_S` (default 30), `..._SETTLE_MS` (default 3000) | startup; within settle after canonical-media change; within settle after scene-owner change; transition phase != Idle. `exclude=1` if any. |
| Sampler log alignment | `SceneColor` / `VideoColor` 10 s stat lines (`..._SCENE_COLOR_STATS=1`, `..._VIDEO_COLOR_STATS=1`) | Added `stream=FX|RAW`, `mono=` (same clock as the CSV), `effMode=` (mode actually used), `gpuLinear=`; RAW line also gained `notReady`, `discardTimeout`, `discardWaitFailed`, `invalid`, `latencyMs`. |
| Aggregation | `scripts/oc003/aggregate_frames.py` | Pools individual frames of all files per condition; nearest-rank p50/p95/p99/worst, mean, mean draw CPU, count > threshold (default 100 ms). Window percentiles are never averaged. |
| Planner | `scripts/oc003/plan_interleaved.py` | Prints the interleaved command list. Runs nothing. |

## 2. Sampling attribution — evidence audit (existing stats)

| Gate | Covered by | Status |
|---|---|---|
| Main-thread sampler p95 / worst blocking | `stageMs submit/wait/convert/push/total` p50/p95/max (FX and RAW) | Sufficient. Ring = last 256 samples (~2 min at 2 Hz); `max` is cumulative since start. |
| GPU submission / retrieval wait | `submit`, `wait` | Sufficient |
| Result latency | `latencyMs` (fence mode) | Sufficient; RAW line added |
| Forced blocking fallback | None exists by A7 design (no blocking fallback). Only `effMode=sync` (capability fallback) is a blocking path | Now visible via `effMode` |
| Fence failures | `discardTimeout`, `discardWaitFailed`, `notReady` | Sufficient; RAW line added |
| Drops, gaps, generation rejection | `droppedGen`/`dropGen`, `busy`, `gaps` | Sufficient |
| Validity | `invalid`, `domValid`, `vibValid`, `vibInvalid` | Sufficient |

Cumulative counters and `max` include warm-up and transitions. For steady-state counters use the DIFFERENCE between the first stat line after warm-up and the last line, aligned by `mono`.

## 3. Controlled media playback

Shared Video has no public seek. Startup media was random (`VideoSelectionPolicy` shuffles with `std::random_device`); `EXPERIENCE_RUNTIME_VIDEO_DWELL` stops advancement but not the first pick. Resolved with a narrow verification control, no ownership change:

- `EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID=<canonical mediaId>` -> `VideoPlaybackService::Config::verificationStartupMediaId`. The service asks the policy for that catalog item (`VideoSelectionPolicy::requestSpecificCandidate`, read-only) and activates it through the normal `attemptSelection` / `confirmActivationSucceeded` path (origin Startup, same resume handling, generation and activation count).
- Unset/empty = unchanged random startup. An unknown, disabled or unloadable id is REJECTED visibly: health Failed, `media.error.verification_media_unavailable`, an error log listing the catalog, and NO Recovery fallback (one load attempt only). `list` is therefore a catalog lister.
- Verification runs force the resume trace on: the log carries `VideoResume: start media=<id> duration=.. class=.. start=<offset>s` and `VideoVerification: startup media ACTIVE id=.. file=.. size=WxH duration=..`.
- Initial position: the first visit of a clip starts at 0 s for short/medium clips; only a LONG clip can start at a saved resume offset (none exists on a first visit, and under the dwell pin every clip classifies short). So the activation start offset is 0 s and is recorded per run. Remaining mismatch: the actual playhead when the sampler first runs depends on decode start latency and process start time, and short clips loop (Snail: 26.894 s), so loop phase at any instant is not reproducible; over a 300 s run it covers ~11 loops in every condition.

## 4. Experiment (same binary, scene, media, resolution, `EXPERIENCE_RUNTIME_VIDEO_DWELL` pin)

| Cond | Environment (in addition to the common block printed by the planner) |
|---|---|
| P0 | `EXPERIENCE_RUNTIME_SCENE_COLOR=0` |
| P1 | `SCENE_COLOR=1 SCENE_COLOR_MODE=deferred SCENE_COLOR_DEFER=fence SCENE_COLOR_STATS=1` (FX AVG only; RAW and DOM/VIB off) |
| P2 | P1 + `EXPERIENCE_RUNTIME_PREDOMINANT_COLOR=1 EXPERIENCE_RUNTIME_VIDEO_COLOR=1 EXPERIENCE_RUNTIME_VIDEO_COLOR_MODE=deferred EXPERIENCE_RUNTIME_VIDEO_COLOR_STATS=1` |

Procedure: `python3 scripts/oc003/plan_interleaved.py --bin <binary> --out <dir> --media-id <id> --scene <scene id> --resolution <WxH> --rounds 4 --duration 300` (all three of media/scene/resolution required; writes `<dir>/plan.json` with the binary sha256) → run the printed commands in order (P0 P1 P2 repeated) → `python3 scripts/oc003/verify_run.py <dir>/plan.json` (must print VERIFIED: media, scene, resolution, binary hash, start offset) → `python3 scripts/oc003/aggregate_frames.py P0=<a.csv,b.csv> P1=... P2=... --sampler-log <stdout log>`.
Check before comparing: identical `media` and `scene` columns, `effMode=deferred gpuLinear=1 defer=fence` in P1/P2, `discardTimeout=0 discardWaitFailed=0`, equal frame counts within a few percent.

## 5. Uncontrolled variables

- Media loop phase and decode start latency (no seek control, see §3); the clip itself is now pinned.
- Thermal state / DVFS and background load on the board; interleaving spreads but does not remove them.
- Scene-internal state (blob scene) evolves with time since start; only steady-state rows after warm-up are compared.
- Probe overhead itself (CSV row per frame, buffered) is present in all three conditions, so it cancels in comparisons but is not zero.
- The 2 Hz sampling interval is the production constant; FX and RAW samples are not phase-locked to each other.
- macOS desktop numbers above (smoke run: 28 s) validate the tooling only; they are not Pi evidence.
