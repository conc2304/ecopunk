TEMPORAL-FIELDS CONTEXT QUESTIONS — ANSWERS

Answered by direct inspection of the `EcopunkVideoCollage` repo (as of the current
working tree) for the incoming `sketches/temporal-fields/` sketch (amorphic
time-offset video grid, BSP + metaball-masked blob grid patterns, preset
save/recall). `sketches/temporal-fields/` currently exists as an **empty
directory** — nothing has been scaffolded there yet.

A. PROJECT & BUILD STRUCTURE

1. Several independent openFrameworks project folders that share common code
   and conventions, not one ofApp with runtime scene switching. Each
   `sketches/<name>/` is its own full OF project (own `Makefile`, `bin/`,
   `config.make`, optionally `.xcodeproj`), generated via `projectGenerator`.
   Confirmed live examples: `sketches/blueprint_emergence/`,
   `sketches/quadrant-crosshair/`. `sketches/hud_elements/` is a shared
   widget *library*, not a runnable sketch. `sketches/temporal-fields/` is
   empty and will need the same `projectGenerator` treatment
   (`projectGenerator -o <of_root> -s ../../shared/src sketches/temporal-fields`,
   per this repo's `README.md`).

2. `docs/blueprint_emergence_engineering_plan.md` header states
   "**Framework:** openFrameworks 0.12.x · C++17 · GLSL ES 1.0" and
   "**Platform:** macOS (dev/validation) → Raspberry Pi 3B+ (deploy)" — matches
   the original design doc assumption. I could not find a `VERSION`/version
   file to independently confirm the exact `0.12.x` patch level against the
   `libs/` actually checked out; treat "0.12.x" as documented-but-unverified.

3. Addon usage, by actual `addons.make` contents:
   - `sketches/blueprint_emergence/addons.make` — **empty** (zero addons).
   - `sketches/quadrant-crosshair/` — **no `addons.make` file exists at all**
     (equivalent to zero addons).
   - `sketches/hud_elements/` — not a project, no addons.make.
   - No sketch in this repo uses `ofxGui`, `ofxGstVideoPlayer`, or any custom
     addon. Grepped for `ofxGui`/`ofxPanel` across the whole repo — zero hits.
   - For contrast: the sibling top-level app `apps/myApps/FireplaceWaterfall`
     (a separate app, not part of this monorepo, targeting a Rock Pi 4B, not
     the Pi 3B) has `addons.make` containing exactly `ofxGui`. That's the only
     ofxGui precedent anywhere in the codebase, and it's outside this project.

4. Build: `make Release -j4` / `make RunRelease` per sketch, or open the
   sketch's `.xcodeproj` in Xcode (per repo `README.md`). **Deploy: not yet
   built for this monorepo.** Searched for deploy scripts / systemd units /
   rsync anywhere under `EcopunkVideoCollage` — none exist. The engineering
   plan *describes* an intended path (rsync binary+data to Pi, restart a
   systemd service; `Type=simple`, `Restart=on-failure`,
   `Environment=DISPLAY=:0`) but its own open-questions table (lines
   658–660) still marks HDMI output mode (X11 vs framebuffer/EGL), peak VRAM,
   and seek latency as `_pending_` — i.e. this is a written plan, not a
   verified/working deploy. The one **real, working** precedent for this
   pattern in the user's environment is the sibling `FireplaceWaterfall` app:
   `apps/myApps/FireplaceWaterfall/deploy/systemd/fireplace-waterfall.service.template`,
   `deploy/x11/xinitrc.template`, and `rockpi-runbook.md` (documents
   `systemctl start/stop/enable/status` + `journalctl -u ... -f`). Different
   hardware tier (Rock Pi 4B vs Pi 3B) and a separate app, but the closest
   "how we actually do this" example available.

5. Yes: `shared/src/` at the `EcopunkVideoCollage` repo root, pulled into a
   sketch via `config.make`'s `PROJECT_EXTERNAL_SOURCE_PATHS`. Confirmed
   contents (file names, not paraphrased):
   `CompositionBase.h/cpp`, `Fragment.h/cpp`, `GridSystem.h/cpp`,
   `VideoSampler.h/cpp`, `AnnotationRenderer.h/cpp`, `ErosionFBO.h/cpp`,
   `LFOBank.h/cpp`, `TriggerBus.h/cpp`, `GridState.h/cpp`,
   `MotionExtraction.h/cpp`, `ShaderLibrary.h/cpp`, `Settings.h` (just 13
   lines of shared `ofColor` tokens), `shaders/` (a few `.frag`/`.vert`),
   `ridgeline/RidgelineRenderer.h/cpp`, `hud/` (widget set).
   **Important caveat:** adoption is inconsistent. `blueprint_emergence`'s
   `config.make` sets `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src` (pulls
   everything). `quadrant-crosshair`'s `config.make` sets
   `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src/ridgeline` (pulls
   *only* `RidgelineRenderer`) — it has its own independent, duplicate copies
   of `GridState`, `LFOBank`, `MotionExtraction`, `ShaderLibrary`, `TriggerBus`
   in its own `src/`. `docs/quadrant-crosshair-reuse-analysis.md` states this
   explicitly: quadrant-crosshair "does not use [shared/src] at all... is
   fully self-contained." So there is no codebase-wide mandate to build on
   `shared/src` — one sketch does, one doesn't, by choice.

B. SHARED MEDIA BANK

1. **No single shared media bank exists today.** Media lives per-sketch at
   `sketches/<name>/bin/data/media/`. Confirmed file counts:
   `sketches/blueprint_emergence/bin/data/media/` — **27 files**, flat
   directory, mostly stock-footage names (`vecteezy_...mp4`) and
   AI-generation-tool timestamped names (`2025-06-23T20-39-31__bolt_cam__roll_left.mp4`).
   `sketches/quadrant-crosshair/bin/data/media/` — **0 files** (empty; that
   sketch currently has no real footage loaded). There is also a vestigial
   `apps/myApps/EcopunkVideoCollage/bin/data/media/` at the repo root
   containing only a `.gitkeep` — looks like leftover scaffold from before
   the per-sketch layout, not an active bank. No categorized subfolders, no
   sidecar metadata files, anywhere.

2. No shared index/service. `VideoSampler::setup()` (`shared/src/VideoSampler.cpp`)
   does its own directory scan per sketch instance:
   ```cpp
   ofDirectory dir;
   dir.allowExt("mp4");
   dir.listDir(mediaPath);
   for (const auto& file : dir.getFiles()) mediaFiles.push_back(file.getAbsolutePath());
   ```
   Selection is sequential, not random: `selectVideoForCycle()` does
   `currentFileIndex = (currentFileIndex + 1) % mediaFiles.size()`.

3. `tools/convert_media.sh` (repo-root, actually exists and is real, not
   aspirational) enforces exactly the spec from the original design doc:
   MP4 container, H.264 baseline/main profile, `yuv420p`, ≤720p (downscales
   to 540p via `-vf scale=-2:540` if taller), forces 24/30fps
   (`fps_is_acceptable`, ±0.5fps tolerance), strips audio, tonemaps HDR down
   via `tonemap=hable`. Default output target in the script is
   `sketches/quadrant-crosshair/bin/data/media` (override via `OUTPUT_DIR`
   env var). I could not confirm every existing `.mp4` in
   `blueprint_emergence/bin/data/media/` was actually run through this
   script — filenames suggest a mix of raw stock downloads, so don't assume
   100% spec compliance of what's already sitting on disk without
   re-probing.

4. Yes and no — two independent implementations exist, not one shared by
   both sketches. `shared/src/VideoSampler.h/cpp` (used by
   `blueprint_emergence`) is a single-`ofVideoPlayer` wrapper with an
   async seek→settle→capture request queue (max queue depth 2, per a code
   comment: "Maximum 2 live (non-frozen) fragments"). `quadrant-crosshair`
   has its own separate, unrelated `VideoSystem.h/cpp` (50/133 lines) — it
   does not use `VideoSampler` at all.

5. None found. No tags, categories, duration, or crop-region metadata exists
   alongside any media file today.

C. EXISTING COMPOSITION / SCENE ARCHITECTURE

1. Yes: `CompositionBase` (`shared/src/CompositionBase.h/cpp`) is exactly
   this — an abstract phase-cycle state machine (`enum class CyclePhase {
   BLANK, PLACEMENT, DENSITY, DISSOLVE, RESET_HOLD }`), a per-cycle random
   seed (`cycleSeed`, logged; `srand(cycleSeed)` called in `startCycle()`),
   and virtual hooks (`attemptPlacement()`, `onCycleStart()`, `onUpdate()`,
   `pickPlacementInterval()`, etc.) a sketch subclasses — `blueprint_emergence`'s
   `BEComposition : public CompositionBase` is the one live example. Used by
   exactly one sketch today. `quadrant-crosshair` does **not** use it — it
   has its own bespoke `ExpansionDirector` instead. So it's a strong, working
   precedent for temporal-fields' pattern-cycle/preset system, but not an
   enforced convention.

2. **No kiosk launcher, systemd rotation, or "which sketch runs when" logic
   exists anywhere in this repo.** Each sketch is a fully separate binary,
   built and run manually per-sketch (`make RunRelease`). Nothing found
   switches between sketches at the OS level. So pattern-switching inside
   temporal-fields would be fully internal — there is no external rotation
   system to coordinate with today. (Confirmed by exhaustive search for
   `deploy`/`.service`/`systemd` in this repo — zero hits, aside from the
   unrelated `FireplaceWaterfall` app noted in A4.)

3. Flat `sketches/<name>/src/` with one class per header/cpp pair (no
   subfolders except `shared/src/hud/` and `shared/src/ridgeline/`).
   `blueprint_emergence` prefixes its own sketch-local classes with `BE`
   (`BEComposition`, `BEFragment`, `BESettings`, `BETriggers`, `BECycleMode`,
   `BELFOLanes`, `BECompositionState`) to distinguish its own extensions from
   the unprefixed shared/src base classes it builds on. `quadrant-crosshair`
   uses no prefix at all (`CrosshairSystem`, `QuadrantManager`,
   `ExpansionDirector`, `DebugMode`, `HudManager`, plus its own unprefixed
   `GridState`/`LFOBank`/`MotionExtraction`/`ShaderLibrary`/`TriggerBus`,
   which — note — collide in name with, but are independent duplicates of,
   the shared/src classes of the same name). Tuning constants live in one
   `ALL_CAPS` constexpr header per sketch (`BESettings.h`), sometimes split
   into a `*_presets.h` for compile-time preset switching
   (`BESettings_presets.h`). `main.cpp` is a minimal, near-identical OF entry
   point in every sketch.

D. EXISTING GUI / SETTINGS PATTERNS

1. **`ofxGui` is not used anywhere in this codebase.** All existing
   debug/tuning UI is hand-rolled: `quadrant-crosshair/src/DebugMode.h/cpp`
   (507 lines) draws its own on-screen overlay with raw OF primitives,
   toggled by `d`/`D`, not an `ofxGui` panel. The only `ofxGui` usage
   anywhere in the user's environment is the separate `FireplaceWaterfall`
   app (different Pi target, not part of this monorepo) — so there's no
   in-house panel-styling or keybinding convention to match if
   temporal-fields introduces `ofxGui`; it would be a new addon dependency
   for this monorepo (needs its own `addons.make` entry) and a new UI idiom
   relative to both existing sketches' raw-overlay approach.

2. **No existing settings/preset file schema exists anywhere** — no
   `ofxXmlSettings`, no `ofJson`/`ofSerialize`, no hand-rolled save/load
   found in this repo. Every "preset" found today is a **compile-time C++
   construct**, never written to or read from disk:
   - `CrosshairPreset` (`quadrant-crosshair/src/CrosshairSystem.h`) — a
     hardcoded `std::vector<CrosshairPreset>` literal in
     `CrosshairSystem.cpp`, cycled at runtime via `TAB` →
     `nextPreset()`/`setPreset(int)`, never persisted.
   - `BEPreset`/`BEPresetValues` (`blueprint_emergence/src/BESettings_presets.h`)
     — an `enum class BEPreset { Default, SlowCinematic }` selected by
     changing a `constexpr` and recompiling, not loaded at runtime.
   So temporal-fields' preset save/recall system (a runtime, disk-persisted,
   `S`-key-triggered save) would be introducing a genuinely new pattern with
   **no in-house format to match** — this needs a deliberate decision, not
   an inference from existing code.

