# RT-004 — Offline SIGSEGV Localization (TRV-20261013T012337Z-d5e7)

**To:** Architecture · **From:** Pi Runtime & Performance (offline, read-only) · **Date:** 2026-10-13
**Mode:** no Pi access, no SSH/deploy, no runtime launch, no experiment.

## 1. Verdict

**UNRESOLVED** — crash mechanism not localized. Confidence that the crash *mechanism* is known: none. Basis: **no TRV artifact is present on the development host** (searched `~/Desktop`, `~/Downloads`, `~` to depth 6 for `TRV`, `d5e7`, `20261013`); the only inputs available here are the markers quoted in the Architecture assignment and repository source. Two narrower points *are* established from the supervisor's own source (§3): the SIGSEGV **preceded** the watchdog stop, and the ~39 MiB value is a **post-exit** reading, not a pre-crash one.

## 2. Evidence inventory and identity

| Item | Status |
|---|---|
| `/home/conc/rt004fp/TRV-20261013T012337Z-d5e7/` (manifest.json, samples.tsv, watchdog.tsv, frame-phase.log, stall-probe.log, driver.log, app-stdout.log, app-stderr.log) | **not available locally — not inspected** |
| Kernel/journal segfault line (fault address, IP, faulting library) | not available |
| Core dump / coredumpctl metadata / backtrace | not available; unknown whether one exists |
| Executable + debug symbols matching sha256 `c648024c1581dfb7924483a4b03b2fdafe39bcf867bc27e8ad5d0eb9beab22c5` | not available; the desktop build is a different (macOS) binary and cannot be used for symbolization |
| Source revision / dirty flag | **gap explained:** the documented TRV command (`RT-004-Temporal-Release-Pi-Verification.md` §3) did not pass `--build-id/--source-revision`, so the supervisor records `unavailable` for those fields by design (`memattr_supervisor.py` manifest `build_id`/`source_revision`); the binary hash is recorded separately in `manifest.binary`. The deploy-time `scripts/rt004/PROVENANCE.json` on the Pi would supply git head + dirty + diff hash. |

Markers used below are **as reported by Architecture**, not re-derived: release #1 ≈20.83 s; reactivation ≈24.85 s; release #2 ≈28.87 s; Blueprint setup ≈29.06 s; Blueprint activation ≈29.78 s; supervisor `app_exited returncode=-11` ≈29.94 s; dma-buf ≈47.5 → ≈26.0 MiB after Temporal deactivation; collector min CmaFree 48.9 MiB; watchdog last reading ≈39.0 MiB.

## 3. Timeline and watchdog ordering

Clock domain for all items: CLOCK_MONOTONIC (supervisor `time.monotonic()`, app `steady_clock`), seconds since launch. Cadences: collector 1.0 s (`memattr_supervisor.py:65`), watchdog 0.25 s (`:66`).

| rel s | Event | Kind |
|---|---|---|
| ≈28.87 | Temporal release #2 marker (`TimeOffsetVideoBuffer.cpp:143`, emitted **after** history reset and decoder pause-then-close returned, `:136-138`) | reported marker = completed operation |
| ≈29.06 | `scene.setup: blueprint_emergence` (emitted **before** `setup()` runs, `SceneManager.cpp` setupEntry) | marker ≠ completion |
| ≈29.78 | `scene.activate: blueprint_emergence` (emitted **before** `activate()` runs, `SceneManager.cpp:133-134`) | marker ≠ completion; does **not** prove a Blueprint update/draw happened |
| ≈29.78–≈29.94 | SIGSEGV — bounded interval only (last marker → exit detection); at Pi frame rates this is ≈1–2 frames | inference |
| ≈29.94 | watchdog tick finds `proc.poll()` non-None → `terminate("app_exited returncode=-11")` (`memattr_supervisor.py:577-578`) | observed |
| ≈29.94 | `stop_last_reading` ≈39.0 MiB taken inside `terminate()` **after** exit detection (`:418`) | post-exit reading |

Answers:
- **Did SIGSEGV precede the watchdog stop?** Yes. Each tick checks app exit *before* any threshold (`:577` precedes `:589`), and the stop reason is `app_exited`, not `cma_free<=40MiB_floor`. Had CmaFree been ≤40 MiB on any tick while the app was alive, that tick would have stopped with the floor reason.
- **Was SIGTERM/SIGKILL sent?** Not to the app: `terminate()` skips signalling when `proc.poll()` is already non-None (`:423`). It sends TERM/KILL only to *remaining process-group members* (`:445`); whether any existed is recorded as `signal` rows in the Pi `watchdog.tsv` (not inspected).
- **CMA at the crash:** the last *alive* watchdog tick (≤0.25 s before detection) saw CmaFree >40 MiB (inferred from the stop reason); the 1 Hz collector minimum was 48.9 MiB; the ≈39.0 MiB reading was taken after the process had died, while its GPU/dma-buf memory was being torn down. A sub-40 MiB value therefore is **not** shown to have existed while the app ran, and is not a SIGSEGV explanation. Recovery is visible only in the Pi `samples.tsv` `postexit` rows (not inspected).

## 4. Existing backtrace

**No.** No core, coredumpctl record, kernel segfault line or symbolized stack is available here. Crashing thread and owning library are unknown.

