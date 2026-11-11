# ExperienceRuntime — Rock Pi Validation Harness Inventory and Execution Runbook

**Date:** 2026-10-13 · **Basis:** current working tree (HEAD `44044c1` + uncommitted RT-004 diagnostics/Temporal release correction + Rock Pi port fixes in `config.make`, `GridSystem.cpp`, `MediaCatalog.cpp`, `TFImageCycler.h`, `TFParameterPanel.cpp`, `scripts/sync-shared-shader-runtime-assets.py`). Investigation only — nothing was run on the Rock Pi, no code changed.

## Quick start — run the full production experience

_Added 2026-10-19 (HUD & Scene Experience v0.3)._ A plain run with no harness env vars is the production experience. It has the full five-scene roster: Blob → Temporal → Blueprint → Fragment → Quadrant, starting on Blob. It includes every mode, the HUD shell and the bottom navigation row, and saves installation settings to `bin/data/installation-settings.json`. Sections 1–5 below were written for the earlier three-scene build, so read their scene counts and keys with that in mind.

### Mac (desktop)

```bash
cd ~/openFrameworks/apps/myApps/EcopunkVideoCollage/sketches/experience_runtime
make Release -j8
EXPERIENCE_RUNTIME_HUD_SKIN=dark-moss-experimental make RunRelease
```

To launch the app directly instead: `cd bin && EXPERIENCE_RUNTIME_HUD_SKIN=dark-moss-experimental ./experience_runtime.app/Contents/MacOS/experience_runtime`. Leave out `EXPERIENCE_RUNTIME_HUD_SKIN=…` to get the plain wireframe HUD.

### Rock Pi 4B (manual run over SSH)

The values below are for this board: user `conc`, host `rockpi-4b`, X display `:0` on `vt7`, HDMI output `HDMI-1`. Check `HDMI-1` with `xrandr` on the device. The app needs a real X11 display, and it has no keyboard unless one is plugged into the Rock Pi.

**1. Connect and build.** Build on the device. `scripts/deploy.sh` can also sync the tree and build it remotely.

```bash
ssh conc@rockpi-4b
cd ~/of_v0.12.1_linuxaarch64_release/apps/myApps/EcopunkVideoCollage/sketches/experience_runtime
make Release -j4
```

**2. Free the display.** Only one GL app can own the screen. Check what owns it, and stop it first. Stopping a service that owns X also stops its X server. This is an operator decision:

```bash
pgrep -a Xorg                                        # prints a line if an X server is running
systemctl is-active ecopunk-runtime                  # "active" means the service owns the display (see step 5)
sudo systemctl stop ecopunk-runtime                  # operator only; stop whatever else owns the display the same way
pgrep -af bin/experience_runtime                     # stop any older instance first (Esc on its keyboard, or pkill)
```

**3. Start the app.** Pick 3a if no X server is running, or 3b if one is.

**3a. No X running: start X with the experience as its only client.** First create the session script once:

```bash
cat > ~/.xinitrc-ecopunk <<'EOF'
#!/bin/bash
command -v openbox >/dev/null && openbox &
xset s off; xset -dpms; xset s noblank           # no screen blanking
xrandr --output HDMI-1 --mode 1920x1080 --rate 60 || true
cd ~/of_v0.12.1_linuxaarch64_release/apps/myApps/EcopunkVideoCollage/sketches/experience_runtime
exec env EXPERIENCE_RUNTIME_HUD_SKIN=dark-moss-experimental ./bin/experience_runtime >> ~/ecopunk-run.log 2>&1
EOF
chmod +x ~/.xinitrc-ecopunk
```

Then start X on the console VT. `nohup` keeps it running after SSH disconnects:

```bash
nohup startx ~/.xinitrc-ecopunk -- :0 vt7 -keeptty -nolisten tcp > ~/ecopunk-startx.log 2>&1 &
tail -f ~/ecopunk-run.log                        # app log; Ctrl-C stops tailing, not the app
```

Starting X from an SSH session (not a console) may fail with _"Only console users are allowed to run the X server"_. That is the Xorg wrapper policy. Two ways around it:

- Start from the Rock Pi's own console/keyboard: the same `startx ~/.xinitrc-ecopunk -- :0 vt7` without `nohup`.
- Have the operator allow it once, in `/etc/X11/Xwrapper.config`: `allowed_users=anybody` and `needs_root_rights=yes`. That is a system change; don't make it without the device owner's approval.

**3b. X already running (desktop session or a kept-alive X):** attach to that display and run the binary directly:

```bash
export DISPLAY=:0
export XAUTHORITY=$(ps -ef | grep '[X]org' | grep -o -- '-auth [^ ]*' | cut -d' ' -f2)   # only if X was started by startx/another session
cd ~/of_v0.12.1_linuxaarch64_release/apps/myApps/EcopunkVideoCollage/sketches/experience_runtime
nohup env EXPERIENCE_RUNTIME_HUD_SKIN=dark-moss-experimental ./bin/experience_runtime > ~/ecopunk-run.log 2>&1 &
```

**4. Fullscreen, stop, restore.**

- The window opens at 1280×720. Press `f` on the Rock Pi keyboard for fullscreen. Or turn on _launch fullscreen_ in the installation settings, which persists in `bin/data/installation-settings.json`.
- To stop: `Esc` on the Rock Pi keyboard, or over SSH `pkill -f bin/experience_runtime`. In 3a, X exits with the app.
- To restore whatever you stopped in step 2, start it again with `sudo systemctl start <service>` (operator).
- With no keyboard attached, keys can be sent with `xdotool`; see §1 _Sending keys to a running instance_.

**5. Run as a service (optional).** If the device has an `ecopunk-runtime` systemd service installed, these are the commands to manage it. The unit file is not in this repository, so check it on the device before relying on it.

| Action                  | Command                                           |
| ----------------------- | ------------------------------------------------- |
| Start                   | `sudo systemctl start ecopunk-runtime`            |
| Stop                    | `sudo systemctl stop ecopunk-runtime`             |
| Restart                 | `sudo systemctl restart ecopunk-runtime`          |
| Check status            | `systemctl status ecopunk-runtime`                |
| Disable startup at boot | `sudo systemctl disable ecopunk-runtime`          |

### Keys (HUD-006 v2)

| Key               | Action                                                                                                                                   |
| ----------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `[` / `]`         | previous / next scene                                                                                                                    |
| `p` / `n`         | previous / next media (previous wraps at the start of history)                                                                           |
| `m`               | NEXT MODE (pattern / mode / strategy of the current scene)                                                                               |
| `2`               | Regenerate (Blueprint) · `1` reset the current scene                                                                                     |
| `e`               | ENERGY adjustment: arrows adjust, Enter closes                                                                                           |
| `f` / `h` / `Esc` | native fullscreen / HUD on-off / quit                                                                                                    |
| `i`               | IMMERSIVE: the scene over the whole window, HUD hidden, no OS fullscreen. Any key or click exits, and that first input does nothing else |
| `v` / `V`         | next HUD skin (manual) / back to the day-night schedule (see below)                                                                      |
| arrows + `Enter`  | HUD button row: PREV SCENE · NEXT SCENE · NEXT MODE · PREV MEDIA · NEXT MEDIA · FULL SCREEN                                              |

