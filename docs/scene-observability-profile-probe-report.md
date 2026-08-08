# Scene Observability Profile — EcopunkVideoCollage

**Purpose**: a code-grounded inventory of what each sketch under `apps/myApps/EcopunkVideoCollage/sketches/` *knows about itself* — its runtime state, its commands, and its tunable parameters — translated into visitor/curator/developer-meaningful concepts, as the factual foundation for a future HUD Information Architecture and HUD Blueprint. This report does not design the HUD, redesign the runtime, or propose SceneManager changes; it inventories and classifies what already exists in code.

**Method**: built on `docs/scene-consolidation-probe.md` (full per-sketch draw-order/state-machine/build inventory, already established), a prior HUD-data/command-surface research pass covering all 9 interactive sketches, and two new focused research passes reading every `ofParameterGroup`, hand-rolled tuning struct, and named constant block across all 9 sketches plus the shared video-effects catalog they draw from. Every classification below is an **initial recommendation**, not a final decision — this report explicitly separates code facts from judgment calls throughout, per the task's own constraint.

**Scope**: all 11 directories under `sketches/` are addressed. Nine are real, interactive, visually-distinct pieces and receive full profiles: `blob-region-prototype`, `blueprint_emergence`, `contour-portrait`, `fragment-trail`, `quadrant-crosshair`, `radar-effects-gallery`, `radar-pulse`, `shader-effect-debugger`, `temporal-fields`. Two are not observability subjects in the same sense and are noted briefly rather than profiled in full: `hud_elements` (not a buildable project — a pointer-only README) and `hud_validation_harness` (a non-interactive, self-driving screenshot-diff tool, not a scene). `shader-effect-debugger` is a real, interactive sketch and is fully profiled below, but per prior work in this initiative it is a developer tuning tool for the shared effect catalog, not a piece the installation would show visitors — this is noted at the top of its profile and carried through to the recommendations in Part 10, not silently assumed.

---

## Executive Summary

**What this report found, in brief:**

