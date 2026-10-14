# RT-004 — Temporal Resource-Lifetime Correction: Focused Pi Verification

**Status: NOT RUN — Pi execution pending.** The development-host agent has no Pi access (SSH/deploy are prohibited by the project rules), and the required scene sequence needed a narrow driver-only addition that is not yet on the Pi binary. No verdict is claimed.

## 1. Preflight / GO-NO-GO

- Pi offline gate (21/21, 0 skipped) and preflight: **not executed in this session.** Last recorded Pi gate: campaign `campaign-20261012T023119Z-747b` `preflight.json` (21/21, CmaFree 231.9 MiB, debugfs readable) — for a different build; must be re-run.
- Driver fit (brief step 4): the existing `smoke-circuit` ends back in Temporal; on the Pi an exit with Temporal active risks the destructor-time EOS wait (~5 s) past the 45 s ceiling (Blueprint at ~30 s wall, exit ~41 s + 5 s). **Gap → narrow driver-only change implemented, requires approval + redeploy:** `release-verify` in `sketches/experience_runtime/src/Rt004StallDriver.cpp`: Blob baseline 0–3 s → Temporal 1st activation 3 s → Blob (deactivation release) 8 s → Temporal reactivation 12 s → Blueprint first setup 16 s → exit from Blueprint 23 s (≈38–40 s wall with ~15 s Pi startup). No production code, thresholds or watchdog changed.
- **Recommendation: NO-GO until** the driver change is approved, deployed (`python3 scripts/rt004/pi_validation.py provenance` then `scripts/deploy.sh`) and the Pi offline gate + preflight pass.

## 2. Build identity and release path (source)

- Correction (already implemented, desktop-verified): `TemporalProductionScene::deactivate()` → `TimeOffsetPlaybackAdapter::releaseForDeactivation()` → `TimeOffsetVideoBuffer::releaseForInactive()` = history + playhead texture reset, decoder pause-then-close (Pi-measured close 97.7 ms, S1 frame 427), `mediaLoaded=false`, adapter forgets `loadedMediaId_`. Reactivation unchanged: `invalidateForReactivation()` → `loadExplicit()`. Rollback toggle `EXPERIENCE_RUNTIME_TEMPORAL_INACTIVE_RELEASE=retain`.
- Markers: stall-probe NOTE `temporal.releaseForInactive` (bytes before, reset/close ms, counts after), `temporal.loadExplicit`, `scene.setup/activate/deactivate`; scopes `Temporal.deactivate.releaseInactive`, `TimeOffsetVideoBuffer.releaseForInactive.decoderClose`.
- Pi binary SHA-256/mtime and git revision/dirty: to be recorded by the supervisor manifest and preflight of the actual run.

## 3. Exact command for the single authorized run (Pi, `sketches/experience_runtime`)

```bash
python3 ../../scripts/rt004/test_memattr_supervisor.py -v      # must be 21 tests, OK, no skips
../../scripts/rt004/run_pi_validation.sh --preflight             # must end PREFLIGHT OK (does not launch the runtime)
python3 ../../scripts/rt004/memattr_supervisor.py run --label TRV \
  --env EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=release-verify
python3 ../../scripts/rt004/release_verify_report.py ~/rt004fp/TRV-*/   # transition table + mechanical verdict
```

Supervisor thresholds unchanged (CmaFree floor 40 MiB, 48 MiB baseline decline, MemAvailable 256 MiB, RSS 300 MiB, collector stale 3 s, 45 s ceiling, log cap, 5 s TERM grace). One run; no retry on any stop.

## 4. Analyzer and comparison basis

`scripts/rt004/release_verify_report.py` (read-only) builds the phase table (CmaFree end/min, V3D kB/BOs, purgeable Mesa cache, binner, dma-buf objects/bytes, RSS/RssAnon, history frames/MiB, playheads) from driver monotonic marks, lists lifecycle/release markers and GL error lines, and applies the brief's predicates mechanically:
PASS only if the run completed without a supervisor stop, two clean releases (`decoderWasLoaded=1` → `historyFrames=0 playheadTex=0 decoderLoaded=0`), ≥2 Temporal reloads, dma-buf objects reduced after the first release, Blueprint setup **and** activation logged, bo_stats readable; FAIL on any supervisor stop or retention; otherwise INCONCLUSIVE. Validated against the prior S1 run: reproduces the historical CmaFree values (71.7 / 54.5 / 49.6 / 48.5 MiB) and yields FAIL (floor stop, no release markers, Blueprint never activated). Desktop smoke of `release-verify` (not a Pi result): full sequence, two releases (history 85.0 / 67.2 MiB, 6 playheads, decoder → 0), reactivation reload, Blueprint setup + activation, exit from Blueprint at driver 23 s.

Prior S1 CmaFree at equivalent phases (comparison basis, not expected values): prelaunch 231.9 · Blob 71.7 · first Temporal 54.5 · Temporal→Blob 54.5 · second Temporal 49.6 · Blueprint setup 48.5 → 33.5 (stop).

## 5. Verdict

**NOT RUN.** Smallest next action: approve the `release-verify` driver-only change, redeploy, then execute §3 once and return the run directory.

## 6. Recommendation to Architecture

Retain **NO-GO** for the full RT-004 campaign; authorize the single §3 run after redeploy. Do not accept the correction on desktop evidence alone.

No stage, commit or push.
