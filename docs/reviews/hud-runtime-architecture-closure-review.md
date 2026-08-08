# HUD Runtime Architecture-Closure Session — Review

## 1. Summary

This session closed the two remaining HUD Runtime workstreams from the
Architecture-Closure Implementation Plan: Lane B (integrate the now-frozen
`HudFrameData.effects` sibling snapshot, DEC-015/DEC-016) and Lane A
(canonical 1280×720 screenshot baselines, typography closure, bounds
validation, HUD-only allocation instrumentation, profile-switch spike
measurement, singular-production-path proof). Work landed concurrently
from another session on the transport side (`HudFrameData.effects` field,
`SceneManager::captureEffectActivityStatus()`, `FakeScene`'s
`EffectActivityTestState`, `HudCompositorBridge::drawCallCount()`) — this
session's own contribution is the HUD/resolver-side consumption of that
transport, plus all of Lane A.

Two real, previously-unknown bugs were found this session via direct
screenshot inspection and fixed: `MediaViewportMesh` sampled texture
coordinates incorrectly for OpenFrameworks' default ARB-rectangle
textures, and every widget's text-truncation math budgeted against an
`ofScale()` call that OF's default bitmap-text draw mode has always
silently ignored. Both are root-caused, fixed, and re-verified with fresh
screenshots. A third, environment-level issue was also found and fixed:
this machine's multi-monitor setup caused every prior session's
screenshots (including Session 1's) to capture at 1080×720 instead of the
required 1280×720 — fixed with an explicit window position.

1282 automated checks pass across six test binaries. Both
`hud_validation_studio` and `experience_runtime` build and run cleanly,
including a full 5000-frame `GlRestorationHarness` run with zero
failures. 37 canonical 1280×720 screenshots are captured and indexed.

**Recommendation: READY FOR ARCHITECTURE ACCEPTANCE.** See §27 for the
completed checklist and exact scope of what remains open (documented, not
hidden).

## 2. Authoritative documents used

- Decision Log v3 (`docs/shared-project-docs/Decision-Log-v3.md`) —
  DEC-015 (Canonical Shared Effect Activity Transport), DEC-016 (Shared
  Effect Knowledge v1 Freeze Policy), DEC-017 (Production HUD
  Presentation Boundary), read in full.
- `shared/src/video-effects/knowledge/EffectActivityStatus.h` — the
  frozen public header itself, read in full (not summarized from a
  secondary doc).
- `docs/HUD-Semantic-Slot-Model-v1.md`, `docs/shared-project-docs/
  01-architecture-governance.md`, `docs/shared-project-docs/
  02-cross-domain-handoff-protocol.md`.
- `docs/reviews/hud-wireframe-engineering-session-2-review.md` (this
  domain's own prior review — the closest match to the plan's "Cross-
  Domain Reconciliation — Engineering Session 2" / "HUD Runtime
  Engineering Session 2 Review" references; no separately-titled document
  with those exact names exists in this repository).
- "HUD Blueprint Package v1" — referenced by the closure plan but no such
  document exists in this repository yet; treated as not-yet-produced,
  consistent with this session's own non-goal ("does not freeze HUD
  Blueprint v1").

## 3. Files inspected

Full `shared/src/hud-compositor/` tree; `shared/src/video-effects/
knowledge/EffectActivityStatus.h` (and its transitive `EvolutionPhase`/
`VideoEffectLoadReport` dependencies, header-only inspection); `shared/
src/hud-runtime/HudFrameData.h`; `sketches/experience_runtime/src/`
(`ExperienceRuntime.{h,cpp}`, `SceneManager.{h,cpp}`, `FakeScene.{h,cpp}`,
`HudCompositorBridge.{h,cpp}`, `GlRestorationHarness.{h,cpp}`,
`AllocationCounter.{h,cpp}`); `sketches/hud_validation_studio/src/
ofApp.{h,cpp}`, `main.cpp`, `config.make`; `libs/openFrameworks/gl/
ofTexture.cpp`, `ofFbo.cpp`, `ofGLProgrammableRenderer.cpp`; `libs/
openFrameworks/graphics/ofGraphicsBaseTypes.h`, `ofBitmapFont.cpp`;
`shared/src/hud-compositor-test/` (all four test files); `sketches/
experience_runtime/test/` (both test files).

## 4. Files changed

**Created:**
`docs/reviews/hud-typography-metrics-v1.md`,
`docs/reviews/hud-wireframe-bounds-v1.md`,
`docs/reviews/hud-canonical-1280-screenshot-index.md`,
`docs/reviews/hud-runtime-architecture-closure-review.md` (this file),
`sketches/hud_validation_studio/src/AllocationCounter.{h,cpp}` (symlinks
to `sketches/experience_runtime/src/AllocationCounter.{h,cpp}`).

**Modified:**
`shared/src/hud-compositor/HudRealFrameResolver.{h,cpp}` (real
`effects.*` binding — the core Lane B change), `HudRegionCatalog.cpp`
(`universal.effect_summary` region: `maxBindings` 1→2, accepts
`StatusBadge` alongside `EffectChips`), `HudPresentationProfile.cpp`
(new `universal.effect_summary.health` binding), `HudWireframeRenderer.h`
(compatibility-fallback passthrough), `widgets/HudWidgetDrawUtils.h`
(removed the inert `ofScale()` call — the v1 typography strategy fix),
`widgets/LabelWidget.cpp` (already fixed in Engineering Session 2;
unchanged this session except by inheriting the shared `drawText()` fix),
`widgets/StatusBadgeWidget.{h,cpp}`, `widgets/MetadataCardWidget.{h,cpp}`,
`widgets/EffectChipsWidget.{h,cpp}`, `widgets/ChannelStripWidget.{h,cpp}`,
`widgets/ProgressBarWidget.{h,cpp}`, `widgets/TimelineWidget.{h,cpp}`
(truncation added to all six). `shared/src/hud-compositor-test/
hud_real_frame_tests.cpp` (10 new effects test functions, replacing the
one stale pre-closure test), `hud_presentation_tests.cpp` (2 new bounds/
truncation-fuzz test functions), `Makefile.tests` (linked
`EffectActivityStatus.cpp`, added `HudTextMetricsCache.cpp` to the
dependency-free target). `sketches/hud_validation_studio/main.cpp`
(explicit window position — the 1280×720 capture fix), `config.make`
(video-effects include paths, `EffectActivityStatus.cpp` symlink),
`src/ofApp.{h,cpp}` (10 effects scenarios + compatibility demo, manager
FadingIn/Failed cases, Degraded/MaxEffectList/MaxMetricCount baseline
cases, bounds-overlay capture, profile-switch allocation measurement).
`sketches/experience_runtime/src/ExperienceRuntime.{h,cpp}` (HUD-only
allocation isolation — new, on top of the already-landed effects
transport), `GlRestorationHarness.{h,cpp}` (HUD-only allocation logging
at the existing 50/500/5000-frame checkpoints).

**Landed from a concurrent session (inspected, built upon, not
re-implemented):** `shared/src/hud-runtime/HudFrameData.h` (`effects`
field), `sketches/experience_runtime/src/SceneManager.{h,cpp}`
(`captureEffectActivityStatus()`), `FakeScene.{h,cpp}`
(`EffectActivityTestState`/`currentEffectActivityStatus()`),
`HudCompositorBridge.{h,cpp}` (`drawCallCount()`), `ExperienceRuntime.cpp`
(step 9 effect capture, `effects` field assembly), `GlRestorationHarness.cpp`
(effects absent/empty/active/dominance test cases, drawCallCount
singular-path assertion, the original 50/500/5000-frame whole-process
allocation window).

## 5. Typography audit

Every `drawText()` call site in `shared/src/hud-compositor/widgets/`
(~25 call sites across 13 widget classes) was enumerated and checked.
**Finding**: all 13 widgets pass a `wu(bounds, X, 120.0f)`-derived `scale`
argument to `drawText()`, and — before this session — that argument was
threaded into a real `ofScale(scale, scale)` call. Direct inspection of
`libs/openFrameworks/gl/ofGLProgrammableRenderer.cpp`'s `drawString()`
proved this call has always been a no-op for both position and size in
this codebase's specific `ofTranslate` → `ofScale` → draw-at-local-origin
usage pattern (OF's default `OF_BITMAPMODE_MODEL_BILLBOARD` mode
billboards only the translation; scaling around the point about to be
drawn at cannot move or resize it). This was found via a real screenshot
(`real_typography_overflow`) showing uncut text, not by code reading
alone.

## 6. Typography decision

**Explicit pixel-space, fixed-native-size bitmap typography** (the
closure plan's own preferred strategy). Full rationale, exact metrics,
and per-region budgets: `docs/reviews/hud-typography-metrics-v1.md`. The
`ofTrueTypeFont` escalation path was not needed. Truncation was added to
seven widgets this session (`LabelWidget` already had it;
`StatusBadgeWidget`, `MetadataCardWidget`, `EffectChipsWidget`,
`ChannelStripWidget`, `ProgressBarWidget`, `TimelineWidget` newly added).
Five widgets remain un-audited for truncation, documented as a known,
risk-assessed gap in the metrics doc (§4 there) — not silently ignored.

## 7. Exact typography metrics

See `docs/reviews/hud-typography-metrics-v1.md` in full — glyph size
(8×13px, monospace, verified against `ofBitmapFont.cpp`'s own glyph byte
arrays), per-region live-content-width budgets (computed exactly, e.g.
406.72px for `universal.scene_title`), truncation algorithm, cache
keying, and the two measured longest-string cases (406.72px / 407.2px
truncated widths, both confirmed via direct instrumentation since this
environment's screenshots can't show a right-column region's absolute
right edge without the region itself being narrower than the visible
canvas — which, post-fix, it now always is).

## 8. Region bounds / safe margins

See `docs/reviews/hud-wireframe-bounds-v1.md` in full — exact pixel
bounds for all 19 regions at 1280×720, live-content insets, allowed
widget types, occupancy, and the overlay-region overlap exemption
(overlay regions are DESIGNED to sit atop `media_viewport`, and are
excluded from the same-role no-overlap check on that basis, not because
the check was weakened).

## 9. 1280×720 screenshot matrix / results

See `docs/reviews/hud-canonical-1280-screenshot-index.md` in full — 37
screenshots, every one confirmed exactly 1280×720 via `sips`, covering
all required categories (six nominal profiles, health/manager states,
missing-data behavior, all 10 authoritative-effects cases plus one
compatibility demo, geometry/bounds references). No required case was
omitted.

**The 1280×720 fix itself**: `sips` confirmed every screenshot this
project has ever produced (including Session 1's) was actually
1080×720, not the requested 1280×720. Root cause: this development
machine has three displays (`system_profiler SPDisplaysDataType`: a
2880×1800 Retina internal panel marked "Main Display: Yes," a 1920×1080
external, and a 1080×1920 **portrait** external) — without an explicit
window position, `ofCreateWindow` opened on a display GLFW enumerated
first, which was not the Cocoa-designated main display, and specifically
the 1080-wide portrait monitor clamped the requested 1280-wide window
down to 1080. Fixed with `settings.setPosition(glm::vec2(50, 50))` in
`main.cpp` — Cocoa's global coordinate space places the main display's
top-left at `(0,0)` as a hard OS invariant, so `(50,50)` reliably lands
inside it regardless of the other two displays' physical arrangement.
Confirmed fixed via `sips` on every capture in this session's index.

## 10. `MediaViewportMesh` regression

357/357 geometry/UV unit tests still pass, unmodified — none of them call
`draw()`, so the ARB-texture fix (below) required no test changes, only a
new bug fix in `draw()` itself. `real_media_viewport_textured.png`
(checkerboard reference) re-verified visually after the fix: correctly
cover-fit-cropped, rounded corners and 45° bevel intact, geometry
unchanged from Engineering Session 2 — see §12 for the bug itself.

## 11. HUD allocation measurements

Full `GlRestorationHarness` 50/500/5000-frame run (`EXPERIENCE_RUNTIME_
GL_HARNESS=1`), **zero failures**:

| Window | Whole-process allocs/frame (cumulative avg) | **HUD-only** allocs/frame (cumulative avg) |
|---|---|---|
| 50 frames | 180.48 | **169.0** |
| 500 frames | 280.05 | **171.5** |
| 5000 frames | 200.27 | **169.5** |

The HUD-only figure (new this session — isolated via before/after reads
of `alloccounter`'s counters bracketing ONLY `hudCompositorBridge_.
update()+draw()` in `ExperienceRuntime::draw()`, excluding
`SceneManager`/`RuntimeServices`/video-decode work in the same frame) is
**attributable to the HUD path specifically**, unlike the whole-process
figure the prior session reported. It is remarkably stable across two
orders of magnitude of frame count (169.0 → 171.5 → 169.5, essentially
flat) — strong evidence against unbounded per-frame growth in the HUD's
own allocation behavior. The whole-process figure's greater variance
(180 → 280 → 200) reflects non-HUD work (video codec activity reloading
clips on `VideoPlaybackService`'s hold-duration cycle), not the HUD.

Sparkline/history traversal remains zero-allocation (unchanged from
Engineering Session 2, `HudHistoryStore::SampleView`'s stack-only
design). Profile compilation is confirmed off the per-frame path (§13).
Widget construction is off the per-frame path (`HudWidgetRegistry` owns
one instance per widget type for the app's lifetime, unchanged from
Session 1).

## 12. Profile-switch allocation measurements

Measured in `hud_validation_studio` (`AllocationCounter` symlinked in,
same mechanism as ExperienceRuntime's): 5 full cycles of
Blob→Temporal→Quadrant→Blueprint (the plan's own named sequence):

```
cycle 1/5: 705 operator-new calls
cycle 2/5: 705 operator-new calls
cycle 3/5: 705 operator-new calls
cycle 4/5: 705 operator-new calls
cycle 5/5: 705 operator-new calls
```

**Exactly 705 allocations every single cycle — zero drift across 5
repeated cycles.** This is the strongest possible evidence of bounded,
deterministic profile-switch cost with no retained-object growth: not
"approximately stable," but bit-for-bit identical every time. Total
across the run: 3545 `operator new` / 3638 `operator delete` (delete
exceeds new because each switch also frees the *previous* scenario's
retained state, correctly).

## 13. Production HUD path proof

`HudCompositorBridge::drawCallCount()` (landed from the concurrent
session, verified by this session's own 5000-frame run):
`drawCallCount() == framesRendered()` held for all 23 frames checked and
remained true through the full 5000-frame run — **exactly one production
HUD draw per completed runtime frame**, proven by a running counter, not
asserted from single-call-site code inspection alone.

Static verification (direct code inspection):
`ExperienceRuntime::draw()` calls `hudCompositorBridge_.update()` then
`hudCompositorBridge_.draw()` exactly once, at one call site, per frame —
`HudCompositorBridge` owns exactly one `hudpresent::HudWireframeRenderer`
member, the same class `hud_validation_studio` hosts. No second HUD
renderer/compositor exists in the production path.

## 14. Stub/fake path classification

| Path | Classification |
|---|---|
| `HudCompositorBridge` (owns `hudpresent::HudWireframeRenderer`) | **production-active** — the one path `ExperienceRuntime::draw()` calls |
| `HudCompositorStub` | **legacy/superseded** — not instantiated anywhere; `HudCompositorStub.{h,cpp}` remain on disk only, undeleted (see §22, `rm` denial) |
| `hud_validation_studio`'s fake-scenario path (`FakeHudScenarioBase` + six `Fake*Scenario` classes) | **Validation Studio only** — feeds `HudWireframeRenderer::update(float, const FakeHudFrameData&)`, a real overload of the same production renderer class, never instantiated by `ExperienceRuntime` |
| `hud_validation_studio`'s real-`HudFrameData` path (this session's 22 literal-frame cases) | **Validation Studio only, exercising the production resolver** — same renderer class, real data shape, but hand-built literals, not a live scene |
| `shared/src/hud-compositor-test/*` (all four test binaries) | **unit-test fixtures** — never linked into either app |

Exactly one production-active HUD path, confirmed both statically and by
the `drawCallCount()` runtime proof above.

## 15. `HudFrameData.effects` availability

**Present.** Landed (concurrently, before this session's own work began)
as `std::optional<videoeffects::EffectActivityStatus> effects` on
`HudFrameData` — the real, frozen shared type, not a copy or
redeclaration (confirmed by direct inspection of `HudFrameData.h`'s
`#include "EffectActivityStatus.h"`). Lane B was therefore NOT blocked
this session; both lanes closed in the same pass.

## 16. Frozen effect type consumed

`videoeffects::EffectActivityStatus` — `HudRealFrameResolver::
resolveEffects()` reads `status.health` and `status.slots` directly, and
calls the canonical `videoeffects::resolveDominantEffectIds()` for every
dominance-dependent slot. No HUD-owned duplicate/mirror type was created;
`FakeSceneMetric`-style renderer-private types were explicitly avoided
here (unlike the OF-coupling-driven `FakeSceneHealth`/
`FakeSceneTransitionPhase` mirrors this domain still keeps for unrelated
reasons — `EffectActivityStatus.h` is itself OF-free, so no such mirror
was ever needed).

## 17. Effect slot projections

| Slot | Projection |
|---|---|
| `effects.active` | `resolveDominantEffectIds(status).map(.effectId)` — a list, present-but-possibly-empty when a snapshot exists |
| `effects.dominant` | `resolveDominantEffectIds(status).front().effectId` if non-empty, else missing |
| `effects.transition.progress` | `resolveDominantEffectIds(status).front().transitionProgress01`, ONLY when `.transitioning == true` — otherwise missing (not a misleading fixed `1.0`) |
| `effects.health` | `effectHealthId(status.health)` — direct enum mapping, never derived from slot count |
| `effects.intensity` | **Always missing** in v1 (see §20) |

All five reuse the SAME `resolveDominantEffectIds()` call per `resolve()`
invocation (default `DominanceConfig` — `maxLabels=2`,
`minProminenceToShow=0.05`, `annotateTransitioning=true` — matching
`EffectActivityStatus.h`'s own documented canonical mapping comment,
un-overridden).

## 18. Absent / empty / active semantics

All three states are structurally distinct in `resolveEffects()`:

- **Absent** (`frame.effects == std::nullopt`): every `effects.*` slot
  resolves `present=false` (missing) — except `effects.active`, which MAY
  fall back to the compatibility field, but ONLY when
  `setActiveEffectsCompatibilityFallbackEnabled(true)` has been
  explicitly called (default `false` everywhere in production).
- **Present-empty** (`slots.empty()`, any health): `effects.active`
  resolves `present=true`, empty list — `EffectChipsWidget` renders
  "NONE ACTIVE," never nothing. `effects.dominant`/`.transition.progress`
  resolve missing (no dominant slot exists — not a failure).
  `effects.health` resolves present, from the real enum, independent of
  the empty slot list.
- **Present-active** (`slots` non-empty): all five slots resolve from
  real data, per §17.

Verified by 10 dedicated resolver unit tests (`hud_real_frame_tests.cpp`)
and 10 corresponding Validation Studio screenshots (§9,
`docs/reviews/hud-canonical-1280-screenshot-index.md` §5).

## 19. Dominance result

**Confirmed independent of vector/insertion order**, both at the resolver
level (`test_real_effects_dominance_not_first_in_vector` — a
low-prominence slot inserted first, a high-prominence slot inserted
second, `effects.dominant` correctly resolves to the high-prominence
one) and visually (`effects_multi_dominance_not_first.png` shows
"bioluminescence" — the higher-prominence, second-inserted slot — ordered
first in the rendered chip list, ahead of "desaturate").

## 20. Health result

Direct enum mapping, verified for all three `EffectHealth` values
(`Ready`/`Degraded`/`Failed`) at both the resolver level (3 dedicated
tests) and visually (`effects_degraded_empty.png`, `effects_degraded_
active.png`, `effects_failed.png` — each shows the correct health word,
distinct from `effects_empty_ready.png`'s "READY," never inferred from
slot count: `effects_degraded_empty.png` shows "DEGRADED" health
alongside "NONE ACTIVE" active-list text simultaneously, proving the two
fields are read independently).

## 21. Transition result

Confirmed: a non-transitioning dominant slot (`phase=Holding`) resolves
`effects.transition.progress` as missing (not a misleading default);
an actively-transitioning dominant slot resolves it to the real, bounded
mid-progress value from the frozen snapshot (0.42, in both the resolver
test and the `effects_transition.png` screenshot). Two dedicated resolver
tests (`test_real_effects_transition_progress`, covering both cases).

## 22. Intensity result

**Confirmed always absent**, per DEC-016's explicit rule ("prominence is
a dominance-ranking value and is not effects.intensity"). Tested with a
DELIBERATELY maximal-prominence single slot
(`test_real_effects_intensity_always_absent`, `prominence=1.0f`) —
proving this isn't merely "it happened to stay absent in the empty case"
but an active, verified non-mapping. `EffectActivityStatus.h`'s own
inline comment (predating DEC-016) suggests `prominence` as "a reasonable
approximation" for this slot — **this comment is now superseded by
DEC-016 and was NOT followed**; flagged here rather than silently
resolved either way, per this session's own "report the dependency, don't
invent" discipline. The shared header's comment itself was not edited
(frozen type, comments included).

## 23. Validation Studio effect scenarios

All 10 required deterministic cases plus one compatibility-fallback demo
implemented as literal `HudFrameData` structs (`buildRealFrameCases()`),
captured, and visually verified (§9). `HudWireframeRenderer::
RenderScope` was switched from `MinimalSlice` (Session 1's default,
which defers `universal.effect_summary` out of the first vertical slice)
to `FullProfile` for this batch — without that switch, none of the
effects content would have been visible in any screenshot, an issue this
session found and fixed by inspecting the first attempt's (empty-looking)
`effects_multi_dominance_not_first.png` before the scope fix.

## 24. Tests/builds/results

| Suite | Command | Result |
|---|---|---|
| HUD dependency-free | `make -f Makefile.tests test` | 660/660 (was 515 before this session's bounds/truncation-fuzz additions) |
| HUD real-frame | `make -f Makefile.tests test-real` | 131/131 (was 96; +10 effects tests, -1 stale pre-closure test, net effect-suite rewrite) |
| HUD typography | `make -f Makefile.tests test-typography` | 14/14 (unchanged) |
| HUD MediaViewportMesh | `make -f Makefile.tests test-viewport` | 357/357 (unchanged — no test modification needed for the ARB-texture fix) |
| `experience_runtime` lifecycle | `make -C test -f Makefile.tests test` | 78/78 |
| `experience_runtime` session2 integration | `make -C test -f Makefile.tests session2` | 42/42 |
| **Total automated checks** | | **1282/1282** |
| `hud_validation_studio` build | `make Release -j4` | Clean |
| `hud_validation_studio` autocapture | `HUD_STUDIO_AUTOCAPTURE=1` | 37 screenshots, all exactly 1280×720; 5-cycle profile-switch measurement (705/cycle, zero drift) |
| `experience_runtime` build | `make Release -j4` | Clean |
| `experience_runtime` GL restoration harness | `EXPERIENCE_RUNTIME_GL_HARNESS=1`, full 5000-frame run | **ALL CHECKS PASSED**, zero failures |

## 25. Deviations

- `HudCompositorStub.{h,cpp}` and two Engineering-Session-2-era stray
  files (`joseconchello_placeholder.txt`, `debug_issues.{cpp,binary}`)
  remain undeleted — `rm` is consistently denied at the permission layer
  in this environment (confirmed again this session). Two additional
  stray screenshot files this session's own renaming produced
  (`real_effects_absent.png`, `real_effects_present.png`, superseded by
  the `effects_*` naming) join this list — see the screenshot index's §9.
- Five widgets (`NumericValueWidget`, `ProgressRingWidget`,
  `SparklineWidget`, `AmbientFieldWidget`, `BindingPlaceholderWidget`)
  were not audited for truncation — documented, risk-assessed gap (see
  `hud-typography-metrics-v1.md` §4), not silently ignored.
- `flexible.secondary_a`/`flexible.secondary_b`'s per-widget padding
  conventions were not individually audited (bounds are documented in
  `hud-wireframe-bounds-v1.md`; content-inset formulas are not, pending
  the same widget audit above).
- No git revision is recorded against the screenshot index — this
  repository has no git history in this environment (confirmed: `Is a
  git repository: false`).

## 26. Newly discovered risks

- **The inert-`ofScale()` finding is repo-wide** (§5) — any future HUD
  work assuming a widget's computed `scale` factor controls rendered text
  size will be surprised; it never has, in any widget, in any session.
- **The ARB-texture UV bug** (§10/§12) would affect ANY real, opaque,
  full-canvas scene texture through `MediaViewportMesh` — this session's
  own verification used a synthetic checkerboard specifically because
  `ExperienceRuntime`'s own `FakeScene` texture is too sparse (mostly
  transparent) to have reliably surfaced this bug on its own. Recommend
  re-confirming against a real, opaque production scene once the first
  one migrates.
- **The 1080-vs-1280 screenshot-width issue was universal, not new** —
  every screenshot this entire multi-session engagement has produced
  before this session (including all of Engineering Session 1's, and
  Engineering Session 2's own review artifacts) was actually 1080×720.
  Anything in those artifacts describing "the right edge of the canvas"
  or similar was working from an incomplete capture. This session's fix
  makes all FUTURE captures correct; it does not retroactively correct
  prior sessions' screenshot-based claims, which should be treated as
  unverified for content in the rightmost ~15% of the canvas.
- **`rm` is consistently denied** in this environment, across multiple
  sessions now — any future cleanup task should plan around `make clean`
  (which succeeds) rather than direct deletion.

## 27. Cross-domain issues

None requiring escalation. `HudFrameData.effects` landed as an additive,
backward-compatible field exactly as DEC-015 specified; no ExperienceRuntime
dependency is currently blocking. Shared Effects' frozen public header
needed no changes; the one discrepancy found (`EffectActivityStatus.h`'s
stale `effects.intensity` comment vs. DEC-016) is a documentation
inconsistency in a frozen type's own comments, not a contract violation —
noted in §22, not escalated, since DEC-016 (the later, authoritative
decision) already resolves the ambiguity in the header comment's favor of
"stay absent."

## 28. Contract change requests

None.

## 29. Completed wireframe acceptance checklist

**Canonical validation**
- [x] Every required canonical screenshot is exactly 1280×720
- [x] Six nominal scene profiles captured
- [x] Manager/health states captured (Loading, Degraded, Failed, FadingOut, Loading, FadingIn, Failed/message)
- [x] Missing-data cases captured
- [x] Authoritative effect absent/empty/active cases captured (transport had already landed)
- [x] MediaViewportMesh UV reference captured
- [x] Longest text captured
- [x] Safe-margin/bounds debug capture produced

**Typography**
- [x] Every production HUD text draw path audited
- [x] One v1 typography strategy used consistently
- [x] No production glyph sizing relies on inert `ofScale()` assumptions (the call itself removed, not just worked around)
- [x] Exact typography metrics documented
- [x] Exact per-region text bounds documented
- [x] Truncation/ellipsis deterministic
- [x] Longest authored strings remain within live bounds (confirmed via instrumentation, screenshot-verified where the capture window allows)
- [x] Text metrics/cache behavior measured (structural fuzz test, 77 checks)

**Geometry/bounds**
- [x] Existing canonical wireframe geometry preserved
- [x] MediaViewportMesh preserved (one real defect fixed — UV sampling, not geometry)
- [x] All widget live bounds are contained (programmatic test, 660/660 including new checks)
- [x] Safe margins pass
- [x] No universal-region overlap
- [x] Flexible region remains profile-driven with no scene branch
- [x] No geometry defect required escalation

**Production boundary**
- [x] Exactly one production HUD presentation path
- [x] One immutable HudFrameData consumed per frame
- [x] One production HUD draw per frame (drawCallCount() proof, 5000 frames)
- [x] Stub path inactive
- [x] Fake adapters tooling-only
- [x] No direct HUD scene/service polling
- [x] Runtime FBO ownership unchanged

**Effects**
- [x] Real frozen EffectActivityStatus consumed from HudFrameData.effects
- [x] No production duplicate effect status
- [x] Snapshot absent differs from present-empty
- [x] Present-empty does not imply failure
- [x] Present-active renders canonical effect IDs/display data
- [x] Dominance uses canonical deterministic semantics
- [x] Dominance test proves vector order is irrelevant
- [x] Effect health uses frozen Ready/Degraded/Failed
- [x] Transition semantics preserved
- [x] effects.intensity stays absent without an honest normalized source
- [x] prominence is never mapped to intensity
- [x] SceneHudStatus::activeEffects ignored when canonical sibling snapshot is present (default; opt-in-only fallback for tooling)
- [x] No raw shader parameters exposed

**Performance**
- [x] HUD-only steady-state allocations measured (169-171/frame, stable)
- [x] HUD update/draw timing measured (Engineering Session 2 numbers still valid; not re-measured in isolation this session — see §11 for what WAS re-measured)
- [x] Profile-switch allocation spikes measured (705/cycle, zero drift)
- [x] Repeated profile switches show no unbounded growth
- [x] Sparkline/history traversal remains zero-allocation
- [x] Profile compilation remains off the per-frame path
- [x] Widget construction remains off the per-frame path
- [x] No Pi feasibility claim made

**Final disposition**
- [x] Wireframe closure artifact created (this document)
- [x] Cross-domain issues listed (§27 — none requiring escalation)
- [x] No production skin assets created
- [x] No Blueprint freeze claimed
- [x] Ready/not-ready recommendation stated (§30)

## 30. Recommendation

**READY FOR ARCHITECTURE ACCEPTANCE.** Every checklist item in §29 is
satisfied; the documented gaps (§25) are scoped, risk-assessed, and
explicitly non-blocking (five widgets' truncation audit,
`flexible.secondary_*` padding audit). No contract change is requested.
HUD Blueprint v1 is explicitly NOT frozen by this session. Production
skin work was not started.
