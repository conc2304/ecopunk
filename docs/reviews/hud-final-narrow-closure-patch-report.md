# HUD Runtime — Final Narrow Closure Patch Report

Status: complete. This patch closed the two gaps identified in
`docs/reviews/hud-runtime-architecture-closure-review.md`'s §25
(deviations) plus a defect this patch itself found in that same
session's own `effects.active` implementation.

## 1. `effects.active` implementation — exact before/after

**Before** (Architecture-Closure Session — incorrect):

```cpp
if (sourceId == "effects.active") {
    HudIdentifierList list;
    for (const auto& d : dominant) list.push(d.effectId); // dominant = resolveDominantEffectIds(status)
    return HudResolvedValue::list(list);
}
```

This ran `effects.active` through the SAME `resolveDominantEffectIds()`
call as `effects.dominant` — dominance-ranked, threshold-filtered
(`minProminenceToShow=0.05`), capped at `maxLabels=2`. A below-threshold
active effect would silently vanish from `effects.active`; a third or
later active effect would never appear at all, regardless of prominence.

**After** (this patch):

```cpp
HudIdentifierList allActiveEffectIds(const videoeffects::EffectActivityStatus& status) {
    HudIdentifierList list;
    for (const auto& slot : status.slots) {
        bool alreadyPresent = false;
        for (size_t i = 0; i < list.count; ++i) {
            if (list.items[i] == slot.effectId) { alreadyPresent = true; break; }
        }
        if (!alreadyPresent) list.push(slot.effectId);
    }
    return list;
}
// ...
if (sourceId == "effects.active") {
    return HudResolvedValue::list(allActiveEffectIds(status));
}
```

Complete active set, deduplicated by `effectId` (two slots running the
same effect collapse to one entry — an "effect," not a "slot," concept),
first-seen slot order, no threshold, no cap beyond `HudIdentifierList`'s
own structural `kMaxItems=8` capacity limit (a container limit, not a
dominance cut). `effects.dominant`/`.transition.progress` still call
`resolveDominantEffectIds()` — unchanged, now computed independently
rather than sharing state with `effects.active`.

## 2. Proof that `effects.dominant` remains separate

`shared/src/hud-compositor/HudRealFrameResolver.cpp`'s `resolveEffects()`
now computes `allActiveEffectIds(status)` for `effects.active` and calls
`resolveDominantEffectIds(status)` separately (only when a
dominance-dependent slot — `effects.dominant` or
`effects.transition.progress` — is actually requested). No shared
mutable state; no code path derives one from the other's output.

## 3. >2-effect regression proof

`hud_real_frame_tests.cpp`'s
`test_real_effects_active_full_set_with_below_threshold_and_dominance_not_first()`:

**Input** (4 slots, in this exact vector order):

| Index | slotId | effectId | prominence |
|---|---|---|---|
| 0 | quadrant_0 | heatmap_recolor | 0.3 |
| 1 | quadrant_1 | desaturate | **0.02** (below `minProminenceToShow=0.05`) |
| 2 | quadrant_2 | bioluminescence | 0.5 |
| 3 | quadrant_3 | channel_shift | **0.9** (highest — the dominant slot, deliberately last) |

**Resolved output:**

- `effects.active` = `["heatmap_recolor", "desaturate", "bioluminescence", "channel_shift"]` — **all 4, including the below-threshold `desaturate`**, in slot order.
- `effects.dominant` = `"channel_shift"` — the canonical dominance result **only**, correctly not index 0, correctly excluding the below-threshold slot from consideration (irrelevant here since it's also the lowest-prominence, but the resolver never even considers `effects.active`'s content when computing this).

Also verified end-to-end through the real widget chain: Validation Studio
case `effects_multi_below_threshold_and_dominance_not_first` (screenshot
in `sketches/hud_validation_studio/bin/data/captures/`, indexed in
`hud-canonical-1280-screenshot-index.md` §5) shows all four effect IDs
resolved into the chip list, confirming the semantic value carries
through the real profile/binding/widget pipeline, not just the resolver
in isolation.

