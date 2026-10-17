# Blob Experiment 2 — Pi Capacity Runbook (operator)

**Purpose:** measure how many Blob fragments can carry shader treatments at once on the Raspberry Pi 3B+, and how many different shaders can be used in one frame. Everything runs from one build: you switch configurations with keys, or run them unattended from a script. You don't need to know the source code.

**This is an experiment, not a production setting.** Nothing here changes Blob unless `EXPERIENCE_RUNTIME_BLOB_EFFECT_CAPACITY` is set.

## 1. Prerequisite

- The current `blueprint-emergence-updates` working tree is built on the Pi as usual: `cd <repo>/sketches/experience_runtime && make Release -j4`. Shaders and media must be synced exactly as for any ExperienceRuntime run.
- Attached keyboard (needed for interactive mode only), a display, and `export DISPLAY=:0`.
- Run everything from `<repo>/sketches/experience_runtime`.
- Logs go to `~/pi-capacity/`. Create it once: `mkdir -p ~/pi-capacity`.

## 2. Thermal / power — before and after every session

```sh
vcgencmd measure_temp; vcgencmd get_throttled; vcgencmd measure_clock arm
```

- Write down the start and end values.
- `throttled=0x0` is required for a valid result. Any non-zero value, such as `0x50005` (under-voltage or throttling), means the session's numbers are suspect: fix the power or cooling, wait, and rerun.
- Start each session after the Pi has idled about 2 minutes, with the case and fan as deployed.
- Each 5 s window also logs `tempC` and `throttled` from sysfs. `throttled=NA` means that sysfs node is missing; then rely on `vcgencmd`.

## 3. Recommended order (unattended, one command each)

Every run pins the Fern clip (`EXPERIENCE_RUNTIME_BLOB_CAPACITY_MEDIA=fern`) and holds it (`EXPERIENCE_RUNTIME_VIDEO_DWELL=900,900`). Media is therefore the same in every case.

Each step starts with a 10 s warmup, followed by 5 s measurement windows. The app exits on its own and prints a summary.

```sh
cd <repo>/sketches/experience_runtime && export DISPLAY=:0
B="EXPERIENCE_RUNTIME_BLOB_EFFECT_CAPACITY=1 EXPERIENCE_RUNTIME_BLOB_CAPACITY_MEDIA=fern EXPERIENCE_RUNTIME_VIDEO_DWELL=900,900"

# 1) Matrix A–G (~5 min)
vcgencmd measure_temp; vcgencmd get_throttled
timeout 600 env $B EXPERIENCE_RUNTIME_BLOB_CAPACITY_SCRIPT=matrix ./bin/experience_runtime > ~/pi-capacity/matrix.log 2>&1
vcgencmd measure_temp; vcgencmd get_throttled

# 2) Distinct-shader count: 1, 2, 4, 6 similar-cost shaders, then 18 diagnostic (~2.5 min)
timeout 400 env $B EXPERIENCE_RUNTIME_BLOB_CAPACITY_SCRIPT=distinct ./bin/experience_runtime > ~/pi-capacity/distinct.log 2>&1

# 3) Per-effect scaling: recolor, heatmap_recolor, edge_glow, water_refraction, pixel_sorting × 2/4/6/8/10 fragments (~13 min)
timeout 1200 env $B EXPERIENCE_RUNTIME_BLOB_CAPACITY_SCRIPT=scaling ./bin/experience_runtime > ~/pi-capacity/scaling.log 2>&1

# 4) Optional: any other effect, e.g. solarize and ink_outlines at 4 and 10 fragments
timeout 400 env $B EXPERIENCE_RUNTIME_BLOB_CAPACITY_SCRIPT="solarize@4:30,solarize@10:30,ink_outlines@4:30,ink_outlines@10:30" ./bin/experience_runtime > ~/pi-capacity/extra.log 2>&1

# 5) Optional realism check: matrix with real tracked fragments instead of the fixed load
timeout 600 env $B EXPERIENCE_RUNTIME_BLOB_CAPACITY_LOAD=tracked EXPERIENCE_RUNTIME_BLOB_CAPACITY_SCRIPT=matrix ./bin/experience_runtime > ~/pi-capacity/matrix-tracked.log 2>&1
```

**Wait about 2 minutes between runs.** If `tempC` in the windows climbs past roughly 75 °C, pause until it drops back.

### Script step syntax

Steps are comma-separated:

| Step | Meaning |
|---|---|
| `E:30` | Preset E for 30 s |
| `edge_glow@6:30` | 6 of 10 fragments use edge_glow, the rest Original |
| `w#4@10:30` | All 10 fragments shaded, round robin over the first 4 effects of set `w` |
| `matrix`, `distinct`, `scaling` | Built-in sequences |

## 4. Presets

All presets use 10 fragments. Original is unshaded.

| Key | Case | Original | Shaded | Distinct | Effects |
|---|---|---|---|---|---|
| F1 | A | 10 | 0 | 0 | — (baseline) |
| F2 | B | 0 | 10 | 1 | recolor |
| F3 | C | 5 | 5 | 1 | recolor |
| F4 | D | 5 | 5 | 2 | recolor, heatmap_recolor |
| F5 | E | 2 | 8 | 4 | recolor, heatmap_recolor, solarize, threshold |
| F6 | F | 0 | 10 | 2 | recolor, desaturate (max compatible set) |
| F7 | G | 0 | 10 | 10 of 18 | all 18, diagnostic stress |

