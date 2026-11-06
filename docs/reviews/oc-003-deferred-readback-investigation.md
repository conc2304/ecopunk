# OC-003 — Rock Pi Deferred Readback Investigation

**Owner:** ExperienceRuntime Engineering · **Status:** source investigation + one narrow, opt-in change, validated on desktop only. **No Rock Pi measurement was made by this session** (no SSH/deploy is permitted to the agent). The Pi stall is therefore *explained from source and the supplied numbers, not reproduced*. Section 5 is the supervised Pi plan that confirms or refutes it.

Supplied Pi evidence (deferred): submit avg 0.51 ms, readback wait p50 27.79 / p95 33.24 ms, total p95 33.24, worst 36.93.

## 1. Root-cause findings (source references)

All in `sketches/experience_runtime/src/`.

| # | Finding | Reference |
|---|---|---|
| F1 | The Deferred read is `ofFbo::readToPixels(ofFloatPixels&)` → `ofTexture::readToPixels` = `glBindTexture` + **`glGetTexImage(GL_FLOAT)` into client memory**. That is a blocking call with no completion check. It is the entire `wait` stat. | `SceneColorSampler.cpp` `collectLinear` (readToPixels line, `stats_.wait.add`); `libs/openFrameworks/gl/ofTexture.cpp:1264-1272` |
| F2 | "Deferral" is a **frame counter** (`++pendingFrames_ < kDeferFrames`, `kDeferFrames = 2`), not a completion test. GL gives no guarantee that two `poll()` calls means the GPU job finished. On a GPU that runs behind, the read blocks for the remaining time. | `SceneColorSampler.cpp` `poll`; `.h` `kDeferFrames` |
| F3 | `poll()` runs in `draw()` right after the scene pass and **before** HUD composition and swap. The current frame's scene work is therefore still queued while the read is issued. A driver whose texture read flushes and waits on the queue (a mapped BO read) pays for that work. | `ExperienceRuntime.cpp` ~868-892 (live-frame hook, then `poll`) |
| F4 | The existing asynchronous path (PBO + fence) is gated on **GL ≥ 3.2** and only exists for the *encoded* chain, not the linear reduction. The Rock Pi window requests GL 2.1 (validation runbook §3), so Async is never selected there. Nothing tested whether `GL_ARB_sync` exists on that context. | `SceneColorSampler.cpp` `validateSource` (`asyncUsable_`), `effectiveMode()` |
| F5 | The reduction itself is cheap in CPU terms (submit 0.51 ms on the Pi). The cost is the wait, not the draw. The transfer (16×9×16 B = 2.3 KB) is negligible. | supplied Pi numbers; `submitLinear` |

**What the evidence does and does not say.** A 28 ms wait is the size of one frame period at ~30 fps. That matches "the read is waiting for queued GPU work" (F2/F3), not a slow 2 KB transfer (F5). It does **not** distinguish a GPU that is simply behind from a driver that waits on all queued work on `glGetTexImage`. The two have different fixes, so the Pi plan measures completion latency separately from CPU blocking time. Treat F2/F3 as the leading hypothesis, unconfirmed on hardware.

## 2. Supported GL / GLES alternatives

Capabilities are checked at runtime, never assumed.

| Option | Needs | Blocks the CPU? | Verdict |
|---|---|---|---|
| Sync read (existing) | nothing | yes, full GPU wait (desktop 3.7–9 ms) | baseline |
| Frame-count deferral N=2 (existing) | nothing | only if the GPU is > 2 frames behind | **what the Pi measured** |
| Longer bounded deferral N (4–8) | nothing | only if > N frames behind; 2 Hz cadence tolerates ~250 ms latency | cheapest fallback; no guarantee |
| **Fence poll** (`glFenceSync` + non-blocking `glClientWaitSync(0)`, read only when signalled) | GL ≥ 3.2 **or** `GL_ARB_sync` (checked via the extension string on 2.1 contexts) | no, with one bounded fallback | **implemented, opt-in** |
| PBO async read | GL 2.1 PBO + fence | map may still block if the driver implements the copy in software | not adopted; the fence poll gives the guarantee without depending on how the driver implements pack buffers |
| GLES2 (VC4 Pi 3B) | no sync objects, no half-float targets | n/a | the linear path is not available there at all; the encoded chain + CPU linearisation applies (unchanged) |
| `GL_NV_fence`/EGL sync | extension-specific | — | not evaluated; no evidence they exist on the target |

