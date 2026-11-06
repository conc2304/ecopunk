# OC-004 — Scene Visual Activity Probe: Supervised Pi Test Plan

**Status:** DRAFT for operator review. Nothing in this plan has been run. I have not connected to any Pi, and I will not run it. Every step is for the operator to execute or to explicitly supervise.
**Companion to:** `oc-004-scene-visual-activity-probe.md` (desktop results, §5 Pi risks).
**Question this session answers:** can the 64×36 luminance activity probe run at ~10 Hz on the Pi without hurting frame time, and does it give the same signal as on the desktop?

Not covered here: HUD region #6 integration, contract ratification, any production code change.

## 0. Ground rules

- The operator runs everything. The agent does not use SSH, SCP, rsync, `systemctl`, or `sudo`. It does not deploy. The operator copies the build or builds on-device.
- Stopping `ecopunk-runtime` or `fireplace-waterfall` is an operator decision. Record what was stopped and restore it at the end.
- One GL app on the display at a time. Don't run the probe next to the interactive instance.
- Every run is capped with `timeout`. Stop at the first crash, GL error, or any run that pushes RSS or CmaFree past the limits in §3.
- Targets: **Pi 3B (VC4, GLES2) is the production target and the real gate.** The Rock Pi 4B (Panfrost, GL 2.1 context) is a cheaper dry run of the same steps, not a substitute.
- The probe is opt-in. With `EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE` unset it is inert. Confirm this once in step A.

## 1. Known risks the plan is designed to expose

| # | Risk (from desktop report §5) | Where it shows up | Step |
|---|---|---|---|
| R1 | The probe's 64×36 targets are `RGBA32F`. VC4/GLES2 may not support a float color attachment. | FBO incomplete, black grid, GL error | B |
| R2 | The luma pass reads up to 16×16 taps per cell. Fragment cost on VC4 may be many times the desktop's 0.1–0.2 ms. | Frame-time p95/p99 delta, `submit` stat | D |
| R3 | `glReadPixels` of 64×36×4 floats is cheap on desktop but may block on Pi. Deferred (2-frame) readback may not hide it. | `wait` stat, cadence loss | D |
| R4 | GPU diff path (8×6 → 1×1) needs float render targets and 1-texel readback. | Same as R1, plus `gpuCpuDelta` | C |
| R5 | GL timer queries do not exist on GLES. | Not measurable; use wall-clock only | n/a |
| R6 | Concurrent decode (v4l2/GStreamer) and probe contend for memory bandwidth. | Stalls during media change | E |
| R7 | 8-bit grid, if RGBA8 is the only option. | Raw level quantisation | C |
| R8 | CMA/GPU memory margin is already thin at Blueprint setup. The probe adds ~35 KB of textures, which is small but nonzero. | `CmaFree` timeline | F |

## 2. Preparation (operator, on the Mac first)

1. Rebuild the Mac binary from the working tree and run `test/scene_activity_core_tests.cpp` (bare compiler). It passes today.
2. Decide which target to start on: Rock Pi dry run first (recommended), then the Pi 3B.
3. Build on the device with `make Release -j4` from `sketches/experience_runtime`. Do not ship the Mac binary.
4. `mkdir -p ~/oc004` for logs. Use a fresh output directory per run: `~/oc004/<label>/`.
5. Free the display (operator), then `export DISPLAY=:0`.

Helper, same pattern as the validation runbook §I:

```bash
L=$HOME/oc004
P="EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE=1 EXPERIENCE_RUNTIME_FRAME_PHASE_WINDOW_MS=1000 EXPERIENCE_RUNTIME_VIDEO_DWELL=900,900 EXPERIENCE_RUNTIME_STALL_PROBE=1"
run(){ n=$1; shift; mkdir -p $L/$n; timeout ${T:-120} env $P \
  EXPERIENCE_RUNTIME_FRAME_PHASE_LOG=$L/$n/phase.log EXPERIENCE_RUNTIME_STALL_PROBE_LOG=$L/$n/stall.log \
  "$@" ./bin/experience_runtime > $L/$n/stdout.log 2>&1; echo "$n exit=$?"; }
```

