# ROCK Pi Color, Activity & Memory Investigation — Workstreams A and B

**Status:** Workstream A: source attribution, instrumentation correction and desktop controlled measurements done; **Rock Pi attribution not done**. Workstream B: source ownership review and a supervised plan only; **nothing measured**.
**No hardware was touched by this session** (the agent may not SSH/deploy). The Architecture notice refers to interim ROCK Pi evidence; **that evidence was not supplied to this session**, so no claim below rests on it. Where the Pi evidence would decide something, that is stated.

---

## Workstream A — GPU readback attribution

### A1. Stages, and what each really measures (source)

`SceneColorSampler` (Scene Color / RAW Video Color) and `SceneActivityProbe` (Scene Visual Activity) share the same pattern: reduction draw → readback → CPU collect → publish.

| Stage | Scene Color / RAW (`SceneColorSampler.cpp`) | Activity probe (`SceneActivityProbe.cpp`) | Cost class |
|---|---|---|---|
| Submission | `submitLinear` (2 passes) → `stats_.submit` | `submitGrid` (+ diff passes in gpu/verify) → `submit` | CPU, ~0.03–0.06 ms desktop |
| GPU completion | invisible to the CPU unless a fence is used | same | wall latency, **not** CPU time |
| CPU blocking | `collectLinear` → `readToPixels` (= blocking `glGetTexImage`) → `wait` | `complete()` → `grid.readToPixels` / `diff2.readToPixels` → `wait` | blocks until the GPU is done |
| Collection | CPU average → `convert` | CPU diff → `diffCpu` | CPU, µs |
| Publication | `series_.push` → `push`, plus snapshot rebuild (`sceneColorPublisher_`) | CSV only (not published) | CPU, µs |

Before this change the two lumped GPU completion latency into CPU blocking, and (activity only) the stats were not comparable across modes, see A2.

### A2. The GPU verify-mode instrumentation discrepancy (found, corrected)

`SceneActivityProbe::complete()`, verify mode (`needCpu` and `needGpu` both true):

1. `wait` is recorded **only** around the CPU grid read (`grid[slot].readToPixels`); the GPU 1×1 read (`diff2.readToPixels`) was timed only when `!needCpu` (i.e. gpu-only). In verify mode **the GPU read was not timed at all**.
2. The CPU grid read comes first and absorbs the GPU completion wait for *both* paths; by the time the 1×1 read runs the work is done. Per-stage timings in verify mode therefore cannot be used to compare the CPU and GPU paths: the CPU path is charged for the GPU's completion, the GPU path looks free.
3. `total` was inconsistent: sync mode added `submit + complete`, deferred mode added only the `complete` portion (poll), omitting submit.
4. With `GPU_TIMER=1`, the `GL_QUERY_RESULT` read blocks until the GPU finishes and lands inside the `wait` window.

**Correction (diagnostic only, no contract):** new `gpuRead` stat times the 1×1 read in every mode; deferred `total` now = submit + collect; new `latency` (issue → fence observed signalled), `notReady`, `forced`. **Verify-mode timings should be read only as: total, correctness delta, and `gpuRead`; not as a CPU-vs-GPU path comparison.** Desktop check: verify `gpuRead` p50 0.09–0.12 ms, `maxGpuCpuDelta` ≤ 6.3e-8.

The same applies to any Pi verify-mode run already collected: its `wait` is the CPU-grid read and says nothing about the GPU diff path.

### A3. Global frame-time distributions

Percentiles are now computed over **every frame interval** (`frames.csv`), pooled across repeats, by `oc-004-evidence/scripts/global_frame_stats.py`. It also prints the mean of the one-second WINDOW p95s for comparison. A mean of window percentiles is not the run's percentile: it is dominated by how many windows contain a hiccup and understates (or, for sparse spikes, overstates) the tail.

### A4. Controlled desktop comparison (not Pi evidence)

macOS, AMD Radeon Pro 560X, GL 2.1 (sync objects present), Temporal, limiter 30 fps + vsync, Scene Color OFF (probe measured alone), probe at 10 Hz, 3 interleaved repeats × 40 s per arm, same build, scene, media, resolution. Load average 1–5 at run start (never idle; another session active). Raw data: `oc-004-evidence/wsa/`.

