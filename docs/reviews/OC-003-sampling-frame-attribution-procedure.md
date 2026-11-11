# OC-003 — Sampling-frame attribution: schema, provenance and supervised ROCK Pi P0 / P2 / P2F procedure (hardware handoff)

> **SUPERSEDED FOR OPERATION.** The authoritative, self-contained hardware procedure is now
> `docs/reviews/OC-003-rockpi-attribution-validation-operator-package.md` (shipped inside the validation package as `OPERATOR.md`).
> This file is kept as the event-schema and design reference only; where it differs from the operator package (git-based preflight steps,
> `systemctl` scope assumptions, media discovery by launching the binary, the `grep` for GL errors), the operator package wins.


Status: probe implemented and desktop-verified; hardware-readiness closure prepared. **No ROCK Pi result exists and the campaign has not started.**
Prepared by the coding agent, which `.claude/CLAUDE.md` bars from SSH, deploy and service changes: nothing in section 5 has been run on hardware.
Hardware execution belongs to the ROCK Pi Runtime & Performance Manager and the authorized operator. Diagnostic only: no sampling cadence,
scheduling, flush, readback format, HUD or production default was changed.

**This file lives in `docs/`, which `scripts/deploy.sh` excludes from rsync (`--exclude 'docs/'`). It does not reach the Pi by deployment. Copy it
to the operator by hand (see 2.3). The runner, the analysis scripts and the source manifest live in `scripts/oc003/`, which deployment does sync.**

## 0. Established facts vs unresolved prerequisites

| # | Established (evidence) | Source |
|---|---|---|
| E1 | Desktop (macOS, AMD Radeon Pro 560X), real runtime, FX + RAW deferred fence: 2,232 frames, FX issue/collect 139/139, RAW 138/137, no failures, exact reconciliation; fault-injected failures are never counted as successes; five-scene harness 44/44; focused tests and Release build pass; no new output when disabled | implementation report |
| E2 | The implementation is **uncommitted**: working tree on branch `blueprint-emergence-updates` over HEAD `6246fe21b2f0eba1d4a80946426fa5c0560f3bb9`. **No commit SHA contains it.** Provenance is therefore a file manifest (2.1), not a revision | `git status` |
| E3 | Changed source files: `SceneColorSampler.{h,cpp}`, `Rt004FramePhaseProbe.{h,cpp}` (this task); `SceneColorSamplerDiag.cpp` and the other OC-003 files are part of the earlier, still-uncommitted OC-003 work | `git status` |
| E4 | Build configuration: `cd sketches/experience_runtime && make Release -j4`. `scripts/deploy.sh` rsyncs the source and builds **on the Pi** (`make Release -j$MAKE_JOBS`), excluding `bin/ obj/ docs/ .git/ .claude/` | `scripts/deploy.sh` |
| E5 | The Pi checkout has no `.git`; the Pi binary is a Pi-side native build. Its hash cannot equal any desktop hash (different OS, architecture and toolchain) and no hash of it can be predicted | Pi preflight, E4 |
| E6 | Installed Pi binary sha256 `09ddc030af4c73c502335e7151015e0a754805d1dfcb8fb038dbeb349bb34948`; production service restored after the preflight (active, PID 1504, 0 restarts); profile has `DEFER_FLUSH=1` | Pi preflight |
| E7 | `DEFER_FLUSH` is **presence-based** in the runtime: P2 must leave it *unset* (the planner `env -u`s it); `=0` would still turn it on | `plan_interleaved.py`, source |
| E9 | **Installation auto-cycles defeat the media pin.** In a normal (non-harness) run the runtime loads `installation-settings.json` (device-local, git-ignored, `bin/data/`); its `mediaAutoCycle` / `sceneAutoCycle` override `EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID` + `EXPERIENCE_RUNTIME_VIDEO_DWELL`. Desktop rehearsal with the desktop settings (media 45 s, scene 300 s): the pinned media was left at exactly the cycle time and `verify_run.py` rejected the run. The Pi's installed file has not been seen; assume the same hazard | desktop rehearsal, `ofApp.cpp` (explicit path "always wins") |
| E8 | Attribution fields are present in the Pi's `SceneColorSampler.h` and `Rt004FramePhaseProbe.h` | Pi preflight |

