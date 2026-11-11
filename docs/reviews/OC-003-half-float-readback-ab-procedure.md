# OC-003 — Experimental RGBA16F (GL_HALF_FLOAT) readback: supervised ROCK Pi A/B procedure

Status: EXPERIMENTAL, default OFF. Desktop evidence only; no Pi result exists. No production-readiness claim.
Operator-run: the coding agent is barred by `.claude/CLAUDE.md` from SSH, deploy and service changes.

## 1. What the switch does
`EXPERIENCE_RUNTIME_SCENE_COLOR_READBACK=float|half|half-verify` (unset = `float` = the unchanged baseline; applies to BOTH the FX and RAW samplers).

| Value | Transfer | Published data |
|---|---|---|
| `float` | `ofFbo::readToPixels` (GL_FLOAT), 230,400 B per 160x90 readback | baseline |
| `half` | `glGetTexImage(GL_RGBA, GL_HALF_FLOAT)`, 115,200 B, then exact binary16 -> binary32 widening into the same float buffer | identical code downstream (AVG, histogram, tracker, publication) |
| `half-verify` | both transfers from the same texture | the FLOAT data; counts any bitwise difference of the widened half data (correctness pass, not a timing condition) |

An unknown value is logged and the float baseline is used. A half transfer that raises a GL error (or is built for GLES) is NOT replaced by float: the sample is recorded as a gap (`half`) or skipped (`half-verify`), counted in `halfFailures` / `glErrors`, and logged. Three consecutive invalid samples disable telemetry through the existing A7 gate (never rendering).

Log fields (SceneColor / VideoColor 10 s lines, and the harness summary): `rb=`, `widenMs[p50/p95/max]`, `halfReadbacks`, `halfFailures`, `glErrors`, `verifySamples`, `verifyMismatchSamples`, `verifyMismatchWords`, `verifyFirst=idx(base vs half)`, and one `experimental half-float readback: texture internalFormat=.. redBits=.. redType=.. transfer=.. bytesPerReadback=.. glRenderer=.. glVersion=..` line at the first half transfer. Existing fields unchanged: `readbacks`, `readbackBytes` (actual bytes), `stageMs submit/wait/convert/push/total`, `latencyMs` (GPU completion, fence), `predCpuMs`, `notReady`, `discardTimeout`, `discardWaitFailed`, `invalid`, `gaps`, `dropGen`. Separation: GPU completion latency = `latencyMs`; CPU blocking = `wait` (the transfer call) plus `widenMs` (conversion) plus `predCpuMs` (histogram) inside `convert`/`total`.

## 2. Desktop evidence (macOS, AMD Radeon Pro 560X, GL "2.1 ATI-7.0.24" context, the same fixed-function context class the Pi window requests)
Real-runtime PredominantHarness, 5 scenes, FX + RAW, deferred fence, 8 s per scene (scratchpad `rb/`):

| Mode | harness checks | bytes/readback | FX wait p50/p95 ms | widen p50 ms | FX sample total p50/p95 ms | frame p50/p95/p99 ms |
|---|---|---|---|---|---|---|
| float | 44/44 | 230,400 | 0.194 / 0.993 | - | 0.504 / 1.302 | 33.33 / 33.39 / 72.6 |
| half | 44/44 | 115,200 | 0.272 / 1.066 | 0.117 | 0.631 / 1.471 | 33.33 / 33.37 / 75.5 |
| half-verify | 44/44 | 172,800 avg of two | - | 0.115 | - | 33.33 / 33.36 / 74.1 |

- `half-verify`: **109 FX + 111 RAW samples compared, 0 mismatching samples, 0 mismatching words**; `halfFailures=0 glErrors=0`; texture `internalFormat=0x881a (RGBA16F)`, `redBits=16`.
- On this desktop GPU the half path is NOT faster (wait p50 +0.08 ms, plus 0.12 ms widening). Bytes halve; time does not improve here. Frame-time differences are within run-to-run noise (p99 is dominated by scene transitions). Single short runs: not statistically meaningful. Desktop says nothing about Panfrost.

## 3. Hardware preconditions (stop conditions)
- Same checkout/service safety steps as `OC-003-rockpi-smoke-gate-runbook.md` (record `git status`, service unit state, original binary copy; stop the production service before ANY graphical run; never two graphical instances).
- STOP if the Pi build lacks `HalfFloat.h`/the switch (check `strings <bin> | grep EXPERIENCE_RUNTIME_SCENE_COLOR_READBACK`), if `git status` shows unexpected Pi edits, or if safe restoration is not established.

## 4. Correctness pass on the Pi (once, ~2 min)
Pin media/scene as in the attribution procedure and run one `P2V` run (`--conditions P2V --rounds 1 --duration 120`). Pass criteria: `halfFailures=0`, `glErrors=0`, `verifySamples >= 100` per stream, `verifyMismatchSamples=0`, `effMode=deferred gpuLinear=1 defer=fence`. Any mismatch: record `verifyFirst` (index and both values) and STOP; do not proceed to timing.

## 5. A/B timing (interleaved)
```bash
python3 scripts/oc003/plan_interleaved.py --bin <abs binary> --out <dir> \
  --media-id <ID> --scene <scene id> --resolution <WxH> --conditions P0,P2,P2H --rounds 4 --duration 300
# run the 12 printed commands in order (P0 P2 P2H repeated), service stopped, one instance at a time
python3 scripts/oc003/verify_run.py <dir>/plan.json --require-all          # must print VERIFIED
python3 scripts/oc003/aggregate_frames.py P0=<csvs> P2=<csvs> P2H=<csvs> --sampler-log <stdout logs>
python3 scripts/oc003/sampler_stats.py <dir>/*_P2_*.stdout.log <dir>/*_P2H_*.stdout.log --csv
```
Matched across conditions: binary (sha256 in plan.json), scene, pinned media, resolution, 2 Hz cadence, deferred-fence mode, warm-up/settle exclusion. P0 is the no-sampling floor.

Report per condition: frame p50/p95/p99/worst and >100 ms count (pooled frames, `aggregate_frames.py`); per stage p50/p95/max for submit, wait, convert, total, latency, widen, predCpu (FX and RAW); `gaps`, `dropGen`, `invalid`, `notReady`, `discardTimeout`, `discardWaitFailed`, `glErrors`, `halfFailures`; GL errors in the phase log; confirmation that P2H never printed a half failure (otherwise it is a fallback-behaviour finding, not a timing result).

Decision: adopt only if P2H shows (a) zero failures/mismatches, and (b) a measured reduction in `wait`/total p95 or frame p99 beyond P2 run-to-run spread across the 4 rounds. If `wait` rises as on this desktop, the experiment has no benefit on that GPU. Production adoption additionally needs Architecture review (it adds a default-off switch to a ratified sampler path).

## 6. Restoration
Restore the production service and verify unit state/`git status`/binary exactly as in the smoke-gate runbook §5.

## 7. Known limits
- A GL error already pending before the half transfer is consumed (drained) by it; the RT-004 GL trace therefore cannot attribute such an error in half modes.
- Half readback is desktop-GL only; GLES builds report it as unavailable (and have no DOM/VIB).
- Widening is exact (all 65,536 binary16 patterns verified); non-finite halves stay non-finite, so the existing finite check still invalidates the sample.