| Arm (probe mode / retrieval) | frames | p50 | p95 | p99 | p99.9 | max | >50 ms | >100 ms | mean(window p95) |
|---|---|---|---|---|---|---|---|---|---|
| base (probe off) | 3120 | 33.31 | 34.16 | 34.52 | 50.4 | 51.9 | 4 | 0 | 34.43 |
| cpu, frames:2 | 3120 | 33.33 | 34.33 | 34.79 | 48.9 | 65.3 | 3 | 0 | 34.83 |
| cpu, **fence** | 3127 | 33.33 | 34.36 | 34.87 | 51.7 | 56.4 | 6 | 0 | 35.14 |
| gpu, frames:2 | 3125 | 33.35 | 34.29 | 34.68 | 42.7 | 47.3 | 0 | 0 | 34.65 |
| gpu, **fence** | 3130 | 33.34 | 34.27 | 34.72 | 52.5 | 57.3 | 6 | 0 | 34.86 |
| verify, frames:2 † | 3033 | 33.28 | 34.51 | 35.75 | 310 | 191 665 † | 11 | 7 | 33.03 |
| cpu, **sync** | 3104 | 33.28 | **43.53** | **46.73** | **192** | 763 | 24 | 9 | 48.32 |

† verify repeat 1 contains a 191 s gap in the app's own timeline followed by a 3.8 s frame: the process was not scheduled (machine-level, not the probe). Without that run, verify is p95 34.5 / p99 35.7 (the remaining >100 ms frames are in the same disturbed run). Treat the verify row as contaminated.

Probe-internal stages (p50 / p95 ms; per-repeat ranges):

| Arm | submit | wait (CPU block) | gpuRead | latency (issue→signalled) | total | notReady | forced |
|---|---|---|---|---|---|---|---|
| cpu frames:2 | 0.030 / 0.037 | 0.24–0.26 / 0.33 | n/a | n/a | 0.36–0.40 / 0.50 | n/a | n/a |
| cpu fence | 0.031 / 0.041 | 0.20–0.25 / 0.32 | n/a | 33.3 / 34.0–34.5 (max 299, one) | 0.34–0.39 / 0.50 | ≈ 1/sample | 0 |
| gpu frames:2 | 0.045–0.047 / 0.06 | 0.19–0.21 / 0.28 | 0.19–0.21 | n/a | 0.31–0.34 / 0.44 | n/a | n/a |
| gpu fence | 0.049 / 0.061–0.065 | 0.19–0.22 / 0.30–0.32 | 0.19–0.22 | 33.3 / 34.1–34.2 | 0.34–0.36 / 0.44–0.47 | ≈ 1/sample | 0 |
| cpu sync | 0.03 / 0.04–0.06 | **7.6–9.1 / 10.2–11.5** | n/a | n/a | 7.7–9.2 / 10.4–11.7 | n/a | n/a |