| # | UNRESOLVED prerequisite | How it is closed |
|---|---|---|
| U1 | Whether binary `09ddc030…` actually contains the probe. Source presence on the Pi does not prove it (the binary may predate the sync, or the build may have failed after rsync) | 2.2 checks 1-3. If they fail → 2.4 |
| U2 | Whether the Pi's source equals the desktop-accepted source (only two headers were seen) | `sha256sum -c` on the manifest (2.2 check 1) |
| U3 | Whether `scripts/oc003/run_campaign.py`, `reconcile_events.py`, `test_event_analysis.py` and the manifest are on the Pi | `ls` (2.2 check 0); sync with the normal deploy if absent |
| U4 | systemd scope of `ecopunk-runtime`. Two repository runbooks disagree: `…RockPi-Validation-Runbook.md` uses system scope (`sudo systemctl`), `…rockpi-smoke-gate-runbook.md` uses `systemctl --user` | The runner autodetects and refuses if the unit is found in both scopes or neither (`--scope`) |
| U5 | Unit `WorkingDirectory`, `DISPLAY`, user | Recorded by the runner (`systemctl show`); validation runbook: user `conc`, `DISPLAY=:0` (vt7), HDMI-1 |
| U6 | Pinned media id, scene id, resolution `WxH` for the campaign | Operator/Manager choice (3.1); no default is assumed |
| U7 | Python on the Pi (scripts need ≥ 3.6; `test_event_analysis.py` ≥ 3.7) and `timeout`, `pgrep`, `sudo` availability | `python3 --version`; the runner refuses if `timeout` is missing |
| U9 | Content of the Pi's installed `installation-settings.json` (auto-cycle values, orbit observations) | Copied and neutralized by the planner (3.2); record its sha256 from `plan.json` |
| U8 | Whether a rebuild is acceptable. A rebuild replaces `sketches/experience_runtime/bin/experience_runtime` (the production binary path) | Manager decision; section 2.4 |

## 1. What the probe records

Enabled only with the existing `EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE=1` **and** `EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES=<csv>`. Without the CSV
the phase log is byte-for-byte the previous format; without the probe nothing is registered.

Event columns appended after `exclude` (all earlier columns unchanged):

| Column | Meaning |
|---|---|
| `ev` | bitmask of SUCCESSFUL events in this frame: 1 fxIssue, 2 fxCollect, 4 rawIssue, 8 rawCollect. `0` = no-event frame |
| `ev_fail` | bitmask of FAILED attempts, same bit order (1 fx issue refused, 2 fx collection failed, 4 raw issue refused, 8 raw collection failed) |
| `fx_issue fx_collect raw_issue raw_collect` | successful event counts in the frame (several events in one frame stay individually countable) |
| `fx_ifail fx_cfail raw_ifail raw_cfail` | failed attempt counts |

Definitions (sampler counters observed, never altered; `SceneColorSampler.cpp`):
- **issue success** = `Stats::issued` advanced. **issue failure** = `telemetryDisabledSkips` or `unsupportedFormat` advanced (due, but refused).
- **collect success** = `Stats::collectOk` advanced (a *valid* sample accepted by the series). **collect failure** = `collectFail` advanced (invalid data, fence timeout, wait failed, generation changed in flight, series rejection).
- Events belong to the frame (update + draw) in which the counter advanced (delta between consecutive frame closes, same `steady_clock` and frame id as the existing rows). Events in partial startup/exit frames are `unattributed`, never put on another frame.
- `droppedBusy` and gaps recorded for static/transition frames are NOT events; see the sampler stat lines.
- The phase log gains one `EVENTS …` line per 10 s window and at exit with cumulative totals (`csv`, `unattributed`, `abs`, `base`) for exact reconciliation.

## 2. Provenance and deployment prerequisites

### 2.1 Source manifest (replaces a commit SHA, which does not exist)
`scripts/oc003/attribution-source.sha256` lists sha256 of the source files that define the probe and of the analysis tooling, with paths relative to
the project root. It is generated from the desktop-accepted working tree (HEAD `6246fe2…` + uncommitted changes). Verify on the Pi from the project root:

```bash
cd <REPO>                                   # the project root that contains sketches/ and scripts/
sha256sum -c scripts/oc003/attribution-source.sha256        # every line must say OK
```
The manifest also lists `ExperienceRuntime.cpp/.h` (they implement the startup-media pin, the dwell override and the installation-settings load that the campaign depends on) and the whole `scripts/oc003` toolchain. Every line `OK` = the Pi source is byte-identical to the accepted source for these files. A `FAILED` line is **not** automatically a defect (the Pi may
legitimately differ in a file outside the probe) but the campaign must not start until the Manager has seen it. This proves source identity only.