## 4. Typography audit table (five named widgets)

| Widget | Classification | Production-visible? | Truncation added | Overflow defect found |
|---|---|---|---|---|
| `NumericValueWidget` | dynamic text (caption + formatted value) | Yes | Yes | No |
| `ProgressRingWidget` | dynamic text (caption + centered value) | Yes | Yes | No |
| `SparklineWidget` | dynamic text (caption only) | Yes | Yes | No |
| `AmbientFieldWidget` | dynamic text (caption only) | Yes | Yes | No |
| `BindingPlaceholderWidget` | static/tooling text | **No — tooling-only** | Not added (out of scope by its own classification — see its header comment: "Never instantiated by production rendering") | N/A |

Full metrics: `docs/reviews/hud-typography-metrics-v1.md` §4.

## 5. `flexible.secondary_a`/`_b` padding findings

Both regions: 192.0×43.2px. Per-widget live-content width: NumericValue
189.12px, ProgressBar 192.0px, ProgressRing 190.56px, StatusBadge
189.12px, Sparkline 192.0px, AmbientField 190.92px. No canonical geometry
was changed — only each widget's truncation-budget implementation.
Confirmed non-overlapping with each other and with `flexible.primary` by
a new dedicated test
(`test_flexible_secondary_regions_exact_bounds_and_no_overlap`, 16
checks, part of the dependency-free suite). Full table:
`docs/reviews/hud-wireframe-bounds-v1.md` §1a.

## 6. Updated 1280×720 validation result

37 screenshots (Architecture-Closure Session) + 1 new regression
screenshot (this patch) = 38 total, ALL confirmed exactly 1280×720 via
`sips`. Full index: `docs/reviews/hud-canonical-1280-screenshot-index.md`.

## 7. HUD-only allocation — old vs. new

| Window | Baseline (Architecture-Closure Session) | This patch |
|---|---|---|
| 50 frames | 169.0 allocs/frame | 170.42 allocs/frame |
| 500 frames | 171.5 allocs/frame | 169.43 allocs/frame |
| 5000 frames | 169.5 allocs/frame | 169.12 allocs/frame |

Consistent with baseline within normal run-to-run variance (< 1%
difference at the 5000-frame window). No regression.

## 8. Profile-switch allocation — old vs. new

| | Baseline | This patch |
|---|---|---|
| Per-cycle allocations | 705 (every cycle, 5 cycles) | **705 (every cycle, 5 cycles)** |
| Drift across cycles | Zero | **Zero** |

Bit-for-bit identical to the baseline, despite the `effects.active`
resolver change and the four widget truncation additions — confirms
those changes introduced no new profile-switch-time allocation cost.

## 9. Production-path verification

`HudCompositorBridge::drawCallCount() == framesRendered()` held for the
full 5000-frame `GlRestorationHarness` run (confirmed at the 23-frame
checkpoint and, via a concurrently-landed "Seam Proof" phase exercising a
real production `TFEffectPicker`, at 5028 cumulative frames). Zero
failures across the entire run. `HudCompositorStub` remains uninstantiated;
no second compositor exists; static inspection of
`ExperienceRuntime::draw()` confirms exactly one
`hudCompositorBridge_.update()`/`.draw()` call site.

## 10. Summary / Files inspected / Files changed

**Files inspected**: `HudRealFrameResolver.{h,cpp}`, `HudSourceResolver.{h,cpp}`,
`HudPresentationProfile.*`, `HudCompiledProfile.*`, `HudProfileCompiler.*`,
`widgets/EffectChipsWidget.*`, `widgets/StatusBadgeWidget.*`, all five
named typography-audit widgets, `HudWidgetDrawUtils.*`,
`HudTextMetricsCache.*`, `HudRegionCatalog.*`, `HudWireframeRenderer.*`,
`sketches/hud_validation_studio/src/*`, `sketches/experience_runtime/src/
ExperienceRuntime.*`, `HudCompositorBridge.*`, `HudCompositorStub.*`,
`sketches/experience_runtime/test/**`, all four
`shared/src/hud-compositor-test/*` files.