### HUD skins: day/night schedule and switching

Three Blueprint v2 skin packages are installed in `assets/hud/skins/`:

| Skin ID                   | Look                           | Period          | Text palette                       |
| ------------------------- | ------------------------------ | --------------- | ---------------------------------- |
| `plywood-marble`          | Plywood + Marble (light)       | day             | dark ink, with light button labels |
| `dark-moss-experimental`  | Dark Moss (dark)               | night (default) | light text                         |
| `abyssal-bioluminescence` | Abyssal Bioluminescence (dark) | night           | light text                         |

**Automatic (normal runs).** `assets/hud/skins/skin-schedule.json` picks the skin by local time. It shows the first listed day skin from `dayStart` (07:00) and the first night skin from `nightStart` (19:00). It checks about once a second and switches only when the period changes. To change the times or the order, edit that file and restart the app.

```json
{
  "dayStart": "07:00",
  "nightStart": "19:00",
  "day": { "skins": ["plywood-marble"], "palette": "day" },
  "night": {
    "skins": ["dark-moss-experimental", "abyssal-bioluminescence"],
    "palette": "night"
  }
}
```

**Manual.**

- `v` loads the next skin: day skins first, then night skins, wrapping. This pauses the schedule.
- `V` (Shift+v) returns to the schedule.
- Switching is live, with no restart. A skin that fails to load keeps the current one.

**Pin one skin.** Launch with `EXPERIENCE_RUNTIME_HUD_SKIN=<skin-id>`. That skin stays until you press `v`; the schedule is not used.

```bash
cd sketches/experience_runtime/bin
./experience_runtime.app/Contents/MacOS/experience_runtime                                            # schedule (day/night)
EXPERIENCE_RUNTIME_HUD_SKIN=plywood-marble ./experience_runtime.app/Contents/MacOS/experience_runtime   # pinned
```

On the Rock Pi, use the same commands with `DISPLAY=:0`, plus `EXPERIENCE_RUNTIME_FULLSCREEN=1` for native fullscreen. Harness runs ignore the schedule unless `EXPERIENCE_RUNTIME_SKIN_SCHEDULE=1` is set.

**Adding a skin.**

1. Put the package in `assets/hud/skins/<skin-id>/`. The folder name must equal `skinId`, and `skin.json` must declare `"blueprintVersion": 2`.
2. Add its ID to `day.skins` or `night.skins` in `skin-schedule.json`.
3. Restart the app.

Each skin may also ship shared button faces: `controls/button/idle.png`, `focused.png` and `active.png`, 80×80 each, used by all six buttons. All three are optional and load independently.

v1 skins are rejected (`SKIN_BLUEPRINT_VERSION_MISMATCH`), and an optional `controls/scene_next/pressed.png` must be 81×81 (see `docs/hud-art/blueprint-v2-reference/HUD_ART_HANDOFF.md`).

## Summary (three-scene baseline, 2026-10-13)

This summary and sections 1–5 describe the three-scene build. For the current five-scene roster and skins, see the quick start above.

- The binary registers **three production scenes**: `blob-region-prototype` (Blob), `temporal-fields` (Temporal), `blueprint_emergence` (Blueprint) — ring order Blob → Temporal → Blueprint (`ExperienceRuntime.cpp:27-38`). Fragment Trail (RT-006) is **not** in this tree.
- Scene switching **is implemented**: `]` next scene, `[` previous scene (`InputRouter.cpp:41-42`). The running instance was launched over SSH, so it has no keyboard unless one is attached to the Rock Pi.
- **Fastest way to see the other scenes with no new code:**
  1. On the running instance: a USB keyboard on the Rock Pi, pressing `]` / `[` (optionally `s` to capture). `xdotool` is an alternative if the operator chooses to install it.
  2. On a fresh instance: start directly in a scene with `EXPERIENCE_RUNTIME_TEMPORAL_SCENE=1` or `EXPERIENCE_RUNTIME_BLUEPRINT_SCENE=1`.
  3. Automated: the existing RT-004 driver (`EXPERIENCE_RUNTIME_STALL_DRIVER=1` with script `smoke-circuit`, `release-verify` or `temporal-cycles`) injects the same keys and exits by itself.
- Every app-level harness launches **its own** `experience_runtime` process. It is not safe to run against the current interactive instance: two GL apps on the same display and GPU contaminate each other's timing and visuals. The operator must decide when to stop the interactive instance; this runbook does not stop it.
- Main Rock Pi blockers, none of them test failures:
  - the tracked unit-test binaries are **macOS x86_64 Mach-O** and must be rebuilt;
  - the RT-004 `memattr_supervisor.py` / `pi_validation.py` tooling is calibrated for the **Pi 3B** (VC4 CMA thresholds, `vcgencmd`, VC4 `bo_stats`) and is not valid on the RK3399/Panfrost without Architecture review;
  - three older harnesses report RSS as −1 on Linux.

## 1. Launch and scene-selection controls (current source)

| Control                                                                                 | Effect                                                         | Source                                                 |
| --------------------------------------------------------------------------------------- | -------------------------------------------------------------- | ------------------------------------------------------ |
| (none)                                                                                  | Blob first; ring Blob→Temporal→Blueprint                       | `ofApp.cpp` scene install; `ExperienceRuntime.h:95-99` |
| `EXPERIENCE_RUNTIME_TEMPORAL_SCENE=1`                                                   | start in Temporal (full ring)                                  | `ofApp.cpp` `wantTemporalScene`                        |
| `EXPERIENCE_RUNTIME_BLUEPRINT_SCENE=1`                                                  | start in Blueprint (full ring)                                 | `ofApp.cpp` `wantBlueprintScene`                       |
| `]` / `[`                                                                               | next / previous scene (rejected during transitions)            | `InputRouter.cpp:41-42`                                |
| `n` / `p` (either case)                                                                 | next / previous canonical media (Blob, Temporal, Blueprint)    | `InputRouter.cpp:15-18`                                |
| `1` / `2`                                                                               | Reset / Regenerate (Blueprint RESEED)                          | `InputRouter.cpp:7-8`                                  |
| `h`, `f`, `Esc` (letters either case)                                                   | toggle HUD, fullscreen, exit                                   | `InputRouter.cpp:36-40`                                |
| arrows / Enter                                                                          | HUD focus menu (includes NEXT MEDIA)                           | `InputRouter.cpp:25-29`                                |
| `s`                                                                                     | scene-only capture → `bin/data/captures/scene_only_manual.png` | `ExperienceRuntime.cpp:425-427`                        |
| `3`, `4`, `5`, `6`, `c`                                                                 | FakeScene-only dev hooks; no effect in production scenes       | `ExperienceRuntime.cpp:434-488`                        |
| `EXPERIENCE_RUNTIME_STALL_DRIVER=1` + `EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=<script>` | automated key injection; `exit` at the end                     | `Rt004StallDriver.cpp`                                 |

