QUADRANT-CROSSHAIR REUSE ANALYSIS

Cross-sketch survey — informs design decisions only, no code changes implied or made.

Purpose: catalog which systems in `sketches/quadrant-crosshair/` could realistically be reused to build other sketch compositions, with `sketches/blueprint_emergence/` as the first candidate target. This is a snapshot for future decision-making, not a migration plan — it deliberately does not prescribe whether a given system should land in the shared library (`shared/src/`) or be copied/adapted directly into a target sketch.

Scope note: only systems that are actually part of quadrant-crosshair today are covered. A prior design iteration included a Gray-Scott reaction-diffusion system and a ghost-crosshair trail effect; both were intentionally dropped from the project and are not reuse candidates.

CONTEXT: WHAT BLUEPRINT_EMERGENCE ALREADY HAS

`blueprint_emergence` is built on top of the app's shared library (`shared/src/`: `Fragment`, `CompositionBase`, `GridSystem`, `VideoSampler`, `AnnotationRenderer`, `Settings`), which `quadrant-crosshair` does not use at all — quadrant-crosshair is fully self-contained inside its own `src/`. So "reuse" for blueprint_emergence means either promoting a quadrant-crosshair system into `shared/src/` (available to any sketch) or adapting it directly into blueprint_emergence's own `src/` (e.g. `BEComposition`, `BEFragment`). That choice is left open here.

blueprint_emergence's own engineering plan (`docs/blueprint_emergence_engineering_plan.md`) already names two unfinished pieces relevant to this survey: desaturation ramping during its DISSOLVE phase, and an erosion/dissolve dissolution animation for fragments leaving the composition.

PER-SYSTEM ASSESSMENT

High reuse potential / low coupling — likely drop-in with little adaptation:

- **LFOBank** (`src/LFOBank.h/.cpp`) — zero dependencies, pure sinusoidal oscillator bank returning `[-1, 1]` per lane. Any sketch wanting slow, non-repeating parameter drift (instead of hand-tuned timers) could use this as-is, just redefining the lane enum for its own parameters.
- **ShaderLibrary** (`src/ShaderLibrary.h/.cpp`) — generic name→`ofShader` registry. The only sketch-specific part is the list of `load()` calls; the class itself is a trivial, reusable pattern.

Medium reuse potential / needs adaptation — the concept transfers, the specifics don't:

- **Quadrant's erosion/residue FBO pattern** (`erosion.glsl` + the `fbo_read`/`fbo_write` ping-pong in `Quadrant.cpp`) — a single-shader, single-texture-read decay-and-blend (`history * decayRate + current * videoAlpha`). This is the closest existing match to blueprint_emergence's two known open gaps: it's a working answer to "fade old content while layering in new" (relevant to the dissolve/erosion animation) and to "ramp an effect's visibility over time" (relevant to desaturation ramping, by substituting a desaturate pass for the erosion blend). It's already proven on Pi 3B hardware in this app.
- **TriggerBus** (`src/TriggerBus.h/.cpp`) — a clean, generic listener/event-bus pattern over a state struct. The bus mechanics (cooldowns, activation-edge firing, listener registration) are reusable; the six trigger definitions (`EDGE_PROXIMITY`, `VELOCITY_HIGH`, etc.) are crosshair-specific and would need a blueprint_emergence-specific event vocabulary (e.g. fragment-arrival, phase-transition, zone-density events).
- **GridState** (`src/GridState.h/.cpp`) — a 24×18 float grid that accumulates "how often has activity passed through this cell" and decays over time, uploaded as a texture for shaders to sample. The mechanism (CPU float array → decay → texture upload) is generic, but it serves a different purpose than blueprint_emergence's existing 6×8 occupancy `GridSystem` in `shared/src/`, which tracks fragment placement, not accumulated activity. The two could coexist rather than replace each other.

Low/no reuse, sketch-specific — built for this composition's choreography, not portable as a unit:

- **CrosshairSystem** — Perlin-cursor motion is the wrong model for blueprint_emergence's grid-placement composition; there's no cursor concept in the target sketch.
- **MotionExtraction**, **ExpansionDirector**, **DebugMode**, **HudManager** — all tightly coupled to quadrant-crosshair's specific choreography (motion-reactive crosshair, periodic expansion sequences, crosshair/quadrant debug tooling, crosshair-specific telemetry). None of these map cleanly onto a fragment-and-grid composition without a substantial rewrite, at which point it's not really "reuse."

SIDE NOTES

- `desaturate.glsl` currently exists as two independent copies — one in `shared/src/shaders/`, one in `quadrant-crosshair/data/shaders/`. Not a reuse opportunity so much as a latent duplication worth knowing about regardless of what gets extracted from this survey.
- `hud_elements/` (`src/hud_elements/`) is a separate, already self-contained, Pi-friendly vector widget library (`hud::` namespace, documented in its own README) that's reusable as-is in any sketch. It's orthogonal to this analysis — not video-effect or composition logic, just UI chrome — but worth keeping in mind as a ready-made option if blueprint_emergence ever wants on-screen telemetry.

MOST PROMISING FIRST MOVE

If one thing from this survey is worth a closer look first, it's the **erosion/residue decay pattern**. It's a small, already-working, Pi-proven shader-and-FBO idiom that solves a problem blueprint_emergence already has on its open-issues list (the dissolve/desaturation gaps), rather than introducing a new capability the target sketch doesn't yet need.