### 2.2 Binary checks (source presence proves nothing about the binary)
```bash
B=sketches/experience_runtime/bin/experience_runtime
ls scripts/oc003/run_campaign.py scripts/oc003/attribution-source.sha256        # 0: tooling present (U3)
sha256sum $B                                                                   # record; installed value so far: 09ddc030…4bb34948 (E6)
for s in raw_cfail fx_cfail EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH; do
  printf '%s: ' $s; strings -a $B | grep -c -- "$s"; done                     # 1-3: every count must be >= 1
find sketches/experience_runtime/src -newer $B -name '*.cpp' -o -newer $B -name '*.h'    # 4: sources NEWER than the binary: must print nothing
```
- `raw_cfail` / `fx_cfail` exist only in the new CSV header; their presence means the probe was compiled in. If either is 0, **the installed binary lacks the probe**.
- Check 4: a source file newer than the binary means the binary is stale relative to the source on the Pi (rsync used `--no-times`, so mtimes are the rsync time; a build after the sync is newer than all sources).
- The runner repeats the string check (`raw_cfail`, `EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES`, `EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH`) and refuses to start without them; `verify_run.py` additionally fails any run whose CSV has no event columns. Those two are the definitive proof; the first three are early warning.

### 2.3 Getting the documents and tooling to the Pi
- `scripts/oc003/**` is synced by `scripts/deploy.sh` (not excluded). Run the existing deployment tooling's `--dry-run` first and read the itemized changes: **do not use `--clean`** for this task.
- This document (`docs/`) is excluded from deployment: copy it by hand (operator step) next to the repo, e.g. `~/OC-003-sampling-frame-attribution-procedure.md`.
- Hash limits: a platform-dependent binary hash can only be compared with a hash of the **same file on the same device** (pre vs post, run vs run). Never compare the Pi hash with a desktop hash.

### 2.4 If check 1, 2 or 4 fails (provenance gap)
Do not start the campaign. Narrow, Manager-approved options, in order of preference:
1. Re-run the normal deployment (rsync + remote build) so the Pi builds the current tree, then repeat 2.1-2.2. This **replaces the production binary file**: copy the existing one first (`cp -p $B ~/experience_runtime.pre-oc003-attr`), record both hashes, and report the new hash. Whether the production service should then be running the new binary, or the old copy put back after the campaign, is a Manager decision (U8); the runner never swaps binaries.
2. Build into a scratch copy of the tree and run the campaign from that binary (`--bin` of the planner accepts any absolute path), leaving the installed binary untouched. The planner records the sha256 of whichever binary is used.

## 3. Conditions (identical except where stated)

| Cond | Sampling | AVG/DOM/VIB | Readback | `DEFER_FLUSH` |
|---|---|---|---|---|
| P0 | FX and RAW disabled (`SCENE_COLOR=0`) | – | – | unset |
| P2 | FX + RAW | AVG + DOM + VIB | deferred, `DEFER=fence`, RGBA32F, 2 Hz (compiled) | **unset** (never `=0`) |
| P2F | = P2 | = P2 | = P2 | `=1` (the installed profile) |

Same binary, scene, pinned media, resolution, warm-up 30 s, settle 3 s, limiter/vsync, 2 Hz per source (FX and RAW are staggered half a period) in every run. The planner unsets every colour/diagnostic variable a condition does not set, so an exported profile cannot leak into P0/P2. Four interleaved rounds (P0 P2 P2F ×4), 300 s per run, of which the first 30 s are excluded; ≈ 540 steady-state events per source and kind per sampling run (≥ 100 required).

### 3.1 Choices the operator must make and record
- **Media id:** run `EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID=list timeout -s TERM 20 $B` (rejects and logs the catalog, `VideoVerification` lines); choose a media id (longest clip = fewest loops).
- **Scene id** (e.g. `blob-region-prototype`) and **resolution** (must equal the `window=WxH` the runtime reports; `xrandr` shows the mode). Both are checked per run.