### Sending keys to a running instance without a keyboard

Requires `xdotool` on the Rock Pi (operator-installed). Focus the runtime window, then send the key by its X keysym name:

```bash
export DISPLAY=:0
WIN=$(xdotool search --sync --onlyvisible --pid "$(pgrep -f bin/experience_runtime)" | head -1)
xdotool windowactivate --sync "$WIN" key --clearmodifiers n
```

For example, `DISPLAY=:0 xdotool key bracketright` sends `]`. Key names:

| Key                                 | xdotool name                       |
| ----------------------------------- | ---------------------------------- |
| letters and digits (`n`, `p`, `h`…) | the same character (`n`, `p`, `h`) |
| `]` / `[`                           | `bracketright` / `bracketleft`     |
| arrows                              | `Left` `Right` `Up` `Down`         |
| Enter                               | `Return`                           |
| Esc                                 | `Escape`                           |

Existing driver scripts (test-only seam, no new code):

| Script                                                                                                                           | Sequence                                                             | Ends                |
| -------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------- | ------------------- |
| `smoke-circuit`                                                                                                                  | Blob→Temporal→Blob→Temporal→Blueprint (+RESEED)→Temporal             | ~26 s after startup |
| `release-verify`                                                                                                                 | Blob→Temporal→Blob→Temporal→Blueprint, then exits **from Blueprint** | ~23 s               |
| `temporal-cycles`                                                                                                                | Blob plus 1–3 × (Temporal↔Blob)                                      | ~26 s               |
| `first-activation`                                                                                                               | single Blob→Temporal→Blob                                            | ~22 s               |
| `temporal-steady`, `temporal-memattr`, `ab-compact`, `blob-short`, `upload-split`, `playhead-ab`, `temporal-ab`, `ambient-split` | measurement scripts                                                  | —                   |

All commands below run on the **Rock Pi**, from `<repo>/sketches/experience_runtime`, after `export DISPLAY=:0`. Logs go to `~/rockpi-validation/` (`mkdir -p ~/rockpi-validation` first). For app-level runs, read the `RESULT:` line, not the exit code: older harnesses exit 0 even on failure.

## 2. Inventory of existing suites

**Unit / headless** (no GL, no display; safe to run alongside the interactive instance):

| Suite                        | Location                                          | Command                                                                                                             | Covers                                                                                                                                    |
| ---------------------------- | ------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| ExperienceRuntime units      | `sketches/experience_runtime/test/Makefile.tests` | `make -B -f Makefile.tests test` (`focus`, `skin`, `blueprint-semantic` are extra targets)                          | lifecycle state, scene-switch controller, Blob/Temporal/Blueprint semantic mapping, Session 2 effects integration, skin loader, HUD focus |
| HUD compositor               | `shared/src/hud-compositor-test/Makefile.tests`   | `make -B -f Makefile.tests test-all`                                                                                | presentation, real-frame, media viewport, typography                                                                                      |
| Shared Video                 | `shared/src/video-playback/test/Makefile.tests`   | `make -B -f Makefile.tests test`                                                                                    | catalog, selection policy, resume windows, advance modes                                                                                  |
| Shared Effects knowledge     | `shared/src/video-effects/test/Makefile.tests`    | `make -B -f Makefile.tests test`                                                                                    | effect knowledge extension                                                                                                                |
| Temporal timeline            | `sketches/temporal-fields/test/Makefile.tests`    | `make -B -f Makefile.tests test`                                                                                    | preset timeline/binding/easing                                                                                                            |
| HUD skin manifest            | `tools/hud-skin-manifest/test/Makefile.tests`     | `make -B -f Makefile.tests test`                                                                                    | skin manifest parsing                                                                                                                     |
| Static checks                | `scripts/`                                        | `python3 ../../scripts/check-video-effect-drift.py`, `sh ../../scripts/check-hud-library-uniqueness.sh`             | effect-registry/shader drift, HUD library duplication                                                                                     |
| RT-004 supervisor self-tests | `scripts/rt004/`                                  | `python3 ../../scripts/rt004/test_memattr_supervisor.py -v`, `python3 ../../scripts/rt004/test_pi_validation.py -v` | watchdog/orchestrator using dummy processes only                                                                                          |

**Application-level, real GPU** (each launches `bin/experience_runtime`, exclusive use of display and GPU):

| Harness (env var `=1`)                                                                                                 | Scenes            | Purpose                                                                                                      | Captures / evidence                  | Desktop runtime |
| ---------------------------------------------------------------------------------------------------------------------- | ----------------- | ------------------------------------------------------------------------------------------------------------ | ------------------------------------ | --------------- |
| `EXPERIENCE_RUNTIME_GL_HARNESS`                                                                                        | FakeScene         | GL state restoration                                                                                         | stdout                               | short           |
| `EXPERIENCE_RUNTIME_BLOB_RENDER_AB_HARNESS`                                                                            | Blob              | render-path pixel A/B, exits                                                                                 | stdout                               | seconds         |
| `EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS` (soak variant: `EXPERIENCE_RUNTIME_BLOB_SOAK_HARNESS`)                     | Blob              | lifecycle, stale state, captures                                                                             | `captures/blob_lifecycle_*.png`      | ~1–2 min        |
| `EXPERIENCE_RUNTIME_TEMPORAL_LIFECYCLE_HARNESS`                                                                        | Temporal          | lifecycle, reactivation stale-history, effects, shutdown (297 checks)                                        | `captures/temporal_lifecycle_*.png`  | ~1 min          |
| `EXPERIENCE_RUNTIME_TEMPORAL_WARM_HARNESS`                                                                             | full ring, 8 legs | first/repeat activation timing, RSS per leg (Linux supported), ambient render hash, mask load/release cycles | `captures/temporal-warm/results.txt` | ~70 s           |
| `EXPERIENCE_RUNTIME_SWITCH_HARNESS`                                                                                    | Blob+Temporal     | RT-003 switching (5,351 checks)                                                                              | stdout                               | ~3 min          |
| `EXPERIENCE_RUNTIME_TWO_SCENE_ACCEPTANCE`                                                                              | Blob+Temporal     | RT-002 20-cycle acceptance; uses `grabScreen` (heavy)                                                        | `captures/`                          | ~10 min         |
| `EXPERIENCE_RUNTIME_THREE_SCENE_HARNESS`                                                                               | full ring         | RT-005: 10 circuits / 30 switches, GL errors, FBO, RSS (Linux supported)                                     | `captures/rt005-three-scene/`        | several min     |
| `EXPERIENCE_RUNTIME_PARITY_HARNESS`                                                                                    | full ring         | three-scene production parity soak                                                                           | stdout                               | several min     |
| `EXPERIENCE_RUNTIME_BLUEPRINT_HARNESS`                                                                                 | starts Blueprint  | roster check (known 16/18: capture-contaminated timing)                                                      | `captures/`                          | short           |
| `EXPERIENCE_RUNTIME_BLUEPRINT_HUD_HARNESS`                                                                             | full ring         | Blueprint HUD semantics/RESEED (26 checks)                                                                   | `captures/blueprint-hud/`            | short           |
| `EXPERIENCE_RUNTIME_RESUME_HARNESS` (+ `EXPERIENCE_RUNTIME_VIDEO_DWELL=10,20 EXPERIENCE_RUNTIME_VIDEO_RESUME_TRACE=1`) | full ring         | Shared Video resume windows, Previous                                                                        | `captures/resume-window/`            | several min     |
| `EXPERIENCE_RUNTIME_CADENCE_HARNESS`                                                                                   | full ring         | Blueprint cycle-driven media cadence                                                                         | stdout                               | ~12 min         |
| `EXPERIENCE_RUNTIME_FOCUS_HARNESS`                                                                                     | —                 | HUD focus menu                                                                                               | stdout                               | short           |
| `EXPERIENCE_RUNTIME_SKIN_HARNESS`                                                                                      | —                 | HUD skin loader                                                                                              | stdout                               | short           |
| `EXPERIENCE_RUNTIME_HUD_ART_EXPORT` (+ `…_HUD_ART_EXPORT_DIR`)                                                         | —                 | HUD art reference export                                                                                     | export dir                           | short           |

