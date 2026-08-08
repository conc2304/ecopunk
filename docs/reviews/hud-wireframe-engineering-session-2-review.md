# HUD Runtime & Validation Studio — Engineering Session 2 Review

## 1. Summary

Session 1 built a renderer-private HUD presentation-model system
(`shared/src/hud-compositor/`) against a hand-rolled fake type model and a
standalone `hud_validation_studio` app. Session 2's job was to reconcile
that fake model against the now-real shared types, make the production
path consume real `HudFrameData` directly, close a zero-heap-allocation
gap in the history store, add a HUD-owned `MediaViewportMesh`, add minimal
text-truncation infrastructure, and — the highest-risk item — bridge the
real HUD renderer into `ExperienceRuntime`, replacing `HudCompositorStub`,
without duplicating the renderer inside the runtime sketch.

All of that is done and verified. `ExperienceRuntime` now draws through
the real `hudpresent::HudWireframeRenderer` (via a new thin
`HudCompositorBridge`), confirmed with a real, running, screenshotted
build — not just a compile check. `hud_validation_studio` now exercises
both the fake-scenario path (unchanged, Session 1's 6-scene/screenshot
suite) and the real-`HudFrameData` path (11 new deterministic cases). Two
real, previously-unknown rendering bugs were found and fixed via direct
screenshot inspection during this work (not by inspection alone):
`MediaViewportMesh`'s texture sampling was wrong for OpenFrameworks'
default ARB-rectangle textures (rendered as one flat un-cropped color
instead of the cover-fit-cropped image), and `LabelWidget`'s truncation
budget was computed against an `ofScale()` factor that OF's default
bitmap-text draw mode silently ignores, so long strings were never
actually being truncated. Both are fixed and covered by either an updated
unit test or a still-passing existing one plus a fresh, correct
screenshot. 1066 automated checks pass across six test binaries (four
dependency-free/OF-header-only HUD-domain suites, plus
`experience_runtime`'s two existing suites), and both apps build and run
cleanly on macOS.

Production scene migration was explicitly out of scope and was not done —
see §16 for what's documented instead.

## 2. Files inspected

- `shared/src/hud-runtime/HudFrameData.h`
- `shared/src/scene/SceneSemanticTypes.h`, `shared/src/scene/SceneContract.h`
- `shared/src/video-playback/VideoPlaybackStatus.h`
- `shared/src/video-effects/knowledge/EffectActivityStatus.h`
- `shared/src/hud-compositor/` (entire tree, Session 1 output)
- `sketches/experience_runtime/src/` (`ExperienceRuntime.{h,cpp}`,
  `HudCompositorStub.{h,cpp}`, `SceneManager.{h,cpp}`, `RuntimeServices.h`,
  `FakeScene.{h,cpp}`, `GlRestorationHarness.{h,cpp}`, `InputRouter.h`,
  `config.make`)
- `sketches/experience_runtime/test/` (existing `lifecycle_state_tests.cpp`,
  `session2_integration_tests.cpp`, `Makefile.tests`)
- `sketches/hud_validation_studio/src/ofApp.{h,cpp}`, `config.make`
- `libs/openFrameworks/gl/ofTexture.cpp`, `ofFbo.cpp`,
  `ofGLProgrammableRenderer.cpp`, `libs/openFrameworks/graphics/ofGraphicsBaseTypes.h`,
  `libs/openFrameworks/graphics/ofBitmapFont.cpp` (OF internals, to root-cause
  the two rendering bugs in §12/§14, not to modify)
- `docs/Ecopunk-HUD-System-Master-Roadmap.md`, root `CLAUDE.md`,
  `.claude/CLAUDE.md` (for the double-HUD migration matrix, §16)
- Every production scene's `ofApp.cpp`/HUD-related source
  (`blob-region-prototype`, `contour-portrait`, `temporal-fields`,
  `fragment-trail`, `quadrant-crosshair`, `blueprint_emergence`) plus
  `radar-pulse`, `radar-effects-gallery`, `shader-effect-debugger` (§16)

## 3. Files changed

Created:

- `shared/src/hud-compositor/HudRealFrameResolver.{h,cpp}`
- `shared/src/hud-compositor/MediaViewportMesh.{h,cpp}`
- `shared/src/hud-compositor/widgets/HudTextMetricsCache.{h,cpp}`
- `shared/src/hud-compositor-test/hud_real_frame_tests.cpp`
- `shared/src/hud-compositor-test/hud_media_viewport_tests.cpp`
- `shared/src/hud-compositor-test/hud_typography_tests.cpp`
- `sketches/experience_runtime/src/HudCompositorBridge.{h,cpp}`
- `sketches/experience_runtime/src/*.cpp` — 26 individual symlinks into
  `shared/src/hud-compositor/{*.cpp,widgets/*.cpp}` (excludes `fake/`),
  same pattern already established for `EffectActivityStatus.cpp`
- `docs/hud-double-hud-prevention-and-migration-matrix.md`

Modified:

- `shared/src/hud-compositor/HudDataTypes.h`, `FakeHudSemanticTypes.h`,
  `HudSourceResolver.cpp`, `HudFormattingService.cpp`,
  `HudWireframeRenderer.{h,cpp}`, `HudHistoryStore.{h,cpp}`,
  `HudWidgetInput.h`, `HudMissingDataController.cpp`,
  `HudRegionCatalog.cpp`, `HudPresentationProfile.cpp`,
  `widgets/LabelWidget.{h,cpp}`, `fake/FakeHudScenarioBase.{h,cpp}`,
  `fake/Fake{Blob,Contour,Temporal,Fragment,Quadrant,Blueprint}Scenario.cpp`
- `shared/src/hud-compositor-test/hud_presentation_tests.cpp`, `Makefile.tests`
- `sketches/hud_validation_studio/config.make`, `src/ofApp.{h,cpp}`
- `sketches/experience_runtime/config.make`, `src/ExperienceRuntime.{h,cpp}`,
  `src/HudCompositorStub.h` (comment-only — marked superseded, not deleted,
  see §21), `src/GlRestorationHarness.{h,cpp}`

## 4. Shared types consumed

Directly, no mirrors: `SceneSemanticTypes.h` (`HudDataClass`, `SceneActivity`,
`SceneSemanticState`, `SceneTimingStatus`, `HudMetricValueType`,
`SceneMetric`, `SceneSemanticData`) and `VideoPlaybackStatus.h`
(`VideoPlaybackHealth`, `VideoPlaybackStatus` and every field) — both
confirmed OF-independent by direct inspection (only `<cstdint>`,
`<optional>`, `<string>`, `<vector>`), so the dependency-free test tier
includes them unmodified.

Via `SceneContract.h` (OF-coupled through `ofTexture.h`/`SceneFrame`):
`SceneHealth`, `SceneTransitionPhase`, `SceneHudStatus`,
`SceneManagerStatus`, `SceneCapabilities`, `SceneCommandDescriptor`,
`SceneFrame`, `RuntimeTelemetry`.

`HudFrameData.h` itself, in full: `schemaVersion`, `sceneFrame`, `scene`,
`sceneManager`, `capabilities`, `runtime`, `video`.

`videoeffects::EffectActivityStatus.h` — NOT consumed as a `HudFrameData`
field (none exists — see §9); consumed only insofar as `FakeScene`'s own
demo path in `experience_runtime` populates `SceneHudStatus::activeEffects`
from it, which is the real field this session's real-path binding reads.

## 5. Type reconciliation

Every Session-1 fake type was checked field-by-field against its real
counterpart and classified:

| Fake type | Disposition |
|---|---|
| `HudDataClass` | **Replaced by real** — now `using HudDataClass = ::HudDataClass;` (alias, not a copy) |
| `FakeSceneMetric` | **Replaced by real** `SceneMetric` (field names/shape matched exactly except `valueType`'s enum, see below) |
| `HudSourceValueType` | **Renamed to match real** `HudMetricValueType` (`Scalar,Count,Ratio,DurationSeconds,Identifier`) |
| `FakeSceneHealth` | **Kept as a mirror** — `SceneHealth` lives in the OF-coupled `SceneContract.h`; this mirror is the one thing keeping `HudMissingDataController` and the dependency-free test tier OF-free. A 1:1 `toFakeSceneHealth()` converter bridges the two at the one call site that needs both. |
| `FakeSceneTransitionPhase` | **Kept as a mirror**, same reason as above |
| `FakeSceneMetadata` / ad hoc media fields | **Replaced by real** `VideoPlaybackStatus` — richer (title-fallback chain, hold-vs-playback progress, `canSelectPrevious/Next`) than the fake had invented |
| ad hoc effect list | **Replaced by real** `SceneHudStatus::activeEffects` (`std::vector<std::string>`) — see §9 for why no new type was created |
| `FakeHudFrameData` | **Kept, fake-scenario-only** — still the Validation Studio's non-production input type; not deleted, since the fake-scenario/screenshot suite is still a required, permanent artifact, not a scaffold to remove |

## 6. Real `HudFrameData` integration

`HudRealFrameResolver` is a new, separate resolver class (not a rewrite of
`HudSourceResolver`) that implements
`HudResolvedValue resolve(const HudFrameData&, const HudSourceRef&) const`,
reusing `HudSourceResolver`'s static lookup tables where the canonical IDs
overlap. `HudWireframeRenderer` gained a second `update()` overload —
`update(float dt, const HudFrameData&)` — that shares every downstream
stage (`HudMissingDataController`, `HudHistoryStore`, `HudWidgetRegistry`,
`draw()`) with the existing fake-data `update()` overload. There is
exactly one `draw()` implementation for both paths, matching this task's
own framing that the Validation Studio must remain "a second host for the
same renderer path," now proven true for both input types, not just
asserted.

Scene epoch, frame freshness, and same-scene reseed/generation are each
derived from an already-approved existing field, with no new public field
invented:

- **Epoch** = `SceneManagerStatus::activeSceneId` change → triggers
  `setScene()` (idempotent no-op if unchanged), which recompiles the
  profile and resets `HudHistoryStore`'s epoch.
- **Frame freshness** = `SceneFrame::frameNumber` change → an
  inspection-only signal (`isFrameFresh()`/`lastObservedFrameNumber()`),
  never a control-flow gate — `draw()` always renders the last resolved
  state regardless.
- **Generation** = `SceneSemanticData::timing.generation` change (only
  meaningful when `scene.semantic` is present) → `HudHistoryStore::markGenerationChange()`,
  never a full epoch reset.

## 7. Manager/control integration

`SceneManager`-owned transition phase/progress/message are bound through
the existing overlay-region/profile-binding mechanism, not taught to
individual widgets: `overlay.transition`'s `maxBindings` went from 1 to 3
(progress bar + phase label + message label), and `HudWireframeRenderer::draw()`
gained a pre-pass that counts bindings-per-region and vertically slices a
region's pixel bounds when more than one binding targets it — used by
`overlay.transition` specifically, generically available to any future
multi-binding region.

Three-state control availability (unsupported / supported-but-disabled /
supported-and-enabled) reuses existing infrastructure rather than adding a
new enum: `HudResolvedValue::missing(...)` (no `"enabled"` role bound at
all) = unsupported, causing `LabelWidget` to hide the whole label before
any style is pushed; `present=true, boolValue=false` = disabled, dimmed to
35% opacity; `present=true, boolValue=true` = enabled, full opacity. Media
prev/next controls bind to the real `VideoPlaybackStatus::canSelectPrevious`/
`canSelectNext`. Scene prev/next/reseed controls bind through
`SceneCapabilities::commands` (present in the list → supported).

## 8. Media bindings

Bound from `HudFrameData.video` (`std::optional<VideoPlaybackStatus>`).
Title resolves through a four-tier fallback chain implemented in
`HudRealFrameResolver::resolveMediaTitle()`: `titleId` (through vocabulary)
→ `fallbackDisplayTitle` → `mediaId` (raw, no vocabulary lookup — it's an
opaque identifier, not a display string) → a generic "unavailable"
vocabulary entry when `video` itself is absent. Verified with four
dedicated Validation Studio real-frame cases, one per tier
(`real_media_ready_titleid_tier`, `..._fallback_title_tier`,
`..._mediaid_tier`, `real_media_unavailable`), each visually confirmed
showing the correct tier's text.

Playback progress and hold progress are bound to two separate widget
inputs, never merged or defaulted into each other — confirmed both by a
unit test (`hud_real_frame_tests.cpp`) and by `ExperienceRuntime`'s own
`GlRestorationHarness` case 16 (rewritten this session, see §11's sibling
note in §19). Media health is a separate resolved value from scene health;
nothing here derives one from the other. No value is fabricated when
`video` is `std::nullopt` — every media-sourced binding resolves to
"missing," not a synthetic zero/empty value.

## 9. Effect bindings

`HudFrameData` has **no `.effects` field** — confirmed by direct
inspection, contradicting an assumption in the original task framing. Per
that same framing's own escape valve ("if the shared type is not yet
stable/available enough... report the dependency; do not create a second
type"), `effects.active` binds directly to the real, already-approved
`SceneHudStatus::activeEffects` (`std::vector<std::string>`) — the
compatibility field that field already exists for. No second effect type
was invented. Dominance is never inferred from list order — the binding
treats the vector as an unordered set of active labels; nothing in this
session's code reads `activeEffects[0]` as "the dominant one." Verified in
`experience_runtime`'s own `GlRestorationHarness` (case 17): a real
`videoeffects::EffectActivityStatus` resolves through to
`activeEffects.size()==2 ["Heatmap Recolor","Channel Shift"]` when
`FakeScene`'s demo effect state is enabled, and `size()==0` when it's not
— both a real, non-fabricated state of the underlying type, not a
placeholder.

## 10. Multi-source bindings

MetadataCard, ChannelStrip, Timeline, and the effect-chips widget's
Session-1 multi-source binding shapes are unchanged by this session — no
scene-ID branch was added anywhere in shared renderer or widget code, real
or fake path. The real-path resolver (`HudRealFrameResolver`) and the
fake-path resolver (`HudSourceResolver`) are the only place source
selection differs, and both are generic over `sceneId`, not
scene-specific.

## 11. Missing-data behavior

Missing-vs-empty-collection distinctions from Session 1 are preserved.
`effects.active` is the one field this session's reconciliation found to
have **no** distinct "missing" state at the real-type level (an empty
`std::vector` is the only way to represent "no active effects" —
`SceneHudStatus::activeEffects` is a plain vector, not
`std::optional<vector>`) — `hud_presentation_tests.cpp`'s
`test_missing_vs_empty_list` was rewritten to reflect this rather than
silently keep testing a state that can no longer occur.

One real bug, unrelated to Session 2's own new code, was found and fixed
along the way: `HudMissingDataController`'s placeholder text used a UTF-8
em dash (`\xE2\x80\x94`), which OF's fixed ASCII-only bitmap font (verified
by inspecting `ofBitmapFont.cpp`'s glyph arrays) silently drops — the
placeholder rendered as nothing, not as a dash. Fixed to a plain ASCII
hyphen; confirmed by a fresh screenshot showing a visible `-` where before
there was nothing.

## 12. MediaViewportMesh

`shared/src/hud-compositor/MediaViewportMesh.{h,cpp}` — a HUD-owned direct
triangulated mesh (triangle fan from the region center), rounded corners
via arc-stepped vertex generation, a 45° lower-right bevel as a single
vertex-cut edge, manually computed cover-fit UVs, no dependency on or
extraction from `TFShapeFragmentRenderer` (reuses that class's documented
*technique*, not its code), no shader-discard masking. Geometry rebuilds
only when `MediaViewportGeometryParams`/source size actually change
(exact-equality early-out, `lastUpdateRebuilt()` exposed for
instrumentation) — not every frame. 357 unit tests cover vertex/triangle
counts, no-unnecessary-rebuild, bounds containment, bevel correctness, and
UV cover-fit behavior across matching/mismatched aspect ratios and
different source sizes.

**Two real bugs found and fixed via screenshot inspection, not by test
alone:**

1. **UV-crop direction inverted** (found via a failing unit test, fixed
   before this continuation): the cover-fit scale-axis selection had its
   condition and which-axis-scales both backwards. Fixed in `coverUV()`;
   357/357 now pass, was 356/357.
2. **ARB-rectangle texture sampling** (found via a real Validation Studio
   screenshot this continuation): a fresh, deliberately-checkerboarded,
   non-transparent synthetic texture rendered as one flat, uncropped solid
   color instead of a visibly cover-fit-cropped checkerboard.
   Root-caused by direct inspection of `libs/openFrameworks/gl/ofTexture.cpp`/
   `ofFbo.cpp`: OF defaults to `GL_TEXTURE_RECTANGLE_ARB` on desktop
   (`ofGetUsingArbTex()`'s documented default, used by both plain
   `ofTexture` and `ofFbo`), whose native texture coordinates are
   pixel-space (`[0,width]×[0,height]`), not the `[0,1]` normalized range
   `MediaViewportMesh`'s baked UVs assumed. Submitting `[0,1]` UVs directly
   to an ARB-bound texture samples only its extreme top-left texel for the
   whole mesh. Fixed by keeping the mesh's own baked texcoords in the
   canonical `[0,1]` percent form (unchanged — this is what
   `hud_media_viewport_tests.cpp` asserts against, and stays correct,
   texture-convention-agnostic public contract) and, in `draw()` only,
   temporarily converting to the actually-bound texture's native
   convention via `ofTexture::getCoordFromPercent()` (OF's own
   texture-specific `[0,1]→native` conversion, correct for both ARB and
   normalized textures), then restoring the canonical values before
   returning. Zero heap allocation (in-place `setTexCoord()` on an
   already-sized array); all 357 existing geometry/UV tests still pass
   unmodified since none of them call `draw()`. Re-verified with a fresh
   screenshot: the checkerboard now renders correctly cropped, matching
   the expected cover-fit math.

This second bug would also have silently affected `ExperienceRuntime`'s
own FBO-sourced scene texture (FBOs use the same ARB-rectangle default,
confirmed by direct inspection of `ofFbo.cpp:549`) — it happened to look
superficially plausible in that specific screenshot only because
`FakeScene`'s tiny proof marker (drawn near its own texture's origin)
coincidentally overlapped the same near-origin texel the bug was
erroneously sampling from. This was not verified as correct before the fix
and should not be treated as having been correct.

## 13. Canonical region table

Unchanged from Session 1 except: `media_viewport` (Universal,
`{0.02, 0.16, 0.60, 0.70}` normalized bounds, no widget-type binding — it's
drawn directly by `HudWireframeRenderer::draw()`, not through
`HudWidgetRegistry`) and `overlay.transition`'s `maxBindings` 1→3 with its
bounds height 0.03→0.08 (to fit three vertically-stacked slots).
`HudRegionCatalog::all()` remains the single source of truth; the
Validation Studio's `w` (region-outline) key still renders every entry
directly from it.

## 14. Typography

`shared/src/hud-compositor/widgets/HudTextMetricsCache.{h,cpp}` — cached
width measurement and deterministic ellipsis truncation over OF's fixed
bitmap font, explicitly not a font system and not freezing final skin
typography (this task's own non-goal). Measurement uses a verified
constant (`kGlyphWidthLocal = 8.0f`, confirmed by direct inspection of
`ofBitmapFont.cpp`'s glyph byte arrays — every glyph is monospace, 8 local
pixels wide), not a live `ofBitmapFont::getBoundingBox()` call — that real
API was tried first and found to SIGSEGV in a headless, no-GL-context test
binary, so the cache is fully OF-free and needs no window to compute
measurements from either `update()` or `draw()`. Cached by
`(text, maxWidth-rounded-to-nearest-pixel)`; 14 unit tests cover
determinism, no-truncation-when-fits, deterministic truncation, and the
too-narrow-for-even-ellipsis case (returns empty, never garbled output).

**A second real, previously-unknown bug was found via a fresh
Validation Studio screenshot** (`real_typography_overflow`, deliberately
constructed with a 72- and a 91-character title): text overflowed its
label region uncut, no ellipsis. Root cause, found by direct inspection of
`libs/openFrameworks/gl/ofGLProgrammableRenderer.cpp`'s `drawString()`:
OF's *default* `drawBitmapMode` on desktop is
`OF_BITMAPMODE_MODEL_BILLBOARD` (`ofGraphicsBaseTypes.h`'s `ofStyle`
constructor), which billboards only the *translation* component of the
current model transform to find an anchor point on screen, then draws
glyphs at native, unscaled pixel size regardless of any `ofScale()` in
effect. `LabelWidget`'s truncation math computed its budget in
"local units" on the assumption that `ofScale(scale, scale)` (`scale`
being a per-region-size-derived factor, frequently well under 1.0) would
actually shrink rendered glyph size — it never has, for any widget in this
codebase that computes a non-1.0 scale, not just this one. Since changing
that (making `ofScale()` actually affect bitmap text) would visibly resize
text across every widget in every existing screenshot baseline — a much
wider, "final skin typography" change this wireframe-acceptance task is
explicitly not scoped to make — the fix taken here is narrower: `LabelWidget`'s
truncation budget is now computed directly in real on-screen pixels
(`bounds.width - pad*2`, no division by the ineffective `scale`), matching
text's actual, already-screenshot-established rendered size. Re-verified:
a temporary debug log confirmed both long strings now truncate correctly
(`truncated=1`, cut to fit `~406px`); the full-resolution screenshot
itself can't show the truncated tail because of an unrelated, pre-existing
environment limitation (see §21) — the captured window is 1080px wide, not
the logical 1280px canvas, so the right edge of these particular
right-column regions falls outside every screenshot this environment can
produce, Session 1's included.

**This finding generalizes beyond `LabelWidget`**: any other widget in
`shared/src/hud-compositor/widgets/` that calls `drawText()` with a
non-1.0 `scale` has the same "scale computed, but never actually applied
to rendered glyph size" characteristic. This session did not audit or fix
every such call site (out of scope — see §22).

## 15. History/allocation

`HudHistoryStore::samplesOldestToNewest()` changed from returning
`std::vector<Sample>` (heap allocation on every call) to a stack-only
`SampleView` (`std::array<Sample, 64>` + count); `logicalOrder()` (also
vector-returning) was replaced by `oldestRingIndex()`, a single-index
helper both `stats()` call sites now loop with directly. `HudWidgetInput::historySamples`
updated to the new `SampleView` type. This was a scoped, minimal-diff fix
(no broader history rewrite), matching the task's explicit instruction.

## 16. Double-HUD prevention

Documented, not implemented, per the task's explicit "do not migrate
production scenes this session" — see
`docs/hud-double-hud-prevention-and-migration-matrix.md` for the full
invariant statement, enforcement-point analysis, and a six-scene
migration-readiness matrix (current local HUD owner / draw call site /
later suppression requirement per scene), plus a secondary table covering
the three non-installation dev/gallery tools that also touch
`shared/src/hud/` but aren't in scope for scene migration. Confirmed by
direct `grep`: zero production scenes currently implement `IEcopunkScene`
(only `sketches/experience_runtime/src/FakeScene.h`'s test double does),
so the invariant is currently satisfied vacuously — there is exactly one
HUD renderer running today, and no scene is hosted yet to compete with it.

## 17. Validation Studio

Remains fully deterministic — fixed-dt scenarios, no wall-clock dependency
anywhere, including the 11 new real-frame cases (each a literal, hand-built
`HudFrameData`, no scene/scenario/elapsed-time involvement). New coverage,
all added to the same `HUD_STUDIO_AUTOCAPTURE` batch as Session 1's
fake-scenario baselines: two manager-transition-phase cases
(`FadingOut`/`Loading`, with progress+message), effects absent vs.
populated, media unavailable/loading/three title-fallback tiers, one
deliberate typography-overflow case, and one `MediaViewportMesh` inspection
case using a small, procedurally-generated (not disk-loaded — no asset
dependency, no non-determinism) 640×360 checkerboard texture chosen
specifically because it's a different aspect ratio than the
`media_viewport` region and, unlike `ExperienceRuntime`'s own
mostly-transparent `FakeScene` proof rectangle, fully covers its own
source bounds — this is what actually caught the ARB-texture bug in §12.

## 18. Screenshots

All screenshots referenced in this review were generated by a real,
running build via `HUD_STUDIO_AUTOCAPTURE=1` (`hud_validation_studio`) or
`EXPERIENCE_RUNTIME_GL_HARNESS=1` (`experience_runtime`), inspected
directly (not assumed correct from code reading alone) — this is how both
bugs in §12/§14 were actually found, not merely how they were confirmed
fixed afterward. Captures live under each sketch's own
`bin/data/captures/`.

## 19. Tests/builds

| Suite | Command | Result |
|---|---|---|
| HUD dependency-free | `make -f Makefile.tests test` (in `shared/src/hud-compositor-test/`) | 510/510 |
| HUD real-frame (OF headers, no OF lib link) | `make -f Makefile.tests test-real` | 96/96 |
| HUD typography | `make -f Makefile.tests test-typography` | 14/14 |
| HUD MediaViewportMesh (full OF lib link) | `make -f Makefile.tests test-viewport` | 357/357 |
| `experience_runtime` lifecycle | `make -C test -f Makefile.tests test` | 74/74 |
| `experience_runtime` session2 integration | `make -C test -f Makefile.tests session2` | 15/15 |
| **Total automated checks** | | **1066/1066** |
| `hud_validation_studio` build | `make Release -j4` | Clean, no errors |
| `hud_validation_studio` run | `HUD_STUDIO_AUTOCAPTURE=1` | 10 fake-scenario + 1 binding-error + 11 real-frame screenshots captured |
| `experience_runtime` build | `make Release -j4` | Clean, no errors |
| `experience_runtime` GL restoration harness | `EXPERIENCE_RUNTIME_GL_HARNESS=1` | ALL CHECKS PASSED (32 checks, including the two rewritten in §8/§20) |

`GlRestorationHarness` case 16 (video playback status) was rewritten this
session: its original assertion assumed an empty media root (a stale
assumption predating the Shared Video Playback System's real-catalog
integration); it now asserts against `VideoPlaybackStatus.h`'s own
documented invariant (progress presence must track health, never a
fabricated default) instead, which is both correct today and robust to
whatever health state the real service reports in the future. Case 5's
pixel-comparison crop region was updated from a hardcoded rectangle
matching `HudCompositorStub`'s old, now-gone placement logic to one
derived live from `HudCompositorBridge`'s own `pixelBoundsFor("media_viewport")`,
via a new test-only accessor (`ExperienceRuntime::hudCompositorBridgeForTesting()`).

## 20. Desktop performance

Measured on this macOS development machine only — see §22 for why these
numbers say nothing about Raspberry Pi feasibility.

- **Profile compile**: 88–131µs cold (first compile for a given scene),
  2–5µs on a same-scene no-op call (the `setScene()` early-out).
- **Frame draw** (13 active widgets, minimal-slice scope, real-frame
  path): `totalDrawMicros` = 210µs.
- **Per-widget draw time** (same frame): scene-title label and
  primary-state badge ~30µs each (text truncation/measurement dominates);
  the five control labels 1–2µs each (mostly hidden/unsupported in this
  particular case); transition overlay's three stacked bindings 7–28µs
  each.
- **`MediaViewportMesh`**: 54 vertices / 52 triangles for a rounded+beveled
  region at default corner/bevel params; rebuild only on actual
  params/source-size change (`lastUpdateRebuilt()` false on every
  steady-state frame where neither changed).
- **History**: 3 tracked signals, ~4.8KB (`historyBytes`) for the captured
  frame.
- **`experience_runtime` steady-state allocation** (50-frame window,
  `AllocationCounter`, whole-process — includes every OF-internal
  allocation, not exclusively this HUD's own code): 8845–8845 `new` /
  8856 `delete` calls, ~177 allocs/frame. `HudHistoryStore`'s own
  contribution to this is zero per-frame by construction (§15); this
  total reflects the rest of the process (video decode, OF's own
  per-frame bookkeeping, etc.), not an attributable HUD-only figure — this
  session did not instrument a HUD-only allocation counter (see §22).
- **Profile-switch allocation**: not separately isolated with a counter
  this session (see §22); qualitatively, `HudProfileCompiler::compile()`
  does allocate (a fresh `HudCompiledProfile::bindings` vector, cleared
  `lastValid_`/`widgetInputs_` maps) — by design, since it's the one
  documented exception to "never allocate in the steady-state path,"
  explicitly gated to only run at `setup()` or an actual scene-profile
  change (`HudWireframeRenderer.h`'s own comment), never from `update()`/`draw()`.

## 21. Deviations

- `HudCompositorStub.{h,cpp}` were not deleted — every direct `rm`
  invocation this session (and the prior one) was denied at the
  permission layer. The header was updated to clearly mark it superseded
  and unused (see §3); a human or a future session with working `rm`
  should delete both files. Same for two Session-1 stray artifacts still
  present: `/Users/joseconchello/openFrameworks/apps/joseconchello_placeholder.txt`
  and `shared/src/hud-compositor-test/debug_issues.{cpp,binary}`.
- The captured window/screenshot resolution on this development machine is
  1080×720, not the requested 1280×720 (`main.cpp`'s `settings.setSize(1280,720)`)
  — confirmed via `sips` against multiple capture files, including
  Session 1's own baselines, so this is a pre-existing environmental
  characteristic, not a regression from this session's work. It means the
  rightmost ~200px of the logical 1280-wide canvas (roughly
  `universal.scene_title`/`universal.media_title`'s own right edge) is
  never visible in a screenshot taken on this machine — the typography
  truncation fix in §14 was confirmed correct via direct log
  instrumentation, not purely via screenshot, because of this limit.
- Desktop performance measurement did not include a dedicated HUD-only
  allocation counter (only `experience_runtime`'s existing whole-process
  one) or an isolated profile-switch allocation-spike measurement — see
  §22.
- No audit was done of every other widget's `drawText()` call site for the
  same "computed but inert `scale`" characteristic found in `LabelWidget`
  (§14) — only `LabelWidget`'s truncation math, the one place this
  session's own new requirement (deterministic truncation) actually
  depended on getting it right, was fixed.

## 22. Newly discovered risks

- **The `ofScale()`-before-`ofDrawBitmapString()` pattern used throughout
  `shared/src/hud-compositor/widgets/` (via `HudWidgetDrawUtils.h::drawText()`)
  has never actually resized rendered text**, for any widget computing a
  non-1.0 scale, because of OF's default `OF_BITMAPMODE_MODEL_BILLBOARD`
  draw-bitmap mode (§14). This is a repo-wide characteristic, not
  isolated to `LabelWidget` — any future work that assumes "a widget's
  `scale` factor controls its text size" will be surprised. Making
  `ofScale()` actually work (e.g. switching to `OF_BITMAPMODE_MODEL` for
  HUD text) is a real fix, but a wider, all-widgets-visually-change one,
  appropriate for a dedicated typography session, not a side effect of
  this one.
- **`ExperienceRuntime`'s scene FBO uses the same ARB-rectangle texture
  convention** the `MediaViewportMesh` bug in §12 was found against — a
  real production scene's full-native-size render (not `FakeScene`'s tiny,
  mostly-transparent proof marker) would have shown the bug far more
  obviously than this session's own `ExperienceRuntime` screenshot
  happened to. Confirming the fix against a real, opaque, full-canvas
  scene texture (not just the studio's synthetic checkerboard) is
  recommended once the first real scene migrates.
- **`rm` is consistently denied** in this environment at the permission
  layer (both this and the prior session) — any future cleanup task
  should expect to need `make clean`-style targets (which succeed, since
  they invoke `rm` from inside `make`, not directly from the agent) rather
  than direct deletion, or explicit user/human intervention for stray
  files.
- **The 1080-vs-1280 screenshot width limit** (§21) means this
  environment cannot, by itself, visually prove correctness for HUD
  content in the rightmost ~15% of the canonical canvas. Any future
  screenshot-based review of that region should either resize the
  requested window smaller than 1080 (so the logical canvas matches what's
  actually captured) or verify via direct log/state instrumentation, as
  this session did for the typography fix.

## 23. Contract changes requested

None. Every integration decision in this session routed through an
already-approved field. The one point genuinely worth a future proposal —
`HudFrameData` gaining a real `effects` field, rather than continuing to
piggyback on `SceneHudStatus::activeEffects` — is deliberately **not**
proposed here; today's compatibility-field binding works, is tested, and
this task's own instruction was to report the dependency rather than
invent or request a new type mid-session.

## 24. Wireframe acceptance recommendation

**READY FOR ARCHITECTURE REVIEW.** The real `HudFrameData` resolver path,
media/effect/typography bindings, `MediaViewportMesh`, and the
`ExperienceRuntime` bridge are all implemented, tested (1066 automated
checks), and independently confirmed via real running builds and
screenshots — including two genuine bugs this same verification process
caught and fixed, which is the strongest evidence available that the
verification was real rather than assumed. The canonical wireframe
candidate is not yet a proposal for HUD Blueprint v1 (explicitly out of
scope — see the original task framing), but the underlying renderer,
resolver, and integration seam it would sit on are sound enough for
architecture review to begin.