3. Actual keybindings in use today (all raw `keyPressed(int key)`, no addon
   involved):
   - `blueprint_emergence`: `g` (toggle occupancy debug), `r` (restart
     cycle), `e` (toggle erosion bypass — code comment marks it "TEMP
     DIAGNOSTIC"), `f` (flip divider axis).
   - `quadrant-crosshair`: `d`/`D` (toggle debug mode), `TAB` (next
     crosshair preset), `n`/`N` (next video file), `f`/`F` (toggle
     fullscreen), `[`/`]` (motion overlay alpha down/up), `m`/`M` (cycle
     motion-extraction output mode), `h`/`H` (toggle HUD), `ESC` (quit),
     plus `e`/`E` and `F1`–`F4` (present in the source, not traced further
     in this pass).
   Neither `G` nor `S` (capital) is used by either sketch today, so no
   direct runtime collision — but since each sketch is its own binary,
   there's no unified keymap to begin with (`f`/`g` already mean different
   things per-sketch). Adopting `G`/`S` is consistent with "not yet
   claimed," not with any single house-wide scheme, because none exists.

E. PERFORMANCE & HARDWARE DATA ALREADY KNOWN

1. **No real on-device Pi 3B measurements exist yet.**
   `shared/src/VideoSampler.cpp` has the instrumentation wired and ready
   (`recordLatency()`, `logLatencyStats()`, fires automatically after
   `LATENCY_SAMPLE_COUNT = 20` seeks) but the engineering plan's own
   open-questions table (`docs/blueprint_emergence_engineering_plan.md`,
   item 1) still lists "Seek latency on Pi 3B+ with ofGstVideoPlayer" as
   `_pending_`. Treat this as unmeasured, not as a validated number to
   design around.