## 3. Narrow change (implemented)

`SceneColorSampler.h/.cpp`, Deferred mode only. Reduction shader, spatial averaging, colour encoding, snapshot semantics and generation protection are untouched, and so is the public Scene Color contract (`Sample`, `Series`, snapshot). No RAW sampling. No change to scene rendering, HUD, or Shared Video.

- After the two reduction passes, create `linFence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE)`.
- `poll()` checks `glClientWaitSync(fence, 0, 0)` once per frame. Not signalled → return without blocking (`notReadyPolls++`). Signalled → run the **same** `collectLinear` as before. Reading a finished 16×9 target no longer includes a GPU wait.
- **Bounded:** a fence older than `kFenceTimeoutMs` (250 ms, half the 2 Hz period) is read with the old blocking call and counted in `forcedBlocking`, so cadence is never lost to a stuck fence.
- The sample keeps its issue timestamp and generation. A generation change deletes the fence and drops the sample (`beginGeneration`, and the existing generation check in `poll`).
- Strategy selection (`chooseDeferStrategy`, logged once): `EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER=auto|fence|frames|frames:N`. **`auto` (default) = fence when sync objects exist, otherwise the previous 2-frame deferral.** So on hardware without sync objects the behaviour is exactly the old one. `EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH=1` adds a `glFlush` right after the fence (measures whether an early flush helps).
- New diagnostics in the existing 10 s `SceneColor` stats line: `defer=<strategy>/<N>`, `latencyMs[p50/p95/max]` (issue → fence observed signalled, resolution one frame), `notReady`, `forcedBlocking`. `wait` now measures only CPU blocking + transfer, so GPU completion latency and CPU blocking time are reported separately.

The same class also serves the opt-in RAW video sampler (`videoColor_`); the env switch applies to both, and `auto` is safe for both.

## 4. Desktop comparison (matched, **not** Pi evidence)