**Diagnostics** (env-gated, any normal or driver run):

| Env var                                                             | Output                                                                                                                             |
| ------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| `EXPERIENCE_RUNTIME_STALL_PROBE=1` (+ `…_LOG`, `…_PROBE_MS`)        | scope tree for long frames, per-label stats, NOTE events (`mono=`)                                                                 |
| `EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE=1` (+ `…_LOG`, `…_WINDOW_MS`) | `CONFIG` line with **glRenderer/glVersion**; per-segment frame timing; RSS/RssAnon; Temporal residency; scene/media/effects EVENTs |
| `EXPERIENCE_RUNTIME_GL_ERROR_TRACE=1` (with the stall probe)        | GL error attributed per scope; one deliberate self-test 0x500 proves the tracer is active                                          |

## 3. Rock Pi compatibility

| Item              | Status                                                                                                                                                                                                                                                                                                                                                              |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Unit suites       | Portable C++17 (`c++`, relative include paths into the OF tree). **Blocker:** the tracked binaries in each test directory are **Mach-O x86_64**, so plain `make … test` may treat them as up to date and fail with "Exec format error". Use `make -B` (rebuilds them in the Rock Pi copy only).                                                                     |
| App harnesses     | Compile on Linux: all macOS code is behind `__APPLE__`. On Linux the **Switch, Two-scene acceptance and Skin loader** harnesses report RSS as −1 (no Linux branch), so their memory figures are not evidence. Temporal-warm, Three-scene and the frame-phase probe read `/proc` correctly.                                                                          |
| GL / GLSL         | Window requests GL 2.1 (`main.cpp`, `ofGLWindowSettings` default), so OF uses the fixed-function `ofGLRenderer`. All 24 runtime shaders are `#version 120`, within Panfrost's GL 3.1. Compilation and execution still have to be **observed** on the device (§4 step 3). Shader files being present does not prove they execute.                                    |
| Display           | Harnesses need X11 (`DISPLAY=:0`) and a real context; nothing is headless. Window size is 1280×720.                                                                                                                                                                                                                                                                 |
| Media / fixtures  | Canonical media from `assets/shared/media/` (26 .mp4); ambient `bin/data/backgrounds/`; synced shaders, knowledge pack and Blueprint assets via the `config.make` sync targets; skin fixtures in `test/skin_fixtures/`. GStreamer decode path on RK3399 has not been tested (HW decoder vs software).                                                               |
| RT-004 Pi tooling | `memattr_supervisor.py` thresholds are VC4 CMA-specific (preflight CmaFree > 96 MiB, floor 40 MiB). `pi_validation.py` preflight requires `vcgencmd` and **fails closed** on the Rock Pi. VC4 `bo_stats` paths do not exist under Panfrost. **Not valid on the Rock Pi** without an Architecture-reviewed profile. Use plain `timeout` caps and the probes instead. |
| Deploy            | `scripts/deploy.sh` reads the git-ignored `scripts/deploy.local.env` (currently `pi-3b`) and builds `sketches/experience_runtime` remotely. A Rock Pi needs its own host/path values (operator).                                                           |

## 4. Ordered Rock Pi runbook

"Safe now" means the step can run while the interactive instance keeps running. Every other step needs the interactive instance stopped **by the operator**.

