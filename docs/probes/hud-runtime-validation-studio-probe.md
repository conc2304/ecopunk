# HUD Runtime and Validation Studio — Discovery Probe

**Status**: Discovery only. No production code was implemented, refactored, renamed, moved, or deleted by this probe.

**Scope note on terms used throughout**: every claim below is tagged **Code fact**, **Approved project decision**, **Recommendation**, or **Unresolved**, per the task's reporting rules. "Approved project decision" means the claim is stated as settled in one of the seven source-of-truth documents or the Decision Log. Where a document (notably `HUD-Semantic-Slot-Model-v1.md`) is itself still in draft/review status, its content is cited as **Recommendation** (the document's own recommendation) or **Unresolved**, never as an approved decision, and this is flagged explicitly at first use.

---

## 1. Executive Summary

- **No `HudCompositor`, `MediaViewportMesh`, `ExperienceRuntime`, `SceneManager`, or `IEcopunkScene` implementation exists in the codebase today.** *(Code fact — confirmed by repo-wide search; also stated as current status in the Roadmap's "Not yet implemented" list.)* The runtime side of this initiative has not started.
- **A real, mature, canonical shared HUD *widget* library already exists** at `shared/src/hud/` (18 widgets + `WindowChrome`, one shared base class, one shared frame renderer, one shared theme/bounds/color model) and is consumed today by four of the six in-scope production scenes (`blueprint_emergence`, `fragment-trail`, `quadrant-crosshair`, `temporal-fields`). It draws only immediate-mode vector primitives — no FBOs, no shaders, no textures anywhere in `shared/src/hud/` or `shared/src/hud_overlay/` *(Code fact, verified by grep across both trees)* — which is a strong, load-bearing Pi-cost precedent for the new HUD, but it is explicitly **developer/scene-local ambient chrome**, not the frozen `HudCompositor`/`SceneFrame` architecture, and per the frozen contract it must not become that by accretion.
- **A second, separate HUD system — `shared/src/hud_overlay/`** — is a self-contained "glitch overlay" ambient/organism system, explicitly isolated from scene internals by its own README, gated behind a debug key (`o`) in three scenes, and excluded from the runtime scene cycle per the Roadmap/Decision Log. It reuses several `shared/src/hud/` widgets and is a second, independent precedent for theme mapping (`HudOverlayTheme.h`'s `Palette → hud::HudTheme` function) worth reusing as a *pattern*, not as code.
- **`hud_validation_harness` is real, small, and narrowly scoped**: a non-interactive, fixed-dt, deterministic screenshot harness that exercises 8 of the 18 `shared/src/hud/` widgets (plus all 5 frame styles) through ~60 named scenarios, with one genuinely reusable idea — a per-scenario blend-mode-leak check comparing GL blend state before/after each widget's `draw()`. It has **no scene concept, no fake `SceneFrame`, no `MediaViewportMesh`, no GL-contamination injection, no interactivity** — it is a widget-level regression tool, not a HUD-studio precursor, and 10 of the 18 widgets it links against are never actually exercised in its scenario list.
- **`hud_elements` is not code** — a single pointer-only `README.md` (confirmed: `sketches/hud_elements/README.md`, one file) directing readers to `shared/src/hud/`.
- **No vocabulary, skin, palette, or curated-display-name infrastructure exists anywhere in the repository.** *(Code fact, confirmed by repo-wide search)* This matches the Scene Observability Profile's own Open Question #2 finding the same gap. The closest existing precedent is the shared video-effects catalog's per-effect `displayName` string field (`shared/src/video-effects/core/VideoEffectDefinition.h`) — a compiled, ID-keyed display string, structurally close to one layer of the semantic model's proposed vocabulary resolution chain, but with no pack/override/fallback mechanism around it.
- **A strong, in-repo, already-battle-tested precedent for `MediaViewportMesh` exists in two independent places**: `TFShapeFragmentRenderer`'s arc-stepped wedge-mesh triangulation (`sketches/temporal-fields/src/TFShapeFragmentRenderer.cpp`) and `Fragment::drawTexturedCircleMesh` (`shared/src/Fragment.cpp`). The second is especially significant: the codebase **already tried shader-based circular clipping (`gl_FragCoord` discard) for live video and explicitly abandoned it** for a textured-mesh approach after a documented, real bug (outline-only rendering from a window-space/mask-space mismatch). This is direct, first-party evidence favoring the mesh approach the Contract's §11 already names as the algorithmic precedent to reuse.
- **No stencil-buffer usage exists anywhere in the repository.** Scissor-test usage is narrowly confined to two files (`FTFragment.cpp`, `Quadrant.cpp`), both well-commented and both restore state (`glDisable(GL_SCISSOR_TEST)`) before returning. This is a small, well-understood GL-state surface for the future `SceneRenderGuard`/validation-studio GL-contamination tests to target.
- **Font/text infrastructure is thin and inconsistent.** The canonical `shared/src/hud/` widget library has **no TrueType font loading at all** — every widget's text goes through one fallback helper (`hud::drawTextFallback` → `ofDrawBitmapString`), a fixed bitmap font scaled via `ofScale`. The only real `ofTrueTypeFont` usage in the whole repo is one scene-local class (`shared/src/AnnotationRenderer.h`, used by `blueprint_emergence`) and one excluded-tool class (`sketches/radar-effects-gallery/src/GalleryCompositor.h`). Neither exposes text measurement, truncation, or wrapping as a public API. Building real, legible typography for the wireframe HUD is close to a from-scratch task, not an extraction task.
- **The Six Fake-Profile Evidence Maps (Section 11) are largely already drafted** by `HUD-Semantic-Slot-Model-v1.md` §18 and the Scene Observability Profile Parts 1–5 — both documents converge independently on the same six scenes' candidate signals. This probe cross-checks that draft content against actual accessor names in code and reclassifies each candidate by exposure status, which the two source documents deliberately left at a coarser grain.

---

## 2. Documents Reviewed

| # | Document | Path | Status as found |
|---|---|---|---|
| 1 | Ecopunk HUD System Master Roadmap | `docs/Ecopunk-HUD-System-Master-Roadmap.md` | Active project tracker |
| 2 | Scene/HUD Contract v1 | `~/Downloads/Scene-HUD-Contract-v1.md` (not yet copied into `docs/`) | "Ready for freeze… pending final sign-off" |
| 3 | Scene Observability Profile | `docs/scene-observability-profile-probe-report.md` | Complete, dated Aug 4 2026, cited as approved-status source in Roadmap §3 |
| 4 | Shared Project Context | `~/Downloads/ecopunk-runtime-project-starter/shared-project-docs/00-shared-project-context.md` | Active |
| 5 | Architecture Governance | `.../01-architecture-governance.md` | Active |
| 6 | Cross-Domain Handoff Protocol | `.../02-cross-domain-handoff-protocol.md` | Active |
| 7 | Decision Log | `.../03-decision-log.md` | 8 approved decisions (DEC-001…DEC-008) |
| — (additional, closely relevant, read for grounding) | HUD Semantic Slot Model v1 | `docs/HUD-Semantic-Slot-Model-v1.md` | **Draft, "Status: Architecture draft for cross-domain review."** Roadmap's own milestone checklist (§8) still shows "semantic slot model approved" **unchecked**. Treated throughout this probe as Recommendation/Unresolved content, not frozen contract. |
| — | `CLAUDE.md` (repo root of `EcopunkVideoCollage`) | project root | Active engineering guardrails, mainly about the shared video-effects service; read for orientation |

**Inconsistency flagged, not silently resolved**: the task's own "source-of-truth documents" list names "Scene/HUD Contract v1" as approved-for-constraints, and the Contract document's own header says "Ready for freeze… pending final sign-off from both initiatives" — i.e. it is written as *reviewed and stable* but not self-declared frozen. This probe treats every constraint explicitly reiterated in the task's "Frozen constraints" list as binding (per the task's own instruction to treat approved contracts and decisions as constraints), and treats every other Contract-only detail as "Approved project decision" only where the Decision Log or Roadmap independently corroborates it, flagging the rest as decisions made by document review but not yet logged in `03-decision-log.md`.

---

## 3. Files and Code Areas Inspected

```text
shared/src/hud/                          — canonical widget library (18 widgets, WindowChrome, shared/)
shared/src/hud_overlay/                  — glitch-overlay ambient/organism system
shared/src/ridgeline/RidgelineRenderer.* — GL-state sample (clean push/pop)
shared/src/Fragment.h / .cpp             — circular-mask precedent (mesh vs. shader-discard)
shared/src/AnnotationRenderer.h          — only real TTF font usage in shared/src
shared/src/video-effects/core/VideoEffectDefinition.h, catalog/DefaultVideoEffectCatalog.cpp
                                          — closest existing "vocabulary" precedent (displayName)
sketches/hud_validation_harness/         — full inspection (ofApp.h/.cpp, Makefile, captures/, obj/)
sketches/hud_elements/                   — full inspection (README.md only)
sketches/temporal-fields/src/TFShapeFragmentRenderer.*, TFFragmentShape.h, TFHudLayer.h
sketches/quadrant-crosshair/src/HudManager.h/.cpp, Quadrant.cpp (scissor), CrosshairSystem.cpp
sketches/blueprint_emergence/src/BEComposition.h/.cpp (hud:: usage), BEFragment.cpp (drawMaskedFill)
sketches/fragment-trail/src/FTFragment.h/.cpp (scissor, WindowChrome), FTBackgroundLayer.h,
   FTFragmentPool.h, FTOverlayDirector.cpp
sketches/blob-region-prototype/src/  — confirmed no hud:: dependency
sketches/contour-portrait/src/       — confirmed no hud:: / shared/src dependency at all
```

Repo-wide greps run for: `HudCompositor`, `MediaViewportMesh`, `TFShapeFragmentRenderer`, `ofScissor`/`glScissor`/`GL_SCISSOR_TEST`, `GL_STENCIL`, `ofTrueTypeFont`/`loadFont`, `ofFbo` and `ofShader` inside `shared/src/hud*`, `chamfer`/`octagon`/`vertexCut`, `displayName`/`vocabulary`/`curatedTitle`, `getCurrentFilename`/`mediaName` across the six in-scope scene source trees.

---

## 4. Current HUD Ownership Map

| Item | Path | Ownership | Public API surface | Dependencies | Category | Reuse recommendation |
|---|---|---|---|---|---|---|
| Canonical widget library | `shared/src/hud/` | Shared, cross-scene (per its own README: "the only copy… do not copy into a sketch") | `hud::HudElements.h` aggregator; each widget: `setup()`, `update(dt)`, `draw()`, `setBounds()`, `setTheme()`, `setColors()`, `setFrameOptions()`, `randomize(seed)` | `ofMain.h` only; zero FBO/shader/texture use | Production runtime (scene-local, not compositor-owned) | **Use only as precedent** for the new HUD's widget *shapes* (Status Badge ≈ `StatusLightWidget`, Metadata Card ≈ `DataCardWidget`, Progress Ring/Bar ≈ `GaugeWidget`, Timeline ≈ none exists) — do not wire these directly into `HudCompositor`; see §5 for why. |
| `HudFrameRenderer` | `shared/src/hud/shared/HudFrameRenderer.*` | Shared, composed-not-inherited by every widget | `draw(bounds, colors, options, t)` | none | Production runtime (scene-local) | Reuse as-is *within* the existing widget library; not a `HudCompositor` dependency. |
| `hud_overlay` (Glitch Overlay System) | `shared/src/hud_overlay/` | Shared, cross-scene ambient layer | `HudOverlayLayer::setup/update/drawAmbient/drawOrganisms/draw`, `trigger*()` passthroughs, `HudOverlayDialPanel` (ofxGui) | `shared/src/hud/` widgets, `ofxGui` | Developer/scene-local ambient toggle (behind `o` key in 3 scenes) — explicitly **not** the cinematic HUD (`ofxGui`-driven dial panel, isolation boundary documented in its own README) | **Use only as precedent** — its `Palette → hud::HudTheme` mapping function (`HudOverlayTheme.h`) is a clean, small pattern worth mirroring for the future skin/vocabulary loader's theme resolution, but the module itself must stay excluded from the runtime scene cycle per Decision Log/Roadmap. |
| `hud_validation_harness` | `sketches/hud_validation_harness/` | Standalone sketch, developer tooling | `ofApp` scenario runner; not a library, nothing to consume | `shared/src/hud/` (all 18 widgets linked, only 8 exercised) | Developer tooling, non-interactive, screenshot-diff only | **Retain as a lower-level visual-regression tool** for the existing widget library (see §10); do not extend it into the new interactive studio — its architecture (fixed scenario list, self-driving `ofApp::draw()` loop, no input handling) is structurally the wrong shape for an interactive tool needing fake `SceneFrame`/scene switching/state simulation. |
| `hud_elements` | `sketches/hud_elements/README.md` | N/A — pointer only | None | None | Obsolete/placeholder | **Retire** — nothing to migrate; the README already correctly redirects to `shared/src/hud/`. Confirms task's own instruction not to assume similarly-named things are equivalent: `hud_elements` (the sketch directory) and `HudElements.h` (the real aggregator header inside `shared/src/hud/`) are different things sharing a name. |
| `AnnotationRenderer` | `shared/src/AnnotationRenderer.h/.cpp` | `blueprint_emergence`-scene-local (lives in `shared/src` but is only consumed by that one sketch per current code) | `loadCodeFont(path,size)`, `drawCornerLabel`, `drawGrid`, `drawDivider`, `drawMeasurementLines`, `drawCodeText`, `drawHubHighlight`, `clearCodeTextInRect` | One `ofTrueTypeFont` member, `GridSystem`, `Fragment` | Scene-local content (structural annotation chrome, not a reusable text widget) | **Use only as precedent** for real-font loading conventions (`loadFont(path, size)` signature) — the class itself is coupled to `BEComposition`'s grid/fragment model and not a generalizable text-rendering utility. |
| Video-effects `displayName` | `shared/src/video-effects/core/VideoEffectDefinition.h` + `catalog/DefaultVideoEffectCatalog.cpp` | Shared effects catalog | `VideoEffectDefinition::displayName` (plain `std::string`, compiled literal per effect) | none beyond the catalog itself | Production runtime data, not vocabulary infrastructure | **Adapt** — this is the closest existing precedent for "effect.\<id\>.label" vocabulary entries the Semantic Slot Model's draft proposes (§14.3 example key `effect.chromatic_aberration.label`), but it has no pack/override/fallback chain around it and is owned by the Shared Effects domain, not HUD. |
| Screenshot/visual-regression tooling | `sketches/hud_validation_harness/src/ofApp.cpp` | Developer tooling | `ofSaveScreen(...)` per named scenario; deterministic filenames (`<Widget>_<variant>.png`) | none beyond OF | Developer tooling | Reuse the **pattern** (fixed-dt stepping to a named scenario, deterministic filename, `ofSaveScreen`) directly in the new studio's screenshot capability; the harness's own scenario-list/`ofApp` shape does not need to be extended, per §10. |

**No theme/skin/palette/font-atlas/asset-loader system exists beyond the two items above** (`HudOverlayTheme.h`'s palette mapping, `AnnotationRenderer`'s font loader). No `HudSkin`, `HudSkinLoader`, `skin.json`, or manifest parser exists anywhere in the repo — confirmed by the Roadmap's own "Not yet implemented" list (§3) and by repo-wide search. **Code fact.**

---

## 5. Existing Widget Inventory

All 18 widgets share one base (`hud::HudWidget`, `shared/src/hud/shared/HudWidget.h`) and one bounds/theme model (`hud::HudBounds`, `hud::HudTheme`, `hud::WidgetColors`, `hud::FrameOptions`, `hud::MotionSettings`, all in `shared/src/hud/shared/HudTypes.h`). Cross-cutting facts, established once here rather than per-row:

- **Data inputs**: every widget is configured through a per-widget `*Options` POD struct (`setOptions(...)`) plus a handful of single-field setters (`setValue`, `setState`, `setLabel`, `pushLine`, …) — none read from a shared/runtime status struct; all are driven by direct scene code calling setters every frame or on event.
- **Text/font dependency**: **zero TrueType font usage** — all text goes through `hud::drawTextFallback()` (`shared/src/hud/shared/HudUtils.h:41`), which wraps `ofDrawBitmapString` in an `ofScale`. No measurement, no wrapping, no truncation exists anywhere in this library. *(Code fact.)*
- **Animation/update**: every widget accumulates its own `time` via `HudWidget::update(dt)`; several (`FlowFieldWidget`, `NodeNetworkWidget`, `ReticleWidget`) persist seeded procedural state and support `randomize(seed)`; the rest recompute purely from `time`/`ofNoise` each frame (no persisted layout to reseed).
- **FBO/shader/mesh use**: **none** anywhere in `shared/src/hud/` — confirmed by grep. Vector primitives only (`ofDrawRectangle`, `ofDrawCircle`, `ofPolyline`, `ofMesh` is not used here at all — even `ScannerWidget`'s "hero" rendering is immediate-mode).
- **Dynamic allocation during update/draw**: mostly none; `DataCardWidget` resizes/refills a `std::vector<float> spark` once in `setup()` and mutates it in place afterward (`erase(begin())` + `push_back()` each ~0.2s — a per-tick vector shift, not a per-frame allocation, but a real per-tick memory move worth measuring on Pi). `LogScrollWidget` uses a `std::deque<std::string>`, capped at `maxLines`, appended only on explicit `pushLine()` calls (event-driven, not per-frame).
- **GL-state modifications**: every widget's `draw()` observed wraps itself in `ofPushStyle()`/`ofPopStyle()` and toggles `ofEnableBlendMode(OF_BLENDMODE_ADD)` vs. `ofEnableAlphaBlending()` based on `theme.additive`, then calls `ofDisableBlendMode()` before `ofPopStyle()` — self-contained, does **not** rely on caller discipline for blend state (see §8).
- **Fixed-size assumptions**: none — every widget scales via `sx`/`sy`/`su` helpers (`HudUtils.h`) relative to its own `HudBounds`, confirmed responsive via `README.md`'s `windowResized` example.
- **Clipping assumptions**: none — no widget clips its own content to `bounds`; anything drawn near/past the edge (e.g. `ScannerWidget`'s sweep, `FlowFieldWidget`'s streamlines) is trusted to stay within bounds by its own math, not enforced by a scissor/mask. This is a real gap if the new HUD needs region-strict clipping for flexible slots.
- **Likely Pi cost**: low per-widget (vector primitives, small vertex counts, no texture upload) — but **not yet measured on Pi hardware anywhere in the repo** (confirmed: `README.md`'s own "Raspberry Pi notes" section states "Pi runtime validation… is currently outstanding — no Pi-specific build target exists in this repo yet"). Treat all Pi-cost claims here as **Recommendation**, not **Code fact**, per the task's own instruction not to claim Pi feasibility from desktop behavior.

### Per-widget table

| Widget | Path | Suitability for new HUD's approved candidate set |
|---|---|---|
| `ScannerWidget` | `ScannerWidget/` | Ambient semantic widget precedent only — hero circular treatment, has in-code-only tuning dials (`scaleMultiplier`, `globalOpacity`, `lineWidthScale`, `hueShift`) never exposed as options |
| `PulseEmitterWidget` | `PulseEmitterWidget/` | Ambient semantic widget precedent — uniquely exposes external state (`getActivePulses()`, `onPulseUpdate` callback); built for `radar-pulse` (excluded scene) |
| `NodeNetworkWidget` | `NodeNetworkWidget/` | Ambient semantic widget precedent; no direct match to the approved candidate set |
| `FlowFieldWidget` | `FlowFieldWidget/` | Ambient semantic widget precedent; `density` option is a real perf-scaling knob worth copying |
| `ContourWidget` | `ContourWidget/` | Ambient semantic widget precedent only |
| `GaugeWidget` | `GaugeWidget/` | **Closest existing precedent for Progress Ring/Progress Bar** (`GaugeStyle::Ring/Segmented/SemiCircle`); hand-rolled arc polyline (README notes `ofDrawArc` does not exist in this OF version — a real constraint for any new ring/arc widget) |
| `DataCardWidget` | `DataCardWidget/` | **Closest existing precedent for Metadata Card**; bundles title/value/meter/sparkline in one card — the sparkline part is explicitly out of scope for the new HUD's v1 candidate set per this task's own instruction (no Sparkline/Mini Graph) and would need to be dropped/adapted, not reused whole |
| `HexGridWidget` | `HexGridWidget/` | Ambient semantic widget precedent; `pulseAt(nx, ny)` ties ambient activation to a real external event — a good pattern for Ambient-classified slot binding |
| `ReticleWidget` | `ReticleWidget/` | Ambient/flexible scene-specific precedent (Standard/Tracking presets); already extended once (`triggerAt`) for `hud_overlay` reuse — a real precedent for additive widget extension without breaking existing callers |
| `GlitchTearWidget` | `GlitchTearWidget/` | Ambient one-shot effect; used today as a real-event-triggered accent (`trigger()`) in both `HudManager` and `hud_overlay` |
| `StatusLightWidget` | `StatusLightWidget/` | **Closest existing precedent for Status Badge** — Idle/Active/Alert, `setState()` single-field update matches the "compact update API" pattern the new HUD should use for per-frame slot binding |
| `LogScrollWidget` | `LogScrollWidget/` | Validation-tooling only / unsuitable for the cinematic HUD's approved candidate set — closer to a developer console than any approved widget type |
| `TickBurstWidget` | `TickBurstWidget/` | Ambient one-shot; built for `hud_overlay`'s Lock Sequence organism |
| `RadarStationWidget` | `RadarStationWidget/` | Ambient semantic widget; built for `hud_overlay` |
| `BreathingTickClusterWidget` | `BreathingTickClusterWidget/` | Ambient semantic widget; built for `hud_overlay` |
| `TelemetryReadoutWidget` | `TelemetryReadoutWidget/` | Ambient text readout (`FrameCounter`/`CoordinateWalk` modes) — explicitly decorative/flavor text, not a real-data binding; a real precedent for the "Ambient" `HudDataClass` category the (draft) Semantic Slot Model defines |
| `HalftonePatchWidget` | `HalftonePatchWidget/` | Ambient one-shot; built for `hud_overlay`'s Fault Cascade organism |
| `TextCalloutWidget` | `TextCalloutWidget/` | **Closest existing precedent for Label** as a triggerable one-shot; not a persistent/bound label widget as-is |
| `DashedLineWidget` | `DashedLineWidget/` | Ambient one-shot connector; built for `hud_overlay`'s Handshake organism |
| `WindowChrome` | `WindowChrome/` | Pure framing chrome (border/title bar/close-box glyph), draws no content itself — closest existing precedent for a per-region frame treatment, not a data widget |

### Gaps against the approved candidate widget set

| Approved candidate | Existing precedent | Gap |
|---|---|---|
| Label | `TextCalloutWidget` (one-shot only) | No persistent, bound, always-visible label widget exists |
| Value | none | No generic scalar/text "Value" widget exists — every widget bundles a value into a larger card/gauge |
| Status Badge | `StatusLightWidget` (dot+label, 3 states) | Real precedent exists but is a light/dot metaphor, not badge/pill text — needs adaptation, not reuse-as-is |
| Progress Bar | `DataCardWidget`'s internal meter bar (not standalone) | No standalone Progress Bar widget exists |
| Progress Ring | `GaugeWidget` (`GaugeStyle::Ring`) | Real precedent exists, bundled with Segmented/SemiCircle styles the new HUD may not need |
| Effect Chips | none | No existing widget represents "N named active effects as chips" — `HudManager`'s `quadCards[4]` (per-quadrant `DataCardWidget`) is the closest informal precedent but is a full card per channel, not a compact chip row |
| Metadata Card | `DataCardWidget` | Closest match, needs the sparkline dropped per this task's instruction |
| Icon Strip | none — `hud_overlay`'s label tables (`ReticleOptions::labelOverride`) are the nearest text-only analog | No icon-asset or icon-strip rendering exists anywhere in the repo (art pipeline is unstarted per Roadmap) |
| Timeline | none | The Scene Observability Profile (Part 7) and Semantic Slot Model (§15's `HudWidgetType::Timeline`) both call for one; no widget in this library represents a multi-state sequence |

**Recommendation**: build Label, Value, Progress Bar, Effect Chips, Icon Strip, and Timeline as new HUD-owned widgets under the future `HudCompositor`/wireframe tree, reusing this library only as visual/behavioral precedent (bounds model, `su`/`sx`/`sy` scaling helpers, composition-over-inheritance for frame rendering) — not by direct instantiation, since `shared/src/hud/` widgets are scene-local content today and the frozen contract gives `HudCompositor` its own asset/type boundary (§12 of the Contract) fully separate from `SceneServices`.

---

## 6. Scene-Local HUD Inventory

Per-scene shared-widget usage, confirmed by direct source inspection (not inferred from names):

| Scene | Uses `shared/src/hud/`? | Uses `hud_overlay`? | Scene-local HUD/overlay classes |
|---|---|---|---|
| `blob-region-prototype` | **No** — confirmed, no `hud::` references anywhere in `sketches/blob-region-prototype/src` | No | None — "deliberately thin `ofApp`" per its own code comment (Observability Profile) |
| `contour-portrait` | **No** — confirmed, no `shared/src` dependency of any kind (Observability Profile, reconfirmed here) | No | None |
| `blueprint_emergence` | Yes — `BEComposition.h/.cpp` | Yes (`o` key, `ofApp.h`) | `AnnotationRenderer` (own TTF font, grid/measurement/code-text chrome); rotating `hud::HudWidget*` slot (`hudWidget` union of `DataCardWidget`/`GaugeWidget`/`NodeNetworkWidget`/`ReticleWidget`/`HexGridWidget`/`StatusLightWidget`) plus a standalone `hud::ScannerWidget circleScanner` |
| `fragment-trail` | Yes — `FTBackgroundLayer.h`, `FTFragment.h/.cpp`, `FTFragmentPool.h/.cpp` | Yes (`FTOverlayDirector.cpp`, `ofApp.h`) | `FTFragment` frames its own video crop with `hud::WindowChrome`; `FTBackgroundLayer` owns a `hud::HexGridWidget`; `FTOverlayDirector` drives `hud_overlay`'s organisms off real spawn/mode events (a real precedent for event-coupled ambient triggers, per `hud_overlay`'s own README "Event Coupling" caveat) |
| `quadrant-crosshair` | Yes — `HudManager.h/.cpp` (richest direct widget consumer: 9 distinct widget types + a 4-element `DataCardWidget` array) | Yes (`o` key, `ofApp.h`) | `HudManager` — a real, working "compositor-shaped" class today: owns a pool of rotating widgets plus always-on ones, a `themedAt(alpha)` theme-fade helper, `onVideoFileChanged` event hook |
| `temporal-fields` | Yes — `TFHudLayer.h/.cpp` (2nd-richest consumer), `TFMoireUnderlay.h` | Yes (`o` key, `ofApp.h`) | `TFHudLayer` — the "richest per-sketch HUD compositor in the repo" per the Observability Profile, corroborated here: 12 widget members including a `scanTheme` variant with scan-lines enabled |

**`HudManager` and `TFHudLayer` are the two most direct existing precedents for a scene-owned HUD compositor** — both are real, working "own a pool of `hud::` widgets, theme them, update/draw them every frame" classes. Per the frozen contract (§4, sibling-system diagram) neither is a candidate for direct promotion to `HudCompositor`: they are scene-coupled (constructed with references to `TriggerBus`, `CrosshairSystem`, `QuadrantManager`, etc., or coupled 1:1 to `TFComposition`'s pattern state) in a way the frozen `SceneFrame`/`HudFrameData` boundary explicitly forbids (`HudCompositor` must not read scene internals). **Use only as precedent** for compositor-shape ideas: pooled/rotating widget assignment, single `themedAt()`-style theme-fade helper, event-hook pattern (`onVideoFileChanged`).

**`hud_validation_harness`** — see §4 and §10; a standalone tool, not scene-local content.

**`hud_elements`** — see §4; not code.

---

## 7. Media Viewport Geometry Comparison

### Direct precedents found

1. **`TFShapeFragmentRenderer`** (`sketches/temporal-fields/src/TFShapeFragmentRenderer.cpp`) — builds an `ofMesh` (triangle fan for pie slices, triangle strip for ring segments) with an **arc-stepping** helper (`arcSteps()`: one step per ~6°, clamped 2–64) and manually computed, texture-normalized UVs per vertex. This is the exact "arc-stepping approach" the Contract's §11 revision log names as reusable algorithmic precedent. It does **not** implement rounded corners or a bevel — it implements pie/ring wedges — so it is precedent for *technique* (mesh triangulation + manual UV mapping for a non-rectangular silhouette), not a directly transplantable rounded-rect-with-bevel implementation.
2. **`tfDecomposeFrameToRects`** (`sketches/temporal-fields/src/TFFragmentShape.h`) — decomposes a rectangular annulus into up to 4 axis-aligned rectangle strips rather than using `ofPath` winding/tessellation, explicitly for Pi-friendliness ("cheaper and more Pi-friendly… since both bounds are already axis-aligned"). Directly relevant precedent for the media viewport's 45° bevel: a bevel can similarly be expressed as a small number of extra triangles/vertices cut from a rectangle's corner rather than a generalized path-fill.
3. **`Fragment::drawMaskedFill` / `drawTexturedCircleMesh`** (`shared/src/Fragment.cpp:156-243`) — **the strongest precedent in the repo**. `blueprint_emergence`'s circular fragments used to rely on a fragment-shader `gl_FragCoord`-based circular discard; the code comment (`Fragment.h:113-114`, `BEFragment.cpp:589-590`) documents that this **broke for live video** ("the old mask could discard every pixel when the y-axis/window-space math did not line up, leaving only the outline visible") and was replaced with a **textured triangle-fan disk mesh** with UVs computed via `videoTexture->getCoordFromPoint()`. This is first-party, already-shipped evidence that the shader-discard approach (candidate approach 4) has a real, documented failure mode in this exact codebase for exactly the kind of live-video content `MediaViewportMesh` must clip.
4. No stencil-buffer clipping precedent exists anywhere in the repo (§8/§3 grep, zero hits for `GL_STENCIL`).
5. No alpha-mask-texture-based clipping precedent exists in `shared/src` (no `ofFbo`-based mask compositing found in the searched trees); `contour-portrait`'s own mask machinery (`ContourMaskSource`, not inspected in depth here — out of runtime-HUD scope, it is a subject-silhouette mask for its own effect, not a viewport-shape mask) is the closest tangential precedent but solves a different problem (image-content masking, not chrome/viewport-boundary clipping) and was not designed for reuse.

### Comparison table

| Approach | Visual correctness | Anti-aliasing | UV complexity | GL-state risk | Resize behavior | Screenshot determinism | Pi draw/texture cost | Skin-layer compatibility |
|---|---|---|---|---|---|---|---|---|
| **1. Direct triangulated mesh, shaped boundary + mapped UVs** | High — exact silhouette, no discard artifacts (per Fragment.cpp precedent) | Depends on edge vertex density; arc-stepping (§1 above) gives a tunable smooth/cost tradeoff | Moderate — manual per-vertex UV math, but the repo already has two working examples of exactly this math | Low — no scissor/stencil, only texture bind/mesh draw, matches `SceneRenderGuard`'s planned baseline cleanly | Rebuild mesh on `setBounds()`/canvas resize (matches `TFShapeFragmentRenderer`'s per-draw rebuild pattern, or cache and rebuild only on resize) | High — pure geometry, deterministic given fixed bounds/UVs | Low — single small mesh, one texture bind, no extra render target | High — mesh is HUD-owned per Contract §11, independent of skin texture layers, and the skin's own shell can be composited around it without needing to know the mesh's construction |
| **2. Rectangular texture draw + alpha mask texture** | High if the mask is authored correctly; correctness now depends on a second asset (the mask PNG) matching viewport geometry exactly, a new failure surface | Good if mask is pre-antialiased at authored resolution; degrades on resize unless mask is regenerated or vector-sourced | Low (plain rect UVs) but requires a second draw pass and blend-mode discipline (`GL_SRC_ALPHA`/multiply or an FBO composite) | Medium — needs a blend-mode change and correct draw order (content, then mask-multiply, or FBO-composite) around every viewport draw, a new discipline surface | Awkward — mask texture must be regenerated or stretched on resize; the project's own `tfDecomposeFrameToRects` comment explicitly favors axis-aligned geometry over texture/path tricks for exactly this reason | High if mask asset is fixed | Low geometry cost, but a persistent mask texture in memory — a new texture-memory line item disallowed by "no unnecessary active-skin textures" in this task's own performance-risk framing (§8/§12) | Couples viewport shape to a specific raster asset before skins exist — directly conflicts with "Production skin artwork is deferred until the wireframe is accepted" (frozen constraint), since a mask-texture approach would need *some* mask asset even for the wireframe phase |
| **3. Stencil-based clipping** | High, hardware-correct | Depends on multisampling/stencil precision; typically harder to anti-alias softly than a mesh edge | Low | **High** — this repo has zero existing stencil usage (confirmed by grep) and the frozen GL baseline (Contract §5) explicitly lists "Stencil disabled" as part of the known-good baseline `SceneRenderGuard` restores; introducing stencil use inside `HudCompositor` would be the *only* stencil user in the whole codebase, a new discipline surface with no in-repo precedent to build on or copy from | Fine on resize (stencil buffer sized to window/FBO) | High | Unknown/unmeasured — no Pi stencil-buffer cost data exists anywhere in this repo or its docs | Independent of skin layers, but adds a persistent renderer-state mode nothing else in the runtime currently needs |
| **4. Shader-based signed-distance clipping** | High in theory | Best in theory (smooth SDF falloff) — but **this repo has no SDF-clipping precedent to build from at all** (grep for shader-based clipping precedent returned nothing) | Low (plain rect UVs, math lives in the fragment shader) | Medium-high — requires a dedicated shader bound during viewport draw, and this codebase has one directly-relevant cautionary precedent: the *exact* failure this task warns against already happened once for circular clipping in `BEFragment`/`Fragment.cpp`, which is why the maintainers moved away from shader-based discard to a mesh | Trivial (shader math, not geometry) | High (deterministic shader) | Unknown/unmeasured; likely acceptable on Pi for one small shader pass, but this repo has no shader running exclusively over a UI-chrome-sized region (all existing shaders in `shared/src/video-effects` run over full-frame video content) to extrapolate from | Independent of skin layers |

### Recommendation

**Approach 1 (direct triangulated mesh, shaped boundary + mapped UVs)** for the wireframe phase — not implemented here, per this task's constraints. Rationale: it is the approach the Contract's §11 already commits to by name ("build a new HUD-owned viewport mesh class first… reuse the algorithmic precedent"); it is the only approach with **two independent, already-shipped, working precedents in this exact codebase** (`TFShapeFragmentRenderer`'s wedge/arc-stepping, `Fragment::drawTexturedCircleMesh`'s textured-disk-for-live-video); and it is the only approach with a **documented in-repo failure story for the alternative** (shader-discard clipping of live video, `Fragment.h:113-114`). Approaches 2–4 are not recommended for v1: approach 2 conflicts with the "no skin artwork before wireframe acceptance" constraint by needing a mask asset early; approaches 3 and 4 would each be the first user of a GL technique with zero in-repo precedent, adding discipline risk to a system whose #1 stated goal (per Contract §5) is a clean, defensible GL baseline.

---

## 8. GL-State Risk Inventory

### Code facts (measured/grepped, not inferred)

| State | Where modified | Restored? |
|---|---|---|
| Scissor (`glEnable/glScissor/glDisable(GL_SCISSOR_TEST)`) | `sketches/fragment-trail/src/FTFragment.cpp` (crop technique, documented: "Crop via glScissor + an oversized/offset full-texture draw, NOT…"), `sketches/quadrant-crosshair/src/Quadrant.cpp` | Yes in both — each site pairs its `glEnable` with a `glDisable(GL_SCISSOR_TEST)` later in the same function, confirmed by line inspection |
| Stencil | **None found anywhere in the repo** | N/A |
| Blend mode | `shared/src/hud/**/*.cpp` (all 19 files, every widget's `draw()`), `sketches/contour-portrait/src` (1 file), `sketches/temporal-fields/src` (2 files) | `shared/src/hud/` widgets: yes, every widget observed pairs `ofEnableBlendMode(...)`/`ofEnableAlphaBlending()` with `ofDisableBlendMode()` inside its own `ofPushStyle()`/`ofPopStyle()` bracket — **self-restoring, does not rely on caller discipline** |
| Bound shader | `shared/src/Fragment.cpp` (`fragmentShader.begin()`/`.end()` pair around one `drawSubsection` call); `shared/src/video-effects/effects/*.cpp` (per-effect shaders, out of HUD scope) | Yes in the one HUD-adjacent case inspected (`Fragment.cpp`'s `begin()`/`end()` pair is directly adjacent, no early-return path between them) |
| Bound texture | `Fragment::drawTexturedCircleMesh` (`videoTexture->bind()`/`.draw()`/`.unbind()`), `TFShapeFragmentRenderer::tfDrawPatternFragment` (`tex.bind()`/`.draw()`/`.unbind()`) | Yes, both are tight bind/draw/unbind sequences |
| Viewport / projection / model-view matrices | Not directly modified by any `shared/src/hud/` or `hud_overlay` code (no `ofSetupScreen`/`ofViewport`/matrix-push calls found in those trees) | N/A — no HUD-side risk found; scene-side matrix manipulation (e.g. `ofPushMatrix`/`ofTranslate`/`ofRotateDeg` for pattern layout) is common but scoped and paired within scene draw code, consistent with the Contract's "scenes may use internal FBOs/scissor… must not intentionally leave unsafe state enabled" obligation |
| Framebuffer binding | No `ofFbo` usage anywhere in `shared/src/hud*` (confirmed by grep); scene-side FBO usage (`ErosionFBO`, `TFAmbientTextureLayer`, per-quadrant erosion FBOs, etc.) exists but is out of this probe's HUD-domain scope except as a documented hazard: `Fragment.h:43-48`'s own comment flags that `ofFbo::end()` resets the matrix/viewport to the *window*, not the previously-bound FBO, so **nested FBO use without care silently corrupts the parent FBO's subsequent draws** — a real, already-documented landmine for any future `HudCompositor` code that might naively nest an FBO inside a scene's own FBO-based draw | Documented as a hazard, not solved generically anywhere in the repo |
| Line smoothing / other persistent render state | Not found modified anywhere in `shared/src/hud*` | N/A |

### Which current HUD widgets restore their own state vs. rely on caller discipline

**All 19 files in `shared/src/hud/` restore their own blend-mode/style state** (`ofPushStyle`/`ofPopStyle` bracketing every `draw()`, confirmed by direct inspection of `WindowChrome.cpp`, `DataCardWidget.cpp`, and by the harness's own contamination check described below) — **Code fact**, not an assumption. None of them touch scissor, stencil, shaders, textures, or matrices, so there is nothing else for them to restore. This means the existing widget library is, by construction, a low-risk GL citizen; the real risk surface for the future HUD is **new** code (`MediaViewportMesh`, any FBO-based masking if chosen against this probe's recommendation) that doesn't yet exist.

### Which tests could intentionally contaminate state before HUD rendering

None exist today. `hud_validation_harness`'s blend-mode check (`ofApp.cpp:232-241`, comparing `ofGetStyle().blendingMode` before/after each widget's `draw()`) is a **detection**, not a **contamination-injection** test — it never deliberately leaves scissor/stencil/a bound shader/a non-default blend mode active *before* calling `draw()` to see if the widget or the compositor cleans it up. This is a real, identified gap between what exists and what the future validation studio must do (task requirement: "GL-state contamination tests").

### How a validation scene could verify the approved post-scene GL baseline

Not designed here (explicitly out of this probe's scope — `SceneRenderGuard` belongs to the ExperienceRuntime domain). What the HUD studio needs to **observe and assert against**, based on the Contract §5 baseline and the code facts above: scissor disabled, alpha blending enabled with the runtime's standard mode, no shader bound, no texture manually bound, viewport/projection/model-view restored, `ofPushStyle`/`ofPopStyle` balanced, expected framebuffer bound. The harness's existing before/after blend-mode comparison (`ofApp.cpp:232-241`) is a directly reusable *pattern* (not code, since it is written against a single widget's `draw()`, not a `SceneRenderGuard`-wrapped scene draw) for how such an assertion could be structured in the new studio: snapshot relevant `ofGetStyle()`/GL query state immediately before and after a simulated scene draw, log a named failure on mismatch.

---

## 9. Text and Vocabulary Infrastructure

### Code facts

- **Font loading**: exactly two classes in the entire repository load a real `ofTrueTypeFont`: `shared/src/AnnotationRenderer.h` (`loadCodeFont(path, size)`, one font, one size, `blueprint_emergence`-only in current usage) and `sketches/radar-effects-gallery/src/GalleryCompositor.h` (excluded tool). **The canonical `shared/src/hud/` widget library loads no fonts at all.**
- **Text rendering in the shared widget library**: 100% of text goes through `hud::drawTextFallback(text, x, y, scale)` (`HudUtils.h:41-47`), which is `ofDrawBitmapString` wrapped in `ofPushMatrix/ofTranslate/ofScale/ofPopMatrix`. This is OF's built-in fixed bitmap font — no glyph atlas, no custom font asset, no kerning/wrapping, scaled by naive `ofScale` (which will visibly distort a bitmap font's fixed-pixel glyphs at non-1.0 scale factors — a known cosmetic limitation, not measured here but inferable directly from the technique).
- **Text measurement**: no measurement API exists anywhere in `shared/src/hud/` (no `getStringBoundingBox`-style helper found). `AnnotationRenderer`'s private `findFreeTextSlot()` does its own ad hoc layout-collision avoidance, not exposed as reusable measurement.
- **Truncation / wrapping**: not found anywhere in `shared/src/hud/` or `AnnotationRenderer`.
- **Glyph caching**: N/A — bitmap-string path has no caching to speak of; `AnnotationRenderer`'s one `ofTrueTypeFont` member is OF's own internal glyph atlas (loaded once via `loadCodeFont`), not app-level caching logic.
- **Per-frame string construction**: `hud::drawTextFallback` is called with whatever `std::string` the caller already holds — no evidence of per-frame `std::to_string`/`ofToString` churn *inside* the widget library itself, though callers (e.g. `DataCardWidget::setValueText`) may construct strings upstream; not exhaustively audited across all six scenes' call sites in this probe.
- **Scene-title / media-name formatting**: **no curated display-name mapping exists anywhere in the repo** (repo-wide grep for `displayName`/`vocabulary`/`curatedTitle`/`titleMap` outside the video-effects catalog and a few scene-internal LFO/trigger identifiers returned nothing relevant) — this directly reconfirms the Scene Observability Profile's own Open Question #2 ("no such mapping exists anywhere in the repo today… this is new content-authoring work"). Raw filenames are what scenes expose today where they expose anything at all (`TimeOffsetVideoBuffer::getCurrentMediaFilename()`, `VideoSystem::getCurrentFilename()`).
- **Vocabulary maps / display-name catalogs**: none exist. The one structurally-closest precedent is `VideoEffectDefinition::displayName` (a compiled `std::string` per effect ID in `DefaultVideoEffectCatalog.cpp`) — a flat ID→string mapping with no pack/override layering, owned by the Shared Effects domain, not HUD.

### What can support the proposed lookup order (scene override → vocabulary pack → canonical vocabulary → stable ID fallback)

This exact 4-step order is defined in `HUD-Semantic-Slot-Model-v1.md` §14.4 — **that document is a draft, not yet approved** (see §2 above), so its resolution-order proposal is reported here as **Recommendation content from an unapproved draft**, not as an approved contract. Against that proposed shape:

- **Stable ID fallback (step 4)**: fully supported today — every scene already has *some* stable string identifier (`sceneId`, filenames, mode/pattern enum names) that could serve as the literal last-resort fallback with zero new code.
- **Canonical vocabulary (step 3)**: no infrastructure exists; would need to be built from scratch. The video-effects catalog's `displayName` field is the only existing example of "one canonical string per stable ID," and it is a different domain's data, not directly reusable as HUD vocabulary storage.
- **Vocabulary pack (step 2)** and **scene-specific override (step 1)**: no infrastructure exists at all; `VocabularyPack` (draft §14.3) is a proposed type with no code counterpart anywhere in the repo.

**Recommendation**: given the total absence of existing vocabulary infrastructure, this is a from-scratch build for the HUD domain, not an extraction/adaptation task — unlike `MediaViewportMesh` (§7) or the widget shapes (§5), there is no in-repo precedent to reuse even algorithmically. The video-effects catalog's `displayName` pattern (a flat compiled-`std::string`-per-ID map) is the only concrete style precedent worth copying for the "canonical vocabulary" tier specifically.

---

## 10. Validation Harness Assessment

### What `hud_validation_harness` actually is (code facts)

- **Non-interactive, self-driving**: `ofApp::draw()` advances through a fixed `std::vector<Scenario>` one at a time, calling `ofExit()` when the list is exhausted (`ofApp.cpp:247-267`). No `keyPressed` override exists (confirmed).
- **Scenario shape**: `{name, build (factory lambda), width, height, seed, steps (fixed-dt ticks before capture), theme, motion}` (`ofApp.h:19-28`). `buildScenarios()` (`ofApp.cpp:30-197`) constructs ~60 named scenarios covering 8 of the 18 widgets across 4 bounds sizes, plus per-widget special cases (Gauge styles, Scanner background/timing, Reticle preset/label combinations, FlowField density, DataCard before/after mutation, all 5 `FrameStyle`s at 2 sizes, one "active motion" case per widget family).
- **Fixed-dt stepping**: `kFixedDt = 1/60`, `update()` ticks `stepsRemaining` times before capture (`ofApp.cpp:220-227`) — a real, working precedent for the studio requirement "fixed-`dt` animation."
- **Screenshot capture**: `ofSaveScreen("captures/" + s.name + ".png")` (`ofApp.cpp:243`) — deterministic, one file per named scenario, confirmed by the actual PNGs present under `bin/data/captures/`.
- **GL-state check**: compares `ofGetStyle().blendingMode` immediately before vs. after each widget's `draw()` call and logs an error (not a hard failure/exit) on mismatch (`ofApp.cpp:232-241`) — a real, narrow instance of the "GL-state contamination test" requirement, but detection-only (see §8) and scoped to blend mode alone (no scissor/stencil/shader/texture-binding check).
- **10 of the 18 linked widgets are never exercised**: `PulseEmitterWidget`, `GlitchTearWidget`, `StatusLightWidget`, `LogScrollWidget`, `TickBurstWidget`, `RadarStationWidget`, `BreathingTickClusterWidget`, `TelemetryReadoutWidget`, `HalftonePatchWidget`, `TextCalloutWidget`, `DashedLineWidget`, `WindowChrome` all have compiled `.o` object files under `obj/osx/Release/` (proving they're linked via `PROJECT_EXTERNAL_SOURCE_PATHS`) but appear in **no** `Scenario` in `buildScenarios()` and have **no** corresponding PNG under `bin/data/captures/`. This is a maintenance gap in the existing tool, not a design flaw in its architecture — noted here as a **Code fact**, not editorialized further, since fixing it is not in this probe's scope.
- **No scene concept whatsoever**: no `SceneFrame`, no fake status, no capability discovery, no command dispatch, no video/media concept — it only ever instantiates and draws bare `hud::HudWidget` subclasses.

### Assessment: extend, replace, or retain-as-lower-level-tool?

**Recommendation: retain `hud_validation_harness` as a lower-level visual-regression tool for the existing `shared/src/hud/` widget library, and build the new interactive HUD Validation Studio as a separate application.**

Rationale:
- The harness's core loop (`ofApp` self-drives through a fixed scenario list, exits when done) is structurally incompatible with the target studio's requirements for interactivity (scene-profile switching, widget bounds/anchor inspection, mask/layer inspection, unsupported-command placeholders) — these all require an app that stays running and responds to input, which is the opposite of this harness's design.
- The harness's scenario/theme/widget types (`hud::HudTheme`, `hud::MotionSettings`, `Scenario`) are all scoped to the *existing scene-local widget library*, not to the frozen `SceneFrame`/`HudFrameData` boundary the new studio must fake. There is no natural extension path from "build a bare `hud::HudWidget` and draw it" to "assemble a fake `HudFrameData` and run it through a not-yet-built `HudCompositor`."
- Its two genuinely reusable *ideas* — deterministic fixed-dt-then-`ofSaveScreen()` capture, and before/after GL-state comparison — are cheap to reimplement directly inside the new studio's own architecture (they are small, self-contained functions, not a framework), so there is no real cost to not extending the old harness.
- Retiring the widget-level harness outright would be a regression: it is the only regression coverage the 18-widget library currently has, and that library remains in active production use by 4 of 6 runtime scenes independent of whether/when `HudCompositor` ships. It should keep validating `shared/src/hud/` on its own timeline.

### Reusable code and missing infrastructure per studio requirement

| Requirement | Reusable code found | Missing infrastructure |
|---|---|---|
| Fake `SceneFrame` | None | Full — no `SceneFrame` type exists yet (blocked on Phase 0/4 per Roadmap) |
| Fake `SceneHudStatus` | None | Full — type exists only as a struct definition in the Contract document, not in code |
| Fake `SceneManagerStatus` | None | Full — same as above |
| Fake `RuntimeTelemetry` | None | Full — same as above |
| Six selectable fake scene profiles | `HUD-Semantic-Slot-Model-v1.md` §18's six mappings + Scene Observability Profile Parts 1–5 are strong *content* precedent (see §11 below); no code | Full — no code representation of any scene profile exists |
| Ready/loading/degraded/failed states | `SceneHealth` enum defined in the Contract document only | Full — no code enum exists yet |
| Missing optional fields | N/A (nothing to be missing from yet) | Depends entirely on the semantic snapshot type landing first |
| Fixed-`dt` animation | **Directly reusable pattern**: `hud_validation_harness`'s `kFixedDt`/`stepsRemaining` loop (`ofApp.cpp:220-227`) | None — small enough to copy directly |
| Widget bounds and anchors | `hud::HudWidget::getBounds()`/`setBounds()` (existing base class) is a real, reusable per-widget bounds accessor | A region/anchor *layout* system (where each universal slot lives in the 1280×720 canvas) does not exist yet |
| Viewport mesh inspection | None (no `MediaViewportMesh` exists) | Full |
| Mask/layer inspection | None | Full — no masking system exists yet (see §7) |
| Unsupported-command placeholders | Contract §8 explicitly scopes this as HUD-tooling behavior, not a contract change — no code precedent, but explicitly sanctioned to build | Full |
| Screenshot capture | **Directly reusable pattern**: `ofSaveScreen(...)` + deterministic naming (`ofApp.cpp:243`) | None — small enough to copy directly |
| Deterministic screenshot naming | Same as above — `<Name>_<variant>.png` convention | None |
| Per-widget and total HUD timing | None found anywhere in `shared/src/hud/`, `hud_overlay`, or the harness — no `ofGetElapsedTimeMillis()`-bracketed per-widget timing exists in any inspected file | Full — the Roadmap's Phase 9 "performance instrumentation extraction" milestone is itself unchecked, confirming this is unbuilt project-wide, not just missing from the HUD layer |
| GL-state contamination tests | **Partial pattern reusable**: harness's before/after blend-mode comparison (`ofApp.cpp:232-241`) is a real, working instance of the general technique, but detection-only and blend-mode-only | Deliberate pre-draw contamination injection (leave scissor/stencil/shader/texture bound before invoking a simulated scene draw) does not exist anywhere |
| Wireframe fallback mode | None (no skin system exists to "fall back" from) | Full, but conceptually simple once a wireframe HUD exists at all — it would just be the wireframe compositor's default, undecorated appearance |
| Later real-scene preview without merging into installation runtime | `HudManager`/`TFHudLayer` demonstrate that scene-embedded HUD code *can* run standalone inside a sketch's own `ofApp` (both already do, today, inside their own production sketches) — a soft precedent that a studio-hosted real scene is architecturally plausible | Full — depends on `IEcopunkScene` and `SceneManager` existing first (cross-domain dependency, see §15) |

---

## 11. Six Fake-Profile Evidence Maps

Method: cross-reference `HUD-Semantic-Slot-Model-v1.md` §18 (draft — reported here as **Recommendation**, since the document is unapproved) against the Scene Observability Profile Parts 1–2 (approved-status source document) and this probe's own direct accessor-name checks. Every row states whether the underlying **accessor already exists in code** (Code fact), is **derivable** through a small scene adapter without new state (Recommendation), or is **currently private/missing** (Code fact — confirmed absence), and separately classifies literal/normalized/derived/ambient. Provisional names use the task's own `candidate*` convention; no slot IDs are being frozen here.

### 11.1 Blob (`blob-region-prototype`)

| Candidate | Classification | Exposure status |
|---|---|---|
| `candidateRegionCount` | Literal | **Already exposed** — `BlobTracker::getTrackCount()` (Observability Profile Part 2) |
| `candidateFragmentCount` | Literal | **Already exposed** — `VideoRegionController::getActiveFragmentCount()` |
| `candidateOccupiedArea` | Normalized (region area / frame area) | **Derivable** — needs a small adapter combining existing per-region bounds with known frame size; no direct accessor today |
| `candidateMotionEnergy` | Normalized (aggregate tracked-region velocity or frame-difference energy) | **Currently private or missing** — Observability Profile explicitly flags "exact aggregate motion formula" as a data gap; only "rolling perf averages" exist and those are private, debug-only timers, not a motion signal |
| `candidateTrackingStability` | Derived | **Recommendation only** — no accessor, no clear existing internal value to derive it from without new bookkeeping |
| `candidateMediaTitle` | Literal (once shared video service lands) | **Currently missing** — Observability Profile: "no accessor exists" for current filename in this scene (`VideoSampler` gap) |

### 11.2 Contour (`contour-portrait`)

| Candidate | Classification | Exposure status |
|---|---|---|
| `candidateSourceAvailable` | Literal | **Already exposed** — `ContourSource::isAvailable()` |
| `candidateInputMode` | Literal (Image/Video/Camera enum) | **Already exposed** via `parameters()` `ofParameter` group — but **note**: camera mode must never surface in the runtime HUD/fake profiles per the frozen "no live camera input" constraint (DEC-002); a fake profile for this scene must hardcode/exclude the camera branch |
| `candidateMaskEnabled` | Literal | **Already exposed** via `parameters()` group |
| `candidateVertexCount` | Literal (debug-classified in Observability Profile) | **Already exposed** — `ContourDisplacementEffect::getVertexCount()`, but classified Debug value, not recommended for a visitor-facing fake profile |
| `candidateActivePreset` | Literal | **Currently missing** — Observability Profile: "not tracked as state anywhere" |
| `candidateDisplacementEnergy` | Normalized/Derived | **Recommendation only** — no accessor; would require sampling the 76-parameter tuning surface's live displacement values, none of which are currently exposed as a single aggregate |
| `candidateBreakupProgress` | Normalized (0..1, when the breakup sweep is bounded) | **Recommendation only** — no accessor found |

### 11.3 Temporal (`temporal-fields`)

| Candidate | Classification | Exposure status |
|---|---|---|
| `candidateActivePattern` (+ name) | Literal | **Already exposed** — `TFComposition::getActivePatternType()` + `tfPatternTypeName()` |
| `candidateCompositionPhase` | Literal | **Already exposed** — `TFComposition::getPhase()` |
| `candidatePhaseElapsed` | Literal (seconds) | **Already exposed** — `TFComposition::getPhaseElapsed()` |
| `candidateCycleSeed` | Literal | **Already exposed** — `TFComposition::getCycleSeed()`, but Debug-classified, not visitor-facing |
| `candidateMediaFilename` | Literal | **Already exposed** — `TimeOffsetVideoBuffer::getCurrentMediaFilename()` |
| `candidatePlayheadCount` / offsets / history fill | Literal | **Already exposed** as raw getters on `TimeOffsetVideoBuffer`, but Debug-classified per Observability Profile ("not, by themselves, visitor-legible") |
| `candidateTemporalDepth` | Derived (a semantic "how far back in time" summary) | **Currently missing** — Semantic Slot Model itself lists "temporal-depth summary formula" as an unresolved data gap; would need new derivation code over the existing raw playhead-offset getters |
| `candidateFieldActivity` | Normalized | **Recommendation only** — Semantic Slot Model flags "cross-pattern normalized activity definition" as unresolved; 9 independently-implemented patterns have no shared activity metric today |
| `candidateBackgroundEffectName` | Literal | **Already exposed** — `TFBackgroundLayer` getters (per Observability Profile) |

### 11.4 Fragment (`fragment-trail`)

| Candidate | Classification | Exposure status |
|---|---|---|
| `candidateActiveFragmentCount` | Literal | **Already exposed** — `FTFragmentPool::getFragmentCount()` |
| `candidateContentMode` | Literal | **Already exposed** — `FTModeController::getCurrentMode()` |
| `candidateCrosshairPresetIndex` | Literal | **Already exposed** — `CrosshairSystem::getCurrentPreset()`, **but only as an index** |
| `candidateCrosshairPresetName` | Literal (DRIFT/SCAN/HUNT/NERVOUS/ORBIT) | **Currently missing** — Observability Profile explicitly notes "no name accessor exists today, only an index"; this is a one-line adapter gap (map index→name), not a new-data problem |
| `candidateCrosshairEnergy` | Derived (from raw speed/state) | **Derivable** — `CrosshairSystem::getState()` exists but returns raw numbers classified Internal-implementation-detail; a small adapter could normalize it into an "energy" signal |
| `candidateTrailPersistence` / `candidateSpawnCadence` | Literal (tunable parameters, not runtime state accessors) | **Currently private or missing as live state** — these exist as `ofParameter`s in `FTParameterPanel` (tunable, developer-facing), not as read accessors on running scene state; exposing the *current effective value* to a fake/real profile is a small adapter, not new computation |

### 11.5 Quadrant (`quadrant-crosshair`)

| Candidate | Classification | Exposure status |
|---|---|---|
| `candidateCurrentFilename` | Literal | **Already exposed** — `VideoSystem::getCurrentFilename()` |
| `candidatePerQuadrantPhase` (×4) | Literal (already near-visitor-shaped strings: "ONLINE"/"QUIET PHASE"/"STANDBY") | **Already exposed** — `QuadrantManager::getTelemetry(quadId).phaseString` — Observability Profile calls this "genuinely closest thing to a pre-built visitor-facing string in this codebase" |
| `candidatePerQuadrantShaderName` (×4) | Literal | **Already exposed** — same `getTelemetry()` call |
| `candidatePerQuadrantDwellProgress` (×4) | Normalized | **Already exposed** — same `getTelemetry()` call |
| `candidateExpansionState` / progress | Literal/Normalized | **Already exposed** — `ExpansionDirector::getState()`/`isActive()`, **but** re-entrancy safety is an unresolved code-level concern per both this probe's source docs — flagged, not silently treated as safe to drive from real (non-fake) triggers |
| `candidateCrosshairPresetName` | Literal | Same gap as Fragment (§11.4) — index-only accessor, no name accessor |
| `candidateMotionOutputMode` | Literal | **Already exposed** — `MotionExtraction::getOutputMode()`, but Debug-classified (raw/reference modes are explicitly diagnostic) |

### 11.6 Blueprint (`blueprint_emergence`)

| Candidate | Classification | Exposure status |
|---|---|---|
| `candidateCyclePhase` | Literal (BLANK/PLACEMENT/DENSITY/DISSOLVE/RESET_HOLD) | **Already exposed** — `BEComposition::getState()` |
| `candidateCycleMode` | Literal (GHOST_LAYERS/PERPETUAL) | **Already exposed** — `BEComposition::getCurrentMode()` |
| `candidateFragmentCount` / zone A/B counts | Literal | **Already exposed** — `BEComposition::getState()` |
| `candidateCycleSeed` | Literal | **Already exposed** — `CompositionBase::getCycleSeed()`, Debug-classified |
| `candidateMediaFilename` | Literal | **Currently missing** — Observability Profile: "no accessor exists" (`VideoSampler` gap, same as Blob) |
| `candidateZoneBalance` | Normalized (zone occupancy / target occupancy) | **Derivable** — from existing zone A/B counts already exposed via `getState()`, no new internal computation needed, just a ratio in an adapter |
| `candidatePhaseProgress` | Normalized | **Recommendation only** — Semantic Slot Model flags "exact phase-progress calculation" as an unresolved data gap |
| `candidatePacingProfile` | Literal (`BE_SETTINGS_PRESET`: default vs. slow-cinematic) | **Currently missing as a live toggle** — Observability Profile Part 4: "selectable only by recompiling," flagged as "the single highest-value candidate in this sketch for a live-toggleable curator control" |

---

## 12. Performance Risks

### Measured facts already available in code or reports

- **Zero FBO passes, zero shader passes, zero texture uploads anywhere in `shared/src/hud/` or `shared/src/hud_overlay/`** (confirmed by grep across both trees) — the entire existing widget library is vector-primitive-only by design, per its own README's explicit Pi-friendliness rationale.
- **No Pi build target or Pi hardware measurement exists anywhere in this repo for the HUD layer** — the widget library's own README states this outright ("Pi runtime validation… is currently outstanding — no Pi-specific build target exists in this repo yet"), and the Roadmap's Phase 9 (Pi build/deploy/profiling) is entirely unchecked in the milestone list (§8 of the Roadmap).
- **`DataCardWidget` performs a small `std::vector<float>` `erase(begin())`+`push_back()` roughly every 0.2s per active instance** (`DataCardWidget.cpp:14-17`) — a real, measured-in-code per-tick memory move (not a per-frame allocation), worth flagging as a many-instances-on-screen-at-once cost if `DataCardWidget`/its successor is reused per-quadrant (as `HudManager` already does today: 4 simultaneous instances via `quadCards[4]`).
- **`hud_validation_harness` exercises only 8/18 widgets** (§10) — meaning any *existing* Pi-cost blind spot in the other 10 widgets (`PulseEmitterWidget`, `RadarStationWidget`, `HexGridWidget`'s ripple system, etc.) has never even been desktop-regression-tested by the repo's own tooling, let alone Pi-measured.
- **`quadrant-crosshair`'s `HudManager` already runs 9 distinct widget types simultaneously in production today** (pool-rotated + always-on) — this is the closest existing "worst case" data point for how many widgets can coexist, but it has never been Pi-profiled (same gap as above).
- **Font rendering is bitmap-string-only in the shared library (§9)** — cheap per the technique, but any move to real `ofTrueTypeFont` glyph atlases for the new HUD (needed for legible cinematic typography — bitmap strings are a fixed small size, not scalable typography) introduces a genuinely new cost category (font atlas texture memory, glyph-quad batching) that nothing in this repo currently measures.
- **`hud_overlay` runs continuously alongside a production scene when active** (it is a full-screen swap in current sketches, not a layer *underneath* the scene) — so today's architecture has never actually tested "HUD widgets drawing every frame concurrently with full scene rendering," which is precisely what the future `HudCompositor` must do. This is a structural gap, not just an unmeasured number.

### Reasoned risks requiring later measurement (not measured anywhere in this repo)

- **Full-screen FBO passes**: none exist today for HUD purposes; if `MediaViewportMesh` or a future skin layer introduces one, it would be new territory with no in-repo Pi baseline to compare against.
- **Mask textures / high-resolution alpha layers**: not used today (§7); if approach 2 (texture+mask) were chosen against this probe's recommendation, its Pi texture-memory cost is entirely unmeasured.
- **Excessive text layout**: cannot be assessed — no real typography exists yet to measure (§9).
- **Per-frame mesh rebuilds**: `TFShapeFragmentRenderer::buildWedgeMesh` already rebuilds its `ofMesh` every draw call for active wedge fragments (`TFShapeFragmentRenderer.cpp:176`, comment confirms "scratch geometry rebuilt each draw") — a real, already-shipped precedent for the cost profile a similarly-rebuilt `MediaViewportMesh` would have, but temporal-fields' own Pi performance has itself never been measured (same overall gap).
- **Dynamic allocations**: the `DataCardWidget` case above is the only one directly observed; a full per-frame-allocation audit across all 18 widgets was not performed as part of this probe (time-scoped) and should be a specific, later task.
- **Shader switches / blend-state changes**: `shared/src/hud/` widgets each toggle blend mode once per `draw()` call (§8) — with many widgets on screen at once (as `HudManager`/`TFHudLayer` already do), this is N blend-mode toggles per frame; whether that is cheap enough on Pi 3B+ GLES is unmeasured anywhere.
- **Large widget histories**: no history-buffer code exists yet anywhere in the HUD layer (the (draft) Semantic Slot Model's §16 `HudSignalHistoryConfig` is unimplemented) — pure future risk, not yet measurable.
- **Multiple font atlases**: only a risk once real TTF fonts are introduced (§9) — currently zero atlases exist in the HUD layer.
- **Unnecessary active-skin textures**: N/A today — no skin system exists (§4).
- **Scene-local HUD layers accidentally remaining enabled underneath the new HUD**: a genuine, concrete risk given current code — `blueprint_emergence`, `fragment-trail`, `quadrant-crosshair`, and `temporal-fields` **all already own live `hud::` widget instances today** (`HudManager`, `TFHudLayer`, `BEComposition`'s rotating `hudWidget`, `FTBackgroundLayer`'s `HexGridWidget`) that draw as part of each scene's *own* `drawToCurrentTarget()`. When these scenes are migrated to `IEcopunkScene` and rendered underneath the new `HudCompositor`, their existing scene-owned widget instances must be explicitly removed or intentionally suppressed — otherwise the installation will show two independent HUD layers simultaneously (the old scene-embedded one plus the new compositor-owned one). This is a **Recommendation-flagged risk**, not yet a measured cost, but it is grounded in the exact current code shape (§6's table), not speculation.

---

## 13. Current-to-Target Ownership Map

| Concern | Current owner (code fact) | Target owner (frozen constraint / approved decision) | Gap |
|---|---|---|---|
| Scene output rendering | Each scene's own `ofApp`/composition class draws directly to the default framebuffer or its own FBO | `ExperienceRuntime` owns the scene output FBO; scenes implement `IEcopunkScene::drawToCurrentTarget()` | Full — no `IEcopunkScene`, no `ExperienceRuntime` exists |
| HUD chrome/widgets during scene draw | Scene-embedded (`HudManager`, `TFHudLayer`, `BEComposition`'s `hudWidget`, `FTBackgroundLayer`) drawing as part of the scene's own draw call | `HudCompositor`, a sibling system to `SceneManager`, consuming a read-only `SceneFrame` | Full — no `HudCompositor`, no `SceneFrame`; today's HUD widgets are inside scene draw order, the opposite of the target architecture |
| Media viewport shaping (rounded corners + bevel) | Does not exist for the composited-scene-in-a-frame concept; only precedent is per-fragment circular masking (`Fragment::drawMaskedFill`) and wedge shapes (`TFShapeFragmentRenderer`) inside individual scenes, at a totally different scale/purpose | `MediaViewportMesh`, new HUD-owned class (Contract §11) | Full implementation gap; strong algorithmic precedent exists (§7) |
| Widget data source | Direct method calls into scene-owned classes (`HudManager` holds raw pointers to `TriggerBus`, `CrosshairSystem`, etc.) | `HudCompositor` binds widgets to slots sourced from an assembled, immutable `HudFrameData` (draft Semantic Slot Model §10) — no direct scene-object reads | Full — current widgets are tightly coupled to scene internals by design; none are "HUD reads a snapshot" today |
| Vocabulary/display text | Raw filenames, raw enum names, or hand-written literal strings (`DataCardOptions::title = "FUNGAL MESH"` as a compiled default) passed directly to widgets | Centralized vocabulary with scene-override → pack → canonical → stable-ID fallback (draft) | Full — no vocabulary layer exists (§9) |
| Theme/skin | `hud::HudTheme` set once per scene at setup, no runtime skin switching; `hud_overlay`'s `Palette → HudTheme` mapping is the only "swap a small named set of visual variants" precedent | `HudSkinLoader`/`HudSkin` package system (Roadmap Phase 11–12, unstarted) | Full — no skin package format, parser, or loader exists |
| Runtime telemetry (fps, memory, temp) | `ofGetFrameRate()` called ad hoc wherever a scene wants it (Debug-classified per Observability Profile); no dedicated struct | `RuntimeTelemetry`, owned by `RuntimeServices`, separate from scene status | Full — no `RuntimeTelemetry` struct exists in code; `cpuTemperatureC`/`residentMemoryBytes`/`throttled` have **no current in-repo source at all**, confirmed by the Contract document itself (§13: "no current in-repo source") |
| GL-state baseline restoration | Ad hoc, per-call-site discipline (scissor sites restore themselves; widget library self-restores blend mode; no centralized guard) | `SceneRenderGuard` (ExperienceRuntime domain, out of this probe's scope to redesign) | Full — no guard class exists; current discipline is good but manual and unenforced |
| Command dispatch | Direct `keyPressed` handlers per scene, mixing visitor-safe/curator/developer commands with no capability gating (Observability Profile Part 3) | `InputRouter` dispatches typed `SceneCommand`/`RuntimeCommand` via `SceneCapabilities` | Full — no capability-gated dispatch exists anywhere; today's keyboard shortcuts are the only "commands" |

---

## 14. Recommended Implementation Sequence

Numbered for reference only, not a commitment to serial execution — several early items are independent and could run in parallel per the Roadmap's own workstream model. Sequence deliberately front-loads private validation infrastructure and avoids blocking on real scene adapters, per this task's instruction.

1. **Geometry tests for `MediaViewportMesh`'s two precedent techniques** — write small, standalone geometry-correctness tests exercising the arc-stepping (`TFShapeFragmentRenderer`-style) and textured-mesh-clip (`Fragment::drawTexturedCircleMesh`-style) techniques adapted to a rounded-corner + 45°-bevel rectangle, independent of any runtime/scene code. *No cross-domain dependency.*
2. **Build `MediaViewportMesh` itself** (`shared/src/hud-compositor/MediaViewportMesh.*`, per Contract §11's named path) against approach 1 (§7 recommendation), validated by (1)'s tests and a plain `ofTexture` stand-in (no real `SceneFrame` needed yet). *No cross-domain dependency — can start immediately, matches Contract §14 "Stage A… isn't blocked on anything else in this contract."*
3. **Fake data adapters**: hand-written fake `SceneFrame`, `SceneHudStatus`/semantic-snapshot-shaped struct, `SceneManagerStatus`, `RuntimeTelemetry` — built directly from the Contract's typed shapes (already defined as struct literals in the Contract document, just not yet in a header), independent of any real scene. *Depends on: Phase 0's `shared/src/scene/SceneContract.h` existing as a real header (currently only exists as prose in the Contract document) — flag this as the one prerequisite worth landing first, even though it is a near-mechanical transcription, since both HUD and ExperienceRuntime domains are meant to build against the same header (Roadmap Phase 0 exit criteria).*
4. **Wireframe compositor skeleton**: a `HudCompositor`-shaped class that consumes the fake `HudFrameData` from (3) and draws the canonical 1280×720 layout with `MediaViewportMesh` from (2) in the upper-left region, undecorated (no skin). *Depends on (2), (3).*
5. **Universal regions**: implement the fixed slots (scene title, media title, state badge, effect summary, timing, navigation, reseed) as new widgets per §5's gap list (Label, Value, Status Badge, Effect Chips, Progress Bar/Ring reusing `GaugeWidget`'s technique), bound to the fake data from (3). *Depends on (4).*
6. **Flexible regions**: implement the one-or-two scene-specific telemetry slot(s), driven initially entirely by the six fake profiles from §11 (already partially evidenced in code, per that section's exposure-status column) — no real scene adapter required yet. *Depends on (4), (5), §11's evidence maps.*
7. **Vocabulary lookup**: build the from-scratch vocabulary layer (§9) — canonical table + resolution order — sized initially just to the universal slots' labels and the six fake scenes' state/mode names already catalogued in §11. *Depends on (5), (6) existing so there's real slot content to attach vocabulary IDs to; otherwise independent of scene/runtime domains.*
8. **Screenshot baselines**: reuse the `hud_validation_harness` pattern (fixed-dt-then-`ofSaveScreen`, §10) directly inside the new studio, capturing the six fake profiles × ready/loading/degraded/failed states. *Depends on (4)–(7) existing to have something worth screenshotting.*
9. **GL-state tests**: build the deliberate-contamination-then-assert pattern described in §8 (not present in the existing harness, which is detection-only) against the wireframe compositor's draw call. *Depends on (4).*
10. **Performance instrumentation**: per-widget and total-HUD timing (nothing reusable exists per §10's table) — add once (4)–(6) give real draw calls worth timing. *Depends on (4)–(6); reasoned-risk items from §12 should be re-evaluated with real numbers at this point, still desktop-only.*
11. **Real `SceneFrame` connection**: swap the fake `SceneFrame` from (3) for one sourced from an actual `ExperienceRuntime` skeleton. **Cross-domain dependency: ExperienceRuntime/SceneManager domain must land Phase 4 (runtime skeleton) first** — per Contract §14 this is explicitly Stage B, gated separately from the HUD-only Stage A work in items 1–10 above.
12. **Real scene-profile connection**: replace the six *fake* profiles from (6)/§11 with real per-scene semantic-snapshot adapters. **Cross-domain dependency: requires each scene's migration to `IEcopunkScene` (Roadmap Phases 6–8, Scene migration domain) and requires the (currently draft, unapproved) semantic-snapshot contract addition (`semanticSnapshot()` or optional field, HUD-Semantic-Slot-Model-v1 §9) to clear Architecture review first**, since it is an additive change to a frozen shared interface.
13. **Layout freeze**: only after (4)–(10) have validated the wireframe across all six fake profiles and (11)/(12) have proven at least one real scene end-to-end (Contract §14 Stage B scenes: `blob-region-prototype` + `temporal-fields`), lock the canonical geometry per Roadmap Phase 10. *Depends on essentially everything above; explicitly the Roadmap's own sequencing, not a new recommendation.*
14. **Skin-loader preparation**: begin `HudSkin`/`HudSkinLoader`/manifest-parsing design only after (13) — per the frozen constraint "Production skin artwork is deferred until the wireframe is accepted," and per Roadmap Phase 11/12 sequencing (skin spec depends on the frozen Blueprint, which depends on (13)). *Cross-domain dependency: HUD Art Production Pipeline domain owns the skin package spec content; HUD implementation only owns the loader mechanism.*

---

## 15. Cross-Domain Dependencies

| This probe's finding | Depends on domain | What's needed from them |
|---|---|---|
| Fake `SceneFrame`/`HudFrameData` types (§14 item 3) | ExperienceRuntime and SceneManager | `shared/src/scene/SceneContract.h` landed as a real header, not just Contract-document prose (Roadmap Phase 0) |
| Real `SceneFrame` connection (§14 item 11) | ExperienceRuntime and SceneManager | `ExperienceRuntime` skeleton (Roadmap Phase 4), including `SceneRenderGuard`'s actual GL-baseline restoration implementation, which this probe explicitly did not redesign (§8) |
| Real scene-profile connection (§14 item 12) | SceneManager + per-scene migration agents; Architecture | Each scene's `IEcopunkScene` adapter (Roadmap Phases 6–8); Architecture approval of the semantic-snapshot additive contract change (draft §22, still Unresolved per §23 of that document) |
| Media/effect status slots (`media.*`, `effects.*` in the draft Semantic Slot Model §12.5/12.6) | Shared Video Playback, Shared Effects | `VideoPlaybackStatus` and `EffectActivityStatus` type definitions — both explicitly deferred/owned by those domains per the draft document §8.3/§8.4/§22 |
| `RuntimeTelemetry` real values | Raspberry Pi Runtime and Performance | `cpuTemperatureC`/`residentMemoryBytes`/`throttled` have no current in-repo source at all (Contract §13) — Pi domain must land real instrumentation before the studio can show anything but fake/zero values for these fields |
| Pi-cost validation of everything in §12 | Raspberry Pi Runtime and Performance | A Pi build target does not exist yet anywhere in the repo (§12); every Pi-cost claim in this probe is a Recommendation pending that domain's own measurement work (Roadmap Phase 9) |
| Vocabulary content for scene titles/state names/media titles (§9, §11) | Content authoring (unclear domain ownership — flagged in §17 below) | Curated display names for all six scenes' filenames and state IDs; none exist today per the Observability Profile's own Open Question #2 |
| Skin package spec / art pipeline (§14 item 14) | HUD Art Production Pipeline | Skin Package Spec v1 (Roadmap Phase 11) must exist before the loader mechanism (owned by this domain) can be meaningfully implemented |

---

## 16. Contract Change Requests

Per the task's instruction, each item below states the limitation, proposes the smallest change, lists affected domains, and marks it for Architecture review — none are implemented here.

### CCR-1 — `shared/src/scene/SceneContract.h` does not exist as code yet

- **Current limitation**: the Scene/HUD Contract v1's struct/interface shapes (`IEcopunkScene`, `SceneServices`, `SceneFrame`, `SceneHudStatus`, `SceneCapabilities`, `SceneCommand`/`RuntimeCommand`, `SceneManagerStatus`) exist only as code blocks inside a Markdown document, not as a compilable header both HUD and SceneManager domains can build against. This blocks even HUD-only Stage A work (§14 items 1–3) from having a single source of truth for fake-data shapes.
- **Smallest additive change**: transcribe the Contract document's struct/interface definitions verbatim into `shared/src/scene/SceneContract.h`, with a contract-version constant, exactly as Roadmap Phase 0 already specifies. This is explicitly **not a new contract change** — it is the Roadmap's own already-approved Phase 0 deliverable, simply not yet executed. Flagged here because it is the one concrete blocker standing between "read the docs" and "write real fake-data code" for this domain.
- **Affected domains**: ExperienceRuntime/SceneManager (primary owner per Roadmap §7's ownership table), HUD Runtime and Validation Studio (consumer).
- **Requires Architecture review**: No — already an approved Phase 0 task; flagged as a sequencing dependency, not a new proposal.

### CCR-2 — Semantic snapshot addition to `IEcopunkScene`

- **Current limitation**: the frozen `SceneHudStatus` (Contract §7) has no way to carry the richer semantic data (`SceneActivity`, `SceneSemanticState`, `SceneMetric` list) the (draft, unapproved) Semantic Slot Model needs for real scene-specific widgets and ambient-data binding. Without it, item 12 of §14's sequence (real scene-profile connection) cannot proceed past fake data.
- **Proposed smallest additive change**: add `virtual SceneSemanticSnapshot semanticSnapshot() const = 0;` to `IEcopunkScene`, per the draft document's own §9 "Option recommended for v1 implementation" — additive, does not remove or reinterpret any existing `SceneHudStatus` field.
- **Affected domains**: ExperienceRuntime and SceneManager (owns `IEcopunkScene`), HUD Runtime and Validation Studio (consumer), all six Scene migration domains (implementers).
- **Requires Architecture review**: **Yes** — this is an addition to a frozen shared public type (`IEcopunkScene`), which the Architecture Governance document explicitly lists as requiring review ("adds or changes a shared public type"). Not implemented here; the draft document itself already states this is "an additive shared-contract proposal and requires Architecture approval before code changes" (its own §9).

### CCR-3 — No vocabulary-pack type exists in any shared header

- **Current limitation**: §9 establishes there is zero vocabulary infrastructure anywhere in the repo. The (draft) `VocabularyPack` type (§14.3 of that document) has no code counterpart, and no domain currently owns "where does vocabulary storage live."
- **Proposed smallest additive change**: none proposed here — this is flagged as a genuine open design question, not a small tweak, since it requires deciding compiled-C++-table vs. JSON-at-setup (the draft document leaves this explicitly open at its own §14.2/§23) before any header can be written.
- **Affected domains**: HUD Runtime and Validation Studio (primary), Architecture (format decision), eventual HUD Art Production Pipeline (vocabulary packs likely ship alongside skins per draft §14.5).
- **Requires Architecture review**: **Yes**, once a concrete proposal exists — flagged here as **Unresolved**, not yet at the proposal stage.

---

## 17. Unresolved Questions

1. **Is `HUD-Semantic-Slot-Model-v1.md` actually approved?** The document's own header says "draft… for cross-domain review," and the Roadmap's milestone checklist shows "semantic slot model approved" unchecked, yet the document is physically present in `docs/` (both in the project's `docs/` and in `~/Downloads`) as though ready to reference. This probe treated it as Recommendation-grade throughout, per §2's flagged inconsistency, but the actual approval status should be confirmed by Architecture before any of §14's later sequence items (which lean on its slot IDs/types) proceed.
2. **Who owns curated display-name/vocabulary content authoring?** Both the Scene Observability Profile (Open Question #2) and this probe (§9, §15) independently find zero existing infrastructure or clear domain ownership for "where do curated scene titles and media display names get written down." This is content-authoring work, not a code-reachability gap, and no domain in the Shared Project Context's collaboration model is explicitly assigned it.
3. **Should the "watching N visitors"-style interaction-aware framing ever be used for `blob-region-prototype`'s region count or `contour-portrait`'s camera-mode status, in either the real HUD or its fake profiles?** Carried forward unresolved from the Scene Observability Profile (its own Open Question #1) — directly relevant to §11.1's `candidateRegionCount` and §11.2's `candidateInputMode` fake-profile entries, which this probe deliberately did not resolve.
4. **Does `MediaViewportMesh` ever share code with `TFShapeFragmentRenderer`?** Explicitly deferred, not decided, per Contract §11/§15 item 5 — this probe's §7 recommendation is to reuse the *algorithm*, consistent with that deferral, but the deferral itself remains open.
5. **What is the actual Pi 3B+ cost of running `HudManager`/`TFHudLayer`-scale widget counts (9–12 simultaneous widgets) concurrently with full scene rendering?** No number exists anywhere in this repo (§12) — this is the single largest unmeasured risk blocking any confident "Pi Safe" quality-profile decision for the wireframe HUD.
6. **Where should `SceneRenderGuard`'s actual contamination-test assertions live — HUD Validation Studio, or ExperienceRuntime's own test suite, or both?** §8 defines what the studio should *observe*, per this task's explicit instruction not to redesign `SceneRenderGuard` itself, but the ownership split between "who defines the baseline" (ExperienceRuntime) and "who tests against it visually" (this domain) was not resolved by any source document reviewed.

---

## 18. Recommended Next Coding Task

**Build `MediaViewportMesh` (`shared/src/hud-compositor/MediaViewportMesh.h/.cpp`) using approach 1 from §7 (direct triangulated mesh, shaped boundary + mapped UVs), validated by standalone geometry tests, with no dependency on `SceneFrame`, `IEcopunkScene`, or any other not-yet-built runtime type.**

Why this task, specifically, next:
- It is explicitly named and path-specified by the frozen Contract (§11: `shared/src/hud-compositor/MediaViewportMesh.*`), removing ambiguity about scope or location.
- It has the strongest in-repo precedent of anything inspected in this probe — two independent, already-shipped implementations of closely related techniques (`TFShapeFragmentRenderer`'s arc-stepped wedge mesh, `Fragment::drawTexturedCircleMesh`'s textured-disk-for-live-video) plus a documented, first-party cautionary tale against the main alternative (shader-discard clipping) — meaning a coding agent building it has real code to study, not just prose.
- Per Contract §14, this is explicitly Stage A work: "This work can start immediately; it isn't blocked on anything else in this contract" — it does not wait on `ExperienceRuntime`, `SceneManager`, real scene adapters, or the (currently unapproved) Semantic Slot Model, directly satisfying this task's own instruction to "avoid waiting unnecessarily for real scene adapters."
- It unblocks the largest number of downstream items in §14's sequence (items 4 through 8 all depend on it existing) for the smallest, most self-contained unit of new code.

---

# Coding-Agent Completion Report

## 1. Summary

Performed a read-only discovery probe of the HUD Runtime and Validation Studio domain in `EcopunkVideoCollage`, per the seven required source-of-truth documents plus the closely-linked (draft) `HUD-Semantic-Slot-Model-v1.md`. Produced `docs/probes/hud-runtime-validation-studio-probe.md` covering current HUD ownership, the full 18-widget shared library inventory, scene-local HUD usage across all six in-scope runtime scenes plus the two excluded HUD tools, a `MediaViewportMesh` geometry-approach comparison grounded in two real in-repo precedents, a GL-state risk inventory, text/vocabulary infrastructure findings, a `hud_validation_harness` assessment, six fake-scene-profile evidence maps, performance risks, a current-to-target ownership map, a recommended implementation sequence, cross-domain dependencies, three contract-change requests, six unresolved questions, and one recommended next coding task.

## 2. Files inspected

See §3 of the probe document for the full path list. Highest-signal files: `shared/src/hud/` (all 18 widgets + shared/ base classes), `shared/src/hud_overlay/` (theme, layer, organisms), `shared/src/Fragment.h/.cpp`, `sketches/temporal-fields/src/TFShapeFragmentRenderer.*`/`TFFragmentShape.h`, `sketches/hud_validation_harness/src/ofApp.h/.cpp`, `sketches/hud_elements/README.md`, `sketches/quadrant-crosshair/src/HudManager.h/.cpp`, `sketches/temporal-fields/src/TFHudLayer.h`, `sketches/blueprint_emergence/src/BEComposition.h/.cpp`, `sketches/fragment-trail/src/FTFragment.*`/`FTBackgroundLayer.h`/`FTFragmentPool.h`, `shared/src/AnnotationRenderer.h`, `shared/src/video-effects/core/VideoEffectDefinition.h`, plus repo-wide greps for GL-state, font, mesh-clipping, and vocabulary keywords. All seven required source documents plus `HUD-Semantic-Slot-Model-v1.md` and `CLAUDE.md` were read in full.

## 3. Files changed

Only `docs/probes/hud-runtime-validation-studio-probe.md` was created. No production code was implemented, refactored, renamed, moved, or deleted, per the task's instruction.

## 4. Tests/builds run

None. This was a read-only discovery task; no build or test infrastructure was invoked.

## 5. Results

Probe document written successfully to the required path with the required 18-section structure and reporting-rule tagging (Code fact / Approved project decision / Recommendation / Unresolved) applied throughout.

## 6. Deviations from prompt

- The source-of-truth document list names "Scene/HUD Contract v1" without a repo path; it was found only under `~/Downloads/Scene-HUD-Contract-v1.md`, not inside `docs/`. Read from that location; flagged in §2 of the probe.
- Read one document beyond the required seven — `HUD-Semantic-Slot-Model-v1.md` — because it is directly referenced by the Roadmap as the Phase 1 deliverable and is load-bearing for §5, §9, §11, and §14 of the required output. Its draft/unapproved status is flagged at every point it is cited, per the task's own instruction not to silently resolve document/code inconsistencies.
- Did not perform an exhaustive per-file line-by-line read of every one of the ~200+ files touched by the six production scenes (e.g. full bodies of `HudManager.cpp`, `TFHudLayer.cpp`, `BEComposition.cpp` beyond the `hud::`-usage-relevant sections) — given the scope of an 18-section report across an entire codebase domain, inspection depth was calibrated to header/API-surface + targeted grep confirmation for breadth, with full-body reads reserved for the handful of files central to specific claims (`Fragment.cpp`, `TFShapeFragmentRenderer.cpp`, `hud_validation_harness/ofApp.cpp`). Flagged here rather than silently presented as exhaustive.

## 7. Newly discovered risks

- Four production scenes (`blueprint_emergence`, `fragment-trail`, `quadrant-crosshair`, `temporal-fields`) currently own live `hud::` widget instances drawn as part of their own scene draw call; migrating them to `IEcopunkScene` without explicitly removing/suppressing these risks a double-HUD-layer bug once the real `HudCompositor` exists (§12).
- `hud_validation_harness` links but never exercises 10 of its 18 widgets, meaning those widgets currently have no regression coverage of any kind, desktop or Pi (§10).
- `ofFbo::end()`'s documented (in `Fragment.h`) behavior of unconditionally resetting matrix/viewport to the window rather than a previously-bound parent FBO is a real, already-known landmine that any future `HudCompositor` FBO usage must respect explicitly.

## 8. Contract changes requested

Three, detailed in full in §16 of the probe document: (CCR-1) transcribe the Contract's struct/interface shapes into a real `shared/src/scene/SceneContract.h` header — already an approved Phase 0 task, not a new proposal, flagged as a sequencing blocker; (CCR-2) add `semanticSnapshot()` to `IEcopunkScene` — an additive change to a frozen shared type, requires Architecture review, already proposed (unapproved) in the draft Semantic Slot Model; (CCR-3) define a vocabulary-pack storage type — no proposal made, flagged as a genuinely open design question requiring Architecture input before a concrete change can even be drafted. None implemented.

## 9. Recommended next step

Per §18 of the probe: build `MediaViewportMesh` (`shared/src/hud-compositor/MediaViewportMesh.h/.cpp`) using the direct-triangulated-mesh approach, validated by standalone geometry tests, with no dependency on not-yet-built runtime types — the highest-precedent, least-blocked, most contract-explicit unit of new code available in this domain today.
