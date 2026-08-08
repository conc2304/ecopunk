# HUD Canonical 1280×720 Screenshot Index

Status: Architecture-Closure Session deliverable. Every canonical closure
screenshot listed below is a real, saved PNG, confirmed **exactly**
1280×720 (`sips -g pixelWidth -g pixelHeight`, not a claim from layout
coordinates) — no 1080×720 capture, no post-resize, no crop/upscale. All
captured from `hud_validation_studio`, `HUD_STUDIO_AUTOCAPTURE=1`, a real
running build (`bin/hud_validation_studio.app`), stored under
`sketches/hud_validation_studio/bin/data/captures/`.

## 1. Capture environment

- Window: `ofGLWindowSettings`, `1280×720`, `OF_WINDOW`, explicit
  `setPosition(50, 50)` (see the closure review's "1280×720 capture fix"
  section for why the position is required on this machine).
- Fixed `dt = 1/60s` for every fake-scenario frame; real-`HudFrameData`
  cases are literal, hand-built structs with no elapsed-time dependency at
  all. No wall-clock timing anywhere in the capture path.
- Profile: `HudWireframeRenderer::RenderScope::MinimalSlice` for the 13
  fake-scenario baselines (Session 1's original "first vertical slice"
  scope, unchanged); `RenderScope::FullProfile` for all 22 real-`HudFrameData`
  cases plus the bounds overlay (switched once, at the start of that
  batch — see the review for why).
- Git revision: not tracked — this repository is not a git repository in
  this environment (`git init` was never run here); re-running
  `HUD_STUDIO_AUTOCAPTURE=1` against the current tree reproduces every
  file below byte-for-byte (full determinism, no revision-stamping
  needed for that guarantee).

## 2. Six nominal scene profiles

| Scenario | Seed/phase | File | Dimensions |
|---|---|---|---|
| Blob — nominal | `FakeCasePhase::Nominal`, t=1.00s | `blob-region-prototype_Nominal_t1.00.png` | 1280×720 |
| Contour — nominal | `FakeCasePhase::Nominal`, t=1.00s | `contour-portrait_Nominal_t1.00.png` | 1280×720 |
| Temporal — nominal | `FakeCasePhase::Nominal`, t=1.00s | `temporal-fields_Nominal_t1.00.png` | 1280×720 |
| Fragment — nominal | `FakeCasePhase::Nominal`, t=1.00s | `fragment-trail_Nominal_t1.00.png` | 1280×720 |
| Quadrant — nominal | `FakeCasePhase::Nominal`, t=1.00s | `quadrant-crosshair_Nominal_t1.00_MultiChannel.png` | 1280×720 |
| Blueprint — nominal | `FakeCasePhase::Nominal`, t=1.00s | `blueprint_emergence_Nominal_t1.00.png` | 1280×720 |

## 3. Health / manager states

| Scenario | Source | File | Dimensions |
|---|---|---|---|
| Scene Loading | `FakeCasePhase::Loading`, blob-region-prototype | `blob-region-prototype_Loading_t1.00.png` | 1280×720 |
| Scene Degraded | `FakeCasePhase::Degraded`, blob-region-prototype | `blob-region-prototype_Degraded_t1.00.png` | 1280×720 |
| Scene Failed | `FakeCasePhase::Failed`, blob-region-prototype | `blob-region-prototype_Failed_t1.00.png` | 1280×720 |
| Manager FadingOut | Real `HudFrameData`, `SceneTransitionPhase::FadingOut`, progress=0.35, message="Fading to next scene" | `real_manager_transition_fading_out.png` | 1280×720 |
| Manager Loading | Real `HudFrameData`, `SceneTransitionPhase::Loading`, progress=0.72, message="Preparing next scene" | `real_manager_transition_loading.png` | 1280×720 |
| Manager FadingIn | Real `HudFrameData`, `SceneTransitionPhase::FadingIn`, progress=0.88, message="Arriving at next scene" | `real_manager_transition_fading_in.png` | 1280×720 |
| Manager Failed/message | Real `HudFrameData`, `SceneTransitionPhase::Failed`, message="Scene transition failed to complete" | `real_manager_transition_failed.png` | 1280×720 |

## 4. Missing-data behavior

| Scenario | Source | File | Dimensions |
|---|---|---|---|
| All optional flexible/semantic values missing | `FakeCasePhase::MissingOptional`, blob-region-prototype | `blob-region-prototype_MissingOptional_t1.00.png` | 1280×720 |
| Video snapshot absent | Real `HudFrameData`, `video = std::nullopt` | `real_media_unavailable.png` | 1280×720 |
| Longest media title | Real `HudFrameData`, 91-char `fallbackDisplayTitle` | `real_typography_overflow.png` | 1280×720 |
| Longest scene/status text | `FakeCasePhase::LongestStrings`, quadrant-crosshair | `quadrant-crosshair_LongestStrings_t1.00.png` | 1280×720 |
| Invalid optional binding (Validation Studio tooling only) | `drawDebugBindingErrorDemo()`, `BindingPlaceholderWidget` | `binding-error-placeholder_demo.png` | 1280×720 |

## 5. Authoritative effects (`HudFrameData.effects`, DEC-015)

All 10 required deterministic cases plus one compatibility-fallback
demonstration — see `HudRealFrameResolver.cpp`'s `resolveEffects()` and
`hud_real_frame_tests.cpp`'s matching resolver-level unit tests
(10 dedicated test functions, all passing) for the logic these
screenshots visually confirm.