**Files changed**: `HudRealFrameResolver.cpp` (new `allActiveEffectIds()`,
rewritten `resolveEffects()`), `hud_real_frame_tests.cpp` (rewrote the
dominance-not-first test's `effects.active` assertions, added the
mandatory >2-effect regression), `hud_presentation_tests.cpp` (new
`flexible.secondary_a/b` containment test), `NumericValueWidget.{h,cpp}`,
`ProgressRingWidget.{h,cpp}`, `SparklineWidget.{h,cpp}`,
`AmbientFieldWidget.{h,cpp}` (truncation added to all four),
`hud_validation_studio/src/ofApp.cpp` (new >2-effect screenshot case),
`docs/reviews/hud-typography-metrics-v1.md`,
`docs/reviews/hud-wireframe-bounds-v1.md`,
`docs/reviews/hud-canonical-1280-screenshot-index.md`,
`docs/reviews/hud-runtime-architecture-closure-review.md` (all four
updated with this patch's findings). `ExperienceRuntime.*`,
`HudCompositorStub.*` were inspected but NOT modified — no genuine
integration defect was found in them.

## 11. Tests/builds run — exact results

| Suite | Command | Result |
|---|---|---|
| HUD dependency-free | `make -f Makefile.tests test` | 676/676 (was 660) |
| HUD real-frame | `make -f Makefile.tests test-real` | 140/140 (was 131) |
| HUD typography | `make -f Makefile.tests test-typography` | 14/14 (unchanged) |
| HUD MediaViewportMesh | `make -f Makefile.tests test-viewport` | 357/357 (unchanged) |
| `experience_runtime` lifecycle | `make -C test -f Makefile.tests test` | 78/78 (unchanged) |
| `experience_runtime` session2 integration | `make -C test -f Makefile.tests session2` | 42/42 (unchanged) |
| **Total** | | **1307/1307** (was 1282) |
| `hud_validation_studio` build + autocapture | `make Release`, `HUD_STUDIO_AUTOCAPTURE=1` | Clean; 38 screenshots, all 1280×720; 5-cycle profile-switch, 705/cycle, zero drift |
| `experience_runtime` build + full 5000-frame GL harness | `make Release`, `EXPERIENCE_RUNTIME_GL_HARNESS=1` | Clean; **ALL CHECKS PASSED**, zero failures |

## 12. Deviations from prompt

None of substance. The regression's exact prominence values (0.3, 0.02,
0.5, 0.9) and effect IDs were chosen by this patch (not specified in the
prompt beyond ">2 effects, one below threshold, dominant not at index
0") — documented here for traceability, not a deviation from intent.

## 13. Newly discovered risks

None new. The `effects.active` defect this patch fixed was itself a risk
already latent in the prior session's work — now closed, not merely
identified.

## 14. Contract changes requested

None. No semantic slot ID added, no `EffectActivityStatus` change, no
dominance semantic change, no `HudFrameData` change, no `SceneFrame`/
`IEcopunkScene`/lifecycle/command-ownership/runtime-HUD-ownership change,
no canonical geometry change, no new compositor abstraction, no
activation-epoch field, no scene-specific renderer branch. **No
architecture review trigger was hit.**

## 15. Final disposition

- Does `effects.active` now contain all active slots? **Yes.**
- Does `effects.dominant` remain independently canonical? **Yes.**
- Did all production-visible typography paths pass? **Yes.**
- Did `flexible.secondary_a`/`_b` padding pass? **Yes.**
- Did canonical 1280×720 validation remain green? **Yes.**
- Is exactly one production HUD path still active? **Yes.**
- Did any shared contract change? **No.**
- Is the wireframe ready for Architecture acceptance?

**Ready for HUD Runtime acceptance.**