| #   | Goal                                           | Command (Rock Pi unless noted)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   | Expected / evidence                                                                                                                                                                              | Pass criteria                                                                                                         | Safe now?                                     |
| --- | ---------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------- | --------------------------------------------- | ------------------------------------- | ------- |
| 0   | Mac baseline (optional)                        | Mac: `cd sketches/experience_runtime/test && make -f Makefile.tests test`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        | all suites PASS                                                                                                                                                                                  | all PASS                                                                                                              | n/a                                           |
| 1   | Headless logic on ARM64                        | `for d in sketches/experience_runtime/test shared/src/hud-compositor-test shared/src/video-playback/test shared/src/video-effects/test sketches/temporal-fields/test tools/hud-skin-manifest/test; do (cd <repo>/$d && make -B -f Makefile.tests $( [ "$d" = shared/src/hud-compositor-test ] && echo test-all                                                                                                                                                                                                                                                                                                   |                                                                                                                                                                                                  | echo test )) ; done 2>&1 \| tee ~/rockpi-validation/unit.log`plus`python3 ../../scripts/check-video-effect-drift.py`  | per-suite "N checks, 0 failures" / PASS lines | every suite passes, no compile errors | **yes** |
| 2   | Scene availability on the **running** instance | Attached keyboard: `]` → Temporal, `]` → Blueprint, `]` → Blob, then `[` backwards; `s` in each scene. Without a keyboard, only if the operator installs xdotool: `DISPLAY=:0 xdotool search --sync --onlyvisible --pid $(pgrep -f bin/experience_runtime) windowactivate --sync key bracketright`                                                                                                                                                                                                                                                                                                               | the scene changes after a fade; "SceneManager: NextScene accepted" in the instance's own terminal; `bin/data/captures/scene_only_manual.png` (overwritten per press, so copy it between presses) | all three scenes render; no crash                                                                                     | **yes** (keyboard path)                       |
| 3   | GPU / shader compilation + GL info             | `timeout 60 env EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE=1 EXPERIENCE_RUNTIME_FRAME_PHASE_LOG=$HOME/rockpi-validation/gl-phase.log EXPERIENCE_RUNTIME_STALL_PROBE=1 EXPERIENCE_RUNTIME_GL_ERROR_TRACE=1 EXPERIENCE_RUNTIME_STALL_PROBE_LOG=$HOME/rockpi-validation/gl-stall.log EXPERIENCE_RUNTIME_STALL_DRIVER=1 EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=smoke-circuit EXPERIENCE_RUNTIME_VIDEO_DWELL=900,900 ./bin/experience_runtime > ~/rockpi-validation/gl-stdout.log 2>&1`                                                                                                                                     | `CONFIG glRenderer=…Panfrost…`; "ShaderLibrary: loaded:" lines in each scene; `glError,…` rows in the probe summary; scene-owner EVENTs for all three scenes                                     | no shader load/link errors; GL errors only in known scopes (record the 0x0500 scopes); all scenes reached; clean exit | no                                            |
| 4   | Blob render + lifecycle                        | `EXPERIENCE_RUNTIME_BLOB_RENDER_AB_HARNESS=1 ./bin/experience_runtime > ~/rockpi-validation/blob-ab.log 2>&1`; then `EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS=1 timeout 900 ./bin/experience_runtime > ~/rockpi-validation/blob-lifecycle.log 2>&1`                                                                                                                                                                                                                                                                                                                                                             | `RESULT` lines; `captures/blob_lifecycle_*.png`                                                                                                                                                  | 0 failures                                                                                                            | no                                            |
| 5   | Temporal render + history                      | `EXPERIENCE_RUNTIME_TEMPORAL_LIFECYCLE_HARNESS=1 timeout 900 ./bin/experience_runtime > ~/rockpi-validation/temporal-lifecycle.log 2>&1`; then `EXPERIENCE_RUNTIME_TEMPORAL_WARM_HARNESS=1 timeout 900 ./bin/experience_runtime > ~/rockpi-validation/temporal-warm.log 2>&1`                                                                                                                                                                                                                                                                                                                                    | 297 checks; warm: per-leg switch timing, RSS, ambient hash, mask cycles, `captures/temporal-warm/results.txt`                                                                                    | `RESULT: PASS` / `N passed, 0 failed`                                                                                 | no                                            |
| 6   | Blueprint                                      | `EXPERIENCE_RUNTIME_BLUEPRINT_HUD_HARNESS=1 timeout 900 ./bin/experience_runtime > ~/rockpi-validation/blueprint-hud.log 2>&1`; optional `EXPERIENCE_RUNTIME_BLUEPRINT_SCENE=1 timeout 120 ./bin/experience_runtime` for a visual look                                                                                                                                                                                                                                                                                                                                                                           | 26 checks; `captures/blueprint-hud/`                                                                                                                                                             | 0 failures (roster harness 16/18 is a known timing contamination, not a pass)                                         | no                                            |
| 7   | HUD presentation                               | step 1 HUD units; `EXPERIENCE_RUNTIME_FOCUS_HARNESS=1 …`, `EXPERIENCE_RUNTIME_SKIN_HARNESS=1 …`, optional `EXPERIENCE_RUNTIME_HUD_ART_EXPORT=1 EXPERIENCE_RUNTIME_HUD_ART_EXPORT_DIR=$HOME/rockpi-validation/hud-art …`                                                                                                                                                                                                                                                                                                                                                                                          | RESULT lines; exported art                                                                                                                                                                       | 0 failures; visual review of art                                                                                      | no                                            |
| 8   | Shared Effects                                 | step 1 `session2_integration_tests` + effect knowledge tests + drift check; step 3 (effects EVENT shows slots/display names; Raw = `slots=0` is valid); step 5 Temporal effect checks                                                                                                                                                                                                                                                                                                                                                                                                                            | listed lines                                                                                                                                                                                     | no effect load errors; effects EVENTs present                                                                         | partly (unit parts)                           |
| 9   | Shared Video                                   | step 1 video-playback units; `EXPERIENCE_RUNTIME_RESUME_HARNESS=1 EXPERIENCE_RUNTIME_VIDEO_DWELL=10,20 EXPERIENCE_RUNTIME_VIDEO_RESUME_TRACE=1 timeout 1200 ./bin/experience_runtime > ~/rockpi-validation/resume.log 2>&1`; optional `EXPERIENCE_RUNTIME_CADENCE_HARNESS=1` (~12 min)                                                                                                                                                                                                                                                                                                                           | resume trace + RESULT                                                                                                                                                                            | 0 failures                                                                                                            | no                                            |
| 10  | Switching + reactivation                       | `EXPERIENCE_RUNTIME_SWITCH_HARNESS=1 …`, then `EXPERIENCE_RUNTIME_THREE_SCENE_HARNESS=1 timeout 1800 …`, optional `EXPERIENCE_RUNTIME_PARITY_HARNESS=1 …`                                                                                                                                                                                                                                                                                                                                                                                                                                                        | 5,351 checks; 10 circuits / 30 switches, GL errors, FBO allocated once, RSS bounded                                                                                                              | 0 failures                                                                                                            | no                                            |
| 11  | Performance / resources                        | `timeout 120 env EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE=1 EXPERIENCE_RUNTIME_FRAME_PHASE_WINDOW_MS=1000 EXPERIENCE_RUNTIME_FRAME_PHASE_LOG=$HOME/rockpi-validation/perf-phase.log EXPERIENCE_RUNTIME_STALL_PROBE=1 EXPERIENCE_RUNTIME_STALL_PROBE_LOG=$HOME/rockpi-validation/perf-stall.log EXPERIENCE_RUNTIME_STALL_DRIVER=1 EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=temporal-steady EXPERIENCE_RUNTIME_STALL_DRIVER_STEADY_SECONDS=60 EXPERIENCE_RUNTIME_VIDEO_DWELL=900,900 ./bin/experience_runtime`; alongside it, read-only sampling `while sleep 1; do grep -E 'MemAvailable\|CmaFree' /proc/meminfo; done` | per-window frame p50/p95, segment shares, RSS/RssAnon, Temporal history/playhead residency, long-frame scope trees                                                                               | no unbounded RSS growth; no stalls > 1 s outside activation/media change. No numeric fps target is defined yet        | no                                            |

Order rationale: step 1 needs no restart and catches portability problems first. Step 2 answers "do the other scenes work" immediately on the running instance. Step 3 proves the shaders actually execute on Panfrost before any long harness runs.

## 5. Verification status and gaps

| Category                                     | Items                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| -------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Proven before by automated tests (Mac)**   | All unit suites; Temporal lifecycle 297; switch 5,351; RT-002 acceptance; three-scene 63/63; parity 37/37; Blueprint HUD 26/26; warm harness 42/42; Temporal adapter self-test; Priority B/C and inactive-release verify modes.                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| **Proven on Mac, not on ARM64**              | All of the above. On the Rock Pi only Release build, Blob startup and shader-file sync have been observed.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| **Requires real Rock Pi GPU**                | Shader compile/link and execution on Panfrost (steps 3–7); scene transitions and reactivation (steps 2, 10) — Blob startup alone does not prove transitions; Temporal history/playhead uploads and GStreamer decode on RK3399; Blueprint FBO/erosion/motion paths; HUD composite; performance/resources.                                                                                                                                                                                                                                                                                                                                                               |
| **Not implemented / not currently testable** | Fragment Trail (RT-006) — not in this tree. Temporal stale imagery after Blueprint RESEED (BLUE-002 open defect) — no pixel-identity check exists. GPU memory attribution on Panfrost — VC4 `bo_stats` tooling does not apply; no Panfrost counterpart. Linux RSS in switch/two-scene/skin harnesses (reports −1). Automated input to an already-running instance: no IPC/remote-control seam exists (keyboard or an operator-installed xdotool only). The Temporal adapter self-test lives in the separate `temporal-fields` sketch; it needs that sketch built on the Rock Pi. Supervised safety runs: RT-004 supervisor/orchestrator are Pi 3B-calibrated (see §3). |