## 5. Interactive mode

Launch:

```sh
env $B EXPERIENCE_RUNTIME_BLOB_EFFECT_CAPACITY=E ./bin/experience_runtime 2>&1 | tee ~/pi-capacity/interactive.log
```

The value of `EXPERIENCE_RUNTIME_BLOB_EFFECT_CAPACITY` is the starting preset letter; `1` means A. After every change wait about 8 s (3 s warmup plus one window) before reading numbers.

| Key | Action |
|---|---|
| F1–F7 | Presets A–G |
| `,` / `.` | Shaded fragments −1 / +1 (0–10); the rest are Original |
| `-` / `=` | Distinct shaders −1 / +1 (first N of the current set) |
| `q` | Set curated: recolor, desaturate |
| `w` | Set pointwise: recolor, heatmap_recolor, solarize, threshold, invert, desaturate |
| `e` | Set multitap: channelshift, chromatic_aberration, bioluminescence, ink_outlines, edge_glow |
| `r` | Set animated: water_refraction, pixel_drift, scanlines, hue_rotate |
| `t` | Set heavy: pixel_sorting, dither, ascii_solarpunk |
| `y` | Set all 18 |
| `u` | Custom set from `EXPERIENCE_RUNTIME_BLOB_CAPACITY_EFFECTS=a,b,c` |
| `j` / `l` | Step through the 18 effects one at a time (single effect on the current shaded count) |
| `m` | Assignment mode: single → round_robin → weighted. Weights come from `EXPERIENCE_RUNTIME_BLOB_CAPACITY_WEIGHTS=original:6,recolor:2,scanlines:2`, default original:5,recolor:5 |
| `g` | Load: synthetic (fixed 10 fragments, default) ↔ tracked (real motion) |
| `z` / `x` | Synthetic fragment count −1 / +1 (0–10) |
| `b` | Mark the next window (`MARKED` / `marked=1`) and save captures |
| `i` | Reprint the active configuration and TSV path |
| `n` / `p` | Next / previous media (normal runtime keys) |
| `Esc` | Clean exit |

Production keys keep working; none of the keys above collide with them.

### Reading the active configuration

Every change prints a banner like this:

```
[BlobCapacity] ---- configuration change (preset) ----
[BlobCapacity] [BLOB EFFECT CAPACITY]
[BlobCapacity] case=E
[BlobCapacity] shaderTarget=8/10 slots
[BlobCapacity] originalTarget=2/10 slots
[BlobCapacity] distinctShaders=4
[BlobCapacity] effectSet=pointwise
[BlobCapacity] effects=recolor[candidate],heatmap_recolor[diagnostic:outside-tolerance],solarize[diagnostic:outside-tolerance],threshold[diagnostic:outside-tolerance]
[BlobCapacity] assignment=round_robin
[BlobCapacity] pattern(id%10)=O,recolor,heatmap_recolor,solarize,threshold,O,recolor,heatmap_recolor,solarize,threshold
[BlobCapacity] load=synthetic:10@x1.00
```

- `[candidate]` marks effects that are production-compatible today: BLOB-002 compatible and visually valid.
- `[diagnostic:…]` marks effects measured for cost only, with the reason they aren't candidates.
- The HUD shows `region: capacity-experiment <case>`. Its region count reflects real tracking, not the synthetic fragments.

## 6. Outputs (all under `bin/data/captures/blob_capacity/`)

- **`blob_capacity_<timestamp>.tsv`:** one row per 5 s window, with every metric below.
- **`blob_capacity_<timestamp>_summary.tsv`:** script runs only; per-step means of the steady windows.
- **`step<N>_<spec>_{scene_only,hud}.png`:** taken at the end of each script step, so the save never lands in a measured window. Pressing `b` produces `mark_*` captures.
- **stdout:** `WINDOW …` lines, followed by `SUMMARY …` lines and `SCRIPT COMPLETE`.

Copy everything after a session:

```sh
cp -r bin/data/captures/blob_capacity ~/pi-capacity/
```

Window metrics:

| Group | Fields |
|---|---|
| Case | `case`, `mode`, `set`, `shaderSlots`, `distinctTarget`, `effects`, `load`, `media` |
| Fragments | `fragments`, `shader`, `original`, `distinctUsed` (shader programs actually bound per frame), `effectsUsed` |
| Timing | `frames`, `fps`, `frameMs`, `sceneDrawMs`, `regionRenderMs` (fragment renderer), `backgroundMs` (desaturate pass), `detectorMs`, `trackerMs` |
| Draw paths | `directDraws`, `legacyDraws`, `unshadedDraws`, `shaderBinds`, `shaderSwitches` (program changes between consecutive shaded draws) |
| Health | `stalls`, `maxStallMs` (frames > 250 ms, excluded from the averages), `tempC`, `throttled`, `MARKED` |

All timings are CPU wall time per frame. `fps` and `frameMs` include GPU waits, so on the Pi they are the authoritative numbers. The app is capped at 30 fps, as in production: a case is "free" when it holds 30.

## 7. Sending results back

Send:
- `~/pi-capacity/*.log`;
- the copied `blob_capacity/` folder (TSVs and PNGs);
- the start and end `vcgencmd` values for each run.

## 8. Clean exit

- Script runs exit on their own after `SCRIPT COMPLETE`.
- Interactive runs: press `Esc`.
- If the app hangs, the `timeout` cap ends it. Otherwise use `pkill -f bin/experience_runtime`.