Scene selection: `EXPERIENCE_RUNTIME_TEMPORAL_SCENE=1`, `_BLUEPRINT_SCENE=1`, `_FRAGMENT_SCENE=1`, `_QUADRANT_SCENE=1` (Blob is the default).

## 3. Abort limits (apply to every step)

Stop the run and report if any of these happens:

- Process crash or SIGSEGV. The Blueprint SIGSEGV after first activation is already an open Pi issue, so avoid Blueprint in steps A–D and bring it in only at E.
- Any new `glError` row attributed to the probe's scopes.
- `MemAvailable` below 256 MiB, or `CmaFree` below 40 MiB (the Pi 3B values in `memattr_supervisor.py`). Rock Pi has no CMA equivalent, so use `MemAvailable` only.
- RSS growing monotonically across a 60 s window.
- Any frame over 1 s outside scene or media change.

## 4. Steps

Each step has one variable. Run in order. Don't start the next if the previous aborts.

### A. Baseline and inertness (no probe) — 2 runs × 60 s

```bash
T=60 run A-base-blob      EXPERIENCE_RUNTIME_STALL_DRIVER=1 EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=temporal-steady EXPERIENCE_RUNTIME_STALL_DRIVER_STEADY_SECONDS=40
T=60 run A-probe-unset-ok EXPERIENCE_RUNTIME_STALL_DRIVER=1 EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=temporal-steady EXPERIENCE_RUNTIME_STALL_DRIVER_STEADY_SECONDS=40
```

Pass: the two runs agree within noise (nothing about the build changed behaviour with the probe off). Record p50/p95/p99 frame interval from `WINDOW` lines. This is the baseline for everything below. Also record `glRenderer`/`glVersion` from the `CONFIG` line.

### B. Capability check — one 30 s run, Blob, `MODE=off`, then `MODE=cpu`

```bash
T=30 run B-off EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE=$L/B-off/out EXPERIENCE_RUNTIME_SCENE_ACTIVITY_MODE=off
T=30 run B-cpu EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE=$L/B-cpu/out EXPERIENCE_RUNTIME_SCENE_ACTIVITY_MODE=cpu EXPERIENCE_RUNTIME_SCENE_ACTIVITY_READBACK=deferred
```

Looks at: stdout for FBO-complete or shader-compile errors; `activity.csv` state column.

- **Pass:** `valid` rows with finite raw values, ~10 per second, no `unsupported` rows, no GL errors.
- **Fail R1:** the probe reports `unsupported`, or the grid is all zeros. Stop. This is a finding, not a bug to patch on the Pi. Next move is a contract note: an RGBA8 luma path, which the desktop run already shows loses ~0.2–0.4% (QUANT8).
- Also run `B-q8` with `EXPERIENCE_RUNTIME_SCENE_ACTIVITY_QUANT8=1` to see whether the quantised values match the float run on the same device.

### C. Correctness on device — CPU vs GPU diff, 60 s per scene on Blob and Quadrant

```bash
T=70 run C-verify-blob EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE=$L/C-verify-blob/out EXPERIENCE_RUNTIME_SCENE_ACTIVITY_MODE=verify
T=70 run C-verify-quad EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE=$L/C-verify-quad/out EXPERIENCE_RUNTIME_SCENE_ACTIVITY_MODE=verify EXPERIENCE_RUNTIME_QUADRANT_SCENE=1
```

Pass: `gpuCpuDelta` max ≤ 1e-3 (desktop: ~1e-7; a Pi float path may be coarser). If R1 failed, skip the GPU parts and record "GPU diff unavailable, CPU path only".

### D. Cost per mode — Blob (cheap scene), 120 s each, interleave 2 repeats

Compare, with matching baseline runs between them:

| Label | Env |
|---|---|
| D-off | probe off (baseline) |
| D-cpu-def | `MODE=cpu READBACK=deferred` |
| D-cpu-sync | `MODE=cpu READBACK=sync` |
| D-gpu-def | `MODE=gpu READBACK=deferred` (skip if R1/R4 failed) |
| D-gpu-sync | `MODE=gpu READBACK=sync` |