## Appendix — RT-004 performance, profiling and A/B tooling (built for the Pi 3B)

Everything below is env-gated diagnostics already in the tree, all off by default. With the switches unset, production behavior is unchanged. All of it compiles and runs on Linux ARM64. Where a figure or threshold is Pi 3B-specific, that is stated.

### A. Diagnostic probes

| Switch                                                        | Default         | What it records                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              | Output                                                                   |
| ------------------------------------------------------------- | --------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| `EXPERIENCE_RUNTIME_STALL_PROBE=1`                            | off             | Scoped timings around update/draw/scene/decoder/Temporal/Blob/Blueprint code; NOTE events (`scene.setup/activate/deactivate`, `canonical.load`, `temporal.loadExplicit`, `temporal.releaseForInactive`, `temporal.historySourceFormat`, `temporal.downscaleMap`, `temporal.playheadTexFormat`, `temporal.ambientResidency`, `driver.action`), each with `mono=` (CLOCK_MONOTONIC). For frames at or above the threshold: the full scope tree plus "uncovered" time. A watchdog thread logs the open scope stack if a frame exceeds 1 s. At **normal exit** it writes a per-label summary (count/mean/p95/p99/max), counters, per-window stats and frame-interval p50/p95/p99 per owner/window                                                | `EXPERIENCE_RUNTIME_STALL_PROBE_LOG` (default `./rt004-stall-probe.log`) |
| `EXPERIENCE_RUNTIME_STALL_PROBE_MS=<ms>`                      | 200             | Long-frame dump threshold                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | —                                                                        |
| `EXPERIENCE_RUNTIME_STALL_PROBE_SAMPLE=1`                     | off             | **macOS only**: runs `/usr/bin/sample` on stalls. No effect on Linux                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         | —                                                                        |
| `EXPERIENCE_RUNTIME_GL_ERROR_TRACE=1` (needs the stall probe) | off             | Drains `glGetError` at every scope edge and charges each error to the innermost scope; pre-draw and post-draw checkpoints too. One deliberate `rt004.glTraceSelfTest` 0x500 proves it is active (exclude it from counts). Adds per-scope overhead, so don't use it for timing                                                                                                                                                                                                                                                                                                                                                                                                                                                                | `glError,<scope>,<code>,<count>` rows; `GLERROR first …` lines           |
| `EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE=1`                      | off             | Per-frame **exclusive** segments: `update`, `preDraw` (window tail + startRender clear + setupScreen + frame limiter), `draw`, optional `glFinish`, `postDraw` (swap + finishRender + pollEvents). Each segment records wall time, thread CPU time and (Linux) voluntary/involuntary context switches. The `CONFIG` line has glRenderer/glVersion/limiter/vsync/window. `WINDOW` summaries: segment p50/p95/max, CPU share, RSS/RssAnon/RssFile/RssShmem, MemAvailable, CmaFree, SwapFree, Temporal history frames/capacity/pixel MiB, playhead textures/estimated MiB. `EVENT` lines only on transitions (scene owner, canonical media, history reset/full, effects snapshot). `SLOW` lines for frames over the threshold (capped at 5,000) | `EXPERIENCE_RUNTIME_FRAME_PHASE_LOG` (default `./rt004-frame-phase.log`) |
| `EXPERIENCE_RUNTIME_FRAME_PHASE_WINDOW_MS=<ms>`               | 10000 (min 250) | `WINDOW` summary period (use 1000 for memory timelines)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      | —                                                                        |
| `EXPERIENCE_RUNTIME_FRAME_PHASE_MS=<ms>`                      | 200             | `SLOW` line threshold                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        | —                                                                        |

### B. Optimization A/B toggles (same binary)

| Switch                                                 | Effect                                                                                                         | Default (shipped)                            | Read when                        | How to compare                                                                                                                                                                                          |
| ------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------- | -------------------------------------------- | -------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `EXPERIENCE_RUNTIME_TEMPORAL_LEGACY_DOWNSCALE=1`       | Priority B: history frames built by copy + `ofPixels::resize` instead of the cached, bit-exact downscale       | fast path                                    | every frame (switchable mid-run) | `ab-compact` with `EXPERIENCE_RUNTIME_STALL_DRIVER_AB_ENV=EXPERIENCE_RUNTIME_TEMPORAL_LEGACY_DOWNSCALE=1`; compare `TOVB.history.fastDownscale@ab-default` vs `TOVB.history.legacyDownscale@ab-toggled` |
| `EXPERIENCE_RUNTIME_TEMPORAL_PLAYHEAD_UPLOAD=legacy`   | Playhead uploads always re-upload, instead of skipping a texture that already holds the selected history frame | skip redundant                               | every call                       | `ab-compact` / `playhead-ab`; compare `TimeOffsetVideoBuffer.playheadUpload@…`, `TOVB.playhead.upload@…`, `uploadSkippedRedundant@…`                                                                    |
| `EXPERIENCE_RUNTIME_TEMPORAL_AMBIENT_RESIDENCY=legacy` | Priority C: keep CPU pixels of all 19 ambient tints and eagerly load all 19 masks                              | bounded (tint CPU released, masks on demand) | once, at ambient preload         | separate processes (`blob-short`); compare the `TFAmbientTextureLayer: residency=…` stdout line and RSS                                                                                                 |
| `EXPERIENCE_RUNTIME_TEMPORAL_INACTIVE_RELEASE=retain`  | Keep Temporal's decoder, CPU history and playhead textures while Temporal is inactive                          | release on deactivate                        | each deactivation                | separate processes (`release-verify` / `temporal-cycles`); compare `temporal.releaseForInactive` NOTEs, RSS, decoder buffers                                                                            |

### C. Correctness verify modes (slow; correctness runs only)

| Switch                                                 | Checks                                                                                                                      | Evidence                                                                   |
| ------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------- |
| `EXPERIENCE_RUNTIME_TEMPORAL_DOWNSCALE_VERIFY=1`       | every fast-path history frame byte-compared with `ofPixels::resize`                                                         | counters `TOVB.history.verifyMatch` / `verifyMismatch`; stderr on mismatch |
| `EXPERIENCE_RUNTIME_TEMPORAL_PLAYHEAD_UPLOAD_VERIFY=1` | every skipped upload: read back the texture and compare with the selected history frame (desktop GL; needs `glGetTexImage`) | `TOVB.playhead.skipVerifyMatch` / `skipVerifyMismatch`                     |

### D. Frame-pacing controls (need the frame-phase probe; diagnostic only, never for a normal run)

