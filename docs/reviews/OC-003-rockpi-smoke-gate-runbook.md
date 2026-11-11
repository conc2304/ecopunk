# OC-003 — ROCK Pi smoke gate runbook (operator-run)

Prepared by the coding agent, which is barred by `.claude/CLAUDE.md` from SSH, deploy and service changes. Nothing here has been executed. Run on the Pi as the normal user; never run a second graphical ExperienceRuntime while the production service is up.

## 0. Preconditions / stop conditions
- STOP if `git status` on the Pi shows modified files you did not expect to overwrite, or if you cannot record the service's exact state (step 1).
- Never stage, commit or push.

## 1. Inspect and record (read-only)
```bash
cd ~/of_v0.12.1_linuxaarch64_release/apps/myApps/EcopunkVideoCollage
git status --short | tee ~/oc003_pi_status_before.txt
git log -1 --format=%H
sha256sum sketches/experience_runtime/bin/experience_runtime | tee ~/oc003_pi_bin_before.sha
systemctl --user list-units --type=service | grep -i -E "ecopunk|experience"   # find the production unit name (<UNIT>)
systemctl --user cat <UNIT> > ~/oc003_unit_before.txt; systemctl --user is-active <UNIT>
cp sketches/experience_runtime/bin/experience_runtime ~/experience_runtime.before   # restoration copy
```

## 2. Bring the desktop changes over WITHOUT overwriting Pi edits
Files changed by OC-003 (compare each against the Pi's version before copying; merge by hand if the Pi differs from its git HEAD):
`sketches/experience_runtime/src/{Rt004FramePhaseProbe.h,Rt004FramePhaseProbe.cpp,ExperienceRuntime.cpp}`, `shared/src/video-playback/{VideoSelectionPolicy.h,VideoSelectionPolicy.cpp,VideoPlaybackService.h,VideoPlaybackService.cpp}`, and the scripts `scripts/oc003/*.py`. The earlier DOM/VIB work (turn 2) must already be on the Pi; if not, that is a larger transfer and needs its own review.

Build (production service still running is fine; building does not start the runtime): `cd sketches/experience_runtime && make Release -j4`.

## 3. Verify the Pi binary has the controls
```bash
B=sketches/experience_runtime/bin/experience_runtime
sha256sum $B                                         # report this value
strings $B | grep -c -E "EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID|EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES"   # expect >= 2
python3 scripts/oc003/verify_run.py --help | head -2
```

## 4. Stop the production service, pick media, smoke-test (bounded)
```bash
systemctl --user stop <UNIT>; systemctl --user is-active <UNIT>    # must report inactive before ANY run
pgrep -x experience_runtime || echo "none running"
# catalog listing (rejects "list", prints id + file; it keeps running until stopped: use timeout)
EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID=list timeout -s TERM 20 $B 2>&1 | grep VideoVerification
```
Pick the longest clip (fewest loops; check durations from a pinned start: the `VideoVerification ... ACTIVE ... duration=` line). Plan and run each condition ONCE, ~60 s, with a short warm-up so steady-state rows exist:
```bash
python3 scripts/oc003/plan_interleaved.py --bin $PWD/$B --out ~/oc003_smoke --media-id <ID> --scene <SCENE> --resolution <WxH> --rounds 1 --duration 60
# run the 3 printed commands one at a time, adding EXPERIENCE_RUNTIME_FRAME_PHASE_WARMUP_S=10 (replace WARMUP_S=30)
python3 scripts/oc003/verify_run.py ~/oc003_smoke/plan.json --require-all
```
Check in each stdout log: P1/P2 `effMode=deferred gpuLinear=1 defer=fence`, `discardTimeout=0 discardWaitFailed=0`, DOM/VIB valid counts in P2, no crash or OOM (`dmesg | tail`), process exits after the timeout.

## 5. Restore and confirm
```bash
pgrep -x experience_runtime || echo "none running"
systemctl --user start <UNIT>; sleep 5; systemctl --user is-active <UNIT>
systemctl --user cat <UNIT> | diff - ~/oc003_unit_before.txt && echo "unit unchanged"
git status --short | diff - ~/oc003_pi_status_before.txt   # only the intended OC-003 files may differ
```
Report: Pi binary sha256, media id, per-condition results, and the restoration confirmations above.

## 6. Full campaign (after the smoke gate passes)
`python3 scripts/oc003/plan_interleaved.py --bin <abs binary> --out <dir> --media-id <ID> --scene <SCENE> --resolution <WxH> --rounds 4 --duration 300` (service stopped; restore afterwards as in step 5), then `verify_run.py <dir>/plan.json --require-all` and `aggregate_frames.py`.