## 5. Ranked hypotheses (none proven)

1. **Fault in Blueprint's first frames on the Pi** — *this is the first time Blueprint ever ran on the Pi* (S1 stopped inside Blueprint setup; RT-005/BLUE-002 were desktop-only). For: crash ≤≈0.16 s after the activation marker; Blueprint's first update/draw allocates lazily (per-fragment `BEFragment` effect FBO pair, `BEFragment.cpp:372-373`; first `MotionExtraction`/`ErosionFBO` use). Against: none available; untested on Pi before.
2. **GPU-driver (Mesa VC4) fault under memory pressure during those allocations.** For: CmaFree margin was ~40–49 MiB with Blueprint's first allocations in flight; CMA fragmentation can fail a contiguous allocation with free memory still reported. Against: no evidence that any allocation failed; the faulting library is unknown. Discriminated only by the kernel segfault line (`… in <library>`) or stderr Mesa messages.
3. **Use-after-release from the Temporal correction.** For: release #2 happened ≈0.9 s before the crash. Against (source): release completes synchronously on the main thread (`TimeOffsetVideoBuffer.cpp:127-147`), GStreamer streaming threads stop when the pipeline goes to NULL inside `close()`, the player object itself is not destroyed, and after deactivation SceneManager neither updates nor draws Temporal (`SceneManager.cpp` `updateActiveScene`/`drawActiveScene` act only on the owner and only on live frames). The only post-release reader is diagnostic (`Rt004FramePhaseProbe` residency/EVENT reads: empty `history` iteration and `isAllocated()` checks on cleared textures — safe). Desktop: the identical `release-verify` sequence (release → Blueprint setup → activate → 7 s of Blueprint → exit) and the three-scene circuit (63/63, many Temporal→Blueprint releases) ran without fault. Weak.
4. **GStreamer thread fault (canonical decoder).** For: `gst_sample_set_caps` criticals are present. Against: the same criticals appear in every prior Pi run (R0–R2, S1) without a crash. Weak.
5. **0x0500 GL error after ambient load.** Present in every Pi run since R0 without a crash; no causal link. Weak.

## 6. Did the correction cause a regression?

**Unknown.** Not proven, and not supported by current evidence (§5 item 3); the confounder is that Blueprint activation on the Pi was never reached before this build. The release itself was observed working on the Pi (dma-buf ≈47.5 → ≈26.0 MiB).

## 7. Smallest next step

No source correction is justified. **Smallest next diagnostic: collect the existing crash evidence (§9) — no new run.** The kernel segfault line alone splits hypotheses 1/2/4 by faulting library (app binary vs `vc4_dri`/`libgallium` vs `libgst*`). Only if that is insufficient: a single, separately authorized re-run with core dumps enabled (Architecture decision; not requested here).

## 8. Separate CMA-margin risk (not acceptance)

Independent of the crash: with ambient residency (~128 MiB V3D), binner (16 MiB) and the canonical decoder, Blueprint's first setup still runs with ~40–50 MiB free even after the Temporal release. The floor was not tripped while the app was alive, but margin is thin. RT-004 remains open.

## 9. Read-only collection for the operator (not executed)

On the Pi, as `conc`, no runtime launch:

```bash
# 1. The run directory (copy back intact)
tar czf ~/TRV-20261013T012337Z-d5e7.tgz -C ~/rt004fp TRV-20261013T012337Z-d5e7
# 2. Kernel segfault line + DRM/CMA messages around the crash (UTC; may need group adm/systemd-journal)
journalctl -k --utc --since "2026-10-13 01:23:30" --until "2026-10-13 01:25:30" -o short-monotonic > ~/trv-kernel.log
journalctl --utc --since "2026-10-13 01:23:30" --until "2026-10-13 01:25:30" | grep -i -E "segfault|experience_runt|vc4|v3d|cma|drm|core" > ~/trv-journal.log
# 3. Core dump presence/metadata (no dump extraction unless Architecture asks)
cat /proc/sys/kernel/core_pattern > ~/trv-corepolicy.txt; coredumpctl list --no-pager 2>&1 | tail -20 >> ~/trv-corepolicy.txt
coredumpctl info --no-pager experience_runtime 2>&1 | head -120 > ~/trv-coredump-info.txt
# 4. Binary identity and deploy provenance
sha256sum ~/openFrameworks/of_v0.12.1_linuxarmv6l_release/apps/myApps/EcopunkVideoCollage/sketches/experience_runtime/bin/experience_runtime > ~/trv-identity.txt
cat ~/openFrameworks/of_v0.12.1_linuxarmv6l_release/apps/myApps/EcopunkVideoCollage/scripts/rt004/PROVENANCE.json >> ~/trv-identity.txt
# 5. X server log tail (GLX/DRI errors)
tail -n 200 ~/.local/share/xorg/Xorg.0.log /var/log/Xorg.0.log 2>/dev/null > ~/trv-xorg.log
```

Return those files with the run directory. Required checks on return: the sha256 equals `c648024c…22c5`; `app-stderr.log` tail (Mesa/GStreamer messages just before exit); `watchdog.tsv` `signal` rows; `samples.tsv` last `running` and `postexit` rows; `stall-probe.log` final NOTE/LONG FRAME (whether any Blueprint update/draw scope was entered).

No code changed. No stage, commit or push.