| Switch                                                | Effect                                                                                                                                |
| ----------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- |
| `EXPERIENCE_RUNTIME_FRAME_LIMITER=off`                | `ofSetFrameRate(0)` after runtime setup (the OF limiter lives in `preDraw`)                                                           |
| `EXPERIENCE_RUNTIME_VSYNC=off`                        | `ofSetVerticalSync(false)` after runtime setup                                                                                        |
| `EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH=1`           | timed `glFinish()` after the app's draw: moves queued GPU work into a measured segment                                                |
| `EXPERIENCE_RUNTIME_FRAME_PHASE_CONTROL=present-only` | skips all runtime/harness work; clear + swap only (display/X pacing control). The driver does not step, so cap the run with `timeout` |

### E. Driver scripts (`EXPERIENCE_RUNTIME_STALL_DRIVER=1`, `EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=<name>`, log `EXPERIENCE_RUNTIME_STALL_DRIVER_LOG`, default `./rt004-stall-driver.log`)

The driver injects the same keys an operator would (`]`, `[`, `n`, `2`) through `ExperienceRuntime::keyPressed`, sets the stall-probe `window` context so the summary splits by phase, and exits with `ofExit()`. Times are driver seconds from the first frame; add device startup (~15 s on the Pi 3B).

| Script                    | Sequence / purpose                                                                                                                                 | Settings                                                                                        | Key outputs                                                                            |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| _(default — full matrix)_ | 3 cycles: Blob→Temporal, `n` while Temporal, Temporal→Blob, `n` in Blob, reactivation, …; ~5.5 min                                                 | —                                                                                               | driver `WINDOW` 5 s frame-time windows + probe                                         |
| `first-activation`        | one Blob→Temporal→Blob, exits at 22 s                                                                                                              | —                                                                                               | first-activation stall                                                                 |
| `ambient-split`           | times decode vs upload of each `backgrounds/` PNG (diagnostic copy, no production code), then exits                                                | —                                                                                               | `IMAGE …`, `AMBIENT-SPLIT …` lines                                                     |
| `temporal-steady`         | Blob→Temporal at 5 s; windows `warmup`/`steady`/`media-change`; `n` at 20+N s                                                                      | `EXPERIENCE_RUNTIME_STALL_DRIVER_STEADY_SECONDS` (default 120, min 10)                          | per-window frame intervals; steady-state baseline                                      |
| `temporal-ab`             | Priority B A/B: alternating `steady-legacy` / `steady-fast` windows in one process                                                                 | `…_STEADY_SECONDS` (window, default 30), `EXPERIENCE_RUNTIME_STALL_DRIVER_AB_PAIRS` (default 4) | `frameIntervalMs[owner=temporal-fields/steady-legacy / steady-fast]`, downscale scopes |
| `playhead-ab`             | playhead upload A/B: `steady-upload-legacy` / `steady-upload-skip` windows                                                                         | same as `temporal-ab`                                                                           | upload scopes and counters per window                                                  |
| `ab-compact`              | generic in-process A/B (~26 s): `ab-default` window (9–17 s) then `ab-toggled` (18–26 s); without a toggle, both windows are default (verify runs) | `EXPERIENCE_RUNTIME_STALL_DRIVER_AB_ENV="NAME=VALUE"`                                           | `frameIntervalMs[…/ab-default / ab-toggled]`, `window,…@ab-*` rows                     |
| `upload-split`            | measurement only: 300 × 640×360 uploads as RGB (GL_RGB8) and RGBA, `glFinish` per upload; exits                                                    | —                                                                                               | `UPLOAD-SPLIT … meanMs p50Ms p95Ms p99Ms glError` lines                                |
| `temporal-memattr`        | Blob baseline 0–8 s → Temporal 8 s → steady 14 s → `n` 19 s → exit 27 s                                                                            | —                                                                                               | memory/history timeline (with `…_FRAME_PHASE_WINDOW_MS=1000`)                          |
| `smoke-circuit`           | Blob→Temporal→Blob→Temporal→Blueprint (+RESEED)→Temporal, exits at 26 s (ends in Temporal)                                                         | —                                                                                               | all three scenes, RESEED media advance, effects EVENTs                                 |
| `blob-short`              | Blob only (Temporal never set up), exits at 8 s; optional on-demand mask cycles at 4 s                                                             | `EXPERIENCE_RUNTIME_STALL_DRIVER_MASK_CYCLES` (0–5)                                             | residency line; `MASK-CYCLE i masks=0->1->0 loadMs releaseMs glError…`                 |
| `temporal-cycles`         | Blob plus N × (Temporal 4.5 s ↔ Blob 3.5 s), no Blueprint                                                                                          | `EXPERIENCE_RUNTIME_STALL_DRIVER_CYCLES` (1–3, default 3)                                       | release/reload markers per cycle                                                       |
| `release-verify`          | Blob→Temporal→Blob→Temporal→Blueprint, exits from Blueprint at 23 s                                                                                | —                                                                                               | two release markers, reactivation, Blueprint setup/activation                          |

### F. Blob renderer profiling

| Switch                                        | Effect                                                                                                                 | Output                                  |
| --------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------- | --------------------------------------- |
| `EXPERIENCE_RUNTIME_BLOB_RENDER_PROFILE=1`    | averages fragment-renderer frame costs over a window and logs one line per window; stall frames are counted separately | stdout `[notice ] BlobRenderProfile: …` |
| `EXPERIENCE_RUNTIME_BLOB_RENDER_LEGACY=1`     | forces the old 3-stage fragment render path, for comparison with direct output                                         | stdout notice                           |
| `EXPERIENCE_RUNTIME_BLOB_RENDER_AB_HARNESS=1` | runs the direct-vs-3-stage pixel A/B once, then exits                                                                  | stdout                                  |

### G. Other runtime switches

| Switch                                       | Effect                                                                                                                                      |
| -------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| `EXPERIENCE_RUNTIME_VIDEO_DWELL=<min>,<max>` | Shared Video hold/dwell override in seconds (verification only; e.g. `900,900` keeps media fixed for steady runs, `10,20` for resume tests) |
| `EXPERIENCE_RUNTIME_VIDEO_RESUME_TRACE=1`    | logs resume-window start/exit decisions (`VideoResume`)                                                                                     |
| `EXPERIENCE_RUNTIME_HUD_SKIN=<skin id>`      | requests a HUD skin (e.g. the experimental Dark Moss shell); unset = functional wireframe                                                   |

### H. External supervision and analysis scripts (`scripts/rt004/`)

