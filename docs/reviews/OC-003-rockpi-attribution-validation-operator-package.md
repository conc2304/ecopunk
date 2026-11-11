# OC-003 — ROCK Pi Attribution Validation Operator Package

Work item OC-003 (Sampling-Frame Attribution) · domain ExperienceRuntime / Scene Observability · hardware owner: ROCK Pi Runtime & Performance
Prepared by the ExperienceRuntime coding agent on the development machine. **The coding agent has not touched the Pi** (no SSH, deploy, build, service operation or campaign): every command below is a template for the authorized operator. Diagnostic only; nothing here changes sampling cadence, scheduling, flush policy, formats, HUD or production defaults, and the diagnostics are opt-in and default-off.

This file is self-contained: it travels inside the package as `OPERATOR.md` because `docs/` is excluded from `scripts/deploy.sh` and is not on the Pi. Every tool is run from the unpacked package (`$PKG/scripts/oc003/`), never from the Pi checkout's own `scripts/oc003/` (which may be older).

---
## A. Readiness matrix

| Item | State | Evidence / how it is closed |
|---|---|---|
| Probe, tooling, runner, analysis on the real desktop runtime (macOS, AMD Radeon Pro 560X) | **DESKTOP VERIFIED** | desktop rehearsal + fixtures + failure-injection tests (see completion report) |
| Runner stop/abort/restore logic against a shimmed `systemctl`/`sudo` | **DESKTOP VERIFIED** (shim only) | `test_run_campaign.py` 10/10 |
| Runner against real systemd, real `sudo`, the Pi's `ps`/`pkill`/`dmesg`, a real runtime's exit status | **NOT VERIFIED — first real use is on the Pi** | section C.5 (smoke test) is the gate |
| Source identity of the Pi checkout vs the accepted sources | **PI PREFLIGHT REQUIRED** | C.2 |
| Pi installed binary contains the probe (hash `09ddc030…4bb34948` was seen) | **PI BINARY CAPABILITY UNKNOWN** | C.3 decides Route A or Route B |
| Binary was built from the accepted sources | **NOT PROVABLE by hashes** (native build; no `.git` on the Pi). Only circumstantial: manifest match + capability + freshness, or an operator-observed build (Route B record) | section G |
| systemd ownership of `ecopunk-runtime` (system vs `--user`) | **PI SERVICE SCOPE UNKNOWN** (two repo runbooks disagree) | C.1 records both; the runner refuses when ambiguous |
| Installed `installation-settings.json` content on the Pi | **UNKNOWN** (desktop's has auto-cycles ON, which break pinning) | neutralized by a private copy for every run (section F); the installed file is only read |
| OC-003-D stall-mechanism arms (P2 / P2G / P2N): planner, audit, summary, stall analysis | **DESKTOP VERIFIED** (synthetic fixtures + short real desktop rehearsal); hardware **NOT AUTHORIZED** | section K |
| Hardware campaign | **NOT EXECUTED** | — |

## B. Package: contents, build, transfer

Built on the development machine (reads the repo, writes only under `--out`):
```bash
cd <dev repo>/apps/myApps/EcopunkVideoCollage
python3 scripts/oc003/make_package.py --refresh-manifest --out <dir>      # prints the tarball path and its sha256
```
Contents (`oc003-attribution-package/`): `OPERATOR.md` (this file) · `PACKAGE_INFO.json` · `PACKAGE_SHA256SUMS` · `scripts/oc003/*.py` and `attribution-source.sha256` · `source/sketches/experience_runtime/src/*` (the accepted sources, same relative paths as in the project — input for the Route B overlay only; never applied automatically). No binary, media, settings or secrets.

Transfer (authorized operator; the coding agent does not do this). Any method the operator already uses for this device; for example from the development machine:
```bash
scp <dir>/oc003-attribution-package.tar.gz <dir>/oc003-attribution-package.tar.gz.sha256 <user>@<pi>:~/
```
On the Pi:
```bash
cd ~ && sha256sum -c oc003-attribution-package.tar.gz.sha256        # must print OK
tar xzf oc003-attribution-package.tar.gz && cd oc003-attribution-package && sha256sum -c PACKAGE_SHA256SUMS | grep -v ': OK$' || echo "package intact"
export PKG=$HOME/oc003-attribution-package
python3 --version                                                    # must be >= 3.7
```
Normal deployment (`scripts/deploy.sh`) is **not** modified and is not used to distribute this package. Needs on the Pi: `python3 ≥ 3.7`, `timeout`, `pgrep`, `ps`, `pkill`, `sudo` (system-scope unit), `systemctl`; optional `ffprobe`, `xrandr`, `dmesg` readable.

Environment variables used below (set once; values are the Pi layout from the existing runbooks — confirm them, do not assume):
```bash
export REPO=$HOME/of_v0.12.1_linuxaarch64_release/apps/myApps/EcopunkVideoCollage
export INSTALLED_BIN=$REPO/sketches/experience_runtime/bin/experience_runtime
export DISPLAY=:0                                  # validation runbook: X display :0 on vt7, HDMI-1
```

## C. Operator procedure

### C.1 Read-only Pi preflight (changes nothing)
```bash
mkdir -p ~/oc003_preflight && cd ~/oc003_preflight
{ date; uname -a; whoami; python3 --version; for c in timeout pgrep ps pkill sudo systemctl; do command -v $c || echo "MISSING $c"; done; echo "DISPLAY=$DISPLAY"; } | tee env.txt
echo "system scope:"; systemctl is-active ecopunk-runtime; systemctl is-enabled ecopunk-runtime
echo "user scope:";   systemctl --user is-active ecopunk-runtime; systemctl --user is-enabled ecopunk-runtime
systemctl cat ecopunk-runtime > unit_system.txt 2>&1;  systemctl --user cat ecopunk-runtime > unit_user.txt 2>&1
systemctl show ecopunk-runtime -p Environment -p WorkingDirectory -p ExecStart -p User -p MainPID -p NRestarts > show_system.txt 2>&1
ls -l /etc/systemd/system/ecopunk-runtime.service.d/ 2>&1 | tee dropins.txt
sha256sum "$INSTALLED_BIN" | tee installed_binary.sha256 ; stat -c '%y  %s bytes' "$INSTALLED_BIN" | tee installed_binary.stat
sha256sum "$REPO/sketches/experience_runtime/bin/data/installation-settings.json" | tee installed_settings.sha256
grep -c DEFER_FLUSH show_system.txt unit_system.txt                 # installed flush setting: >0 means DEFER_FLUSH is set; DO NOT change it
pgrep -a experience_runtime || echo "no runtime process"
xrandr 2>&1 | grep ' connected' | tee display_mode.txt
dmesg 2>&1 | tail -5 | tee dmesg_tail.txt ; df -h "$REPO" | tee disk.txt
```
Decide from the output: **exactly one** of `unit_system.txt` / `unit_user.txt` should contain a unit (the other prints "No files found"/"not found"). Both or neither → **STOP and escalate** (section D).

### C.2 Source manifest verification (source identity only)
```bash
python3 $PKG/scripts/oc003/verify_provenance.py --root "$REPO" | tee ~/oc003_preflight/source_identity.txt     # exit 0 = every manifest file identical
```
`SOURCE_IDENTITY: MATCH` is required. A `DIFFERENT`/`MISSING` line is not automatically a defect (the Pi may legitimately differ) but **no campaign starts until the Manager has seen it**. This proves the *source* only — never that a particular binary was built from it.

### C.3 Binary capability verification
```bash
python3 $PKG/scripts/oc003/verify_provenance.py --root "$REPO" --bin "$INSTALLED_BIN" --record ~/oc003_preflight/provenance_installed.json | tee ~/oc003_preflight/binary_capability.txt
```
Read three separate lines: `SOURCE_IDENTITY`, `BINARY_CAPABILITY` (PRESENT = the CSV event header and variable names are compiled in), `BINARY_FRESHNESS` (STALE? = a source file is newer than the binary). The last line always says the binary↔source link is **not proven**.
* `BINARY_CAPABILITY: PRESENT` and freshness fine and `SOURCE_IDENTITY: MATCH` → **Route A** (use the installed binary; no rebuild).
* `ABSENT`, or stale → **Route B** (needs Manager approval) — go to C.4.

### C.4 Route B — controlled isolated diagnostic build (only after Manager approval)
Preserves the installed production binary, its objects, its `bin/data` and the service. The build happens in a **stage copy**, a sibling directory of the checkout (`${REPO}_oc003diag`, same depth so the relative `OF_ROOT` in `config.make` resolves identically and the already-built shared openFrameworks library is reused).
```bash
python3 $PKG/scripts/oc003/build_diagnostic.py --checkout "$REPO" --jobs 2 --dry-run   # pre-checks only, copies nothing
nohup python3 $PKG/scripts/oc003/build_diagnostic.py --checkout "$REPO" --jobs 2 > ~/oc003_build.out 2>&1 &
STAGE=${REPO}_oc003diag ; tail -f $STAGE/oc003-build.log                          # progress (the native Pi build is long; do not start campaigns meanwhile)
```
What it does: copies the project to the stage (no `.git`/`docs`/`obj`/`bin`; only `sketches/experience_runtime/bin/data`; `assets/` symlinked read-only), verifies the manifest on the **stage** (stops before building if it differs), first runs `make -n Release` and **stops if the shared openFrameworks library would be recompiled** (that would write outside the stage; the Manager can approve `--allow-of-lib-rebuild`), then runs `make Release -j2` in the stage, writes `oc003-build.log`, `oc003-provenance.json`, hashes the binary and checks capability. It deletes nothing and refuses an existing stage. `-j2` keeps the device usable. Do **not** set `OF_ROOT` yourself: an absolute `OF_ROOT` makes `make` treat every shared-library object as new and rewrites the shared OF object tree (observed during development).

**Shared openFrameworks tree caveat.** All sketches in one openFrameworks tree share the compiled OF library and its `.compiler_flags` file. A build of a *different* sketch (or a deployment build) in the same tree, running at the same time or just before, can make `make` consider the shared library stale; the tool's `make -n` guard then refuses ("283 openFrameworks CORE sources would be recompiled…") and builds nothing. This happened once during development when another session was building another sketch. Remedy: make sure nothing else builds in that OF tree, wait for any running `make`, and repeat the `--dry-run`/build. `--allow-of-lib-rebuild` (Manager approval) accepts a rebuild of the shared library — the same thing a normal deployment build does, but it writes outside the stage. The stage directory is never deleted by the tool; remove it yourself when the Manager releases it (`${REPO}_oc003diag`).

If the manifest check on the stage fails because the Pi's own files legitimately differ, the **Manager** may approve `--overlay $PKG/source` (overlays the accepted sources into the **stage only**, printing which files differ). Never overlay unseen differences.

Result: `diagnostic binary: ${REPO}_oc003diag/sketches/experience_runtime/bin/experience_runtime` and `plan/runner workdir: ${REPO}_oc003diag/sketches/experience_runtime`; exit 0 only if source MATCH + probe PRESENT + fresh. Use these as `BIN` and `WORKDIR` below. Narrowest alternative if the stage build is impossible (disk, toolchain): rebuild in place via the normal deployment flow — that **replaces the production binary** and needs explicit Manager approval and a saved copy (`cp -p "$INSTALLED_BIN" ~/experience_runtime.pre-oc003`).

### C.5 Choose campaign inputs, plan, and run the runner SMOKE test (first contact with real systemd)
```bash
# Route A:  BIN=$INSTALLED_BIN   WORKDIR=$REPO/sketches/experience_runtime
# Route B:  BIN=${REPO}_oc003diag/sketches/experience_runtime/bin/experience_runtime   WORKDIR=${REPO}_oc003diag/sketches/experience_runtime
python3 $PKG/scripts/oc003/list_media_ids.py --media-root "$REPO/assets/shared/media"   # read-only, no runtime; pick the longest clip → MEDIA
# SCENE = the scene id the runtime starts in (desktop: blob-region-prototype); RES = the window size the runtime opens (xrandr: e.g. 1280x720)
SM=$HOME/oc003_smoke_$(date +%Y%m%d_%H%M)
python3 $PKG/scripts/oc003/plan_interleaved.py --bin "$BIN" --out "$SM" --media-id "$MEDIA" --scene "$SCENE" --resolution "$RES" \
    --conditions P0,P2,P2F --rounds 1 --duration 90
python3 $PKG/scripts/oc003/run_campaign.py "$SM/plan.json" --workdir "$WORKDIR" --display "$DISPLAY" --stop-service --scope <system|user> \
    --watch "$INSTALLED_BIN" --min-events 60 --dry-run        # pre-flight + the exact run list; starts and stops NOTHING
```
Read the dry-run output (pre-flight problems, `installed DEFER_FLUSH=…`, scope, scrubbed variables). Only if it reports no `PRE-FLIGHT PROBLEM`:
```bash
python3 $PKG/scripts/oc003/run_campaign.py "$SM/plan.json" --workdir "$WORKDIR" --display "$DISPLAY" --stop-service --scope <system|user> \
    --watch "$INSTALLED_BIN" --min-events 60 | tee "$SM.console.txt"
echo "exit=$?"
```
(≈ 5 min: 3 × 90 s + settle; 60 steady-state seconds per run ≈ 120 events per source.) Pass criteria, all required: exit 0; `CAMPAIGN COMPLETE: verified, reconciled, restored`; `restoration.json: ok=true`; the unit `active` again. A smoke result is **rehearsal data only** (1 round): it is not a performance result. If the smoke test fails: **stop, confirm restoration (C.8), report** — do not start the campaign.
If verify reports a *scene* mismatch, the log names the scene the Pi actually runs; re-plan with that `--scene` only if the Manager agrees it is the intended scene.

### C.6 The P0/P2/P2F campaign (after the smoke gate and Manager go-ahead)
```bash
OUT=$HOME/oc003_attr_$(date +%Y%m%d_%H%M)
python3 $PKG/scripts/oc003/plan_interleaved.py --bin "$BIN" --out "$OUT" --media-id "$MEDIA" --scene "$SCENE" --resolution "$RES" \
    --conditions P0,P2,P2F --rounds 4 --duration 300
python3 $PKG/scripts/oc003/run_campaign.py "$OUT/plan.json" --workdir "$WORKDIR" --display "$DISPLAY" --stop-service --scope <system|user> \
    --watch "$INSTALLED_BIN" --min-events 100 | tee "$OUT.console.txt"
```
Do **not** pass `--defer-flush` to the planner (it refuses it with P2+P2F). 12 runs, strictly sequential, interleaved P0 P2 P2F ×4, 300 s each (≈ 61 min + analysis). Run it from an interactive session of the user that owns the display, in `tmux`/`screen` if the connection may drop (the runner restores on SIGINT, SIGTERM and SIGHUP; still run it under `tmux` so the session survives a dropped connection). Nobody touches the device meanwhile: **no keyboard, remote, `xdotool`/`xte` or other input to the runtime's display session** — one stray `]` advances the scene (`NextScene accepted`), which `verify_run.py` correctly rejects as a scene change (seen during the desktop rehearsal: three scene switches in 4 s mid-run aborted the campaign, restored, and was reported).
Conditions (identical binary, scene, media, resolution, 2 Hz compiled cadence, warm-up 30 s excluded): **P0** sampling off · **P2** FX+RAW deferred fence, AVG/DOM/VIB, `DEFER_FLUSH` **unset** · **P2F** = P2 + `DEFER_FLUSH=1`. `DEFER_FLUSH` is presence-based: disabled means unset, never `0`.
What the runner does: pre-flight (binary hash, probe strings, isolation audit, ownership, processes, installed-settings hash) → stop unit → per run: start, wait, then check exit status (124/130 only), no leftover process, evidence files present, no stop-condition text, no kernel throttling/OOM lines, `verify_run.py` on all runs so far → restore on every path → postflight comparison → validation + analysis. State is written to `campaign/state.json` at every step.

### C.7 Result verification (the runner already did these; repeat by hand if in doubt)
```bash
python3 $PKG/scripts/oc003/verify_run.py "$OUT/plan.json" --require-all --min-events 100          # VERIFIED
for r in "$OUT"/*.frames.csv; do b=${r%.frames.csv}; case $b in *_P0_*) L=;; *) L=$b.stdout.log;; esac; python3 $PKG/scripts/oc003/reconcile_events.py $r $b.phase.log $L | tail -1; done   # RECONCILED ×12
python3 $PKG/scripts/oc003/summarize_campaign.py "$OUT/plan.json" --out "$OUT/analysis"          # summary.json + report.md (per round AND pooled)
grep -inE "GL error|gl[A-Za-z]*errors?[A-Za-z]*=(0x)?0*[1-9a-fA-F]" "$OUT"/*.stdout.log "$OUT"/*.phase.log   # expect no output
```
Outputs: `analysis/summary.json` (all numbers), `analysis/report.md` (short report), `analysis/aggregate.{txt,json}`, `analysis/sampler.csv`, `analysis/{verify,reconcile}.txt`. `summary.json` contains, per condition pooled and per round: global p50/p95/p99/worst, long-frame counts, FX/RAW issue- and collect-conditioned (independent, overlapping — never summed) and exclusive (none/single/multi, summing to the total) frame distributions, phase wall/CPU p50/p95, lag +1/+2, event counts and failed attempts; sampler submit/wait/convert/total/latency, `dropGen`, `gaps`, `invalid`, timeouts, `glErrors` per run; reconciliation and GL-error verdicts; P2−P0, P2F−P0, P2F−P2 pooled and per round with `sign_consistent`. **A difference smaller than the round-to-round range, or not sign-consistent across rounds, is not a result.** Association is not GPU causation: the probe observes per-segment wall/CPU time, not GPU timestamps.

### C.8 Restoration and its proof (never claim "restored" without this)
The runner writes `campaign/restoration.json` (postflight vs preflight: `active`, `enabled`, `cat`, `environment`, binary hash, installed `installation-settings.json` hash, `--watch` files, stray processes). Independently, by hand:
```bash
pgrep -a experience_runtime                                        # exactly the production service's process (or none if the unit is meant to be down)
systemctl is-active ecopunk-runtime                                # (or --user) — active
systemctl show ecopunk-runtime -p MainPID -p NRestarts             # compare with ~/oc003_preflight/show_system.txt (a new MainPID is expected)
systemctl cat ecopunk-runtime | diff - ~/oc003_preflight/unit_system.txt && echo "unit unchanged"
sha256sum "$INSTALLED_BIN" ; cat ~/oc003_preflight/installed_binary.sha256         # identical
sha256sum "$REPO/sketches/experience_runtime/bin/data/installation-settings.json" ; cat ~/oc003_preflight/installed_settings.sha256   # identical
ls -l /etc/systemd/system/ecopunk-runtime.service.d/ | diff - ~/oc003_preflight/dropins.txt && echo "drop-ins unchanged"
journalctl -u ecopunk-runtime -b --no-pager | tail -50             # healthy startup, no new errors
```
Any difference is **reported, not fixed**, until the Manager decides.

### C.9 Evidence packaging
```bash
cd ~ && tar czf oc003_evidence_$(basename "$OUT").tar.gz  "$(basename "$OUT")" oc003_preflight "$(basename "$OUT").console.txt" "$(basename "$SM")" "$(basename "$SM").console.txt" \
    $( [ -d "${REPO}_oc003diag" ] && echo ${REPO}_oc003diag/oc003-build.log ${REPO}_oc003diag/oc003-provenance.json )
sha256sum oc003_evidence_*.tar.gz | tee oc003_evidence.sha256
```

## D. Decision tree

1. **C.1 shows the unit in both scopes, or in neither** (ownership ambiguous) → **STOP and escalate.** Do not choose a scope by guessing; the runner refuses anyway.
2. **C.2 `SOURCE_IDENTITY: MISMATCH`** → STOP; send `source_identity.txt` to the Manager; no campaign until reviewed.
3. **C.3 capability PRESENT + freshness OK + source MATCH** → **Route A**: proceed to C.5 with `BIN=$INSTALLED_BIN`.
4. **C.3 capability ABSENT or binary STALE** → **STOP and request approval for Route B** (C.4). Do not rebuild in place without explicit approval.
5. **C.4 build fails or its provenance check is not exit 0** → STOP; return `oc003-build.log` and the console output. Nothing outside the stage was changed.
6. **C.5 smoke test fails (any gate)** → STOP, run C.8, report. Do not start C.6.
7. **All gates pass and the Manager has said go** → C.6 under ROCK Pi Runtime & Performance supervision.
8. **Mid-campaign abort (exit 2) or failures (exit 3)** → section E.

## E. Stop conditions and rollback

**Automatic stops** (the runner aborts, restores, exits 2; partial results are kept for diagnosis and are **never pooled**): crash, hang beyond duration+45 s, exit status other than 124/130 (e.g. the runtime exiting by itself early, or a different SIGINT exit code on this runtime — report it, do not loosen the check), a second/leftover `experience_runtime`, a missing/empty evidence file, `GL error` or a nonzero `gl…Error…=` value, `telemetry DISABLED`, `discardTimeout>0`, `discardWaitFailed>0`, `halfFailures>0`, `verify_run.py` failure (binary hash drift, media/scene/resolution mismatch, `flushAfterFence` ≠ plan — P2 `no`, P2F `yes`, P0 events recorded, fewer than `--min-events`, any failed sampling attempt, auto-cycle not off), kernel throttling/OOM lines in `dmesg` (best effort), failure to stop the unit. Pre-flight problems exit 1 before anything is stopped.
**Operator-judged stops** (the runner cannot see them): temperature/power anomalies, display mode change, someone touching the device, the service restarting itself, anything not understood. Press Ctrl-C once or `kill -TERM <runner pid>`: the run in progress is ended (whole process tree: INT → TERM → KILL) and the unit is restarted.
**Interrupted / crashed runner** (power loss, `kill -9`, closed terminal without tmux): the unit may still be stopped. Then:
```bash
pgrep -a experience_runtime ; pkill -INT -x experience_runtime      # only YOUR diagnostic run; confirm with pgrep that none remain
cat $OUT/campaign/state.json                                         # where it stopped
sudo systemctl start ecopunk-runtime ; sleep 5 ; systemctl is-active ecopunk-runtime      # (or: systemctl --user start …)
```
then run C.8 completely. **Restoration failure** (unit not active again, a differing hash, a stray process): leave the device as it is, collect `restoration.json`, `campaign.log`, `journalctl -u ecopunk-runtime -b`, and escalate; do not edit the unit, the drop-ins, the binary or the installed settings.
**Unexpected service state** (unit active when a run starts, restart counters moving, a second graphical process): STOP; nothing is "fixed" silently.

## F. Configuration isolation (what makes the three conditions comparable)

* Each plan writes **its own** `campaign-installation-settings.json`: a copy of `<bin dir>/data/installation-settings.json` (or `--installation-settings PATH`; runtime defaults if none) with `sceneAutoCycle.enabled` and `mediaAutoCycle.enabled` set to `false`. All other keys are preserved. It is passed to every run through the runtime's own `EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS` (an explicit path always wins). The installed file is only read; its hash is checked before/after. Why: the installed auto-cycles otherwise override the pinned media and scene (observed on the desktop: the pinned clip was left at exactly the configured cycle time).
* Both source and copy hashes are recorded in `plan.json`; `verify_run.py` re-hashes the copy and requires both auto-cycles `false` in it **and** `Installation: applied: sceneAutoCycle off … mediaAutoCycle off` in every run's log.
* Fail-closed isolation audit (`verify_run.audit_plan`, run by the runner's pre-flight and by `verify_run.py`): every run of a condition has an identical environment except its two output paths; every managed variable (all colour/diagnostic switches, limiter, vsync, frame-phase controls) is explicitly set or `env -u`; the variables that differ between P0 and P2 are exactly the sampling switches, between P2 and P2F exactly `DEFER_FLUSH`; `DEFER_FLUSH=0` is rejected; same binary and `timeout -s INT <duration>`; pinned media and settings path identical. Any unverifiable condition → refusal.
* The runner removes every inherited `EXPERIENCE_RUNTIME_*` variable from the environment of the runs (names logged), so nothing exported in the operator's shell or by a profile can reach a condition.
* Note: the runtime's `DEFER_FLUSH` check is mere presence; the installed profile's `DEFER_FLUSH=1` belongs to the production unit only and is never changed.

## G. Source versus binary identity (read this before quoting a hash)
* The implementation is **uncommitted**; there is no commit SHA for it. The identity of the *sources* is `scripts/oc003/attribution-source.sha256` (every file listed in it: the probe, the sampler, the runtime files that implement pinning and settings, and the whole tool chain).
* The Pi's binary is a **native build**; its hash depends on the OS, toolchain and flags and cannot equal a desktop hash. Hashes are only comparable for the same file on the same device (before/after a run, run to run).
* A matching manifest proves the source files are identical; it does **not** prove a given binary was built from them. `verify_provenance.py` therefore reports `SOURCE_IDENTITY`, `BINARY_CAPABILITY` and `BINARY_FRESHNESS` separately and always prints that the link is not proven. The strongest honest evidence is Route B's record: manifest verified on the stage immediately before `make`, binary hashed immediately after, the build log hashed — an operator-observed build, not a reproducible-build proof. The definitive functional check is behavioural: every campaign run must show the event columns and reconcile exactly (`verify_run.py`, `reconcile_events.py`).
* No embedded build identifier was added: it would be a runtime change for a diagnostic need that the compiled-in CSV header and the behavioural check already cover.

## H. Results handoff to ExperienceRuntime Engineering
Return **one** archive (C.9) plus its sha256, containing, per campaign (and the smoke run):
1. `plan.json`, `campaign-installation-settings.json`, the 12 × `*.frames.csv`, `*.phase.log`, `*.stdout.log`.
2. `campaign/{campaign.log, preflight.json, service_before.txt, state.json, run_status.json, restoration.json}`.
3. `analysis/{summary.json, report.md, aggregate.json, aggregate.txt, sampler.csv, verify.txt, reconcile.txt}`.
4. `oc003_preflight/*` (env, unit and drop-in captures, binary and settings hashes, display mode, `source_identity.txt`, `binary_capability.txt`, `provenance_installed.json`); for Route B also `oc003-build.log` and `oc003-provenance.json`.
5. The console output of the runs (`*.console.txt`), the exit codes, the C.8 outputs, the Manager's approvals referenced by date.
6. A short note: route used (A/B), unit scope, anything judged by the operator, anomalies, repeated or discarded runs and why.

## I. What the data mean (the Manager decides, not this document)
Event schema (CSV columns after `exclude`): `ev` bitmask of successful events (1 fxIssue, 2 fxCollect, 4 rawIssue, 8 rawCollect; 0 = no-event frame), `ev_fail` same bits for failed attempts, per-kind counts `fx_issue fx_collect raw_issue raw_collect`, failure counts `fx_ifail fx_cfail raw_ifail raw_cfail`. Events are attributed to the frame (update+draw) in which the sampler's own counter advanced; events in partial startup/exit frames are `unattributed`; `droppedBusy` and static-frame gaps are not events.
* Extra time on **issue** frames in the sampler's own segment → reduction submission / flush placement (compare P2F vs P2).
* Extra time on **collect** frames in draw CPU → readback / convert / histogram.
* Extra preDraw/postDraw **wall with low CPU** on or after event frames (lag +1/+2) → queued GPU work finishing late — consistent with, not proof of, GPU causation.
* Long frames spread equally over **no-event** frames → not sampler-attributable.
* P2F ≈ P2 within the round spread → `DEFER_FLUSH` neither helps nor hurts at this resolution.

## J. Known limits
Desktop evidence only; the probe attributes frames, not GPU time; per-frame bookkeeping is tiny but not zero (≈ 0.4 µs CSV formatting measured in isolation; no whole-system overhead claim); the runner's `dmesg` check is best effort; the FX stat line prints no `completed=`, so one reconcile bound is trivial for FX; a runtime that exits with a status other than 124/130 under SIGINT will be reported as a stop condition rather than tolerated.

---
## K. OC-003-D — stall-mechanism diagnostic (P2 / P2G / P2N)

**Status: tooling DESKTOP VERIFIED (synthetic fixtures + a real short desktop rehearsal). Hardware execution is NOT authorized by this document**: it needs the Manager's go-ahead and the hardware owner's (ROCK Pi Runtime & Performance) authorization. Diagnostic only; no production C++ or setting changes. The measured question: after a sampling *issue* frame the next no-event frame stalls (~94 % of the time in the accepted campaign, ~11–15 ms blocked in `postDraw`). Does the wait move into a measured `glFinish` (late GPU completion) or disappear with vsync off (swap/present scheduling)?

### K.1 Arms (each differs from P2 by exactly one variable; the audit proves it before and after the runs)
| Arm | Difference from P2 | Semantics (verified in `Rt004FramePhaseProbe.cpp`) | The phase log `CONFIG` line must say |
|---|---|---|---|
| **P2** | none (re-measured baseline in the same session) | `DEFER_FLUSH` UNSET (presence-based: never `0`), limiter on, vsync on | `limiter=on vsync=on glFinish=off` |
| **P2G** | `EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH=1` | presence-based (any value turns it on, so the audit insists on exactly `1`): a timed `glFinish()` after the app's draw, before the swap | `glFinish=on` |
| **P2N** | `EXPERIENCE_RUNTIME_VSYNC=off` | value-based: ONLY the exact string `off` acts (`0`, `false`, `OFF` are silently ignored, so the audit insists on exactly `off`): `ofSetVerticalSync(false)` after runtime setup | `vsync=off` |

`EXPERIENCE_RUNTIME_FRAME_LIMITER` is never set (limiter stays on). Everything else equals the accepted campaign: same binary, scene, media, resolution, 30 s warm-up/exclusions, 2 Hz compiled sampling cadence, the same private installation-settings copy (both auto-cycles off). `plan_interleaved.py` refuses `--defer-flush` with P2G/P2N. The `CONFIG` line records what the probe was ASKED to do; it cannot show the driver's real swap interval.

### K.2 Prerequisites on the Pi (all must hold; none is assumed)
1. Hardware owner and Manager authorization recorded (date, who).
2. The installed binary is the one of the accepted campaign: `sha256sum "$INSTALLED_BIN"` = `267d8f67f73e1ab7a07a81cc7cb4b1a944e3467c77bf43ec85fb6f61a5d1ec66`, and `verify_provenance.py --root "$REPO" --bin "$INSTALLED_BIN"` run as in C.2/C.3 (record, do not interpret). A different binary means a different experiment: stop and ask.
3. The accepted campaign's `campaign-installation-settings.json` (it is inside the archive `oc003_attr_20261108_1330`) is on the Pi, so the settings are byte-identical: `ACC=/home/conc/oc003_attr_20261108_1330`.
4. Unit scope known (the accepted campaign used the **system** scope), `sudo systemctl` works, display `:0`, no other graphical process, nobody touches the device (C.6 rules), no concurrent builds.
5. This package (regenerated, with `test_diag_tooling.py`) unpacked and `sha256sum -c`'d; run its tests on the Pi first: `python3 -I -W error $PKG/scripts/oc003/test_diag_tooling.py`.

### K.3 Commands (operator-run; same variables as section C: `$PKG $BIN $WORKDIR $INSTALLED_BIN $DISPLAY`; `MEDIA=media.auto.7021ad87`, `SCENE=blob-region-prototype`, `RES=1280x720` as in the accepted campaign — confirm)
Smoke gate first (3 arms, 1 round, 90 s each, ≈ 5 min; proves the controls are honoured on the Pi before the long run):
```bash
SM=$HOME/oc003d_smoke_$(date +%Y%m%d_%H%M)
python3 -I $PKG/scripts/oc003/plan_interleaved.py --bin "$BIN" --out "$SM" --media-id "$MEDIA" --scene "$SCENE" --resolution "$RES" \
    --conditions P2,P2G,P2N --order rotate --rounds 1 --duration 90 --installation-settings "$ACC/campaign-installation-settings.json"
python3 -I $PKG/scripts/oc003/run_campaign.py "$SM/plan.json" --workdir "$WORKDIR" --display "$DISPLAY" --stop-service --scope system \
    --watch "$INSTALLED_BIN" --min-events 60 --dry-run          # read: no PRE-FLIGHT PROBLEM; starts/stops nothing
python3 -I $PKG/scripts/oc003/run_campaign.py "$SM/plan.json" --workdir "$WORKDIR" --display "$DISPLAY" --stop-service --scope system \
    --watch "$INSTALLED_BIN" --min-events 60 | tee "$SM.console.txt"
grep -H '^CONFIG' "$SM"/*.phase.log | sed 's/targetFps.*window=/ ... window=/'    # each run: the planned glFinish=/vsync=/limiter=
```
Only if the smoke passes (exit 0, `CAMPAIGN COMPLETE`, `ok=true`, unit active again) and the Manager says go:
```bash
OUT=$HOME/oc003d_$(date +%Y%m%d_%H%M)
python3 -I $PKG/scripts/oc003/plan_interleaved.py --bin "$BIN" --out "$OUT" --media-id "$MEDIA" --scene "$SCENE" --resolution "$RES" \
    --conditions P2,P2G,P2N --order rotate --rounds 4 --duration 300 --installation-settings "$ACC/campaign-installation-settings.json"
python3 -I $PKG/scripts/oc003/run_campaign.py "$OUT/plan.json" --workdir "$WORKDIR" --display "$DISPLAY" --stop-service --scope system \
    --watch "$INSTALLED_BIN" --min-events 100 | tee "$OUT.console.txt"
```
12 runs, ≈ 61 min + analysis. `--order rotate` is deterministic: round 1 `P2 P2G P2N`, round 2 `P2G P2N P2`, round 3 `P2N P2 P2G`, round 4 = round 1 (recorded in `plan.json` as `round_orders`, each run has `position`). `plan.json` also holds each run's complete command (its full environment). The planner resolves `--out` to an absolute path (the runner executes from the binary's directory, so relative output paths cannot work).

### K.4 What the runner and tools prove for you (fail closed)
Before the first run and again at verification: every run of an arm has the identical environment; P2G differs from P2 by exactly `GLFINISH=1`, P2N by exactly `VSYNC=off`; `GLFINISH`/`VSYNC`/`FRAME_LIMITER`/`FRAME_PHASE_CONTROL` are unset everywhere else; `DEFER_FLUSH` unset in all three arms; no `EXPERIENCE_RUNTIME_*` variable outside the arm's table; one installation-settings copy, unchanged, auto-cycles off; inherited `EXPERIENCE_RUNTIME_*` variables are scrubbed. After each run: exit 124/130, evidence present, media/scene/resolution, no scene change, event reconciliation, and `verify_run.py` compares the probe's `CONFIG` `glFinish=/vsync=/limiter=` with the plan.

### K.5 Outputs and how to read them
`$OUT/analysis/summary.json` keeps every existing key unchanged and adds `stall` (per condition: pooled and per round; and `stall.differences` for `P2G-P2` and `P2N-P2`). `analysis/report.md` ends with a **MEASURED** part (frames, effective fps, p50/p95/p99/worst, long frames split into event / after-issue / after-collect / lag+2 / quiet, the stall rate of frames after an issue frame, `postDraw` and `glFinish` wall/CPU distributions, fence not-ready excess per run, per-round table, differences with per-round min..max and `sign_consistent`) and then a separate **Interpretation guide (NOT measured)**. Stand-alone: `python3 -I $PKG/scripts/oc003/stall_analysis.py <csv...>`.
A difference counts only if `sign_consistent` is true and it is larger than the round-to-round range of the P2 baseline. Compare against the P2 arm of THIS session; the 2026-11-08 campaign is only a sanity check (≈ 3 % long frames in P2).

### K.6 Expected signatures and limits (hypotheses, not results)
| If... | late GPU completion | swap / present scheduling |
|---|---|---|
| P2G: `glFinish` wall on issue frames | large (≈ the 11–15 ms moves here) | small |
| P2G: after-issue stall rate / `postDraw` blocked time | fall toward 0 (long frames may appear on the issue frame itself) | persist |
| P2N: after-issue stall rate / `postDraw` blocked time | unchanged | fall |
Limits: `glFinish` serialises CPU and GPU and drains ALL earlier GPU work, so a vanished stall alone is not proof; P2G's fence not-ready excess falls by construction; the `CONFIG` line cannot show the driver's real swap interval, so an unchanged P2N cannot tell "ignored" from "not the mechanism"; effective fps should stay near 30 in P2N because the limiter stays on; four rounds; fence not-ready counts are per-run totals, not per frame.

### K.7 Stop, rollback, handoff
Same as sections E, C.8 and H. Additionally stop (and report) if any run's `CONFIG` line does not match the plan, if the binary hash differs from section K.2, or if a run is long-frame-free in all arms (nothing to explain: report, do not tune). Return the same evidence package as H (the whole `$OUT` directory, the smoke directory, console outputs, preflight/restoration records) plus the `PKG` checksum used.

## L. OC-003-F — acquisition isolation P0–P5 and CPU RAW qualification (diagnostic only)
Authoritative hardware target: ROCK Pi 4B. Everything here is **diagnostic, off by default, operator-run**; the coding agent never operates the Pi. Nothing in this section changes production behaviour: with none of `EXPERIENCE_RUNTIME_CPU_RAW`, `EXPERIENCE_RUNTIME_PALETTE_DIAG` set, the runtime is byte-for-byte on its previous path.

### L.1 Arms (plan_interleaved.py `--conditions`)
| arm | FX GPU | GPU RAW | CPU RAW | purpose |
|---|---|---|---|---|
| P0 | – | – | – | no colour sampling (baseline) |
| P1 | on | – | – | FX GPU only |
| P2 | on | on | – | current production combination (FX + GPU RAW) |
| P3 | – | on | – | GPU RAW only (arm defined by OC-003-F as the symmetric one; not specified in the authorisation text) |
| P4 | – | – | on | **CPU RAW only** |
| P5 | on | – | on | **CPU RAW + FX GPU** |
| PCMP | – | on | on | correctness arm only: CPU RAW vs GPU RAW on the same frames. Never use PCMP for cost; it runs both paths together |
Every sampling arm uses 2 Hz and `PREDOMINANT_COLOR=1`. CPU RAW reads the decoder's existing frame through `VideoPlaybackService::currentPixels()` on the main thread, reduces it in place to the 160×90 linear-light grid (no full-frame copy, no retained pointer, no GPU readback, no second decoder) and runs the unchanged AVG/DOM/VIB code. `verify_run.py` checks the runtime's `AcquisitionMode: fxGpu= rawGpu= cpuRaw= palette=` line and the `cpu_raw` frame-CSV column against the plan for every run.

### L.2 Prerequisites
Source manifest and binary capability as in C.2/C.3; the binary must be a build of the OC-003-F sources (it prints `AcquisitionMode ... contract=oc003f-1`). Pick the media with `list_media_ids.py`; scene `blob-region-prototype`, resolution as the display opens it.
Compile the offline replay tool where the report will be produced (Pi or desktop; needs only a C++17 compiler):
```bash
c++ -std=c++17 -O2 -I $PKG/source/sketches/experience_runtime/src $PKG/source/sketches/experience_runtime/test/palette_replay.cpp -o $PKG/palette_replay
```

### L.3 Timing campaign (no palette capture, so analysis file I/O never contaminates timings)
```bash
SM=$HOME/oc003f_smoke_$(date +%Y%m%d_%H%M)
python3 -I $PKG/scripts/oc003/plan_interleaved.py --bin "$BIN" --out "$SM" --media-id "$MEDIA" --scene "$SCENE" --resolution "$RES" \
    --conditions P0,P1,P2,P3,P4,P5 --order rotate --rounds 1 --duration 90
python3 -I $PKG/scripts/oc003/run_campaign.py "$SM/plan.json" --workdir "$WORKDIR" --display "$DISPLAY" --stop-service --scope system --watch "$INSTALLED_BIN" --min-events 40 --dry-run
python3 -I $PKG/scripts/oc003/run_campaign.py "$SM/plan.json" --workdir "$WORKDIR" --display "$DISPLAY" --stop-service --scope system --watch "$INSTALLED_BIN" --min-events 40 | tee "$SM.console.txt"
```
Only after the smoke passes (exit 0, unit active again) and the Manager says go: same with `--rounds 4 --duration 300 --min-events 100`. Read `analysis/summary.json` / `report.md` (pairs P3-P0, P4-P0, P5-P0, P4-P3, P5-P4, P5-P1, P4-P2). The per-stage costs are in the `stream=CPURAW` stat lines (`sampler.csv`): `accessWallMs`/`accessCpuMs` (obtaining the borrowed pixels), `reduceWallMs` (160×90 extraction), `analyzeWallMs` (AVG + DOM/VIB), each wall and thread-CPU, p50/p95/max, plus `totalWallMs`. They are measured independently on purpose: on macOS the first `currentPixels()` after a new frame performs the AVFoundation→RGB conversion inside **access**; on the Pi (GStreamer) access is a reference.

### L.4 Output comparison and palette determinism (correctness; separate from timing)
```bash
PL=$HOME/oc003f_palette_$(date +%Y%m%d_%H%M)
python3 -I $PKG/scripts/oc003/plan_interleaved.py --bin "$BIN" --out "$PL" --media-id "$MEDIA" --scene "$SCENE" --resolution "$RES" \
    --conditions PCMP,P4 --rounds 1 --duration 150 --palette-diag --palette-grids 240 --palette-every 0.5
python3 -I $PKG/scripts/oc003/run_campaign.py "$PL/plan.json" --workdir "$WORKDIR" --display "$DISPLAY" --stop-service --scope system --watch "$INSTALLED_BIN" --min-events 40
for d in "$PL"/*PCMP*.palette "$PL"/*P4*.palette; do python3 -I $PKG/scripts/oc003/palette_report.py "$d" --replay $PKG/palette_replay; done
```
`palette_report.py` PASS/FAIL/N-A for: *replay* (stored grids re-analysed offline reproduce the runtime's CSV rows), *determinism* (two replays byte-identical), *cpu_vs_gpu* (PCMP only: median per-pair mean |ΔE| ≤ 0.02 in sRGB-encoded space and median |ΔAVG| ≤ 0.01 — informational engineering bounds, not a product contract). A FAIL is a finding to report, not something to tune away.

### L.5 Stop and report
Stop on any `GL error`, a CPU RAW gap rate above 5 % in the steady state (`verify_run.py` flags it), `noPixels`/`badFormat`/`dimMismatch` greater than 0 in the `stream=CPURAW` line, or a `cpu_raw` count of 0 in P4/P5. Restore the production unit exactly as in C.8. Return the same evidence as H plus the palette directories and the `palette_report.json` files.

### L.6 Multi-scene correctness plan (generations across scene changes)
```bash
CR=$HOME/oc003f_corr_$(date +%Y%m%d_%H%M)
python3 -I $PKG/scripts/oc003/plan_interleaved.py --bin "$BIN" --out "$CR" --media-id "$MEDIA" --scene "$SCENE" --resolution "$RES" \
    --kind correctness --cycle-scenes 30 --conditions P5 --rounds 1 --duration 220 --palette-diag
python3 -I $PKG/scripts/oc003/run_campaign.py "$CR/plan.json" --workdir "$WORKDIR" --display "$DISPLAY" --stop-service --scope system --watch "$INSTALLED_BIN"
```
`--kind correctness` turns scene auto-cycle on (every N s) and media auto-cycle off in the run's private settings copy; the plan then expects all five scenes, and `run_campaign.py` runs `verify_generations.py <plan.json> --require-all` after `verify_run.py` (a failure is a campaign failure). It checks: five distinct scenes in the frame stream, the runtime's `AcquisitionMode` line against the plan, strictly increasing generation lines per stream, that RAW / CPU RAW began a generation for every scene generation, and that the first palette row of each generation has no drift. Use a P5 or P2 plan to exercise the FX generation check too (PCMP has FX off). Needs `*_STATS=1`, which the planner sets.

### L.7 One-command qualification (operator-run; orchestration only)
```bash
python3 -I $PKG/scripts/oc003/run_qualification.py --out $HOME/oc003f_qual --bin "$BIN" --workdir "$WORKDIR" --media-id "$MEDIA" --scene "$SCENE" --resolution "$RES" \
    --stop-service --scope system --display "$DISPLAY" --timing-rounds 4 --timing-duration 300 --min-events 100 --dry-run     # read the commands first
```
Stages `unit,tooling,plan,timing,palette` (the C++ test stage needs `make`/a compiler and the unpacked `source/`; skip it on the Pi with `--stages tooling,plan,timing,palette` if absent). It writes `qualification.json` / `qualification.md` and exits non-zero if any stage fails. It starts the runtime only through `run_campaign.py` and never touches git.

### L.8 Process monitor and platform access cost
`run_campaign.py` starts `sysmon.py` beside each run (RSS every 5 s; on Linux also thermal zones and CPU frequencies; unavailable values stay blank) and writes `<tag>.sys.csv` and `analysis/sysmon.json`. It is an external observer and cannot change a run. `cpu_access_cost.py` runs CPU RAW alone per scene for the platform access-cost comparison; on the Pi (GStreamer) the expectation is that `accessWallMs` is a reference, not a conversion — **this is unmeasured until run on the ROCK Pi 4B**. Runs ended by `timeout -s INT` do not write `palette_summary.json` (the app installs no signal handler); `palette_report.py` records that and proceeds.

### L.9 Shared-tree and binary identity warning
Several sessions edit this working tree. Always identify the binary by sha256, run campaigns from a frozen copy laid out as `<root>/sketches/experience_runtime/bin` (the media root is `<bin>/data/../../../../assets/shared/media`), and do not start a second `experience_runtime` while a campaign runs — the runner aborts on any foreign process, which is the intended fail-closed behaviour.