### 3.2 Installation settings are neutralized by a private copy (never by editing the installed file)
`plan_interleaved.py` writes `<OUT>/campaign-installation-settings.json`: a COPY of `<bin dir>/data/installation-settings.json` (or `--installation-settings PATH`; defaults if none exists) with `sceneAutoCycle.enabled` and `mediaAutoCycle.enabled` set to `false`, and passes it to every run through the runtime's own `EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS`. All other keys (look, orbit observations, …) are kept, so the workload equals the installed one and is identical in all three conditions. `plan.json` records the source and copy sha256. `verify_run.py` fails any run whose log lacks `Installation: applied: sceneAutoCycle off … mediaAutoCycle off`. The runner records the installed file's sha256 before and after and flags a change in `restoration.json`. The installed file itself is only read.

## 4. Preflight checklist (everything is recorded; STOP on any surprise)
- [ ] 2.1 manifest verified; 2.2 checks 0-4 pass (or 2.4 completed and approved)
- [ ] Python ≥ 3.6 (`python3 --version`); `timeout`, `pgrep`, `strings` present
- [ ] Unit scope known (U4): `systemctl is-active ecopunk-runtime` / `systemctl --user is-active ecopunk-runtime`
- [ ] Installed `DEFER_FLUSH` recorded and **left unchanged** (the runner records it: `preflight.json: installed_DEFER_FLUSH`)
- [ ] Pre-state copied aside: `cp -p $B ~/experience_runtime.pre-oc003-attr`; `systemctl cat ecopunk-runtime > ~/oc003_unit_before.txt`; `ls -l /etc/systemd/system/ecopunk-runtime.service.d/`
- [ ] Output directory on local storage with ≥ 1 GB free (12 CSVs of ≈ 9,000 rows are small; stdout logs larger)
- [ ] `DISPLAY` known (validation runbook: `:0`), the runtime user is the one who owns that display; run from an interactive session of that user
- [ ] Media, scene, resolution chosen (3.1); the installed `bin/data/installation-settings.json` exists or its absence is noted (3.2); do not edit it
- [ ] Nobody else will touch the device, the service, the display or the checkout for ≈ 70 minutes
- [ ] `dmesg` readable (or its unreadability noted: the runner records it and then cannot detect throttling/OOM)

## 5. The supervised campaign (operator; do NOT run from the coding session)

Step 1 — plan (runs nothing; records the binary sha256 and writes `plan.json`):
```bash
cd <REPO>
OUT=~/oc003_attr_$(date +%Y%m%d_%H%M)
python3 scripts/oc003/plan_interleaved.py --bin "$PWD/sketches/experience_runtime/bin/experience_runtime" --out "$OUT" \
    --media-id <MEDIA_ID> --scene <SCENE_ID> --resolution <WxH> --conditions P0,P2,P2F --rounds 4 --duration 300
```
Do **not** pass `--defer-flush` (the planner refuses it together with P2+P2F).

Step 2 — optional rehearsal of the pre-flight only (starts and stops nothing):
```bash
python3 scripts/oc003/run_campaign.py "$OUT/plan.json" --workdir "$PWD/sketches/experience_runtime" --display :0 --stop-service --dry-run
```

Step 3 — the single supervised command (all 12 runs, sequential, ≈ 61 min plus analysis):
```bash
python3 scripts/oc003/run_campaign.py "$OUT/plan.json" --workdir "$PWD/sketches/experience_runtime" --display :0 --stop-service [--scope system|user]
```
What it does, in order, and what it refuses to do:
1. **Pre-flight**, writing `campaign/preflight.json` and `campaign/service_before.txt`: binary sha256 equals the plan; instrumentation strings present; working directory exists; `timeout` present; exactly one scope owns the unit; unit `cat`/`is-active`/`is-enabled`/`Environment`/`WorkingDirectory`/`ExecStart` recorded; installed `DEFER_FLUSH` state recorded; `uname`, Python, `dmesg` tail, drop-in listing recorded. Any problem → exit 1 with nothing stopped.
2. **Refuses** if the unit is active and `--stop-service` was not given, or if any `experience_runtime` process exists that is not the unit it is allowed to stop.
3. Stops the unit (`sudo systemctl stop` in system scope; sudo may prompt), confirms it is inactive and no process remains.
4. Runs the planner's 12 commands **one at a time** from `--workdir` with `DISPLAY` set, each under `timeout -s INT 300` with a 45 s hang guard. Between runs: no stray process, exit status 124/130 only, no stop-condition text in logs, no new kernel throttling/OOM lines, and `verify_run.py` on all runs so far (so insufficient events, a hash/media/scene/resolution mismatch or a flush mismatch aborts early, not after an hour).
5. **Always restores** (normal end, stop condition, exception, Ctrl-C, SIGTERM): terminates a run in progress, starts the unit again **only if this runner stopped it**, waits 5 s, and compares `is-active`, `is-enabled`, `cat`, `Environment` and the binary hash with the pre-flight record → `campaign/restoration.json`. A difference is reported loudly and is **not** fixed.
6. Only after restoration, validates and analyses (3 analysis outputs below). Exit 0 only if everything passed; exit 2 = aborted by a stop condition (partial results kept, never pool them); exit 3 = finished but validation/reconciliation/restoration failed.