| Script                                                                                 | Purpose                                                                                                                                                                                                                                                                                                                   | Rock Pi status                                                                                                                                                                                                                                                                                                                           |
| -------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `memattr_supervisor.py run --label <id> --env KEY=VALUE …`                             | Launches one run in its own process group with a 1 Hz collector (RSS/anon/file/shmem, smaps_rollup, MemAvailable, Swap, CmaFree, readable DRM/dma-buf) and an independent 0.25 s watchdog; preflight refusal; TERM→KILL; `PR_SET_PDEATHSIG`; unique run dir with `manifest.json`, `samples.tsv`, `watchdog.tsv`, app logs | Runs on Linux, but its thresholds are **Pi 3B VC4 values** (preflight CmaFree > 96 MiB, floor 40 MiB, 48 MiB decline, MemAvailable 256 MiB, RSS 300 MiB, 45 s). They are fixed in code (`REAL_THRESHOLDS`) and need an Architecture-approved Rock Pi profile before use. Panfrost does not allocate GPU memory from CMA the way VC4 does |
| `test_memattr_supervisor.py`                                                           | 21 offline tests (dummy processes only; 2 Linux-only)                                                                                                                                                                                                                                                                     | runs on Linux                                                                                                                                                                                                                                                                                                                            |
| `pi_validation.py` (`plan`, `preflight`, `run`, `provenance`) + `run_pi_validation.sh` | one-command campaign of short supervised scenarios                                                                                                                                                                                                                                                                        | preflight **fails closed** on the Rock Pi (`vcgencmd` required); scenarios assume the Pi 3B profile                                                                                                                                                                                                                                      |
| `test_pi_validation.py`                                                                | 18 orchestrator tests (dummy processes)                                                                                                                                                                                                                                                                                   | runs anywhere                                                                                                                                                                                                                                                                                                                            |
| `s1_cma_timeline.py RUN_DIR`                                                           | merges collector/watchdog/frame-phase/stall logs into one monotonic TSV                                                                                                                                                                                                                                                   | parses VC4 `bo_stats` labels; other columns generic                                                                                                                                                                                                                                                                                      |
| `release_verify_report.py RUN_DIR`                                                     | phase table + mechanical PASS/FAIL/INCONCLUSIVE for `release-verify` runs                                                                                                                                                                                                                                                 | memory/dma-buf/history columns generic; GPU-buffer columns VC4-only; S1 comparison column is Pi 3B data                                                                                                                                                                                                                                  |

### I. Rock Pi re-baseline sequence (same binary, interactive instance stopped by the operator)

From `sketches/experience_runtime`, `export DISPLAY=:0`, logs under `~/rockpi-validation/`, each run capped with `timeout`. One variable at a time.

```bash
P="EXPERIENCE_RUNTIME_STALL_PROBE=1 EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE=1 EXPERIENCE_RUNTIME_FRAME_PHASE_WINDOW_MS=1000 EXPERIENCE_RUNTIME_VIDEO_DWELL=900,900 EXPERIENCE_RUNTIME_STALL_DRIVER=1"
L=$HOME/rockpi-validation
run(){ n=$1; shift; timeout ${T:-120} env $P EXPERIENCE_RUNTIME_STALL_PROBE_LOG=$L/$n-stall.log EXPERIENCE_RUNTIME_FRAME_PHASE_LOG=$L/$n-phase.log EXPERIENCE_RUNTIME_STALL_DRIVER_LOG=$L/$n-driver.log "$@" ./bin/experience_runtime > $L/$n-stdout.log 2>&1; echo "$n exit=$?"; }
run steady    EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=temporal-steady EXPERIENCE_RUNTIME_STALL_DRIVER_STEADY_SECONDS=60
run b-ab      EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=ab-compact EXPERIENCE_RUNTIME_STALL_DRIVER_AB_ENV=EXPERIENCE_RUNTIME_TEMPORAL_LEGACY_DOWNSCALE=1
run upload-ab EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=ab-compact EXPERIENCE_RUNTIME_STALL_DRIVER_AB_ENV=EXPERIENCE_RUNTIME_TEMPORAL_PLAYHEAD_UPLOAD=legacy
run verify    EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=ab-compact EXPERIENCE_RUNTIME_TEMPORAL_DOWNSCALE_VERIFY=1 EXPERIENCE_RUNTIME_TEMPORAL_PLAYHEAD_UPLOAD_VERIFY=1
run split     EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=upload-split
run c-bounded EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=blob-short EXPERIENCE_RUNTIME_STALL_DRIVER_MASK_CYCLES=3
run c-legacy  EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=blob-short EXPERIENCE_RUNTIME_TEMPORAL_AMBIENT_RESIDENCY=legacy
run rel       EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=release-verify
run retain    EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=release-verify EXPERIENCE_RUNTIME_TEMPORAL_INACTIVE_RELEASE=retain
run limoff    EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=temporal-steady EXPERIENCE_RUNTIME_STALL_DRIVER_STEADY_SECONDS=30 EXPERIENCE_RUNTIME_FRAME_LIMITER=off
T=40 run present EXPERIENCE_RUNTIME_FRAME_PHASE_CONTROL=present-only
T=60 run gltrace EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT=smoke-circuit EXPERIENCE_RUNTIME_GL_ERROR_TRACE=1
```

Where to read results:

- **Steady and limiter runs:** `WINDOW` lines in `*-phase.log` and the frame-interval rows at the end of `*-stall.log`. The summary is written only at a clean driver exit; a `timeout` kill loses it, while `WINDOW`/`SLOW` lines survive.
- **A/B runs:** the `@ab-default` / `@ab-toggled` rows.
- **Split:** the `UPLOAD-SPLIT` lines in `split-driver.log`.
- **C runs:** the residency line in `*-stdout.log`, plus `MASK-CYCLE` lines.
- **Release/retain runs:** the `temporal.releaseForInactive` NOTEs and the RSS columns in `WINDOW` lines.
- **GL trace:** the `glError,` rows.
- **Memory:** sample `grep -E 'MemAvailable|CmaFree' /proc/meminfo` alongside if wanted (read-only).

### J. Pi 3B reference results (for comparison only; different GPU, driver and memory model)

| Measurement                  | Pi 3B result                                                                                                                            | Source                          |
| ---------------------------- | --------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------- |
| Steady Temporal frame time   | ~86–111 ms (≈9–11 fps); update ≈41 ms CPU-bound, draw 22–51 ms, postDraw blocked 5–30 ms p50; present-only control holds 33.33 ms       | R0–R3 frame-phase logs          |
| History copy/resize (pre-B)  | ≈29 ms per frame; Priority B cut desktop cost ≈6× (6.03 → 1.01 ms interleaved)                                                          | Priority B report               |
| Playhead upload              | ≈6.6 ms per upload, ≈5.7 uploads/frame; redundant uploads only ≈2–3 % in steady state                                                   | Phase 2 report                  |
| RGB vs RGBA upload (640×360) | RGB 6.35 ms vs RGBA 1.92 ms mean (3.3×)                                                                                                 | upload-split on Pi              |
| Ambient tints                | ≈128 MiB of GPU buffers (VC4 stores RGB as 4 B/px); bounded residency removed 114 MiB CPU + 28.5 MiB masks                              | GPU/CMA attribution report      |
| Decoders                     | 22 v4l2 dma-buf buffers ≈21.5 MiB each; pause-then-close 97.7 ms (vs ~5 s EOS wait when closed while playing)                           | S1 stall-probe, Priority A      |
| Temporal inactive release    | dma-buf ≈47.5 → ≈26.0 MiB after deactivation (TRV run)                                                                                  | TRV markers                     |
| Open Pi issues               | SIGSEGV after first Blueprint activation (TRV, unresolved); GL_INVALID_ENUM 0x0500 after ambient preload; CMA margin at Blueprint setup | SIGSEGV and attribution reports |
