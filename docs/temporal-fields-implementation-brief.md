# Temporal Fields — Agent Implementation Prompt

You are implementing **Temporal Fields**, a new openFrameworks sketch, inside the
existing `EcopunkVideoCollage` monorepo. This file is your working brief. Follow it
in order. Where it tells you to verify something before proceeding, do that
verification first — do not assume the codebase matches what's described below
without checking, and do not silently paper over a contradiction. If what you find
conflicts with this brief, **stop and report the conflict back rather than guessing
a resolution and continuing.**

---

## 0. Context (read once, keep in mind throughout)

Temporal Fields is a companion sketch to `sketches/blueprint_emergence/`, sharing its
core device — footage broken into grid fragments — but replacing the archival/filing
metaphor with a temporal one: every visible fragment is the _same_ source video,
sampled at a different point in its own recent history (max 10 seconds back). White
= no delay, black = max delay.

Two selectable composition patterns:

- **Pattern A — Amorphic BSP:** recursive rectangular subdivision, organic split
  ratios, every piece stays a rectangle.
- **Pattern B — Blob Grid:** a square grid masked by a drifting metaball field.
  Cells outside the field are not drawn (transparent holes, for masking/overlay).
  Fragment size varies by depth inside the shape via an adaptive quadtree — larger
  merged blocks deep inside, fine detail at the boundary.

Both patterns share one time-offset mechanic, one transition system, and one
live-tunable parameter/preset system. A composition should be able to switch
patterns between cycles.

This brief is grounded in an HTML/Canvas browser prototype (no real video, fake
luminance simulation) that validated the _behavior_ of both patterns, and in a
direct inspection of the actual repo (`EcopunkVideoCollage`) as of this brief's
writing. **Nothing in this brief has been tested against real footage or real Pi
hardware yet.**

---

## 1. Required Verification Steps — Do These Before Writing Feature Code

Open and actually read each of the following before assuming their behavior. Each
one gates a decision later in this brief:

- [ ] `shared/src/CompositionBase.h` — is `enum class CyclePhase` fixed in the base
      class, or overridable/extensible per subclass? (Gates Section 3.)
- [ ] `shared/src/ErosionFBO.h/cpp` + `erosion.frag` — can it be driven from an
      arbitrary, caller-specified new offset/target, or only its current built-in
      decay behavior? (Gates Section 5.)
- [ ] `shared/src/LFOBank.h/cpp` — does its actual API provide independent
      low-frequency oscillation per named parameter? (Gates Section 7.)
- [ ] `shared/src/TriggerBus.h/cpp` — does its actual API provide scheduled/periodic
      event dispatch suitable for the clocks in Section 2 and Section 7?
- [ ] `shared/src/VideoSampler.h/cpp` — confirm it does seek-per-fragment-on-demand
      (not continuous decode + rolling buffer). Confirm this before assuming you
      need to write new code rather than extend it.
- [ ] Confirm current `addons.make` state for other sketches (should be empty/absent
      per prior inspection) — `ofxGui` will be a **new** addon dependency scoped to
      `sketches/temporal-fields/` only.
- [ ] Confirm exact openFrameworks version actually checked out in `libs/` (prior
      inspection found this documented as 0.12.x but unverified against the actual
      checkout).

If any of these turn up different than described in this brief, stop and surface
the discrepancy before continuing to the phases below.

---

## 2. Phase 0 — Project Scaffolding

- Scaffold `sketches/temporal-fields/` via `projectGenerator` per this repo's
  README convention:
  `projectGenerator -o <of_root> -s ../../shared/src sketches/temporal-fields`
- Structure as an independent OF project (own `Makefile`, `bin/`, `config.make`),
  matching `blueprint_emergence` and `quadrant-crosshair` — **not** a scene bolted
  onto an existing sketch.
- Add `ofxGui` to this sketch's `addons.make`. Do not add it anywhere else.
- Create `sketches/temporal-fields/bin/data/media/` (empty at first) and populate it
  later using this repo's real `tools/convert_media.sh`.