Never run a second graphical process during the campaign; do not touch the display session.

## 6. Validation and analysis (the runner does these; each can be repeated by hand)
```bash
python3 scripts/oc003/verify_run.py "$OUT/plan.json" --require-all --min-events 100            # must print VERIFIED
for r in "$OUT"/*.frames.csv; do b=${r%.frames.csv}; case $b in *_P0_*) L=;; *) L=$b.stdout.log;; esac; python3 scripts/oc003/reconcile_events.py $r $b.phase.log $L | tail -1; done   # RECONCILED each (P0 has no sampler stat lines, so no stdout log)
python3 scripts/oc003/aggregate_frames.py P0=<P0 csvs, comma separated> P2=<…> P2F=<…> --long-ms 40 --json > "$OUT/analysis/aggregate.json"
python3 scripts/oc003/aggregate_frames.py P0=… P2=… P2F=… --long-ms 40 --sampler-log <each P2/P2F stdout log, one --sampler-log per file>
python3 scripts/oc003/sampler_stats.py "$OUT"/*_P2_*.stdout.log "$OUT"/*_P2F_*.stdout.log --csv > "$OUT/analysis/sampler.csv"
grep -inE "GL error|gl[A-Za-z]*errors?[A-Za-z]*=(0x)?0*[1-9a-fA-F]" "$OUT"/*.stdout.log "$OUT"/*.phase.log   # expect no output (a healthy log prints glError=0x00000000; only NONZERO values match)
```
Tooling capabilities (verified by `python3 scripts/oc003/test_event_analysis.py`, 8 fixtures): FX/RAW issue and collect events; failed attempts counted separately and never as successes; event bitmasks; multiple events in a frame; independent (overlapping) vs mutually exclusive categories; no-event frames; long-frame counts by class; phase CPU/wall p50/p95 per population; lag +1/+2 with contiguous ids, excluded frames never used and an own-event-free variant; exclusions (warm-up, media, scene, transition) as before; reconciliation to the probe's own totals and bounded comparison with the sampler stat lines.
`aggregate_frames.py` pools the rounds of a condition. **Per-round statistics are obtained by running it once per round** (`P2=<round-1 csv>`, …) — the 4-round spread required for the comparison is computed from those; the runner does not do this automatically (known gap, section 9).

## 7. Stop conditions
Automatic (runner): crash/hang/exit status other than 124/130; a second or leftover `experience_runtime`; `GL error` or any NONZERO `gl…Error…=` value (a healthy log prints `glError=0x00000000`), `telemetry DISABLED`, `discardTimeout>0`, `discardWaitFailed>0`, `halfFailures>0` in a stdout/phase log; `verify_run.py` failure (binary hash drift, wrong media/scene/resolution, `flushAfterFence` ≠ plan — P2 must log `no`, P2F `yes` —, RAW events in P0, fewer than `--min-events` events, any failed sampling attempt); kernel throttling/OOM lines; unit fails to stop.
Operator judgement (the runner cannot see these): temperature or power anomalies; display mode change; someone else touching the device; the service restarting itself; anything not understood. **An anomalous round is never silently pooled: abort, restore, report.**
Insufficient events → lengthen the runs; do not interpret.

