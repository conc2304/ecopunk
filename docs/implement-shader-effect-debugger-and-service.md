# Implementation Prompt: Standalone Shader/Effect Debugger + Shared Effect Service

Derived from [`shader-effect-system-probe.md`](./shader-effect-system-probe.md).
Read that document first — this one assumes its findings and does not
re-derive them. All paths are relative to
`EcopunkVideoCollage/` (the repo root for this monorepo).

This is an implementation prompt for another coding agent. It is staged so
every phase leaves all sketches in a buildable, behavior-preserving state —
there is no flag day. Phase boundaries are also rollback boundaries: if a
phase needs to be reverted, no later phase has landed to complicate that.

## Non-negotiable constraints (from the probe)

- Two shader contracts exist (probe §4) — Contract A (`tex`/`vTexCoord`, no
  `PLATFORM_PI` guard, used by BRP/BE/TF/FT/QC's effects pool) and Contract B
  (`videoTex`+`maskTex`/`texCoordVarying`, `PLATFORM_PI`-aware, RG only). The
  shared service must record contract as metadata, never assume
  interchangeability. **Scope for this implementation: Contract A only.** RG
  stays on its own compositor per probe §11/§13 open question 1 — do not
  touch `GalleryCompositor.cpp` in this work.
- RP and CP have no registry (probe §1/§11) and are **out of scope** — do
  not add them to the shared service.
- FT's `PROJECT_EXCLUSIONS = ../../shared/src` blanket-excludes `shared/src`
  (probe §11) — **do not attempt to migrate FT onto the shared service in
  this implementation.** Its `ShaderLibrary` fork's load-failure bug still
  gets fixed (cheap, isolated), but FT keeps its own dispatch code
  (`FTFragment.cpp`, `FTFragmentPool.cpp`) untouched otherwise. Resolving
  the exclusion is probe open question 6 and is out of scope here.
- No runtime cross-sketch asset sharing (probe §7) — every sketch keeps a
  physical copy of shader files in its own `bin/data`; the shared C++
  service lives in `shared/src` (compiled per-sketch via
  `PROJECT_EXTERNAL_SOURCE_PATHS`, the existing mechanism BRP/BE/TF already
  use), not a runtime-shared binary.
- Preserve current visual behavior in every migrated sketch unless a step
  explicitly says otherwise. Existing artistic `ofRandom(lo,hi)` ranges
  become each sketch's "override" tier in the new randomizer — they are
  copied, not redesigned.
- Do not make `EffectRegistry`/`EffectKnowledgeBase` a global singleton
  unless a later phase proves ownership is genuinely global; default to each
  `ofApp` owning an instance, matching how `ShaderLibrary` is owned today
  (plain member, not a singleton, in every existing sketch).

## Phase 0 — Shader load reliability fix

**Goal:** remove the one confirmed correctness bug before anything builds on
top of `ShaderLibrary`.

**Files modified:**
- `shared/src/ShaderLibrary.cpp` — `load()` currently computes `bool ok` and
  logs `ofLogError` on failure (already correct), but does **not**
  `shaders.erase(name)` on failure (probe §7/§11), leaving a default-constructed
  `ofShader` in the map so a later `has(name)` false-positives. Add
  `shaders.erase(name);` in the failure branch, matching the pattern already
  present in `sketches/fragment-trail/src/ShaderLibrary.cpp` and
  `sketches/quadrant-crosshair/src/ShaderLibrary.cpp` (both already correct
  as of this session's uncommitted edits).
- `shared/src/MotionExtraction.cpp` — confirm (probe §7) `shared`'s
  `setup()` already checks `accumShader.load()`/`extractShader.load()`
  return values; no change needed there. Note only, not an action item.

**Behavior preserved:** identical for every shader that currently loads
successfully. Only changes behavior for a shader that fails to compile
(previously: silently treated as "loaded," `has()` true, drawing a blank/
default shader; after: `has()` correctly false, callers already fall back
gracefully — e.g. `VideoRegionEffectRenderer::render()`'s `!hasEffect`
branch, `BEFragment::pickAndStartEffect()`'s `!shaderLib->has(name)` skip).

**Validation:** `make Release -j4` in `sketches/blob-region-prototype`,
`sketches/blueprint_emergence`, `sketches/temporal-fields` (the three shared-
`ShaderLibrary` consumers) — confirm clean build, confirm `ofLogNotice`
still fires for every currently-working effect on a short `make RunRelease`
run.

**Rollback boundary:** revert the one-line `shaders.erase(name)` addition;
no other phase depends on anything beyond this file existing.

## Phase 1 — Shared effect metadata (pure data, no behavior change)

**Goal:** consolidate the literal-constant uniform blocks currently
hand-duplicated 4-5× (probe §3A, §10 — bioluminescence's `threshold=0.3f,
intensity=1.2f, glowColor=(0.1,1.0,0.75)` appears verbatim in
`VideoRegionEffectRenderer.cpp`, `BEFragment.cpp`, `TFEffectPicker.cpp`,
`Quadrant.cpp`, `DebugMode.cpp`) into one data table. **This phase writes
data only — no consumer is switched over yet.**

**New files:**
- `shared/src/effects/EffectParameterMetadata.h` — per-effect-per-parameter
  struct: `{ std::string name; ParamType type; float defaultValue; float
  hardMin, hardMax; float artisticMin, artisticMax; bool safeToAnimate; bool
  visibleInDebugger; std::string requiresParam; }` (the last field covers
  probe §5's "requires another parameter to be meaningful" — e.g. `dither`'s
  `maxPixelation` is meaningless without `alpha` set to a mid-arc value).
  `ParamType` enum: `Float`, `Int`, `Bool` (covers every type actually seen
  in the probe — do not add more without a concrete need, per the task's
  "avoid designing a generic reflection framework beyond project needs").
- `shared/src/effects/EffectDefinition.h` — `{ std::string canonicalName;
  std::string fragPath; ShaderContract contract; std::vector<std::string>
  requiredUniforms; std::vector<EffectParameterMetadata> params; bool
  requiresHistoryTexture; bool requiresMotionTexture; bool requiresMask; }`.
  `ShaderContract` enum: `A` only for now (see constraints above) but define
  the enum with `A`/`B` so Contract B is representable when RG is folded in
  later (open question 1) without a breaking rename.
- `shared/src/effects/EffectCatalog.cpp` — one function,
  `std::vector<EffectDefinition> buildDefaultCatalog()`, populated by
  transcribing (not redesigning) the literal values from probe §3A's table
  and the four duplicated dispatch sites. This is mechanical transcription —
  every default value must trace back to an existing call site cited in the
  probe; do not invent new defaults.

**Files read but not modified in this phase:** every file listed in probe
§10's table (source of the literal values being transcribed).

**Validation:** this phase adds files to `shared/src/effects/` but nothing
includes them yet — validate by compiling `EffectCatalog.cpp` standalone
(`g++ -c` with the same include paths `shared/src`'s other files use) to
catch syntax errors only; no sketch rebuild required since nothing consumes
it yet.

**Rollback boundary:** delete `shared/src/effects/`; zero other files
reference it yet.

## Phase 2 — Standalone debugger sketch (extraction from quadrant-crosshair)

**Goal:** build `sketches/shader-effect-debugger/`, sourcing reusable logic
from `sketches/quadrant-crosshair/src/DebugMode.{h,cpp}` per probe §2's
classification table. Consumes Phase 1's metadata for GUI construction but
still binds uniforms via the existing literal-constant dispatch style
(**do not** build `EffectRenderer` yet — that's Phase 7+, keep this phase's
risk surface to "new sketch," not "new renderer").

**Sketch scaffold** (generate via `projectGenerator -o <of_root> -s
../../shared/src sketches/shader-effect-debugger`, matching every other
sketch's generation command per `EcopunkVideoCollage/README.md`):
- `sketches/shader-effect-debugger/src/main.cpp`
- `sketches/shader-effect-debugger/src/ofApp.h`, `ofApp.cpp`
- `sketches/shader-effect-debugger/bin/data/shaders/effects/*.glsl` — copy
  every Contract-A effect file from `sketches/quadrant-crosshair/data/shaders/`
  (the fullest existing set — includes nature pack, `hue_rotate`,
  `ascii_solarpunk`, `heatmap_recolor`) plus `effects/vert.glsl`.
- `sketches/shader-effect-debugger/bin/data/media/` — a `.gitkeep`'d drop
  point for preview video clips, matching every other sketch's `media/`
  convention (`README.md`'s documented pattern).

**Core preview** (`ofApp.h`/`.cpp`):
- Own a `ShaderLibrary shaderLib;` (shared, from `shared/src/ShaderLibrary.h`
  — this sketch is not FT/QC, it can use the shared class directly with no
  exclusion needed).
- Own a minimal single-clip video source. **Do not extract QC's
  `VideoSystem`** — per probe §2 it's "reusable-after-decoupling" but its
  playlist/dial-tuning is Ecopunk-specific polish this debugger doesn't
  need. Instead write a small `DebuggerVideoSource` (new, ~40 lines) wrapping
  `ofVideoPlayer` with exactly the 4 methods `DebugMode` actually calls
  (`update()`, `isFrameNew()`, `getTexture()`, `getPixels()`) plus
  `loadClip(path)`/`nextClip()`/`prevClip()`/`play()`/`pause()`, scanning
  `bin/data/media/*.mp4` at startup (mirrors `VideoSystem`'s playlist
  concept without its per-quadrant baggage).
- Aspect-correct rendering: reuse the crop-fill math already duplicated
  across the repo — `VideoRegionMath::computeCropFillSourceRect` (`shared/src`,
  used by BRP) is the cleanest existing standalone version (no class
  dependencies beyond the struct itself); use it directly rather than
  re-deriving crop math a fifth time.
- Show active effect name and active video filename as on-screen text
  (mirrors `DebugMode::drawPanel`'s header rows, `DebugMode.cpp:398-403`).
- For effects needing `motionTex`/`motionDelayedTex` (only `motion_effect`,
  QC-only, out of Contract-A's common set but present since we copied QC's
  full shader set) or `MotionExtraction`: **link in `MotionExtraction` as-is**
  (probe §2: reusable-after-decoupling, self-contained) via
  `shared/src/MotionExtraction.h` — do not stub it, it's cheap to include.
  Effects needing it show a small on-screen badge ("requires motion
  texture — live") rather than being hidden; effects needing external
  inputs unavailable here (none currently identified beyond motion, per the
  probe's effect catalog) get a "requires unavailable input" badge and are
  skippable but still selectable-to-inspect per the task's requirement.
- `RidgelineRenderer` (`shared/src`): link in directly (probe §2: directly
  reusable) for the `ridgeline` CPU-side effect, fed the debugger's own
  `videoSource.getPixels()`.
- Do **not** port `TriggerBus` types or the `TriggerSim` HUD row (probe §2:
  obsolete-after-extraction) — the debugger has no triggers to simulate.

**Navigation** (keyboard + `ofxGui` buttons, both, per requirement):

| Action | Key | GUI |
|---|---|---|
| Previous effect | `Left` | button |
| Next effect | `Right` | button |
| Previous video | `[` | button |
| Next video | `]` | button |
| Randomize current parameters | `r` | button |
| Save to whitelist | `w` | button |
| Save to blacklist | `b` | button |
| Toggle scene evolution | `e` | toggle |
| Reset effect to defaults | `0` | button |
| Pause/play video | `space` | toggle |
| Deterministic seed field | — | text/number input, optional |

Effect/video cycling logic mirrors `DebugMode::cyclePrev()`/`cycleNext()`
(`DebugMode.cpp:453-463`) exactly — index arithmetic over
`shaderLib`'s registered-name list (built from Phase 1's `EffectCatalog`,
not a hardcoded `SHADER_NAMES` array, so the debugger stays in sync with
the metadata automatically).

**Dynamic ofxGui** — this is the one genuinely new UI-architecture decision
in this phase. Investigated options (per the task's instruction to choose,
not default):
- *Rebuild the parameter group on effect change* (chosen): on
  `cycleNext()`/`cyclePrev()`, call `gui.clear()` (or remove/re-add the
  panel) and rebuild an `ofParameterGroup` from that effect's
  `EffectDefinition.params` (Phase 1 metadata) via a small
  `buildParamGroup(const EffectDefinition&) -> ofParameterGroup` helper. This
  avoids dangling `ofParameter` listeners because no `ofParameter` outlives
  its owning group — the group itself is destroyed and rebuilt, not
  individual parameters swapped in place. `DebugMode`'s current
  `buildParams()` (`DebugMode.cpp:59-186`) is precedent for "params depend
  on active effect," just re-expressed as real `ofParameter`s bound to
  `ofxGui` instead of the custom `DebugParam`/`drawPanel()` immediate-mode
  rows. Swapping active `ofParameterGroup` instances (the alternative) was
  rejected because `ofxPanel` doesn't cleanly support hot-swapping its bound
  group without the same clear/rebuild step anyway — no simpler than
  rebuilding outright.
- Evolution/drift controls (Phase 5/6) and temporal controls (motion-texture
  badge) are separate, always-visible sections outside the per-effect group,
  shown/hidden via `ofxGui`'s group visibility toggle rather than added to
  the per-effect group — they aren't per-effect state.

**Parameter binding at draw time:** for this phase, bind uniforms the same
way `DebugMode::drawShaderFullScreen()` does today (`DebugMode.cpp:224-312`)
— an `if (name == "...")` cascade — but read values from the freshly-built
`ofParameterGroup`'s `ofParameter<float>`/`<int>` members instead of
`DebugMode`'s `pXxx` bare floats. This keeps Phase 2 scoped to "new sketch +
new GUI shape," deferring the bigger `EffectRenderer` consolidation to
Phase 7+.

**Validation:**
- `make Release -j4` in the new sketch directory.
- `make RunRelease`, confirm: default state renders visibly for at least
  `desaturate`/`recolor`/`heatmap_recolor` (representative of the "no extra
  params," "one extra param," "many extra params with shader-side
  fallbacks" cases per the probe); cycling next/prev through the full
  effect list produces no black frames and no stale uniforms bleeding
  between effects (verify by watching `recolor`'s tint sliders reset to
  defaults, not leak into the next effect, when cycling).
- Confirm GUI shows only the active effect's params (visually count rows
  against `EffectCatalog`'s entry for that effect).

**Rollback boundary:** delete `sketches/shader-effect-debugger/` entirely;
no other sketch references it.

## Phase 3 — Whitelist/blacklist persistence

**Goal:** implement probe §8's schema and the debugger's save flows. Net-new
capability, not extraction — no prior art exists (probe §8).

**New files:**
- `shared/src/effects/EffectKnowledgeBase.h`/`.cpp` — `bool
  appendWhitelist(const KnowledgeEntry&)`, `bool appendBlacklist(const
  KnowledgeEntry&)`, `std::vector<KnowledgeEntry> loadWhitelist(const
  std::string& effectName) const`, same for blacklist, `bool
  isDuplicate(const KnowledgeEntry&, const std::vector<KnowledgeEntry>&)
  const` (exact-match on `snapshot` within a small epsilon, per numeric-type
  field). `KnowledgeEntry` struct matches the probe §8 JSON schema exactly
  (`schemaVersion`, `effect`, `list`, `snapshot` as
  `std::map<std::string,float>` — covers Float/Int/Bool uniformly since JSON
  numbers cover all three — `tolerance`, `label`, `notes`, `sourceSketch`,
  `sourceVideo`, `timestampUtc`, `perfObservedFps`, `qualityScore`,
  `sceneContext`).
- Malformed-file recovery: on load, wrap per-entry JSON parsing in a
  try/catch (or `nlohmann::json`'s `.value(key, default)` accessors, which
  the repo already uses per TF's tolerant preset loader, probe §1) so one
  corrupt entry logs a warning and is skipped rather than failing the whole
  file. Preserve-unknown-fields: read into a `nlohmann::json` object first,
  only extract known fields into `KnowledgeEntry`, re-serialize the
  original object (with any extra fields the current code doesn't know
  about) rather than a hand-built `KnowledgeEntry`-only object, so a future
  schema version's extra fields survive a round-trip through an older
  binary.
- Atomic write: write to a temp file in the same directory, then rename
  over the target (standard atomic-write-via-rename; `std::filesystem::rename`
  is available in this project's C++ standard per the existing
  `-std=c++2b` compile flag observed during the Phase 0 build).

**File location** (resolves probe open question 7): store under
`sketches/shader-effect-debugger/bin/data/knowledge/{effectName}.whitelist.json`
and `.blacklist.json` — one file per effect per list, **owned and physically
located inside the debugger sketch's own data directory**, not a
cross-sketch shared path (consistent with probe §7's "no runtime
cross-sketch sharing" finding — a sketch deployed independently to the Pi
must not depend on a relative path escaping its own `bin/data`). Other
sketches that later want to consume this data (Phase 4) get their own
physical copy synced the same way shader assets already are (manual
copy today; a follow-up tooling script is a reasonable future addition but
is **not** part of this implementation — copy by hand when Phase 4 needs it
for the one representative sketch being migrated).

**Debugger wiring:** `w`/`b` keys and GUI buttons call
`knowledgeBase.appendWhitelist/Blacklist()` with a `KnowledgeEntry` built
from the current `ofParameterGroup`'s live values; show a 2-second on-screen
toast ("Saved to whitelist" / "Save failed: <reason>") using the same
`ofDrawBitmapStringHighlight` idiom already used throughout this repo for
transient status text.

**Validation:** save 2+ whitelist and blacklist entries for `heatmap_recolor`,
restart the debugger, confirm they reload (visible via a small on-screen
count "12 whitelist / 3 blacklist entries loaded" at startup); hand-corrupt
one entry in the JSON file and confirm the app still starts and loads the
remaining valid entries with a logged warning; confirm duplicate-save
(same params, same effect) is rejected or deduplicated per
`isDuplicate()`.

**Rollback boundary:** delete `shared/src/effects/EffectKnowledgeBase.{h,cpp}`
and the debugger's `w`/`b` key handlers; nothing outside the debugger
depends on this yet.

## Phase 4 — Shared random-state generation pipeline

**Goal:** implement probe §9's recommended pipeline as `EffectRandomizer`,
consuming Phase 1 metadata and Phase 3's knowledge base.

**New files:**
- `shared/src/effects/EffectRandomizer.h`/`.cpp` —
  ```
  struct RandomizeRequest {
      std::string effectName;
      std::optional<uint32_t> seed;          // deterministic if set
      float whitelistBiasProbability = 0.3f;  // sketch-tunable
      int maxRetries = 5;                     // mirrors QuadrantManager::chooseDitherParams's cap
  };
  EffectParams generate(const RandomizeRequest&, const EffectDefinition&,
                         const EffectKnowledgeBase* knowledgeBase /* nullable */);
  ```
  Pipeline stages exactly as probe §9 lays out: hard limits (from
  `EffectDefinition::params[].hardMin/hardMax`) → sketch override range (an
  optional narrower `overrideMin/overrideMax` a caller can pass per-param,
  defaulting to the artistic range) → artistic range (metadata default) →
  whitelist-biased sampling (per probe §8: some fraction of the time, sample
  near a randomly-chosen whitelist entry within its `tolerance` or a small
  default jitter) → blacklist per-parameter forbidden-range rejection →
  bounded retry (regenerate up to `maxRetries` on blacklist rejection, then
  fall back to the last-generated candidate clamped away from the nearest
  forbidden range — "falls back safely if all candidates are rejected," per
  the task's explicit requirement) → return.
- Seeding: when `seed` is set, use a locally-scoped `std::mt19937` seeded
  with that value for this call only — **do not reseed the global
  `ofRandom()` stream** (probe §11: non-deterministic seeding is currently
  load-bearing behavior elsewhere; this must be additive/opt-in, never a
  global side effect). When unset, fall back to `ofRandom()` exactly as
  every existing system does today, preserving current visual behavior.

**Debugger wiring:** the `r` (randomize) action and its GUI button call
`EffectRandomizer::generate()` with the debugger's own seed field (if set)
and `knowledgeBase` — this is "the same generation path used by production
sketches," satisfying the task's explicit debugger requirement to preview
that exact path, not a debugger-only shortcut.

**Sketch retrofit (only BRP touched in this phase — see Phase 7 for why BRP
goes first):** none yet. Phase 4 only builds and validates the pipeline
inside the debugger; retrofitting BE/TF/FT's inline `ofRandom` call sites
happens in their respective migration phases (7-9) so each sketch's
visual-behavior-preservation check happens exactly once, at its own
migration step, not twice.

**Validation:** in the debugger, save 3+ whitelist entries for one effect
with distinct `mix`/`gamma` values, then repeatedly randomize with a fixed
seed and confirm (by eye, and by logging generated values) that
whitelist-biased draws cluster visibly near saved entries some fraction of
the time; add a blacklist entry with a forbidden `gamma` range and confirm
generated values never land inside it across 50+ repeated randomize calls;
confirm two calls with the same seed produce identical output
(determinism), and that leaving `seed` unset produces different output
each call (non-determinism preserved as the default).

**Rollback boundary:** delete `EffectRandomizer.{h,cpp}`; only the
debugger's `r` handler needs reverting to a stub or removed.

## Phase 5 — Scene evolution consolidation

**Goal:** extract `TFPresetTimeline`'s state+phase+easing model (probe §6's
recommended basis) into `shared/src/effects/EffectEvolutionController`,
scoped down to *effect-parameter* transitions only (probe open question 4)
— not TF's full scene/pattern/composition scope.

**Files read (extraction source, not modified in this phase):**
`sketches/temporal-fields/src/TFPresetTimeline.{h,cpp}`,
`sketches/temporal-fields/src/TFTimelineEasing.h`.

**New files:**
- `shared/src/effects/EffectEvolutionController.h`/`.cpp` — owns: current
  `EffectParams`, target `EffectParams` (both scoped to one effect's
  parameter set, not TF's whole-scene state), `Transitioning`/`Holding`
  phase enum (direct port of `TFPresetTimeline.h:37`'s shape), transition
  duration, hold duration, an `Easing` enum reusing
  `TFTimelineEasing.h`'s four curves verbatim (linear / smoothstep /
  smootherstep / easeInOutSine — copy the functions, they're pure math with
  zero TF-specific dependencies per the probe). `update(dt)` interpolates
  every `Float`-typed param (per Phase 1's `ParamType`) via the active
  easing curve; `Int`/`Bool`-typed params (e.g. `heatmap_recolor`'s
  `palette`, `reverse`) **switch discretely at the transition's midpoint**
  rather than interpolating (satisfies the task's explicit "discrete
  parameters switch safely" validation requirement — do not attempt to
  lerp an enum). `pickNextTarget()` calls into Phase 4's
  `EffectRandomizer::generate()` for automatic next-target generation
  (satisfies "automatic next-target generation" + reuses the already-built,
  already-validated pipeline instead of a second randomization path).

**Effect-switch interaction:** when the debugger (or a migrated sketch)
switches to a *different* effect entirely while evolution is active, reset
the controller's current/target state for the new effect's defaults rather
than attempting to interpolate between two different effects' parameter
sets (they may not share parameter names) — satisfies "effect switching
resets or migrates evolution state correctly" from the validation
requirements; resetting (not migrating) is the correct choice here since
cross-effect parameter migration has no sound definition given probe §5's
finding that even same-named parameters (`alpha`) mean different things
per-effect.

**Debugger wiring:** the `e` toggle enables/disables
`EffectEvolutionController::update(dt)` in the debugger's `ofApp::update()`;
when enabled, the per-effect `ofParameterGroup`'s values are overwritten
each frame from the controller's current interpolated state (GUI sliders
become read-only/reflect the animation while evolution is active — mirror
this with a simple `slider.setEnabled(!evolutionActive)` per parameter, no
new UI framework needed).

**Validation:** enable evolution on `heatmap_recolor` in the debugger,
confirm `mix`/`gamma`/`minLuminance`/`maxLuminance`/`intensity` interpolate
smoothly and `palette`/`reverse` switch discretely (not lerp) at the
expected point; confirm no NaN/invalid uniform values appear across a
5-minute soak (watch for shader compile-time-safe-but-runtime-odd values
like `gamma` briefly at exactly `0.0` mid-transition — Phase 1's
`hardMin`/`hardMax` clamps in `update()`'s interpolation step should
prevent this, verify explicitly); confirm seeded evolution (via Phase 4's
seed plumbing) produces identical transition sequences across two runs.

**Rollback boundary:** delete `EffectEvolutionController.{h,cpp}`; revert
the debugger's `e` toggle to a no-op.

## Phase 6 — Pattern drift consolidation

**Goal:** extract `TFPatternParticleField`'s bounded-velocity-at-spawn +
noise-driven-selection approach (probe §6's recommended basis),
generalized from "which texture" to "which parameter value."

**Files read (extraction source):**
`sketches/temporal-fields/src/TFPatternParticleField.cpp` (and its `.h`).

**New files:**
- `shared/src/effects/PatternDriftController.h`/`.cpp` — per-parameter
  config: `{ bool enabled; float strength; float speed; std::vector<std::string>
  allowedParams; float boundsMin, boundsMax /* or read from
  EffectParameterMetadata's artistic range if unset */; std::optional<uint32_t>
  seed; bool pausesDuringTransition; }`. Drift model: a bounded random walk
  around each allowed parameter's *current evolution-controller target* (not
  its base default), using `ofNoise()`-driven offsets exactly as
  `TFPatternParticleField.cpp:120-134` uses Perlin noise for texture
  selection, generalized to `offset = (ofNoise(paramSeed, noiseTime) - 0.5f)
  * 2.0f * strength`, clamped into `[boundsMin, boundsMax]`. `update(dt)`
  advances `noiseTime += dt * speed` per allowed param (independent phase
  per param, avoiding all drifting params moving in visible lockstep — same
  reasoning `TFPatternParticleField` itself uses per-particle noise
  offsets).
- `pausesDuringTransition`: when true and an `EffectEvolutionController` is
  mid-`Transitioning` phase, skip the drift `update()` call for that frame
  (simple external coordination, no coupling between the two classes beyond
  the caller checking the evolution controller's phase before calling
  drift's `update()`).

**Debugger wiring:** add drift enable/strength/speed controls to the
always-visible evolution/drift GUI section (Phase 2's dynamic-GUI section
boundary); `allowedParams` defaults to every `Float`-typed param in the
active effect's `EffectParameterMetadata` marked `safeToAnimate` (Phase 1
field — this is exactly what that field is for).

**Validation:** enable drift alone (evolution off) on `heatmap_recolor`'s
`gamma`, confirm bounded oscillation stays within configured bounds over a
2-minute soak, never NaN; enable both evolution and drift together with
`pausesDuringTransition=true`, confirm drift visibly pauses during each
transition's `Transitioning` phase and resumes during `Holding`.

**Rollback boundary:** delete `PatternDriftController.{h,cpp}`; revert
debugger's drift GUI section.

## Phase 7 — Migrate blob-region-prototype

**Goal:** first production-sketch migration (probe §1's ranking: thinnest
wrapper, lowest risk). Retire `VideoRegionEffectRenderer`'s hand-copied
uniform block in favor of Phase 1's `EffectCatalog` + a new shared
`EffectRenderer`.

**New file:**
- `shared/src/effects/EffectRenderer.h`/`.cpp` — extraction of the
  three-stage render shape probe §10 identifies as already manually
  copy-pasted three times (source crop → scratch FBO sized to destination →
  plain-textured composite draw preserving outer alpha). Signature mirrors
  `VideoRegionEffectRenderer::render()`'s existing `RenderRequest` shape
  (probe §10's table cites this as the class whose FBO-sizing fix BE/TF's
  comments already credit as the origin pattern — i.e. this extraction is
  formalizing a pattern the codebase has already converged on three times
  independently, not inventing a new one). Uniform binding inside this class
  reads from `EffectCatalog`'s metadata (Phase 1) instead of a hand-written
  `if/else if` cascade — this is the actual duplication removal the probe's
  executive summary identifies as the core value of this whole effort.

**Modified:**
- `shared/src/VideoRegionEffectRenderer.cpp`/`.h` — either (a) become a thin
  wrapper delegating to the new `EffectRenderer` (preferred — keeps BRP's
  existing `#include`/call sites unchanged, smallest diff to
  `sketches/blob-region-prototype/src/*`) or (b) BRP's consumers
  (`VideoRegionController`) switch to calling `EffectRenderer` directly and
  `VideoRegionEffectRenderer` is deleted. **Choose (a) first** — smaller,
  reversible, and validates `EffectRenderer`'s correctness before deciding
  whether the old class is worth deleting outright.

**Behavior preserved:** every one of BRP's current 17 `kEffectChoices`
entries must render identically (same literal uniform values, transcribed
in Phase 1 — verify pixel-for-pixel by comparing a screenshot per effect
before/after, or at minimum eyeball each of the 17 for the same look).

**Validation:** `make Release -j4`; cycle through all 17 effects via the
existing `pEffectIndex` GUI slider, confirm no visual regression against a
pre-migration baseline (screenshot diff or side-by-side eyeball); confirm
`heatmap_recolor` (not currently in BRP's `kEffectChoices` — see note below)
is *available* in `EffectCatalog` even if BRP doesn't select it by default,
proving the metadata layer is sketch-agnostic.

**Note on current BRP state:** `kEffectChoices` in
`sketches/blob-region-prototype/src/ofApp.cpp` currently lists 17 effects
and does not include `heatmap_recolor`. This phase does not change that
selection — it only changes *how* the selected effect's uniforms get bound
(via `EffectRenderer`+metadata instead of the hand-written cascade).
Whether to add `heatmap_recolor` to BRP's choice list is a separate,
optional follow-up, not part of this migration.

**Rollback boundary:** revert `VideoRegionEffectRenderer.cpp` to its
pre-Phase-7 body; delete `EffectRenderer.{h,cpp}` only if no later phase has
adopted it yet (check phases 8+ before rolling back).

## Phase 8 — Migrate blueprint_emergence and temporal-fields

**Goal:** both already build against shared `ShaderLibrary` (no
`config.make`/`PROJECT_EXCLUSIONS` change needed) — migrate their uniform
dispatch and randomization to the shared services.

**Modified:**
- `sketches/blueprint_emergence/src/BEFragment.cpp` — `setEffectUniforms()`
  replaced by a call into `EffectRenderer` (Phase 7) with `EffectCatalog`
  metadata; `pickAndStartEffect()`'s inline `ofRandom` ranges replaced by
  `EffectRandomizer::generate()` (Phase 4) calls, with BE's *existing*
  `ofRandom(lo,hi)` values passed as `overrideMin/overrideMax` per-param so
  visual behavior is unchanged (constraint: "existing artistic ranges
  become each sketch's override tier," not redesigned). `EffectSlot`'s
  `glm::vec4 params` packing (probe §5) can be replaced by a proper
  `EffectParams` (name-keyed map, from Phase 1's types) now that a real type
  exists — this also removes the palette/reverse float-packing hack this
  session's heatmap_recolor work introduced as a stopgap (see
  probe §3A's `heatmap_recolor` row).
- `sketches/temporal-fields/src/TFEffectPicker.cpp` — same pattern:
  `applyEffectUniforms()` → `EffectRenderer`, `randomizeEffectParams()` →
  `EffectRandomizer::generate()` with TF's existing ranges as overrides.
  **Leave `TFPresetTimeline`, `TFFragmentTransition`,
  `TFPatternParticleField`, `TFBackgroundLayer.pickNextMode()` untouched in
  this phase** — those are TF's own scene/pattern-level systems, not the
  effect-parameter-level system Phases 5/6 scoped down from them; TF
  *providing* the extraction source does not obligate TF to consume the
  generalized version in the same phase. A follow-up phase (not specified
  here, low priority per probe open question 4) could retrofit TF's own
  `TFEffectPicker` to sit on top of `EffectEvolutionController` instead of
  its bespoke timer, once the generalized version has proven itself on
  other sketches.

**Behavior preserved:** BE's 18-effect pool (including `heatmap_recolor`,
added this session) and TF's weighted-pick pool must produce visually
equivalent randomized output — same ranges, same distributions, only the
code path generating them changes.

**Validation:** `make Release -j4` for both; run each for several minutes,
confirm the autonomous effect-cycling behavior (BE's fragment-level
fade-in/dwell/fade-out, TF's weighted timer) looks unchanged from a
pre-migration baseline; confirm `heatmap_recolor`'s palette/reverse
selection (previously float-packed into `params.w`/`paramW`) now round-trips
correctly through the real `EffectParams` type.

**Rollback boundary:** two independent sketches, two independent rollback
points — reverting one does not require reverting the other.

## Phase 9 — quadrant-crosshair: production + debug consolidation

**Goal:** the highest-risk migration (probe §11: QC's production
`Quadrant::drawWithEffect()` and dev `DebugMode` are two independent
uniform-binding copies today — migrating only one keeps duplication alive
under a new name). Migrate both together.

**Modified:**
- `sketches/quadrant-crosshair/src/Quadrant.cpp` — `drawWithEffect()`'s
  inline `if (effect == "...")` cascade replaced by `EffectRenderer` +
  `EffectCatalog`, same as BRP/BE/TF. **Texture-unit assignments stay
  exactly as they are today** (probe §2's table: units 0/1/3/4/5 in
  production use, unit 2 free) — `EffectRenderer` must accept QC's
  additional `gridState`/`motionTex`/`motionDelayedTex` textures as optional
  extra bindings (a capability BRP/BE/TF never needed, since they don't
  have QC's multi-texture grid/motion system) — extend `EffectRenderer`'s
  request struct with an optional `std::vector<{name, texture, unit}>`
  extra-textures list rather than hardcoding QC's specific texture names
  into the shared class.
- `sketches/quadrant-crosshair/src/DebugMode.{h,cpp}` — shrink to whatever
  probe §2's classification table marks quadrant-specific-must-stay: since
  the standalone debugger (Phase 2) now covers general effect preview,
  `DebugMode` should be reduced to just the trigger-simulation HUD rows and
  QC-specific live-tuning of `QuadrantManager`'s trigger-driven parameters
  (if any remain useful for debugging quadrant-specific behavior) — the
  general shader-cycling/parameter-panel functionality this phase's
  `EffectRenderer` migration + Phase 2's standalone sketch together
  supersede should be **deleted**, not kept as a third copy. Exact scope of
  what remains is a judgment call at implementation time; the guiding rule
  from probe §2 is: keep only what depends on `QuadrantManager`/`Quadrant`-
  specific state (grid, motion textures, per-quadrant triggers), delete
  everything that's just "preview one shader full-screen with tunable
  params" (now the standalone debugger's job).
- `sketches/quadrant-crosshair/src/ShaderLibrary.{h,cpp}` — once both
  `Quadrant.cpp` and the shrunk `DebugMode` consume `EffectCatalog`+
  `EffectRenderer` instead of this local fork's `get()`/`has()` calls
  directly, evaluate whether QC can switch to the shared `shared/src/
  ShaderLibrary` outright (removing the fork). This requires QC's
  `config.make` to pull in `shared/src` for this one class the way
  BRP/BE/TF's does — check whether QC currently excludes `shared/src`
  wholesale (like FT) or simply never opted in; if it's a simple opt-in gap
  (not a symbol-collision blocker like FT's), make the `config.make` change
  and delete the fork.

**Behavior preserved:** every `QuadrantManager` trigger→`pushShader()` call
site (probe §2's contrast case) is untouched — this migration only changes
*how* `Quadrant::drawWithEffect()` binds uniforms once a shader name is
selected, never *which* shader gets selected or when.

**Validation:** `make Release -j4`; exercise every `TriggerBus` trigger path
manually (or via QC's existing debug-trigger simulation, if retained) and
confirm each still produces its expected effect+params; confirm the shrunk
`DebugMode` (if any remains) still functions for whatever QC-specific
purpose it's kept for; confirm no black frames, no stale uniforms across
effect switches in both the production and (if retained) debug paths.

**Rollback boundary:** this is the largest single-phase diff in the whole
plan — consider landing `Quadrant.cpp`'s `EffectRenderer` migration and
`DebugMode`'s shrink as two separate commits within this phase so either
can be reverted independently if the other causes a regression.

## Phase 10 — Cleanup

**Goal:** remove now-dead code per each phase's findings.

- Delete `sketches/fragment-trail/src/ShaderLibrary.{h,cpp}` **only if**
  probe open question 6 (FT's `PROJECT_EXCLUSIONS` blocker) has been
  resolved in a separate, dedicated piece of work — **not part of this
  implementation's scope** per the constraints section. Until then, FT's
  fork stays, having received only the Phase 0 load-failure fix.
- Delete `sketches/quadrant-crosshair/src/ShaderLibrary.{h,cpp}` if Phase
  9's evaluation determined it's safe to switch to the shared class.
- Remove any `EffectRenderer`-superseded code left behind in
  `VideoRegionEffectRenderer.cpp`/`BEFragment.cpp`/`TFEffectPicker.cpp`/
  `Quadrant.cpp` that Phases 7-9 didn't already clean up inline (dead
  `if/else if` cascades, unused helper functions).
- Re-run the full duplicate-asset diff from probe §7 to confirm no new
  drift was introduced by this project's own multi-phase, multi-session
  editing.

## Acceptance criteria

- [ ] `heatmap_recolor` (and every other Contract-A effect) previews
      correctly in the new standalone debugger with no black output at
      `mix=0` or `mix=1`.
- [ ] GUI in the debugger shows only the active effect's parameters at all
      times, verified by cycling through every registered effect.
- [ ] Whitelist/blacklist save/load round-trips correctly, survives a
      hand-corrupted file, and rejects/deduplicates repeat saves.
- [ ] `EffectRandomizer::generate()` demonstrably favors whitelist regions
      and avoids blacklist regions over repeated calls, with bounded retry
      and a safe fallback when all candidates are rejected.
- [ ] `EffectEvolutionController` interpolates float params smoothly,
      switches discrete params (palette/reverse) without lerping them, and
      never produces NaN/out-of-range uniforms.
- [ ] `PatternDriftController` stays within configured bounds and correctly
      pauses during evolution transitions when configured to.
- [ ] BRP, BE, TF build and visually match their pre-migration behavior
      after adopting `EffectRenderer`/`EffectCatalog`/`EffectRandomizer`.
- [ ] QC's production trigger-driven effect selection is unchanged after
      migration; whatever remains of `DebugMode` still serves its
      QC-specific purpose.
- [ ] FT is untouched beyond the Phase 0 load-failure fix; still builds and
      runs identically to before this work.
- [ ] RG, RP, CP are untouched entirely.
- [ ] No sketch's `Makefile`/`config.make` changes except QC's (only if
      Phase 9 determines the `ShaderLibrary` fork can be retired) — FT's
      `PROJECT_EXCLUSIONS` is explicitly not touched by this implementation.

## Final file-change checklist

**Created:**
- `sketches/shader-effect-debugger/` (full new sketch)
- `shared/src/effects/EffectParameterMetadata.h`
- `shared/src/effects/EffectDefinition.h`
- `shared/src/effects/EffectCatalog.cpp` (+ matching `.h`)
- `shared/src/effects/EffectKnowledgeBase.h`/`.cpp`
- `shared/src/effects/EffectRandomizer.h`/`.cpp`
- `shared/src/effects/EffectEvolutionController.h`/`.cpp`
- `shared/src/effects/PatternDriftController.h`/`.cpp`
- `shared/src/effects/EffectRenderer.h`/`.cpp`
- `sketches/shader-effect-debugger/bin/data/knowledge/*.whitelist.json`,
  `*.blacklist.json` (generated at runtime by debugger use, not authored)

**Modified:**
- `shared/src/ShaderLibrary.cpp` (Phase 0)
- `shared/src/VideoRegionEffectRenderer.{h,cpp}` (Phase 7)
- `sketches/blueprint_emergence/src/BEFragment.{h,cpp}` (Phase 8)
- `sketches/temporal-fields/src/TFEffectPicker.{h,cpp}` (Phase 8)
- `sketches/quadrant-crosshair/src/Quadrant.cpp` (Phase 9)
- `sketches/quadrant-crosshair/src/DebugMode.{h,cpp}` (Phase 9, shrink)
- `sketches/quadrant-crosshair/config.make` (Phase 9, only if fork retired)

**Possibly deleted (conditional, per phase notes above):**
- `sketches/quadrant-crosshair/src/ShaderLibrary.{h,cpp}` (Phase 9/10,
  conditional)
- `sketches/fragment-trail/src/ShaderLibrary.{h,cpp}` (Phase 10, only after
  a separate, out-of-scope resolution of the `PROJECT_EXCLUSIONS` blocker)

**Explicitly not touched at any phase:**
- `sketches/radar-effects-gallery/**`, `sketches/radar-pulse/**`,
  `sketches/contour-portrait/**`, `sketches/hud_elements/**`,
  `sketches/hud_validation_harness/**`

## Risks and rollback notes

- Every phase after Phase 1 depends only on phases before it, never on
  phases after — this ordering was chosen specifically so any phase can be
  rolled back without unwinding later work that doesn't yet exist while the
  implementation is in progress.
- Phase 9 (QC) is the single highest-risk phase — it's the only sketch with
  genuine multi-texture complexity (`gridState`/`motionTex`/
  `motionDelayedTex`) the shared `EffectRenderer` didn't originally need to
  support. If Phase 9 reveals `EffectRenderer`'s extra-texture extension is
  awkward in practice, it is acceptable to leave QC's production path
  (`Quadrant::drawWithEffect()`) on its current hand-written dispatch
  permanently and scope Phase 9 down to just the `DebugMode` shrink +
  linking QC's local `ShaderLibrary` fork's `EffectCatalog` metadata for GUI
  purposes only, without moving QC's actual uniform-binding code — a
  smaller, safer version of this phase if the full migration proves too
  disruptive.
- If any migrated sketch's post-migration visual output cannot be confirmed
  identical to its pre-migration baseline with confidence (e.g. no
  side-by-side screenshot tooling available in this environment), treat
  that as a blocking finding for that phase specifically, not a reason to
  skip validation — flag it in that phase's validation report rather than
  proceeding on an unverified assumption, consistent with how the
  `heatmap_recolor` work earlier in this session flagged Raspberry-Pi-only
  validation as out of reach from a macOS development machine.