**Attribution (desktop).**
- With deferred retrieval the probe is invisible in the global distribution: p50–p99 are within 0.3 ms of base for cpu/gpu × frames/fence (the p99.9/max differences are the same size as base's own 4 isolated >50 ms frames, i.e. ambient noise).
- Sync retrieval is the only arm that moves the tail (p95 +9.4 ms, p99 +12 ms, 9 frames > 100 ms vs 0). Here the CPU-blocking time (`wait` 8–9 ms) is the entire cost; submission and collection are ≤ 0.06 ms.
- Fence retrieval: completion latency is exactly one frame (33.3 ms) because the poll granularity is a frame; blocking is 0.2 ms, which is only the post-completion transfer; `forced` = 0.
- On this machine the old 2-frame deferral already hides the GPU completion, so **the desktop cannot reproduce a deferred stall**; it can only confirm that the fence variant is no worse.

**Interpretation limits.** Machine never idle; repeat counts are small (3); the 30 fps limiter hides sub-33 ms headroom changes; GL2.1 on AMD/Metal-backed macOS is not Panfrost.

### A5. Root cause (what is and is not established)

Established from source (this session, plus the OC-003 report): the Deferred stall is a **blocking `glGetTexImage` issued on a frame-count, not a completion test**; the existing Async path is unreachable on a GL 2.1 context. Stage costs other than the blocking read are ≤ 0.06 ms on the desktop and 0.5 ms on the Pi (supplied OC-003 numbers).

**Not established:** whether the Pi's ~28 ms is GPU lag (completion latency) or a driver that blocks inside `glGetTexImage` even on a finished target. The two predict different outcomes of the fence arm (OC-003 §5): fence `latency` ≈ old wait with `wait` ≈ 0.2 ms ⇒ GPU lag, fixed by the fence; `wait` still ≈ 28 ms ⇒ read-path blocking, which only a different retrieval can fix. Resolving this needs one supervised Pi run with the instrumentation above; nothing in the desktop data decides it.

### A6. Smallest justified correction

1. **Instrumentation fix** (A2): justified by the source defect itself, independent of hardware.
2. **Fence-polled, bounded retrieval for the activity probe** (same 250 ms bounded fallback, same strategy selection `EXPERIENCE_RUNTIME_SCENE_ACTIVITY_DEFER=auto|fence|frames[:N]`) so all three consumers use the same mechanism as the Scene Color change from OC-003. Justified only as consistency + non-regression; **its benefit on Panfrost is unproven.** Activity remains diagnostic-only and not published.

Preserved: valid samples (done == issued in every fence arm; the 1–2 "done = issued − 1" cases are the final in-flight sample at kill time), stale-generation rejection (pending fence is released on a generation change and on `clearStream`), bounded history, and all immutable contracts. FX and RAW defaults unchanged. No HUD change; no public instrumentation contract change (stat lines are logs).

### A7. Supervised Pi attribution run (operator)

Same build for all arms; Temporal and Blob; identical media, display resolution and 2 Hz FX sampling; 120 s × 3 interleaved repeats. Arms: base; FX sync / frames:2 / frames:4 / fence / fence+flush; RAW (video colour) frames:2 / fence; activity cpu / gpu × frames:2 / fence. **One consumer at a time, then the combined production-like set.** Log first: `deferred readback: syncObjects=…`. Record per arm the stage table in A4 plus global percentiles from `frames.csv` (script above) and the decision rule from A5. Stop on any crash, new GL error, or frame > 1 s outside scene/media changes.

---

## Workstream B — Heavy-scene memory ownership (source review; not measured)

### B1. What the source shows

- **Temporal release on deactivate exists and is the default**: `TemporalProductionScene::deactivate()` → `releaseForDeactivation()` → `TimeOffsetVideoBuffer::releaseForInactive()`, which resets CPU history + playhead textures and does pause-then-close of the dedicated decoder (`shared/src/TimeOffsetVideoBuffer.cpp:127-148`). `EXPERIENCE_RUNTIME_TEMPORAL_INACTIVE_RELEASE=retain` is the rollback toggle. It emits a `temporal.releaseForInactive` NOTE with before/after history frames, playhead texture count and decoder state.
- Activation forces a full reload and history reset (TEMP-004), so release-and-rebuild is the designed cycle.
- **No allocator trimming exists** (`grep` for `malloc_trim`/`mallopt` finds nothing in `shared/src` or the sketch). After the history frames are freed, glibc may keep the arena pages, so **RSS can stay high while the memory is reusable** (retained-cache behaviour, not a leak). This is the leading candidate for "growth" that plateaus; it is a hypothesis.
- Only counters that already exist can separate "unreachable" from "retained": history frame count/bytes, playhead texture count/estimate, decoder loaded flag (the release NOTE), plus `RssAnon`/`RssFile`/`RssShmem` and Temporal residency in the frame-phase `WINDOW` lines.
- Fragment has no equivalent per-activation release counters in what I inspected; its attribution needs Fragment Trail input (not inspected for lifetime).

### B2. What cannot be answered without hardware

Whether repeated Temporal/Fragment activations grow memory without bound, and which subsystem owns any growth. No claim is made. No fix is proposed: the owning subsystem is not identified.

### B3. Supervised bounded sequence (operator)

1. **Diagnostics off:** `EXPERIENCE_RUNTIME_SCENE_COLOR=0`, `EXPERIENCE_RUNTIME_VIDEO_COLOR` unset, `EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE` unset.
2. Harness: `EXPERIENCE_RUNTIME_THREE_SCENE_HARNESS=1` (10 circuits / 30 switches; RSS is read correctly on Linux) with `EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE=1 …_WINDOW_MS=1000` and the stall probe for the release NOTEs. Capped by `timeout 1800`. Run it **twice back to back in one session** and once with `…INACTIVE_RELEASE=retain` as the control.
3. External read-only 1 Hz sampler of `/proc/<pid>/smaps_rollup` (Pss, Pss_Anon, Rss, Private_Dirty), `/proc/meminfo` (MemAvailable, Cached), and `/proc/<pid>/status`. The Pi 3B `memattr_supervisor.py` thresholds are VC4/CMA-specific and **not valid on the Rock Pi** without an Architecture-approved profile.
4. Tabulate per circuit: RSS, RssAnon, PSS, MemAvailable, release NOTE (history frames/MiB, playhead textures), scene FBO allocation count (the three-scene harness reports it). **Pass criteria (proposed):** release NOTEs show history/playhead/decoder reaching zero after each deactivate; RssAnon after circuit N+1 within ±5 % of circuit N after warm-up (plateau); PSS sawtooth with a flat floor.
5. **Distinguishing retained vs unreachable:** if RSS plateaus but does not fall after release while the counters read zero, it is allocator retention (a narrow, owner-specific proposal could be a bounded `malloc_trim` after release, in the owning buffer, only if the plateau materially lowers MemAvailable). If counters themselves grow, the owner is the subsystem whose counter grows.
6. Stop on any of: MemAvailable < 256 MiB, monotonic RssAnon growth across three consecutive circuits, playback stall > 1 s outside a switch, GL errors.

The six-playhead Temporal architecture and scene/media authority are untouched by this plan.

---

## Deferred / out of scope (unchanged)
Production RAW + FX enablement, production Scene Visual Activity publication, predominant/vibrant extensions, HUD geometry/interaction, new public contracts: none touched.

## Completion report

1. **Summary** — A: stage attribution from source; instrumentation defect in verify mode corrected; global frame-time script; controlled desktop comparison showing sync is the only arm that moves the tail and deferred/fence are invisible; fence retrieval added to the activity probe. Pi root cause still undetermined. B: source ownership review and supervised plan; no measurement, no fix.
2. **Files inspected** — `SceneColorSampler.h/.cpp`, `SceneActivityProbe.cpp`, `ExperienceRuntime.cpp` (hooks/stats), `TemporalProductionScene.cpp`, `shared/src/TimeOffsetVideoBuffer.cpp`, `TimeOffsetPlaybackAdapter.h`, validation runbook.
3. **Files changed** — `SceneActivityProbe.cpp` (this task: `gpuRead`/`latency`/`notReady`/`forced` stats, consistent deferred `total`, fence retrieval, `DEFER` env); earlier OC-003 changes to `SceneColorSampler.h/.cpp` and the stats line in `ExperienceRuntime.cpp` remain; new: `oc-004-evidence/scripts/global_frame_stats.py`, `wsa.sh`, `oc3.sh`, `run6.sh`, `oc-004-evidence/wsa/`, `oc-004-evidence/oc3/`, this report.
4. **Tests/builds run** — `make Release` (passing); 21 controlled desktop runs (3 repeats × 7 arms); the earlier OC-003 12-run matrix is archived beside. Existing unit suites were not run.
5. **Results** — A4.
6. **Deviations** — No Pi work and no use of the interim Pi evidence (not provided). Verify repeat 1 contaminated by a process-suspension gap (reported, not hidden). Machine not idle. The activity probe change goes slightly beyond "instrumentation": retrieval strategy added for consistency, flagged as unproven on Panfrost.
7. **New risks** — A suspended or heavily loaded desktop can fabricate multi-second frame gaps; pooled percentiles need such runs flagged. The `auto` fence default is untested on a quiet machine or on Panfrost. B: possible allocator retention may be misread as a leak.
8. **Contract changes requested** — None.
9. **Recommended next step** — Operator-run A7 (one consumer at a time) and B3, in separate sessions; decide fence vs frames:N vs reduced cadence from the A5 decision rule.

```text
WORK ITEM: ROCK Pi Color, Activity & Memory Investigation (Workstreams A and B)
PREVIOUS STATUS: Authorized (Architecture)
NEW STATUS: A — desktop attribution + instrumentation correction done, Pi attribution outstanding; B — plan only. Proposed: IN PROGRESS (not ACCEPTED/CLOSED)

WHAT CHANGED: activity-probe instrumentation corrected; fence retrieval added to the probe; global frame-stat tooling; reports.
WHAT WAS PROVEN: sync retrieval alone degrades the global tail on desktop (p95 +9.4 ms, p99 +12 ms); deferred (frames or fence) is within 0.3 ms of base; verify-mode per-stage timings were not comparable (defect, fixed); fence completes every sample within one frame, forced fallback never used.
WHAT WAS NOT PROVEN: Pi root cause (GPU lag vs driver blocking in glGetTexImage); any Pi benefit of fence; any memory-lifetime claim for Temporal/Fragment.

NEW EVIDENCE: oc-004-evidence/wsa/, oc-004-evidence/oc3/, this report.
NEW RISKS: see 7.
NEW BLOCKERS: Pi supervised sessions (A7, B3).

NEXT ACTION: operator-run A7 then B3.
NEXT GATE: Pi attribution against the 5/10/1 ms targets; memory plateau criteria.

REGISTRY UPDATE REQUIRED: YES
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: NO
```