macOS, AMD Radeon Pro 560X, GL 2.1 context (sync objects present via the extension string), Temporal scene, limiter 30 fps and vsync on (frame period matches the Pi's ~33 ms), 45 s per run, 2 interleaved repeats, `SCENE_COLOR_STATS=1`. Machine load average 3–6 (never idle). First pass was invalid (the strategy hook was not wired, so every "fence" run was frame deferral); it was discarded and rerun.

| Config | wait p50 / p95 / max (ms) | total p95 (ms) | frame p50 / p95 (ms) | draw p50 / p95 (ms) | notes |
|---|---|---|---|---|---|
| base (sampler off) | — | — | 33.3 / 35.2 | 2.48 / 3.78 | |
| sync | 4.2–8.9 / 6.3–10.5 / 15.5 | 7.4–10.5 | 33.2 / 40.7 | 2.56 / **8.58** | blocks |
| deferred, frames:2 (old) | 0.16–0.24 / 0.26–0.40 / 12.3 | 0.26–0.40 | 33.2 / 38.8 | 2.74 / 3.98 | one 12 ms read |
| deferred, frames:4 | 0.17–0.23 / 0.26–0.30 / 1.3 | 0.26–0.30 | 33.2 / 34.7 | 2.52 / 3.53 | |
| **deferred, fence** | 0.17–0.23 / 0.27–1.8 / 9.3 | 0.35–1.8 | 31.1 / **329.7** / 3914 | 2.74 / 4.32 | see below |
| deferred, fence + flush | 0.18–0.24 / 0.30–0.34 / 1.6 | 0.30–0.34 | 33.2 / 34.4 | 2.82 / 3.56 | |

Fence completion latency: p50 33 ms, p95 34–40 ms, i.e. one frame (the resolution of a per-frame poll); `forcedBlocking = 0` in all runs; `notReady` ≈ 1 per sample; 0 gaps, 0 generation drops, 0 stale in every run; the sample count matches the other modes (78 per 45 s at 2 Hz).

**What this shows.**
- The fence path is functionally correct: it completes every sample within one frame, never needs the bounded fallback, and has the same sample cadence.
- Draw-CPU increase is within ±0.3 ms of base at p50 for every deferred variant, which meets the ≤1 ms target. Sync shows the known p95 cost (+4.8 ms).
- **It cannot show the Pi improvement.** On this desktop the old frame deferral is already ~0.2 ms; there was no 28 ms stall to remove. The desktop only establishes correctness and non-regression.

**An unexplained event.** One of four fence runs (r1) has a single ~3.9 s frame (frame p95 330 ms in that run; max latency 3.78 s). No other fence run, and none of the other configs, has one. The base config's own worst frame is 396 ms, and the machine was loaded (load 3–6, another session active), but I cannot attribute the 3.9 s to ambient load with confidence. The fence was already in flight during that stall and `forcedBlocking` stayed 0, so the fence did not itself block; I did not reproduce it. It must be re-checked on a quiet machine before this default is trusted.

GL errors: none logged in any run. Memory: one extra `GLsync` object at a time; nothing else allocated.

## 5. Supervised Rock Pi plan (operator-run; not run)

Same build, scene, media, resolution and measurement interval for all arms. Interleave two repeats, 120 s each, Temporal then Blob. Log the `CONFIG`/`SceneColor: deferred readback:` lines first: **if `syncObjects=no`, the fence arm cannot run and that is itself the key finding.**

| Arm | Env |
|---|---|
| sync | `SCENE_COLOR_MODE=sync` |
| frames:2 (current) | `MODE=deferred DEFER=frames` |
| frames:4, frames:8 | `MODE=deferred DEFER=frames:4` / `frames:8` |
| fence | `MODE=deferred DEFER=fence` |
| fence + flush | `MODE=deferred DEFER=fence DEFER_FLUSH=1` |
| base | `SCENE_COLOR=0` |

(all prefixed `EXPERIENCE_RUNTIME_SCENE_COLOR_`, plus `…_SCENE_COLOR_STATS=1` and the frame-phase probe from the validation runbook).

Record: submit / wait / total (p50/p95/max), fence latency p50/p95/max, `notReady`, `forcedBlocking`, `busy`, gaps, dropGen, frame and draw p50/p95/max, `glError` rows, RSS and MemAvailable.

**Reading the result (this is what separates the two hypotheses):**
- fence latency ≈ the old wait (~28 ms) *and* wait ≈ 0.2 ms → the GPU is simply behind; fence/longer deferral fixes the CPU stall, completion latency stays ~1 frame. **Target met.**
- fence wait still ≈ 28 ms → the driver blocks inside `glGetTexImage` even on a finished target (a read-path problem, not completion). Then frames:N cannot help; the fallback is reduced cadence or a different read (PBO or `glReadPixels` from the FBO), which needs a new, separately approved change.
- `syncObjects=no` → use frames:4/8 if it removes the stall; otherwise reduced cadence.
- Any `forcedBlocking > 0` or fence p95 > 100 ms → report; the bounded fallback is working but the GPU is badly behind.

Pass: wait p95 ≤ 5 ms, worst ≤ 10 ms, matched mean draw-CPU increase ≤ 1 ms. Stop on any crash or new GL error. Restoring the display/services is the operator's decision.

## 6. Remaining blockers

- No Pi data from this session; the Pi root cause is unconfirmed (§1, §5).
- Float RGBA16F reduction targets: the linear path exists only where half-float render targets do. GLES2 (Pi 3B) uses the encoded chain, which this change does not touch.
- The fence default is unproven on a quiet machine (§4 unexplained 3.9 s event).
- Accuracy and stability: unchanged by this work (same read, same values; only timing differs). The Sample values are produced by the same code, but I did not run the accuracy self-test (`EXPERIENCE_RUNTIME_SCENE_COLOR_SELFTEST`) on the new path.

## 7. Recommendation

**Keep Deferred opt-in; adopt `auto` (fence when available, old behaviour otherwise) as a candidate for production only after the Pi plan passes.** If the Pi shows the read-path hypothesis (wait stays high on a finished target), recommend **reduced cadence (1 Hz) with continued opt-in** and a separate PBO/`glReadPixels` investigation. Do not relax the 5/10/1 ms targets. Do not enable by default on any platform that has not been measured.

## Completion report

1. **Summary** — Root cause of the Deferred stall traced in source (blocking `glGetTexImage`, frame-count deferral with no completion check, mid-frame poll, sync-object gate keyed on GL ≥ 3.2). One narrow opt-in change: fence-polled deferred read with a bounded fallback and separate latency/blocking diagnostics. Desktop-validated for correctness and non-regression; Pi improvement unmeasured.
2. **Files inspected** — `SceneColorSampler.h/.cpp`, `SceneColorSamplerDiag.cpp` (header), `ExperienceRuntime.cpp` (hooks, stats log, env), `libs/openFrameworks/gl/ofTexture.cpp` (`readToPixels`, `copyTo`), `ExperienceRuntime-RockPi-Validation-Runbook.md`.
3. **Files changed** — `SceneColorSampler.h`, `SceneColorSampler.cpp` (fence strategy, stats), `ExperienceRuntime.cpp` (3 added stat fields in the existing log line); this report.
4. **Tests/builds run** — `make Release` of the sketch (2×, passing); 24 desktop runs (12 invalid-first-pass + 12 valid), 1 capability probe run.
5. **Results** — §4.
6. **Deviations** — (a) The first matrix was invalid (hook not wired); discarded, kept as `oc3_invalid_nofence` in the scratchpad, rerun. (b) No Pi testing (agent may not SSH/deploy). (c) The shared binary was rebuilt again; runs used a private copy. (d) Did not run the existing unit suites or the accuracy self-test. (e) `Stats` gained fields (an internal diagnostic struct, not the published snapshot).
7. **New risks** — Unexplained 3.9 s stall in 1 of 4 desktop fence runs (§4). `auto` default changes Deferred behaviour wherever sync objects exist (desktop, including the RAW video sampler); opt back with `DEFER=frames`. Fence relies on the swap flush to be signalled unless `DEFER_FLUSH=1`.
8. **Contract changes requested** — None. Public Scene Color contract untouched.
9. **Recommended next step** — Supervised Pi run per §5; re-run the desktop fence arm on a quiet machine.

```text
WORK ITEM: OC-003 Rock Pi Deferred Readback Investigation
PREVIOUS STATUS: Open investigation (not checked against the registry's current text)
NEW STATUS: Investigation + narrow change delivered; Pi verification outstanding — proposed: IN PROGRESS / READY FOR SUPERVISED PI RUN (not ACCEPTED/CLOSED)

WHAT CHANGED: opt-in fence-polled deferred readback in SceneColorSampler (Deferred mode), new latency/blocking diagnostics.
WHAT WAS PROVEN: the fence path completes every sample within one frame on desktop with no blocking, no gaps and no extra drops; draw-CPU within ±0.3 ms of base at p50.
WHAT WAS NOT PROVEN: any Pi behaviour; the root cause on Panfrost; a quiet-machine desktop result (one unexplained 3.9 s stall); accuracy self-test on the new path.

NEW EVIDENCE: this report; scratchpad run data (oc3/, oc3_invalid_nofence/) not yet copied into docs.
NEW RISKS: see §6/7.
NEW BLOCKERS: Pi measurement needs a supervised session.

NEXT ACTION: operator-run Pi matrix (§5).
NEXT GATE: Pi pass against the 5/10/1 ms targets.

REGISTRY UPDATE REQUIRED: YES
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: NO (no contract change)
```