- Create `sketches/temporal-fields/bin/data/presets/` (empty at first; see Section 6).
- Class naming: prefix sketch-local classes `TF` (mirroring `blueprint_emergence`'s
  `BE` convention) — e.g. `TFComposition`, `TFFragment`, `TFSettings`,
  `TFPatternBSP`, `TFPatternBlobGrid`, `TFPresetManager`.

**Definition of done:** project builds and runs an empty `ofApp` via
`make RunRelease`, with `ofxGui` linked successfully.

---

## 3. Phase 1 — Composition Lifecycle

- Subclass `CompositionBase` for cycle timing, per-cycle random seed, and hook
  structure (`onCycleStart()`, `onUpdate()`, etc.) — do not re-derive this plumbing
  from scratch.
- Do **not** inherit `blueprint_emergence`'s phase semantics (`BLANK → PLACEMENT →
DENSITY → DISSOLVE → RESET_HOLD`) verbatim. Temporal Fields' patterns don't have
  that fill-up/erode narrative arc. Define what phases actually mean for this
  sketch based on what Step 1's verification found about whether the enum is
  overridable.
- A cycle should be able to select either Pattern A or Pattern B, and — once the
  preset system in Section 7 exists — which pattern to run should come from the
  currently-active preset.

---

## 4. Phase 2 — Shared Time-Offset Mechanic

Build this once; both patterns consume it identically.

- **Continuous decode:** the source video plays forward continuously and loops at
  end-of-file. Only one video player instance, ever — confirmed compatible with
  this repo's actual backend (plain `ofVideoPlayer`, not `ofGstVideoPlayer`).
- **Ring buffer:** rolling history, max depth **10 seconds**, holding downscaled
  frames — target **640×360** (half-res) as the default; fall back to quarter-res
  (320×180) only if memory measurements in Section 8 force it.
- **Playhead pool:** 5–8 logical playheads, each a position within the buffered
  history. Multiple fragments may reference the same playhead simultaneously —
  this is intentional and load-bearing for performance, not an approximation to
  fix later.
- **Playhead motion:** a playhead can hold still, jump abruptly to a new position,
  or ramp speed toward a target — implement as index math into the buffer, no
  extra decode calls.
- **Gray-to-offset mapping:** linear. White = 0s offset, black = 10s offset.
- **Quantize bands:** snap offsets to N discrete steps (default **12**, tunable
  8–16) rather than continuous — this is a real performance requirement (enables
  ordinary alpha-blended draws instead of dependent texture reads), not a
  stylistic toggle.
- Decide, based on Step 1's `VideoSampler` verification: does this live as a new
  mode on `VideoSampler`, or as a new `shared/src` class? Prefer `shared/src`
  placement if there's a reasonable chance another sketch reuses it later.

---

## 5. Phase 3 — Pattern A: Amorphic BSP

Implement recursive rectangular subdivision:

- Split axis follows the longer dimension of the region.
- Split ratio randomized around center; spread controlled by an `Irregularity`
  parameter (default **0.55**, range 0–1).
- Stop subdividing per-branch based on a minimum cell size (`Cell Density`, default
  **6%** of canvas edge) and a depth-based probability curve.
- On a clock (`Geometry Reshuffle Rate`, default **3.0s**), re-partition one or more
  existing leaf cells fresh (`Regions Touched Per Tick`, default **1**) — continuous
  piecemeal evolution, not a single global reshuffle.
- Each leaf cell references a playhead from Section 4's pool and renders through the
  transition system in Section 6.

---

## 6. Phase 4 — Pattern B: Blob Grid

Implement a square grid masked by a metaball field, with adaptive quadtree sizing:

- Base grid resolution: `Grid Resolution` (default **18×18**).
- Metaball field: 1–8 centers (`Blob Centers`, default **3**), each orbiting
  independently via its own sine/cosine motion, scaled by `Drift Speed` (default
  **1.0×**) and sized by `Blob Radius` (default **0.28**, normalized).
- Cells outside the field: **do not draw them at all** — true transparent holes,
  not a painted background color. This is required for masking/overlay use.
- Cell opacity near the boundary feathers per `Edge Softness` (default **0.30**)
  rather than a hard cutoff.
- Adaptive quadtree: cells sampled as safely fully-interior (with margin) may merge
  into a larger block, up to a size controlled by `Size Variation` (default
  **0.50**; 0 = uniform grid, 1 = ~7× base cell size at max interior depth). Cells
  straddling the boundary always stay at the finest available size.
- The quadtree re-quantizes on its own clock (`Fragment Refresh Rate`, default
  **3.0s**) rather than continuously — flag to the user once running whether the
  resulting snap-to-new-size behavior at each refresh reads as acceptable, or
  whether interpolating between two quadtree states is worth the added complexity.
- Each active cell references a playhead exactly as in Pattern A.

---

### 6a. Fragment Masking & Background Compositing (Blob Grid)

- Skip the draw call entirely outside the shape, don't just draw at alpha 0. When Mask to Blob is on (default) and a cell's coverage is below threshold (≈0.02), do not issue its draw call at all. - On the Pi, an alpha-0 draw still costs a fragment shader invocation and overdraw — skipping the call outright is the actual performance win, not an equivalent alternative to it.
- Mask to Blob toggle: add ofParameter<bool> maskToBlob (default true) to the same ofParameterGroup/ofxGui panel as everything else. When false, draw every cell in the underlying grid regardless of coverage — this is a debug/tuning view for seeing the full quadtree structure, not part of normal operation.
- Background mode — read this carefully, it is not literal OS transparency: add a Background choice (Solid Ground / Transparent) to the panel. In the browser prototype, "Transparent" meant a real alpha hole in the canvas. In the OF/Pi context, the deployed sketch runs fullscreen/headless with no other application to composite against — there is no OS-level window transparency to target. Implement "Transparent" mode as: render the blob-grid composition into an off-screen ofFbo allocated with an alpha-capable internal format, clear it with ofClear(0,0,0,0) each frame (not an opaque background fill), draw only the masked cells into it, and then composite that FBO's texture over whatever in-app background layer is meant to sit behind it (a separate texture, a second video layer, a shader-driven pattern, etc.) using ofEnableAlphaBlending() and a normal textured draw. "Solid Ground" mode skips the FBO and just clears to GROUND_DARK each frame as today.
  Stroke/rule-line leak: if a debug grid-line/rule overlay is drawn per cell (matching Blueprint Emergence's RULE_WHITE grid language), gate it behind the same coverage threshold used for the fill (e.g. only draw the rule if alphaMult > 0.25) rather than letting its alpha fade continuously to near-zero — a continuously-fading stroke reads as a faint halo leaking past the actual masked edge, which undermines the point of masking in the first place.

## 7. Phase 5 — Transition System

Whenever a fragment's offset (or, in Pattern B, its size/shape at a refresh tick)
changes, animate via one of three styles, chosen per-event from tunable weights:

- **Hard Cut** (default weight **33**) — instant swap.
- **Crossfade** (default weight **34**) — blend over `Transition Duration`
  (default **0.8s**).
- **Erosion** (default weight **33**) — progressive dissolve/reveal, over the same
  duration.

Build Erosion on `shared/src/ErosionFBO.h/cpp` + `erosion.frag` rather than
reimplementing a new dissolve mask — per Step 1's verification, confirm it can
be driven from an arbitrary caller-specified target before committing to this
reuse; extend it if needed rather than forking a parallel implementation.

---

## 8. Phase 6 — Live Dial Panel & Preset System (ofxGui)

- Build one `ofParameterGroup` containing every parameter from Sections 4–7
  (both patterns' dials, transition weights, quantize bands).
- Bind an `ofxPanel` to that group.
- **`G` key** — toggles the panel's visibility. **Default hidden** on launch — this
  is an authoring-time tool, not part of the deployed visual output. Deployed/kiosk
  runs must never show it unless explicitly toggled on.
- **Save Preset** — a real `ofxGui` button (`ofParameter<void>` in the group), not
  just a keybinding. Also bind **`S`** as a redundant save shortcut for when the
  panel is hidden.
- **Preset storage:** no existing convention in this codebase to match — this is a
  deliberate new schema. Use:
  - One JSON file per preset: `sketches/temporal-fields/bin/data/presets/<name>.json`
  - Serialize via `ofSerialize` / `ofSaveJson` against the full `ofParameterGroup`
    (not the panel's own single-file save/load, which only holds one implicit
    "current settings" state).
  - Each preset file records: which pattern it belongs to (`bsp` or `blobgrid`),
    plus that pattern's full dial state and the shared time-offset/transition
    dial state.
  - Scan `bin/data/presets/` into a list at startup, matching this repo's existing
    per-sketch media-folder scan convention.

**Do not build the parameter-drift/attractor system (Section 9) until a real
preset library exists.** Ship save/recall first; the drift system needs real
vetted data to hover around, not placeholder numbers.

---

## 9. Phase 7 — Parameter Evolution (build only after Section 8 has real presets)

Two independent timescales:

- **Continuous wobble:** every dial drifts slightly and independently via a
  low-frequency oscillator — use `shared/src/LFOBank` if Step 1's verification
  confirmed it fits; otherwise implement the equivalent (one independent LFO per
  tunable dial, not one global random walk).
- **Waypoint changes:** on a much slower clock, pick a new target preset and glide
  current values toward it over some duration. Use `shared/src/TriggerBus` for this
  and for the other periodic clocks in this brief if Step 1 confirmed it fits.
- **Hovering:** pull current values toward the active target preset (spring-like)
  while the continuous wobble keeps adding motion on top — never dead-still on
  exact preset numbers, never drifting into unvetted territory. Add a tunable pull
  strength once this is running against real presets.
- **Pattern switching:** when the next selected waypoint preset belongs to a
  different pattern than the one currently running, trigger a full scene-level
  transition (dissolve/reset), not an in-place parameter glide — the two patterns
  are structurally incompatible mid-cycle.

---

## 10. Phase 8 — Performance Validation (gate before deep polish)

- Confirmed compatible: this repo's plain `ofVideoPlayer` backend is stated as
  sufficient for 1–2 simultaneous video layers, and this design only requires one
  continuous decode session regardless of fragment count — consistent, not a
  conflict.
- **Not yet measured anywhere in this codebase:** sustained continuous-decode +
  buffer-write throughput at 24fps on real Pi 3B hardware. This is a **different**
  measurement than `VideoSampler::recordLatency()`'s existing per-seek
  instrumentation — do not treat existing seek-latency numbers as answering this
  question once they're available.
- Add instrumentation that logs sustained decode+buffer-write throughput
  specifically, and peak VRAM (`vcgencmd get_mem gpu`) at composition density peak,
  on real Pi 3B hardware, before committing further to timing/quality tradeoffs.

---

## Things You Must Not Guess At — Ask/Report Instead

- Any contradiction found during Section 1's verification steps.
- Whether `CompositionBase`'s phase enum can actually be overridden without forking.
- Whether `ErosionFBO` can be redirected to an arbitrary offset.
- Final dial values — everything in this brief is a browser-prototype-validated
  _starting point_, not a locked-in final number. A real tuning pass against actual
  footage on actual hardware should happen before treating any of these as final.
- Whether the Section 6 preset JSON schema is acceptable as designed — it has no
  in-house precedent to fall back on, so flag it for review rather than assuming
  it's correct once implemented.

---

_Work through phases in order. Each phase's "Definition of done" (where stated) or
implicit deliverable should be verifiably true before moving to the next._