Run order: off, cpu-def, cpu-sync, gpu-def, gpu-sync, then repeat. Record per run: frame p50/p95/p99, the probe's own `submit`, `wait`, `diffCpu`, `total` stats from its stats output, and effective sample cadence (rows/s of `valid`).

Pass criteria (provisional; Architecture has not set a Pi frame-time budget):
- the best mode adds ≤ 1 ms at p95 against the repeat-to-repeat noise of the baseline;
- effective cadence ≥ 8 Hz with no `missing` rows in steady state;
- no new stalls above 200 ms attributable to the probe scopes.

If no mode passes, try `HZ=5` and `HZ=2` once each, and report the cadence at which it fits. Also run a **colour-sampler-only vs sampler+probe** pair, since the production plan would run both.

### E. Five-scene run — only after D has a passing mode

One 180 s run per scene with the best mode, `FRAME_SECONDS=20` to save frame dumps as BMP. **BMP, not PNG**: PNG saves stalled the desktop render loop by ~1.6 s every 2 s. On the Pi, keep dumps to one every 30 s or off, since even BMP is I/O.

Bring Blueprint in last because of the open SIGSEGV. If it crashes, record whether the crash reproduces with the probe off. That makes it a pre-existing issue, not an OC-004 finding.

Then one transition run: `EXPERIENCE_RUNTIME_FULL_RING_HARNESS=1` with the probe on, to check generation and transition behaviour (`first`, then 100 `static` rows, no cross-scene difference).

Pass: every scene reaches `valid`; the raw distribution is in the same band as the desktop (compare p50/p95 per scene against report §2.3, allowing for the different media decode path); no scene is pinned at 0 or saturated under the recommended log mapping (floor 3e-4, ceiling 0.30).

### F. Memory and soak — 10 min, one scene, best mode

Run with `FRAME_PHASE_WINDOW_MS=1000` and a read-only side sampler:

```bash
while sleep 1; do grep -E 'MemAvailable|CmaFree' /proc/meminfo | tr '\n' ' '; echo; done > $L/F/meminfo.log
```

Pass: RSS and CmaFree flat (±2 MiB) against a probe-off 10 min control. On the Pi 3B the existing `memattr_supervisor.py` thresholds are valid. On the Rock Pi they are not, so use the plain `timeout` and `/proc` sampling above.

## 5. Evidence to bring back

For each run: `activity.csv`, `frames.csv`, `phase.log`, `stall.log`, `stdout.log`, plus the exact command, the `CONFIG` line, the load average at start (`uptime`), and anything running besides the app. Copy to the Mac, then use the existing scripts in `oc-004-evidence/scripts/` (`analyze.py`, `evidence.py`, `gt.py`) unchanged.

A step counts as passed only with its logs. A step that was skipped is reported as skipped and why.

## 6. Decision table

| Outcome | Meaning | Next |
|---|---|---|
| B fails (R1) | No float targets | Proposal for an RGBA8 luma path; rerun B–D with QUANT8 |
| B–C pass, D fails at 10 Hz but passes at ≤ 5 Hz | Cost-bound | Contract proposes 5 Hz on Pi; the meter smoother already tolerates it |
| D fails at every rate | Fragment cost or readback block | Lower the grid (32×18) or sample fewer taps; re-measure. Needs Architecture input |
| A–F pass | Probe is viable on Pi | Hand the evidence to Architecture for the `scene.visual.activity` ruling |

Nothing here changes production code. Any adjustment to the probe itself (RGBA8 path, tap count, grid size) is a separate, explicitly requested change.

## 7. Estimated operator time

About 2.5 hours on one device: A–C ~30 min, D ~45 min (10 runs plus repeats), E ~25 min, F ~15 min, plus builds and copying. Run the Rock Pi first to find build and capability problems cheaply.

No stage, commit, push or deploy was performed.