- Every sketch already tracks *some* form of meaningful internal state (a phase, a mode, a count, a filename) — but reachability is wildly inconsistent. Some scenes expose this cleanly via `getState()`-style public accessors (`temporal-fields`, `quadrant-crosshair`'s `QuadrantManager::getTelemetry()`); others bury it in private `ofApp` members with no accessor at all (`shader-effect-debugger`, most of `blob-region-prototype`'s own perf timers).
- Command surfaces are real but sparse and non-uniform. No command category (next/prev media, pause, restart, mode-switch) exists in more than 5 of the 9 sketches. This was already established in prior work and is reconfirmed here through the visitor/curator/developer lens: almost every existing command is developer-shaped (a bare keypress with no guard, no label, no undo), not visitor-shaped.
- Tunable-parameter surfaces are large and structurally split into two very different populations: **five sketches expose real live `ofParameter` GUI panels** (`blob-region-prototype`: 27 params; `contour-portrait`: 76 params, the largest in the repo; `temporal-fields`: ~90 params across 14 groups; `fragment-trail`: 15 params; `quadrant-crosshair`'s `DebugMode`: 74 params, explicitly a non-production developer tool per `CLAUDE.md`), while **four sketches have zero GUI exposure at all** (`blueprint_emergence`, `radar-effects-gallery`, `radar-pulse` — every tunable value is a rebuild-only C++ literal — and `shader-effect-debugger`, whose real surface is the ~70-parameter shared video-effects catalog it debugs). This split matters directly for HUD design: "promote a curator param to the HUD" is a live-value read for the first group and requires new plumbing for the second.
- The overwhelming majority of tunable parameters in every sketch are **algorithm-internal tuning knobs** (detection thresholds, smoothing constants, noise scales, morphological-operator iteration counts) that are not visitor-legible even when bounded and safe to change. Across all four parameter-inventory passes, the recurring judgment is: bounded + visually obvious + no domain knowledge required → Candidate; bounded but requires understanding *why* → Curator-only; anything with "Debug"/perf/reproducibility framing → Never expose.
- A small number of genuinely universal concepts recur across every scene despite very different implementations: **something is playing** (media/content presence), **something is happening now** (an active mode/pattern/effect name), **the piece is alive** (a heartbeat/frame-rate signal, though this is a developer concept, not inherently visitor-facing), and **an operator can nudge it** (some command exists, though which one varies completely).
- **Recommendation for Version 1**: a visitor-facing HUD should show almost nothing about *how* a piece works and a small, consistent amount about *what* it's showing right now — title/concept, one or two live status lines already meaningful in plain language, and — only where genuinely safe — one or two bounded, obviously-reversible controls. Everything else surfaced in this report belongs behind a curator/developer layer, not the public HUD.

---

## Part 1 — Per-Scene Observability Profiles

### blueprint_emergence

- **Purpose**: an evolving grid composition where video-textured "fragments" arrive, settle, and dissolve in a continuous cycle, annotated with measurement lines and code-text overlays — a generative/architectural aesthetic.
- **Visual output**: a structural grid with 1–4 simultaneously visible video-cropped fragments, a rotating orange divider, annotation lines/labels, occasional HUD widget overlays, optional motion-trail glow and erosion trail.
- **Inputs**: one continuously-playing video (`VideoSampler`, single `ofVideoPlayer`) — no camera, no generated-only content.
- **Major coordinators/services**: `BEComposition` (extends `CompositionBase`, owns the cycle phase machine), `GridSystem`, `AnnotationRenderer`, `LFOBank`, `TriggerBus`, `GridState`, `ErosionFBO`, `MotionExtraction`, `hud_overlay`'s `HudOverlayLayer` (full-screen alternate mode).
- **Major effects**: `shared/src/video-effects` catalog (migrated), per-fragment autonomous shader-effect cycling, `ErosionFBO` trail (bypassed by default), motion-trail overlay.
- **Runtime modes**: `CycleMode::GHOST_LAYERS` / `PERPETUAL` (alternates continuously), plus an orthogonal full-screen `hud_overlay` mode (`o` key).
- **Media dependencies**: one shared video folder, path is `PLATFORM_PI`-conditional (dead code today, `PLATFORM_PI` never defined).

### blob-region-prototype

- **Purpose**: a live camera/video-region prototype — detects moving "blobs" in the source video and renders an effected fragment over each tracked region; explicitly a bare-bones perf/architecture testbed ("deliberately thin `ofApp`," per its own code comment), not a themed art piece.
- **Visual output**: a video background (off / cover-fit / shader-effected) with 1–10 tracked-region fragments, each independently shader-effected.
- **Inputs**: one continuously-playing video, sampled live (no seek/freeze), read via CPU pixels for blob detection.
- **Major coordinators/services**: `BlobDetector`/`BlobTracker` (OpenCV-backed), `VideoRegionController`, `TimeOffsetVideoBuffer` (used only for its live-texture path, not its history/playhead machinery), `ShaderLibrary`.
- **Major effects**: 18-entry `ShaderLibrary` effect list (background and per-fragment, independently selectable).
- **Runtime modes**: none structural — one continuous detect→track→render loop, entirely parameterized live via its GUI panel (background mode, draw mode, effect selection) rather than switched via discrete named modes.
- **Media dependencies**: `media/` folder, symlinked to `blueprint_emergence`'s clips in this checkout.

### contour-portrait

- **Purpose**: a topographic/contour-line portrait effect — displaces a field of horizontal or vertical lines by luminance/edge/threshold/mask signal from a live source, rendered as an abstracted line drawing over or in place of the source.
- **Visual output**: a mesh of contour lines (4–400 lines), optionally masked to a subject silhouette, optionally source-colored, with an optional "breakup"/dissolve effect sweeping across the frame.
- **Inputs**: **the only sketch of the 9 with camera input as a real, switchable mode** (`Mode { INPUT_IMAGE, INPUT_VIDEO, INPUT_CAMERA }`) alongside static image and video.
- **Major coordinators/services**: `ContourSource` (bespoke video/image/camera multiplexer, no `shared/src` dependency at all — the only such sketch), `ContourMaskSource` (own OpenCV background-subtraction pipeline), `ContourDisplacementEffect` (owns the entire 76-parameter tuning surface).
- **Major effects**: one fixed preprocessing shader (`contour_preprocess`) plus a companion source-color pass — no shader registry, no swappable catalog effects.
- **Runtime modes**: 7 named presets ("Clean Portrait" etc.) applied via GUI buttons, input-mode cycling (image/video/camera via `i`), mask on/off (`m`).
- **Media dependencies**: own `media/`, falls back to a symlinked `sharedMedia/` pointing at `blueprint_emergence`'s clips; camera mode needs no media at all.

### fragment-trail

- **Purpose**: a crosshair-driven fragment-spawning piece — a moving crosshair (one of 5 named movement patterns) leaves a trail of video-cropped fragments behind it as it moves, each independently effected.
- **Visual output**: a dark background with a visible crosshair reticle and up to 40 spawned, decaying video fragments trailing its path.
- **Inputs**: one continuously-playing video via a local fork of `TimeOffsetVideoBuffer` (6 playheads, never seeks).
- **Major coordinators/services**: `CrosshairSystem` (local fork, graduating to `shared/src/crosshair/` per prior decision, not yet done), `FTFragmentPool`, `FTModeController`, `FTBackgroundLayer`, `hud_overlay`.
- **Major effects**: local `ShaderLibrary` fork (9 effects, deliberately not migrated to the shared catalog due to name collisions).
- **Runtime modes**: `FTContentMode` (`TIME_SLICE`/`EFFECT_VARIED`/`EFFECT_PARAM_VARIANT`), switchable via 3 strategies (`TIMER`/`TRIGGER_BUS`/`MANUAL_KEY`); crosshair has 5 named movement presets (DRIFT/SCAN/HUNT/NERVOUS/ORBIT).
- **Media dependencies**: per-file symlinks into `blueprint_emergence`'s clips (a documented workaround, not a design).

### quadrant-crosshair

- **Purpose**: the repo's most structurally complex sketch — a screen divided into 4 quadrants, each independently cycling through shader "acts," steered by a moving crosshair with motion-reactive triggers, plus a periodic "expansion" spectacle sequence that sends one quadrant near-fullscreen.
- **Visual output**: 4 independently-scaled, independently-effected video quadrants, a crosshair reticle, an optional HUD readout layer, a motion-glow background layer.
- **Inputs**: one continuously-looping video via `VideoSystem` (local fork).
- **Major coordinators/services**: `QuadrantManager` (per-quadrant shader-cycling director), `CrosshairSystem` (local fork), `TriggerBus`, `LFOBank`, `GridState`, `MotionExtraction` (all local forks), `ExpansionDirector`, `HudManager`, `DebugMode` (a full parallel developer mode).
- **Major effects**: local 18-effect `ShaderLibrary` fork, `RidgelineRenderer`, per-quadrant erosion trail FBOs.
- **Runtime modes**: 3 mutually-exclusive top-level modes (normal / `DebugMode` / `hud_overlay`), plus `ExpansionDirector`'s `IDLE→TRAVEL_OUT→HOLD→TRAVEL_BACK` spectacle FSM and each quadrant's independent `PLAYING→SILENCING→READY` shader cycle.
- **Media dependencies**: shuffled `.mp4` playlist, `/home/pi/blueprint/media` on Pi (dead code) or local `media/`.

### radar-effects-gallery

- **Purpose**: an 11-mode visual-treatment gallery built specifically to help decide `radar-pulse`'s eventual production look — a "reveal mask" (pulses stamp a growing-ring mask that reveals color/effect content) composited through one of 11 selectable shader modes.
- **Visual output**: a mostly-desaturated video revealed in color/effect through expanding pulse rings from 3 fixed emitter points; mode determines the compositing treatment (heatmap recolor, chromatic split, lens distortion, telemetry ghosting, etc.).
- **Inputs**: one shuffled-playlist video via `RPVideoSampler` (a documented fork of `radar-pulse`'s own class).
- **Major coordinators/services**: `RPRevealMask` (3-FBO ping-pong decay-then-max mask), `GalleryCompositor` (one-shader-per-mode registry, "Contract B" uniform convention), `hud::PulseEmitterWidget`.
- **Major effects**: 11 named modes, one dedicated shader each — not a swappable catalog.
- **Runtime modes**: `modeIndex` (0–10), cycled via `n`/`p` keys only.
- **Media dependencies**: `/home/pi/blueprint/media` (Pi, dead code) or `bin/data/sharedMedia` symlink.

### radar-pulse

- **Purpose**: the production-track sibling of `radar-effects-gallery` — the same reveal-mask mechanic, but with one fixed, deliberately-simple compositing shader (`color_reveal`) rather than a mode gallery.
- **Visual output**: near-identical mechanic to `radar-effects-gallery` (pulses reveal a desaturated video into color), single fixed visual treatment.
- **Inputs**: one shuffled-playlist video via `RPVideoSampler` (original).
- **Major coordinators/services**: `RPRevealMask` (original), `RPCompositor` (single fixed shader, no registry), `hud::PulseEmitterWidget`.
- **Major effects**: one shader (`color_reveal`), threshold-gated color reveal.
- **Runtime modes**: none — one continuous fixed pipeline; only emitter count (`a` key, adds a random emitter) and debug-overlay visibility (`d`) are runtime-adjustable.
- **Media dependencies**: same as `radar-effects-gallery`.

### temporal-fields

- **Purpose**: the largest and most structurally developed sketch in the repo (8,658 LOC) — 9 independently-implemented generative patterns (BSP splits, blob grid, bands, column grid, telescoping frames, particle field, ecological succession, network growth, temporal tides) cycling on a shared timer, each sampling a rolling video-history buffer through 6 playheads, framed by a rich ambient HUD layer.
- **Visual output**: highly pattern-dependent — from rectangular BSP splits to organic particle fields to wave-like "tide" reveals — always video-textured via the shared time-offset buffer, with an extensive ambient HUD chrome (scanner, reticles, data cards, gauges, log scroll) layered on top.
- **Inputs**: one continuously-playing video via shared `TimeOffsetVideoBuffer` (6 playheads, never seeks — assigns playheads to buffered history offsets instead).
- **Major coordinators/services**: `TFComposition` (deliberately not a `CompositionBase` subclass — a different cycle model), 9 `TFPattern*` classes, `TFHudLayer` (the richest per-sketch HUD compositor in the repo), `TFParameterPanel` (the largest live-tunable surface in the repo, ~90 parameters across 14 groups), `TFPresetTimeline` (a standalone, unit-tested multi-state preset sequencer), `TFAmbientTextureLayer`, `TFBackgroundLayer`.
- **Major effects**: shared `video-effects` catalog (migrated) for the background layer's cycling shader treatment, plus pattern-specific rendering (mesh-based for wedge shapes, no shaders).
- **Runtime modes**: `TFComposition::CyclePhase` (`RUNNING`/`PATTERN_TRANSITION`), 9 named pattern types, `TFPresetTimeline`'s own multi-state JSON-driven preset sequences, plus an orthogonal `hud_overlay` full-screen mode.
- **Media dependencies**: own `media/`, 25 real clips present.

### shader-effect-debugger — *not a visitor-facing scene; profiled for completeness, excluded from HUD scope in Part 10*

- **Purpose**: a developer tool for live-tuning and validating the shared `shared/src/video-effects` catalog — cycles through all 26 registered effects with a dynamically-rebuilt parameter GUI, evolution/drift authoring aids, and a whitelist/blacklist knowledge-base writer.
- **Visual output**: whichever single catalog effect is currently selected, applied full-canvas to a looping video.
- **Inputs**: one video via a plain inlined `ofVideoPlayer` (the only sketch with no wrapper class at all).
- **Major coordinators/services**: `VideoEffectService`, `EffectEvolutionController`, `PatternDriftController`, `EffectRandomizer`, `EffectKnowledgeBase`.
- **Major effects**: the entire shared catalog (26 effects) — this sketch *is* the debugging surface for that catalog, not a themed piece of its own.
- **Runtime modes**: one effect active at a time, cycled by index; evolution/drift are layered modifiers, not competing modes.
- **Media dependencies**: own `media/`, scans for `.mp4` *and* `.mov`.

### hud_validation_harness — *not a scene; a non-interactive test harness*

Renders `shared/src/hud/` widgets in isolation and screenshots them for visual-diff regression testing. No video, no runtime modes, no commands (confirmed: no `keyPressed` override at all), no visitor-facing purpose whatsoever. Included in this repo's `sketches/` directory as tooling, not content.

### hud_elements — *not a buildable project*

Contains only a `README.md` pointing to the canonical `shared/src/hud/` widget library. No source code, no profile applicable.

---

## Part 2 — Observable Runtime State

Classification lens: **Internal implementation detail** (exists in code, has no meaning outside the implementation itself) / **Debug value** (meaningful only to someone debugging the code) / **Operational value** (meaningful to a curator/operator keeping the installation healthy) / **Visitor-meaningful value** (translatable into plain language a visitor could care about).

### blueprint_emergence
| Value | Accessor | Classification |
|---|---|---|
| Cycle phase (`BLANK`/`PLACEMENT`/`DENSITY`/`DISSOLVE`/`RESET_HOLD`) | `BEComposition::getState()` | **Visitor-meaningful** — translatable as "the composition is building / settling / dissolving" |
| `CycleMode` (`GHOST_LAYERS`/`PERPETUAL`) | `BEComposition::getCurrentMode()` | **Visitor-meaningful** — a distinct, nameable behavior mode |
| Fragment count, zone A/B counts | `BEComposition::getState()` | **Operational** — useful to a curator gauging activity level; too abstract standalone for a visitor |
| Cycle seed | `CompositionBase::getCycleSeed()` | **Debug value** — reproducibility only |
| Divider orientation/position | `BEComposition` getters | **Internal implementation detail** — pure rendering geometry |
| Current video filename | *No accessor exists* (`VideoSampler` gap) | Would be **Visitor-meaningful** if exposed — "what am I watching" is a natural visitor question |
| fps | `ofGetFrameRate()` | **Debug value** in a museum context — visitors don't care about frame rate |

### blob-region-prototype
| Value | Accessor | Classification |
|---|---|---|
| Tracked-region ("blob") count | `BlobTracker::getTrackCount()` | **Operational**, arguably **Visitor-meaningful** as "the piece is watching N things right now" — but reveals the surveillance-like mechanic explicitly, a genuine editorial question (see Part 10 open questions) |
| Active fragment count | `VideoRegionController::getActiveFragmentCount()` | **Operational** |
| Scratch FBO dimensions | new getters exist per prior work | **Debug value** — pure GPU-memory diagnostics |
| Rolling perf averages (detector/tracker/draw ms) | private, no accessor | **Debug value** |
| GUI param live values | private `ofParameter`s | Mixed — see Part 4 |

### contour-portrait
| Value | Accessor | Classification |
|---|---|---|
| Vertex count | `ContourDisplacementEffect::getVertexCount()` | **Debug value** — a rendering-internal count, not visitor-legible |
| Source availability | `ContourSource::isAvailable()` | **Operational** — "is the camera/video actually working" is a real curator concern |
| Input mode (Image/Video/Camera) | via `parameters()` group | **Visitor-meaningful** — "this is a live camera portrait" is a genuinely interesting fact for a visitor to know, especially since it's the one sketch that responds to *them* |
| Active preset name | *not tracked as state anywhere* | Would be **Visitor-meaningful** if built — named presets read as "moods" |
| Mask enabled / mask source | via `parameters()` group | **Operational** — a curator-facing "is subject detection on" toggle-state |

### fragment-trail
| Value | Accessor | Classification |
|---|---|---|
| Active fragment count | `FTFragmentPool::getFragmentCount()` | **Operational** |
| Content mode name | `FTModeController::getCurrentMode()` | **Visitor-meaningful** — a nameable behavior |
| Mode-switch strategy | `FTModeController::getStrategy()` | **Debug value** — meaningful only to someone who knows the 3 strategies exist |
| Crosshair preset index | `CrosshairSystem::getCurrentPreset()` | **Visitor-meaningful** *if paired with its name* (DRIFT/SCAN/HUNT/NERVOUS/ORBIT are evocative words) — but no name accessor exists today, only an index |
| Crosshair speed/state | `CrosshairSystem::getState()` | **Internal implementation detail** as raw numbers; could seed a **Visitor-meaningful** derived "energy" read |

### quadrant-crosshair
| Value | Accessor | Classification |
|---|---|---|
| Current video filename | `VideoSystem::getCurrentFilename()` | **Visitor-meaningful** |
| Per-quadrant telemetry (phase string, active shader name, dwell progress) | `QuadrantManager::getTelemetry(quadId)` | **Visitor-meaningful** for the shader name (a nameable visual style per quadrant) / **Operational** for dwell progress / phase string as literally coded ("ONLINE"/"QUIET PHASE"/"STANDBY") reads as a deliberate in-universe status readout — genuinely closest thing to a pre-built visitor-facing string in this codebase |
| Expansion-sequence state | `ExpansionDirector::getState()`/`isActive()` | **Operational**, borderline **Visitor-meaningful** — "the piece is about to zoom in on one quadrant" is a real anticipation cue |
| Crosshair preset index | `CrosshairSystem::getCurrentPreset()` | Same as fragment-trail — meaningful only with a name accessor |
| Motion output mode | `MotionExtraction::getOutputMode()` | **Debug value** — raw/reference modes are explicitly diagnostic |
| `DebugMode::active` | reachable only via a new getter | **Debug value**, definitionally |
| `VideoSystem::fileChanged()` | **unsafe to poll** — clears its own flag on read | N/A — flagged as a hazard, not a status field |

### radar-effects-gallery / radar-pulse
| Value | Accessor | Classification |
|---|---|---|
| Mode index/name (gallery only) | `GalleryCompositor::getModeIndex()/getModeName()` | **Visitor-meaningful** — named modes ("Chromatic Split," "Telemetry Ghosting") are evocative |
| Current clip filename | `RPVideoSampler::getCurrentFilename()` (both sketches) | **Visitor-meaningful** |
| Active pulse count | `pulseEmitter.getActivePulses().size()` | **Operational**, mildly **Visitor-meaningful** as an "activity" signal |
| Stamp-blend mode (GL_MAX vs. fallback) | `RPRevealMask::isUsingMaxBlend()` | **Debug value** — a GPU-capability diagnostic |
| Per-stage timing (radar-pulse: `msVideoUpdate` etc.) | private, no accessor | **Debug value** |

### temporal-fields
| Value | Accessor | Classification |
|---|---|---|
| Active pattern type + name | `TFComposition::getActivePatternType()` + `tfPatternTypeName()` | **Visitor-meaningful** — 9 named patterns are exactly the kind of "what am I looking at" label a visitor benefits from |
| Composition phase + phase-elapsed | `TFComposition::getPhase()/getPhaseElapsed()` | **Visitor-meaningful** (phase) / **Operational** (raw elapsed seconds) |
| Cycle seed | `TFComposition::getCycleSeed()` | **Debug value** |
| Active fragment centers | `TFComposition::getActiveFragmentCenters()` | **Internal implementation detail** — geometry, not a status value |
| Current media filename | `TimeOffsetVideoBuffer::getCurrentMediaFilename()` | **Visitor-meaningful** |
| Playhead count / offsets / history fill | `TimeOffsetVideoBuffer` getters | **Debug value** — "how far back in time is it looking" is a lovely *concept* but the raw numbers are not, by themselves, visitor-legible (see Part 5's semantic translation for how this could become one) |
| Background layer mode + active effect name | `TFBackgroundLayer` getters | **Operational**/**Visitor-meaningful** for the effect name specifically |
| Preset-timeline pause/active/debug status | `TFParameterPanel` getters | **Operational** |

### shader-effect-debugger
| Value | Accessor | Classification |
|---|---|---|
| Current effect id/index/kind | private, no accessor | **Debug value**, definitionally — this *is* the developer tool's own state |
| `safeForAutomaticSelection` flag | reachable via `service.getDefinition()` | **Debug value** |
| Evolution phase / drift on-off | private, no accessor | **Debug value** |

**Cross-cutting observation**: no sketch in this repo currently exposes a single clean "what am I called / what am I doing right now, in one sentence" value. Every visitor-legible concept identified above is assembled from 2–4 separate accessors, several of which don't exist yet as public getters. This is the single biggest gap Part 5's semantic translation and Part 10's data model need to close.

---

## Part 3 — Command Inventory

Classification lens: **Installation visitor** (safe, bounded, self-explanatory, no domain knowledge needed) / **Curator/operator** (real and useful, but needs context or carries risk an unsupervised visitor shouldn't carry) / **Developer only** (debug/tuning/reproducibility tooling with no place in a live installation at all).

| Sketch | Command | Current implementation | Classification |
|---|---|---|---|
| blueprint_emergence | Toggle HUD overlay (`o`) | full-screen swap to `hud_overlay` | **Developer only** — replaces the whole piece with an unrelated ambient test layer |
| blueprint_emergence | Toggle occupancy debug (`g`) | debug grid overlay | **Developer only** |
| blueprint_emergence | Restart cycle (`r`) | `composition.startCycle()`, full reset | **Curator/operator** — a legitimate "start fresh" reset, but abrupt and not self-explanatory to a visitor |
| blueprint_emergence | Toggle erosion bypass (`e`) | visual-mode flip | **Developer only** — a visual A/B toggle for tuning, not a piece-level concept |
| blueprint_emergence | Force axis flip (`f`) | divider rotation trigger | **Developer only** — an internal geometry nudge |
| blob-region-prototype | Toggle GUI (`g`) | shows/hides the dev tuning panel | **Developer only** |
| blob-region-prototype | Toggle debug visualization (`d`) | bounding-box overlay | **Developer only** |
| *(blob-region-prototype has no restart/pause/media/mode command at all)* | | | |
| contour-portrait | Cycle input mode (`i`) | image/video/camera | **Curator/operator** — genuinely interesting but changes the piece's whole content source, not a casual toggle |
| contour-portrait | Toggle mask (`m`) | mask-enabled flag | **Curator/operator** |
| contour-portrait | Capture background (`b`) | background-subtraction reference capture | **Curator/operator** — requires understanding *when* to press it (empty scene) to work correctly |
| contour-portrait | Toggle overlay text (`g`) | on/off | **Developer only** |
| contour-portrait | Toggle fps-only overlay (`f`) | on/off | **Developer only** |
| radar-effects-gallery | Next/prev mode (`n`/`p`) | `compositor.nextMode()/prevMode()` | **Installation visitor** — bounded (11-item ring), reversible, no state to corrupt, purely a "try a different look" action |
| radar-effects-gallery | Toggle overlay (`d`) | debug text | **Developer only** |
| radar-pulse | Add emitter (`a`) | spawns a new pulse origin at random position | **Curator/operator** — fun but unbounded over a long session (no cap found), and a fully-random placement isn't a considered visitor choice |
| radar-pulse | Toggle debug overlay (`d`) | perf/timing text | **Developer only** |
| fragment-trail | Direct mode select (`1`/`2`/`3`) | `FTContentMode` switch | **Installation visitor** — bounded 3-way choice, each already named conceptually |
| fragment-trail | Cycle switch-strategy (`m`) | `TIMER`/`TRIGGER_BUS`/`MANUAL_KEY` | **Developer only** — a meta-control over *how* mode-switching happens, not a content choice |
| fragment-trail | Toggle param panel (`h`) | dev GUI visibility | **Developer only** |
| fragment-trail | Cycle crosshair preset (`p`) | `nextPreset()` | **Installation visitor** — 5 named, evocative presets (DRIFT/SCAN/HUNT/NERVOUS/ORBIT), bounded, reversible |
| fragment-trail | Toggle `hud_overlay` panel (`o`) | dial-panel visibility only (documented divergence from other sketches) | **Developer only** |
| quadrant-crosshair | Toggle `hud_overlay` (`o`) | full-screen swap | **Developer only** |
| quadrant-crosshair | Toggle `DebugMode` (`d`) | full parallel dev mode | **Developer only** |
| quadrant-crosshair | Crosshair preset select (`1`-`5`, `TAB`) | `setPreset()`/`nextPreset()` | **Installation visitor** — bounded, reversible |
| quadrant-crosshair | Next video (`n`) | `video.nextFile()` | **Installation visitor** — bounded, obviously reversible ("show something else") |
| quadrant-crosshair | Fullscreen toggle (`f`) | `ofToggleFullscreen()` | **Curator/operator** — a kiosk/display config action, not a content choice (and per prior work, a global-OF-ownership violation that should move to the host anyway) |
| quadrant-crosshair | Motion overlay alpha (`[`/`]`) | ofApp-local int adjust | **Curator/operator** — bounded and visual, but a fine-tuning slider, not a discrete "pick one" action |
| quadrant-crosshair | Motion output mode (`m`) | cycles 3 modes, includes raw/reference diagnostic modes | **Developer only** — two of the three modes are explicitly debug visualizations |
| quadrant-crosshair | Toggle HUD readout (`h`) | on/off | **Curator/operator** |
| quadrant-crosshair | Quit (`ESC`) | `ofExit()` | **Developer only** — must never be visitor- or even casual-curator-reachable in a kiosk installation |
| quadrant-crosshair | Trigger expansion sequence (`e`, `F1`-`F4`) | `ExpansionDirector::trigger()`/`triggerQuadrant()` | **Curator/operator** — re-entrancy safety is unverified per prior work, so not yet safe for unsupervised visitor triggering regardless of how fun it looks |
| shader-effect-debugger | *(every command)* — prev/next effect, prev/next video, pause, randomize, reset, save whitelist/blacklist, toggle evolution/drift | all private `ofApp` methods, mostly `←`/`→`/`space`/`r`/`0`/`w`/`b`/`e`/`p` | **Developer only, universally** — this sketch's entire command surface is a tuning/authoring interface for the shared effect catalog, not installation content |
| temporal-fields | Toggle `hud_overlay` (`o`) | full-screen swap | **Developer only** |
| temporal-fields | Toggle param panel (`G`/`g`) | dev GUI | **Developer only** |
| temporal-fields | Toggle debug GUI (`d`) | dev overlay | **Developer only** |
| temporal-fields | Restart cycle (`r`) | `composition.startCycle()` | **Curator/operator** |
| temporal-fields | Force next pattern (`t`) | `forceNextPattern()` — flagged in prior work as having a real mid-transition edge case | **Curator/operator**, not yet visitor-safe until that edge case is resolved |
| temporal-fields | Save/load preset (`S`/`TAB`) | disk write / preset load + forced pattern switch | **Curator/operator** — a "load a different mood" action, genuinely close to visitor-shaped once named presets are curated, but currently exposes raw filesystem preset names |
| temporal-fields | Pause/resume/advance timeline (`P`, `]`) | `TFPresetTimeline` controls | **Curator/operator** |

**Cross-cutting observation**: exactly three commands across the entire 9-sketch survey are already, as implemented today, shaped the way a visitor-facing control should be — bounded, reversible, self-explanatory without narration: `radar-effects-gallery`'s next/prev mode, `fragment-trail`'s direct mode select and crosshair-preset cycle, and `quadrant-crosshair`'s crosshair-preset select and next-video. Every other command either mutates something with unverified safety, requires contextual knowledge to use correctly, or is transparently a developer/debug tool. This is a much smaller visitor-safe surface than the raw command count suggests, and confirms the capability-discovery design already established in prior SceneManager/HUD alignment work is necessary, not optional.

---

## Part 4 — Tunable Parameters

Full per-parameter tables were produced during research (four passes reading every `ofParameterGroup`, `DebugMode` field, and named constant block in the repo) and are available in full detail on request; presented here grouped and classified, per the task's own instruction, with representative examples and exact counts so every claim remains traceable to source.

### blob-region-prototype (27 live GUI parameters, one `ofxPanel`, `ofApp.h:53-99`)
| Group | Count | Representative examples | Classification split |
|---|---|---|---|
| Detection | 10 | analysis resolution, threshold, min/max blob area, erode/dilate iterations, max blobs | Mostly **Never expose** (perf/CV internals) or **Curator-only** (threshold — needs relighting per room) |
| Motion (tracking) | 5 | association distance, min IoU, position/size smoothing, max-missing-seconds | Mostly **Never expose** — tracker-internal matching heuristics |
| Visual | 9 | background mode, effect index/amount, fragment scale, crop padding, max active fragments, draw mode | Mixed: bounded intensity/scale sliders → **Candidate**; raw uncurated 18-item shader-index pickers → **Curator-only** |
| Debug | 3 | debug visualization, debug timing | **Never expose**, definitionally |

### contour-portrait (76 live GUI parameters across 9 groups — the largest surface in the repo, `ContourDisplacementEffect.h:187-292`)
| Group | Count | Representative examples | Classification split |
|---|---|---|---|
| Visual | ~20 | contrast, brightness, line thickness/length, displacement amount, breakup amount, blend mode | Largest **Candidate installation control** population in the repo — bounded, immediately visible, safe |
| Layout | ~10 | orientation, mesh scale, position X/Y, breakup direction/start/width | Split between **Candidate** (orientation, position) and **Curator-only** (breakup start/width pairing) |
| Simulation | ~15 | displacement source/bias/exponent, spatial/temporal smoothing, density mode/source/floor | Almost entirely **Curator-only** — changes the algorithm's meaning, not self-explanatory |
| Detection | ~6 | threshold/softness, mask softness/source, mask match threshold | **Curator-only**, one (mask spans per row) **Never expose** |
| Color | ~6 | line color/alpha, background color/alpha, source-color blend | **Candidate installation control** — classic, safe color pickers |
| Motion | ~2 | temporal smoothing, mask temporal smoothing | **Curator-only** |
| Debug | 10 | show gui/source/processed/mask/color-debug, mesh bounds, freeze frame, reload shaders, fps, quality preset | **Never expose**, universally |
| Hardcoded-only (not GUI) | 1 block | background-subtraction threshold/area constants, deliberately kept out of the GUI per the code's own comment | **Never expose** |

### quadrant-crosshair — two distinct surfaces
**`DebugMode`** (74 fields, `DebugMode.h:38-135`) — explicitly documented in `CLAUDE.md` as a non-production, never-migrated developer panel:
| Group | Count | Representative examples | Classification |
|---|---|---|---|
| Visual/Color/Motion/Timing/Simulation (all shader uniform families: dither, recolor, hue-rotate, bioluminescence, chromatic aberration, edge glow, ink outlines, pixel drift/sort, temporal trails, water refraction, heatmap recolor, ridgeline, motion-extraction) | 74 | per-shader alpha/threshold/intensity/color/speed uniforms | **Curator-only across the board** — the whole panel is a raw, unlabeled tech-rehearsal tool by design; individual *concepts* (hue, glow color, intensity) are reasonable future visitor-knob material once curated, the panel itself is not |

**Show-pacing constants outside `DebugMode`** (~12 values, `ExpansionDirector.cpp`, `QuadrantManager.cpp`): idle-trigger interval, travel/hold durations, per-quadrant scale ranges, silence/fade/dwell timing. All **Curator-only** (they govern the piece's overall pacing and rhythm — a real, meaningful "how fast does this feel" lever) except the internal randomizer bound constants, which are **Never expose**.

### shader-effect-debugger — real surface is the shared catalog it debugs (26 effects, ~70 parameters, `shared/src/video-effects/catalog/DefaultVideoEffectCatalog.cpp` + `effects/*.cpp`)
| Group | Count | Representative examples | Classification split |
|---|---|---|---|
| Visual (opacity/alpha/intensity/amount sliders) | ~25 | desaturate alpha, chromatic-aberration amount, ridgeline amplitude, water-refraction amplitude | Largest **Candidate installation control** population — most are bounded 0–1 or small-range opacity/intensity knobs |
| Color | ~10 | hue offset/speed, heatmap palette, recolor tint | **Candidate installation control** — discrete, legible choices |
| Simulation | ~15 | thresholds, gammas, feed/kill rates (reaction-diffusion) | **Curator-only**, with `caustics`/`temporal_trails`/`reaction_diffusion` explicitly **Never expose** — catalog itself flags these `safeForAutomaticSelection=false` or "never instantiated in production" |
| Motion | ~10 | decay weight, sensitivity, output/reference/source mode selectors | Mixed — bounded continuous knobs **Curator-only**; raw/reference diagnostic modes **Never expose** |
| Debug (global group) | 3 | evolution enable, drift enable, seed | **Never expose** — authoring/reproducibility tools |

### blueprint_emergence (0 live GUI parameters — every value below is a rebuild-only constant, `BESettings.h`, 508 lines)
| Group | Representative examples | Classification |
|---|---|---|
| Timing | cycle/phase/slot durations, LFO frequencies, trigger cooldowns | **Curator-only** — a real "installation pacing" lever, currently unreachable without a rebuild |
| Simulation | placement scoring weights, drift amplitude/frequency, trigger thresholds | **Never expose** — pure algorithm internals |
| Visual | desaturation ramp, motion/erosion decay, fragment-effect cycling timing | **Curator-only** |
| Layout | canvas size, media path, grid dimensions | **Never expose** — deployment configuration |
| One real gap found: `BE_SETTINGS_PRESET` — an entire "pacing profile" (default vs. slow-cinematic) selectable only by recompiling | **Curator-only, and the single highest-value candidate in this sketch for a live-toggleable curator control**, since it currently requires a rebuild to change something conceptually simple |

### temporal-fields (~90 live GUI parameters across 14 `ofParameterGroup`s — the largest wired surface in the repo, `TFParameterPanel.cpp`)
| Group | Count | Representative examples | Classification split |
|---|---|---|---|
| Simulation (9 per-pattern groups: BSP, Blob Grid, Bands, Column Grid, Telescoping Frames, Particle Field, Ecological Succession, Network Growth, Temporal Tides) | ~60 | irregularity, drift speed, grid resolution, spawn rate, node/particle counts, wave speed/amplitude | Mostly **Curator-only**; several (Max Particle Count, Max Node Count) are explicitly Pi-perf-sensitive and flagged **Curator-only, not Pi-validated** — must stay off any visitor surface until hardware-tested |
| Timing | ~10 | pattern regen rate, transition duration, mode-change intervals | **Curator-only** |
| Visual | background-effect weight sliders (17 individual shader weights) | **Curator-only** — technical, too granular for a visitor dial as-is, though a simplified derived "style mix" control is plausible future work |
| Layout/Visual (ambient texture opacity, dynamic per-manifest-entry) | variable | per-texture underlay/overlay opacity (leaf vein, woodgrain, moss, contour lines) | **Candidate installation control** — visually intuitive, named, bounded |
| Simulation (evolution system) | 4 | evolution enable, waypoint interval/pull-strength | **Curator-only** |
| One global, currently hardcoded-only, high-value gap: `CYCLE_DURATION` (90s) — the single "how often does the whole thing change" dial, not wired to any `ofParameter` today | **Curator-only, high-value if wired** |

### fragment-trail (15 live GUI parameters across 4 groups, `FTParameterPanel.cpp`)
| Group | Representative examples | Classification split |
|---|---|---|
| Timing | fragment sustain/decay, mode dwell, auto-cycle interval | **Candidate installation control** for sustain/decay ("how long images linger" is visitor-legible) / **Curator-only** for the rest |
| Motion | crosshair speed/smoothness/variance, pattern select, auto-cycle | **Candidate** for speed and pattern select (named presets); **Curator-only**/**Never expose** for variance-rate (second-order tuning) |
| Visual | fragment min/max size | **Candidate installation control** |
| Simulation | spawn min-distance/interval, idle-pulse, speed-response strength | **Curator-only** |

### radar-effects-gallery / radar-pulse (0 live GUI parameters in either sketch — every value is a hardcoded C++ literal)
| Group | Representative examples | Classification |
|---|---|---|
| Motion/Timing | pulse frequency/speed/range/concurrency/band-width, trail persistence, emitter positions | **Curator-only** — a coherent, meaningful "pulse tempo and reach" tuning surface, but entirely unwired to any GUI in both sketches today |
| Color | palette/tint arrays, Mode 10 heatmap-recolor literals (alpha/intensity/gamma/luminance range/palette) — confirmed hardcoded rather than routed through the shared catalog, a real inconsistency with this repo's own migration convention | **Curator-only**, with palette selection specifically flagged as a plausible future **Candidate** if offered as a small curated set |
| Debug | mode/overlay keyboard toggles | **Never expose** |

**Cross-cutting observation on Part 4**: across all 9 sketches, the single largest population of "Candidate installation control" material is bounded visual intensity/opacity/color/speed sliders with an obvious, immediate on-screen effect and no domain knowledge required to use safely. The single largest population of "Never expose" material is anything framed as detection/tracking/algorithm-calibration, regardless of how simple the underlying number looks (a threshold slider *looks* as simple as an opacity slider, but requires understanding what it's a threshold *of* to use well). This distinction — visible effect with obvious meaning vs. visible effect with hidden meaning — is the most load-bearing judgment call in this whole report and is applied consistently throughout.

---

## Part 5 — Semantic Translation

| Internal Name | Visitor Concept | Confidence |
|---|---|---|
| `BEComposition::CyclePhase` (BLANK/PLACEMENT/DENSITY/DISSOLVE/RESET_HOLD) | "Building" / "Settling" / "Dissolving" | High |
| `TFComposition::getActivePatternType()` + `tfPatternTypeName()` | "Now showing: [Pattern Name]" | High |
| `QuadrantManager::getTelemetry().phaseString` ("ONLINE"/"QUIET PHASE"/"STANDBY") | Already visitor-shaped as written — a direct reuse candidate | High |
| `GalleryCompositor::getModeName()` | "Now showing: [Mode Name]" | High |
| `CrosshairSystem` preset index (DRIFT/SCAN/HUNT/NERVOUS/ORBIT, once named) | "Movement: [Preset Name]" | High |
| `BlobTracker::getTrackCount()` | "Watching N visitors" / "N shapes detected" | Medium — accurate but raises a genuine editorial question about naming a tracking mechanic explicitly (see Part 10 open questions) |
| `TimeOffsetVideoBuffer::getPlayheadOffset()` averaged | "Looking back N seconds in time" | Medium — the underlying *concept* ("this piece remembers recent moments") is strong and thematically apt; the raw per-playhead numbers are not, by themselves, a good visitor readout |
| Current media filename (multiple sketches) | "Source: [clip name]" | Low-Medium — raw filenames are rarely visitor-meaningful without curation (e.g. `IMG_4021.mp4` vs. a curated title); a filename→display-name lookup would be needed, not present anywhere today |
| `fps` / frame time | *(none — developer-only concept)* | N/A — no visitor translation recommended |
| `ExpansionDirector::isActive()` | "About to change focus..." | Medium — a real anticipation cue, but needs the re-entrancy safety question resolved first (Part 3) before it's trustworthy to surface |
| Active fragment/pulse/particle count (multiple sketches) | "Activity: [low/medium/high]" (bucketed, not raw) | Medium — raw counts are Operational, but a coarse 3-bucket derived read is plausibly Visitor-meaningful |
| Detection threshold, smoothing constants, LFO frequencies, morphological-operator iterations (all sketches) | *(none)* | N/A — no plausible visitor translation exists; these are correctly Never-expose regardless of framing effort |
| `TFHudLayer`'s existing telemetry-readout widgets (FrameCounter/CoordinateWalk modes) | Already decorative/thematic, not literal status — a reuse candidate for *flavor* text rather than real data | High (as flavor, not as a real-data translation) |

---

## Part 6 — Cross-Scene Taxonomy

Concepts that recur, without forcing a category where a sketch genuinely doesn't have one:

- **"What's playing" (media identity)** — present in `blueprint_emergence` (gap), `contour-portrait`, `quadrant-crosshair`, `radar-effects-gallery`, `radar-pulse`, `temporal-fields`. Absent as a concept in `blob-region-prototype` (no filename tracked) and `fragment-trail` (not confirmed present).
- **"What mode/pattern is active" (a nameable behavior state)** — present in `blueprint_emergence` (CycleMode), `fragment-trail` (ContentMode + crosshair preset), `quadrant-crosshair` (crosshair preset + per-quadrant shader), `radar-effects-gallery` (mode), `temporal-fields` (pattern type). Genuinely **absent**, not just unexposed, in `blob-region-prototype` (continuous, parameter-driven, no discrete modes) and `radar-pulse` (one fixed pipeline).
- **"How much is happening" (an activity/density signal)** — present in some form in every sketch except `contour-portrait` and `shader-effect-debugger`: blob/fragment/pulse/particle counts are the same underlying concept (bounded population of active visual elements) implemented completely differently per sketch.
- **"Is content changing right now" (a transition/phase signal)** — present explicitly in `blueprint_emergence`, `temporal-fields`; present implicitly (fade/dwell timers) in `quadrant-crosshair`'s per-quadrant cycle; absent elsewhere.
- **"The piece responds to something" (an interaction/detection signal)** — present only in `blob-region-prototype` (blob tracking) and `contour-portrait` (camera mode). This is the smallest, most editorially sensitive taxonomy bucket in the repo — see Part 10.
- **A concept that does *not* generalize, and should not be forced to**: `temporal-fields`' "looking back in time" (playhead history depth) and `quadrant-crosshair`'s "expansion spectacle" are each structurally unique to their own sketch. Neither has a real analogue elsewhere; inventing a shared abstraction for either would misrepresent what's actually happening in the other 7 sketches.

---

## Part 7 — Widget Recommendations

Widget types are restricted to the list given (Label, Value, Status Badge, Progress Bar, Progress Ring, Sparkline, Histogram, Gauge, Radar, Timeline, Mini Graph, Icon Strip, Effect Chips, Metadata Card). No new widget types are proposed.

| Widget | Universal across scenes? | Best-fit scene(s) | Basis |
|---|---|---|---|
| **Label** | Yes — every scene has *something* nameable (scene title at minimum) | All | Part 5's translation table is almost entirely label-shaped text |
| **Status Badge** | Yes, for the small set of scenes with a discrete named mode | `blueprint_emergence` (CycleMode), `fragment-trail` (ContentMode/preset), `quadrant-crosshair` (per-quadrant phase string — already badge-shaped as coded), `radar-effects-gallery` (mode name), `temporal-fields` (pattern name) | Directly matches Part 2/5's "visitor-meaningful named mode" findings |
| **Metadata Card** | Yes, as the container for "what's playing" | Any scene with a media filename concept | Bundles filename + mode + activity into one glanceable unit, avoiding a wall of separate labels |
| **Progress Bar / Progress Ring** | No — only where a genuine 0→1 progress concept exists | `blueprint_emergence` (cycle phase elapsed), `temporal-fields` (pattern-transition progress, preset-timeline progress), `quadrant-crosshair` (expansion sequence travel/hold, per-quadrant dwell) | These are the only sketches with a real bounded-duration progress value, per Part 2 |
| **Effect Chips** | No — only where multiple named effects/shaders can be "active" as a set | `blob-region-prototype` (background + per-fragment effect names), `quadrant-crosshair` (per-quadrant active shader names, 4 chips) | Matches the "activeEffects" concept from prior HUD contract work |
| **Icon Strip** | Plausible for crosshair movement presets | `fragment-trail`, `quadrant-crosshair` (5 named crosshair presets each) | A small, bounded, glanceable set of named choices — icon-strip-shaped rather than a dropdown |
| **Radar** | No — literal/thematic fit only, not a data-visualization need | None recommended — despite the name, `radar-pulse`/`radar-effects-gallery` don't have radar-shaped *data* (angle/distance pairs); this would be decorative mimicry of the theme, not an honest data widget |
| **Sparkline / Mini Graph** | No — would require a time-series value, and the only real candidates are Debug-classified (fps, per-stage timing) | Not recommended for any visitor-facing use in v1; a curator-diagnostics page could reasonably use one for fps history |
| **Histogram** | No matching data found anywhere in this survey | Not recommended |
| **Gauge** | Plausible only for continuous, bounded, visitor-safe values | `contour-portrait`'s bounded visual sliders *if* mirrored read-only (e.g. displacement amount as a gauge, not just a curator control) | Low priority — most Candidate parameters are better shown as their live *effect*, not a numeric gauge, per the "show what, not how" framing in the Executive Summary |
| **Timeline** | Only where a multi-state sequence exists | `temporal-fields`' `TFPresetTimeline` (states in sequence), `quadrant-crosshair`'s `ExpansionDirector` FSM | Both already model a literal state sequence — a timeline widget is a faithful representation, not a stretch |
| **Value** | Yes, as a generic fallback for any single scalar a curator view wants (counts, timings) | Curator/diagnostics context primarily, not the visitor HUD | Matches Part 2's many "Operational" classifications that don't deserve a bespoke widget |

**Unnecessary widgets, explicitly**: Histogram (no matching data anywhere); Radar (thematic mimicry, not data-driven); Sparkline/Mini Graph for any visitor-facing surface (the only time-series-shaped data in the repo is Debug-classified).

---

## Part 8 — HUD Pages

Grouping only — no layout is finalized here.

- **Navigation** — scene identity/switching (previous/next scene, if/when SceneManager supports it), fullscreen/exit (host-owned, not scene-owned, per prior work). Universal shape, scene-agnostic content.
- **Scene Information** — the Part 5-translated "what am I looking at" bundle: title, active mode/pattern name (Status Badge), media source (if curated to a display name), a coarse activity read. This is the visitor-facing page and should stay small.
- **Scene-Specific Runtime** — the scene-specific concepts from Part 6 that don't generalize (temporal-fields' pattern-transition Timeline, quadrant-crosshair's per-quadrant Effect Chips, blob-region-prototype's Effect Chips). Naturally a *card*, not a full separate page, per Part 9's finding.
- **Curator Diagnostics** — Operational-classified values (counts, FBO stats, build-quality flags like `isUsingMaxBlend()`), plus the small set of Curator-only commands (restart, mode-force, input-mode switch) gated behind an operator-only surface, not the public HUD.
- **Developer Debug** (a genuine fifth grouping this task's example list didn't name, but the evidence strongly supports one existing at all, even if never shown in the installation itself) — fps/timing, raw GL/FBO state, every `DebugMode`/`shader-effect-debugger`-style surface. Explicitly out of the installation entirely, per Part 10.

---

## Part 9 — Uniformity Analysis

- **Identical across every scene**: nothing in the *content* of the status/command surface is truly identical — every sketch's meaningful state is implemented differently. What *is* identical is the **shape** of a small number of concepts: a name/label always exists; `ofGetFrameRate()` is always available (though Never-expose to visitors); every scene has *at most* a handful of visitor-safe commands, never a large uniform set.
- **Capability-driven**: almost everything else. Status Badges, Progress indicators, Effect Chips, and every command all depend entirely on what a given scene actually implements — this reconfirms, from the observability side, the capability-discovery design already established in prior SceneManager/HUD alignment work: the HUD must hide what a scene doesn't support, not assume a fixed set.
- **Should never vary**: the *meaning* of a given widget type, once assigned — a Status Badge should always mean "current named mode," never repurposed per-scene to mean something else; a Progress Ring should always mean "bounded duration completing," never a raw unbounded counter. Consistency of *meaning*, not presence, is what should be enforced uniformly.
- **Should be scene-defined**: the specific labels, the count/existence of Effect Chips, whether a Timeline widget applies at all, and virtually all Candidate-classified parameter exposure (Part 4) — these are all real content differences, not presentation inconsistencies to paper over.
- **Is one canonical layout practical?** For the "Scene Information" page (Part 8): yes — a title + Status Badge + activity read is genuinely universal in shape even though every value behind it differs per scene, and this is exactly the kind of small, consistent public-facing surface Part 10 recommends. For "Scene-Specific Runtime": no — the content is too structurally different (a Timeline for `temporal-fields`, Effect Chips for `quadrant-crosshair`, nothing at all for `radar-pulse`) to force into one fixed layout.
- **Would scene-specific cards fit better than scene-specific layouts?** Yes, clearly, based on this inventory. A fixed page shell (Navigation + Scene Information, uniform) with one flexible "Scene-Specific" card slot — populated per-scene from whatever Part 6/7 recommends for that scene, or left empty where nothing applies (`radar-pulse`, `blob-region-prototype`'s continuous parameter-driven design) — matches both the uniformity findings above and the capability-driven reality Part 3's command inventory already demonstrated. A card is also the natural container for the "unnecessary for this scene" cases (no Timeline, no Effect Chips) to simply not render, rather than a layout needing per-scene conditional logic.

---

## Part 10 — Recommendations

### Universal Information
Scene title/name (needs curation — see Open Questions); a current named mode/pattern, where one exists (Status Badge); a coarse, bucketed activity signal, where a meaningful count exists — never a raw number.

### Optional Information
Media source, shown as a curated display name (not a raw filename) once that lookup exists; a bounded progress indicator, only for the ~4 scenes with a genuine progress concept; a small, curated set of visitor-safe commands (the 5 already identified in Part 3 as implementation-ready today: gallery mode cycling, fragment-trail's direct mode/preset selects, quadrant-crosshair's preset select and next-video) — everything else stays hidden until either re-verified safe or newly built with visitor use in mind.

### Curator Information
Every value classified **Operational** in Part 2; every command classified **Curator/operator** in Part 3; every parameter classified **Curator-only** in Part 4, surfaced through a distinctly separate, presumably PIN/access-gated surface, not a toggle on the public HUD.

### Debug Information
Every value classified **Debug value** or **Internal implementation detail**; `shader-effect-debugger` and `hud_validation_harness` in their entirety; `quadrant-crosshair`'s `DebugMode`. None of this belongs anywhere near the installation-facing HUD, ever, per the task's own framing.

### Candidate Version 1 HUD

**Shown to every visitor**: scene title (curated), one Status Badge where a named mode exists, a Metadata Card bundling media source (once curated) + a bucketed activity read. Nothing more.

**Data model** (deliberately minimal, matching the "smallest contract" instinct already established in prior SceneManager/HUD alignment work):
```
VisitorStatus {
    string  title            // curated display name, not a raw scene id
    optional<string> modeName    // Status Badge text, only if the scene has one
    optional<string> mediaLabel  // curated media display name, only if applicable
    optional<int>    activityLevel  // 0-2 bucketed, never a raw count
}
```
Everything else this report inventoried — every Operational value, every Curator-only parameter and command, every Debug value — is explicitly **not** in this struct. It belongs in a separate curator/diagnostics surface this report does not design, consistent with the task's own instruction not to redesign the HUD here.

**Commands exposed in V1, if any are exposed at all**: the 5 already-safe commands identified in Part 3, each behind capability discovery (a scene that doesn't have one simply shows nothing for that slot) — not a universal control row.

---

## Open Questions

1. **Should the piece ever tell a visitor it's detecting/tracking them?** `blob-region-prototype`'s blob count and `contour-portrait`'s camera-mode status are the only two genuinely interaction-aware concepts in the whole repo. Naming this explicitly ("watching N visitors") is honest and could be compelling, or could read as surveillance-flavored in a way the rest of the installation doesn't. This is a curatorial/experiential call, not a technical one — flagged here rather than decided.
2. **Where does a "curated display name" for media filenames and scene titles actually get authored and stored?** No such mapping exists anywhere in the repo today (confirmed across all 9 sketches) — this is new content-authoring work, not a code-reachability gap like most of this report's other findings.
3. **Is a visitor-facing restart/reset command ever appropriate**, or should every reset-shaped action (found in `blueprint_emergence`, `temporal-fields`) stay curator-only permanently? Part 3 classified both as Curator/operator on safety grounds (abrupt, not self-explanatory) — but this is worth a deliberate product decision, not just a default.
4. **Do the Pi-perf-sensitive parameters flagged throughout Part 4** (`temporal-fields`' Max Particle/Node Count, `fragment-trail`'s Max Fragments, `blob-region-prototype`'s detection resolution) ever graduate out of Curator-only once real Pi 3B+ hardware validation exists, or do they stay permanently fenced regardless of measured headroom? This depends entirely on unresolved Pi-validation work already flagged in prior probes, not on anything this report can settle.
5. **Should `radar-effects-gallery` and `radar-pulse` — cut from SceneManager v1 scope per prior product decision — be included in the *observability* taxonomy going forward**, or does their exclusion from the scene cycle mean their status/command/parameter surfaces are effectively frozen documentation, not live design input? This report includes them for completeness since the task scoped "every sketch," but flags the tension with the settled SceneManager scope explicitly rather than silently resolving it.