2. **Actual backend is plain `ofVideoPlayer`**, not `ofGstVideoPlayer`.
   `shared/src/VideoSampler.h` includes `"ofVideoPlayer.h"` and holds a bare
   `ofVideoPlayer player;` member — despite the engineering plan's §08
   Performance Constraints table naming `ofGstVideoPlayer` (GStreamer +
   VideoCore IV hardware decode) as the intended backend. This repo's
   top-level `README.md` documents the actual, deliberate decision: "Confirmed
   scope: 1-2 simultaneous video layers, so plain `ofVideoPlayer`
   (GStreamer-backed on Linux) should be sufficient without needing OMX/MMAL
   hardware-decode plumbing." So: the original design doc's assumption is
   stale — build against plain `ofVideoPlayer`, matching what
   `VideoSampler` actually does today.

3. **No measured RAM/VRAM headroom recorded anywhere.** The engineering
   plan names a `gpu_mem=128` `/boot/firmware/config.txt` requirement and
   lists "Peak VRAM logged via `vcgencmd get_mem gpu` at composition density
   peak" as a still-open Phase-5 checklist item — a planned measurement, not
   a completed one.

4. Nothing suggests concurrent sketches — no launcher/rotator exists (see
   C2), and every sketch is its own full binary. The working assumption
   throughout the docs is one sketch fully occupying the device at a time,
   but this is inferred from the absence of any concurrency-coordination
   code, not a policy stated anywhere explicitly.

