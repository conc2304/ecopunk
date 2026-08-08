# hud-compositor (HUD Presentation Model foundation)

**Status: renderer-private increment, not the frozen `HudCompositor`.** Everything in this
directory belongs to the HUD Runtime and Validation Studio domain and is scoped to the
"HUD Presentation Model and Validation Studio Foundation" coding task. It is not part of
`shared/src/scene/SceneContract.h`, does not depend on `ExperienceRuntime`/`SceneManager`,
and is not a proposal for the real `HudCompositor`'s internals beyond "this is one way to
wire the pieces this task built together." See `HudWireframeRenderer.h`'s header comment.

## Why this exists

`docs/HUD-Semantic-Slot-Model-v1.md` is still a draft under cross-domain review — its
`SceneSemanticSnapshot`/`HudFrameData` types are not yet approved into
`shared/src/scene/SceneContract.h`. This tree implements the presentation-model layer that
sits on top of that (not-yet-approved) semantic model against a **renderer-private mirror**
(`FakeHudSemanticTypes.h`) instead, so the HUD domain can build real, testable
infrastructure without waiting on or pre-empting that approval. See
`FakeHudSemanticTypes.h`'s header comment for the full rationale and what a later
migration to approved shared types would need to touch.

## Layout

```text
shared/src/hud-compositor/
  HudDataTypes.h              — typed resolved-value model (HudResolvedValue, HudDataClass, …)
  FakeHudSemanticTypes.h      — renderer-private mirror of the draft semantic snapshot
  HudSourceResolver.h/.cpp    — stable source ID -> HudResolvedValue, against one FakeHudFrameData
  HudWidgetTypes.h            — HudWidgetType (the approved candidate widget set)
  HudRegionCatalog.h/.cpp     — provisional region layout (placeholder bounds, NOT the frozen Blueprint)
  HudWidgetContract.h/.cpp    — per-widget-type required source roles + accepted value types
  HudPresentationTypes.h      — HudSlotBinding, HudMissingPolicy, HudFormatKind, …
  HudPresentationProfile.h/.cpp — compiled-C++ profiles: universal + six scenes + two overlays
  HudCompiledProfile.h        — HudProfileCompiler's output shape
  HudProfileCompiler.h/.cpp   — validates + compiles profiles into one HudCompiledProfile
  HudFormattingService.h/.cpp — HudResolvedValue + HudFormatKind -> display text
  HudVocabularyResolver.h/.cpp — scene override -> pack -> canonical -> stable-ID fallback
  HudMissingDataController.h/.cpp — centralized missing/loading/failed/ambient-fallback policy
  HudHistoryStore.h/.cpp      — fixed-capacity per-source sample history
  HudWidgetInput.h            — the resolved/formatted/policy-applied data a widget receives
  HudWireframeRenderer.h/.cpp — renderer-private orchestrator tying all of the above together
  fake/
    IFakeHudScenario.h        — the fake-scenario interface this task specifies
    FakeHudScenarioBase.h/.cpp — shared 14-case phase schedule + frame assembly
    FakeBlobScenario.*, FakeContourScenario.*, FakeTemporalScenario.*,
    FakeFragmentScenario.*, FakeQuadrantScenario.*, FakeBlueprintScenario.* — the six scenes
    FakeHudScenarioRegistry.h/.cpp — sceneId -> scenario factory
  widgets/
    HudWidgetBase.h            — IHudWidget (compositor-oriented — NOT shared/src/hud::HudWidget)
    HudWidgetDrawUtils.h       — small OF drawing helpers (precedent-only reuse of shared/src/hud/'s style)
    HudWidgetRegistry.h/.cpp   — one shared, stateless instance per HudWidgetType
    Label/StatusBadge/NumericValue/ProgressBar/ProgressRing/Sparkline/EffectChips/
    MetadataCard/ChannelStrip/AmbientField/Timeline/BindingPlaceholder — the 12 widgets
```

## Testing

`HudDataTypes.h` through `HudHistoryStore.h`/`fake/` are **openFrameworks-independent by
design** — see `HudDataTypes.h`'s header comment. They're covered by
`shared/src/hud-compositor-test/hud_presentation_tests.cpp`, a standalone binary with zero
OF linkage, following the exact convention already established by
`sketches/temporal-fields/test/`:

```bash
cd shared/src/hud-compositor-test
make -f Makefile.tests test
```

`widgets/` and `HudWireframeRenderer` depend on openFrameworks (drawing, `ofGetElapsedTimeMicros`)
and are exercised by `sketches/hud_validation_studio/` instead — see that sketch's own
`config.make` for why `hud-compositor-test/` is a *sibling*, not a subdirectory, of this tree.

## Reuse relationship to `shared/src/hud/`

This tree does not depend on `shared/src/hud/` (the canonical scene-local widget library) —
`widgets/HudWidgetBase.h`'s header comment explains why a new, compositor-oriented widget
base class was built instead of promoting `hud::HudWidget`. `shared/src/hud/` is reused only
as algorithmic/visual **precedent** (the `sx`/`sy`/`su` responsive-scaling pattern, drawing
only immediate-mode primitives, composing rather than inheriting a frame renderer) — see
`widgets/HudWidgetDrawUtils.h`.