## 8. Recovery and restoration (manual, if the runner could not finish)
```bash
pgrep -a experience_runtime                         # kill only your own diagnostic run: pkill -INT -x experience_runtime  (then confirm none)
sudo systemctl start ecopunk-runtime ; sleep 5 ; systemctl is-active ecopunk-runtime     # (use `systemctl --user` if the unit is user-scoped)
sha256sum sketches/experience_runtime/bin/experience_runtime      # equals the pre-flight value in campaign/preflight.json (restore ~/experience_runtime.pre-oc003-attr if a rebuild replaced it AND the Manager decided to revert)
systemctl cat ecopunk-runtime | diff - ~/oc003_unit_before.txt && echo "unit unchanged"
systemctl show ecopunk-runtime -p Environment | tr ' ' '\n' | grep -c DEFER_FLUSH     # same answer as installed_DEFER_FLUSH in preflight.json
ls -l /etc/systemd/system/ecopunk-runtime.service.d/                                    # unchanged
journalctl -u ecopunk-runtime -b --no-pager | tail -50                                   # healthy startup, no new errors
```
Any difference from the pre-flight record is reported, not "fixed", until the Manager decides.

## 9. Expected output files
`<OUT>/plan.json`; per run `NN_COND_rR.frames.csv`, `.phase.log`, `.stdout.log` (12 each); `<OUT>/campaign/{campaign.log, preflight.json, service_before.txt, run_status.json, restoration.json}`; `<OUT>/analysis/{verify.txt, reconcile.txt, aggregate.txt, aggregate.json, sampler.csv}`.
Known gaps: (a) no automatic per-round tables (section 6); (b) GPU completion latency is the sampler's own `latency` column (issue → collect, in the stat lines / `sampler.csv`), not a GPU timestamp — the probe attributes frames, not GPU time; (c) the FX stat line has no `completed=` field, so one reconcile bound is trivial for FX; (d) the runner's dmesg check is best-effort.

## 10. Evidence report template (what the Pi Manager returns)
1. Identity: date, device, kernel, Python; binary path + sha256 before/after; source-manifest result (2.1) and string checks (2.2); installed `DEFER_FLUSH` (unchanged); unit scope and name; display/resolution; media id; scene id; campaign command line; runner exit code.
2. Table per condition P0/P2/P2F (pooled, then per round): frames; global p50/p95/p99/worst; long-frame count (> 40 ms); event counts (fxIssue, fxCollect, rawIssue, rawCollect, failed attempts = 0).
3. Event-conditioned frame distributions: FX issue, FX collect, RAW issue, RAW collect (independent) and none / single / multi (exclusive), with n, p50/p95/p99/worst and long-frame counts; lag +1/+2 (all vs own-event-free).
4. Phase breakdown per population: update, preDraw, draw (wall and CPU), postDraw p50/p95.
5. Sampler cost (FX and RAW, P2 and P2F): submit, wait, convert, total, latency p50/p95/max; `dropGen`, `gen=` per scene, `gaps`, `invalid`, `discardTimeout`, `discardWaitFailed`, `notReady`, GL/fence errors.
6. Differences P2−P0, P2F−P0, P2F−P2 with the 4-round spread (a difference smaller than the round-to-round spread is not a result).
7. Restoration proof: `restoration.json` fields; `git`-free comparisons from section 8; journal tail.
8. Deviations, anomalies, runs repeated or discarded (with reasons), open questions. No ACCEPTED/CLOSED statements.

## 11. Decision tree (what the result means; the Manager decides, not this document)
- Extra time on **issue** frames inside the sampler's own CPU/draw segment (submit/fence/flush) → reduction submission or flush placement; compare P2F vs P2.
- Extra time on **collect** frames inside draw CPU → readback/convert/histogram (CPU).
- Extra time in **preDraw/postDraw wall with low CPU** on or after event frames (lag +1/+2) → queued GPU work finishing late (tile resolve/queue), consistent with but not proof of GPU causation.
- Long frames spread equally over **no-event** frames → not sampler-attributable; investigate unrelated activity.
- P2F ≈ P2 within round spread → `DEFER_FLUSH` neither helps nor hurts at this resolution.

## 12. Limits
Desktop evidence only (macOS, AMD Radeon Pro 560X): the mechanism, reconciliation and failure handling are verified, not Pi performance. The probe attributes frames, not GPU time. Per-frame bookkeeping is tiny but not zero (≈ 0.4 µs of CSV formatting measured in isolation; no whole-system claim). `run_campaign.py` was rehearsed on the desktop with the real runtime (`--no-service`: 3 sequential runs, verify + reconcile + analysis, exit 0) and its stop / abort / restore / signal branches were exercised against a **fake** `systemctl` shim. They have **not** been exercised against a real systemd unit, `sudo`, or the Pi's `pkill`/`ps`; the first hardware use is therefore itself a test of the runner.