F. DEPLOYMENT / RUNTIME SPECIFICS

1. **Not yet built for this monorepo / the Pi 3B target.** No systemd unit,
   autostart script, or X11/framebuffer configuration exists anywhere under
   `EcopunkVideoCollage`. The engineering plan describes an intended systemd
   approach but explicitly leaves the X11-vs-framebuffer/EGL choice open
   (open-questions item 3). The one real, running example of "headless
   kiosk on this user's Pi hardware" is the separate `FireplaceWaterfall`
   app: `apps/myApps/FireplaceWaterfall/deploy/systemd/fireplace-waterfall.service.template`
   + `deploy/x11/xinitrc.template` + `rockpi-runbook.md` — real and
   working, just on a Rock Pi 4B, not the Pi 3B, and outside this monorepo's
   shared tooling.

2. None — see C2. Nothing exists today for temporal-fields to coordinate
   with.

3. `shared/src/` is the established location for cross-sketch reusable code
   in this repo (that's literally what it already holds —
   `CompositionBase`, `GridSystem`, `VideoSampler`, etc. — pulled in via
   each sketch's `config.make` `PROJECT_EXTERNAL_SOURCE_PATHS`). If a video
   ring buffer / playhead pool / preset manager is meant for future reuse,
   `shared/src/` is where this codebase's own convention says it belongs.
   Caveat from A5/C1: `quadrant-crosshair` opted out of `shared/src` for
   everything except `ridgeline/`, so placing new code there is *necessary
   but not sufficient* to guarantee a future sketch actually adopts it —
   that has to be a deliberate choice each time, not an automatic
   consequence of file location.

OTHER THINGS WORTH FLAGGING (not asked directly, but load-bearing)

- `desaturate.glsl` already exists as two independent copies (one in
  `shared/src/shaders/`, one in `quadrant-crosshair/data/shaders/`) per
  `docs/quadrant-crosshair-reuse-analysis.md` — a known, acknowledged
  duplication, not something to be surprised by if temporal-fields also
  needs a desaturate pass.
- `sketches/hud_elements/` is a separate, self-contained, copy-paste-in HUD
  widget library (its own README, `hud::` namespace) distinct from
  `shared/src/hud/` (which `blueprint_emergence` includes directly via
  `PROJECT_EXTERNAL_SOURCE_PATHS`) — two related-but-separate HUD systems
  exist; worth checking which (if either) temporal-fields actually needs
  before assuming they're the same thing.
- `docs/quadrant-crosshair-reuse-analysis.md`'s own top-rated reuse
  candidate — the erosion/residue decay FBO pattern
  (`shared/src/ErosionFBO.h/cpp` + `erosion.frag`) — is already
  Pi-proven (used live in `blueprint_emergence`) and may be directly
  relevant to temporal-fields' "time-offset" grid concept, since it's
  already a working history-decay-and-blend idiom.