| Case | Source | File | Dimensions |
|---|---|---|---|
| `effects.absent` | `frame.effects = std::nullopt` | `effects_absent.png` | 1280×720 |
| `effects.empty.ready` | Present, `slots={}`, `health=Ready` | `effects_empty_ready.png` | 1280×720 |
| `effects.one.ready` | Present, 1 slot (`heatmap_recolor`), `health=Ready` | `effects_one_ready.png` | 1280×720 |
| `effects.multi.ready` | Present, 2 slots, `health=Ready` | `effects_multi_ready.png` | 1280×720 |
| `effects.multi.dominance-not-first` | Present, low-prominence slot inserted FIRST, high-prominence SECOND | `effects_multi_dominance_not_first.png` | 1280×720 |
| `effects.transition` | Present, dominant slot `Transitioning`, progress=0.42 | `effects_transition.png` | 1280×720 |
| `effects.degraded.empty` | Present, `slots={}`, `health=Degraded` | `effects_degraded_empty.png` | 1280×720 |
| `effects.degraded.active` | Present, 1 slot, `health=Degraded` | `effects_degraded_active.png` | 1280×720 |
| `effects.failed` | Present, `slots={}`, `health=Failed` | `effects_failed.png` | 1280×720 |
| `effects.no-intensity` | Present, 1 maximal-prominence slot — proves `effects.intensity` stays absent (DEC-016) | `effects_no_intensity.png` | 1280×720 |
| Compatibility-fallback demo (tooling only) | `frame.effects = nullopt`, `SceneHudStatus::activeEffects` populated, `setActiveEffectsCompatibilityFallbackEnabled(true)` explicitly opted in | `effects_compatibility_fallback_demo.png` | 1280×720 |

## 6. Geometry / bounds

| Scenario | Source | File | Dimensions |
|---|---|---|---|
| `MediaViewportMesh` checkerboard/UV reference | Real `HudFrameData`, procedural 640×360 checkerboard texture (different aspect than the region — exercises cover-fit cropping visibly) | `real_media_viewport_textured.png` | 1280×720 |
| Bounds/safe-margin debug overlay | `drawRegionOutlines()` — every region's exact pixel bounds + ID label, drawn atop the last real-frame case's resolved state | `bounds_safe_margin_overlay.png` | 1280×720 |
| Maximum widget-load profile (max effect list) | `FakeCasePhase::MaxEffectList`, quadrant-crosshair | `quadrant-crosshair_MaxEffectList_t1.00.png` | 1280×720 |
| Maximum widget-load profile (max metric count) | `FakeCasePhase::MaxMetricCount`, quadrant-crosshair | `quadrant-crosshair_MaxMetricCount_t1.00.png` | 1280×720 |

## 7. Media title-fallback tiers (supplementary — Engineering Session 2 coverage, preserved)

| Tier | File | Dimensions |
|---|---|---|
| `titleId` (vocabulary) | `real_media_ready_titleid_tier.png` | 1280×720 |
| `fallbackDisplayTitle` | `real_media_ready_fallback_title_tier.png` | 1280×720 |
| `mediaId` (raw, bottom tier) | `real_media_ready_mediaid_tier.png` | 1280×720 |
| Media Loading | `real_media_loading.png` | 1280×720 |

## 8. Acceptance verification

Every capture above was checked against the closure plan's §6.3 criteria:

- **Exact 1280×720 saved dimensions** — confirmed for all 37 files via
  `sips`, not assumed (see §1).
- **No text outside live bounds** — the two real bugs this session found
  and fixed (ARB-texture UV sampling, inert-`ofScale()` truncation) were
  both found BY inspecting these exact screenshots; `quadrant-crosshair_
  LongestStrings_t1.00.png` and `real_typography_overflow.png` were
  re-inspected after each fix and now show correctly ellipsis-truncated
  text within every region (see `hud-typography-metrics-v1.md`).
- **No widget outside region bounds** — `bounds_safe_margin_overlay.png`
  makes every region's exact extent visible; cross-checked against
  `hud-wireframe-bounds-v1.md`'s table and the dependency-free
  `test_region_bounds_within_canvas_and_no_same_role_overlap()` test.
- **No overlap into declared safe-margin exclusion zones** — see the same
  test (660/660 checks passing, zero overlap failures).
- **No accidental scene/profile geometry changes** — `MediaViewportMesh`'s
  own geometry tests (357/357) and the checkerboard reference screenshot
  are unchanged in shape from Engineering Session 2, only the UV-sampling
  bug was fixed (mesh triangulation itself untouched).
- **Media viewport shape unchanged unless a documented defect was fixed**
  — the ARB-texture UV bug (§12 of the closure review) is exactly that:
  a documented defect, fixed, re-verified via `real_media_viewport_
  textured.png` now showing correctly cover-fit-cropped content instead
  of one flat color.
- **No tooling/debug placeholder in production screenshots** —
  `binding-error-placeholder_demo.png` is explicitly named and
  documented as Validation-Studio-tooling-only (§4); no other capture
  uses `BindingPlaceholderWidget`.
- **Effect absent/empty/active states are visually distinguishable** —
  confirmed directly: `effects_failed.png` shows "FAILED" where
  `effects_empty_ready.png`/others show "READY", both alongside "NONE
  ACTIVE" placeholder text distinct from populated effect chips.

## 9. Known stray files (not cleaned up — see closure review's deviations)

`real_effects_absent.png` and `real_effects_present.png` are leftover
files from an earlier (Engineering Session 2 era) case-naming scheme,
superseded by the 10-case `effects_*` set in §5 above — the capture code
no longer writes these filenames, but the old files remain on disk since
`rm` is consistently denied in this environment. Not part of the
canonical index; flagged for manual cleanup.
