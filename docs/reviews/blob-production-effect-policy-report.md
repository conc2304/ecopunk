# Blob Visual-Behavior Parity — Fragment Effect Policy

**Date:** 2026-10-15
**Domain:** Blob Scene Migration / Blob Rendering
**Status:** Implemented, desktop-verified. Not yet domain-reviewed. Pi not run.

## 1. Conclusion

**INCOMPLETE — PRODUCTION EFFECT POLICY WAS DEFERRED / NEVER IMPLEMENTED — FIX IMPLEMENTED**

Production did not *lose* multi-effect behavior. The standalone sketch never varied its fragment effect on its own. Effect choice was a developer ofxGui slider whose default, index 0, is `desaturate`. Production inherited that default, and the planned curated effect variety was explicitly deferred:

- `docs/blob-region-architecture.md` §5: `assignEffectForId()` was left as "the seam a later per-blob effect-variety feature would extend".
- `docs/blob-region-prototype-shared-catalog-migration-status.md`: there is "no automatic variation of any kind". Curated selection is deferred to DEC-007 curated reseed.
- `docs/shared-project-docs/blob-scene-migration-first-production-scene-acceptance-review.md` §11 and §17 list "curated Blob Regenerate variants" and "effect preset curation" as **future product enhancements**.

No document approves fixed `desaturate` as the final production behavior.

## 2. Standalone behavior (blob-region-prototype)

| Question | Answer |
|---|---|
| Effect list | `BlobSceneCore::effectChoices()`, 18 entries: desaturate, invert, recolor, threshold, dither, solarize, scanlines, channelshift, hue_rotate, ascii_solarpunk, bioluminescence, chromatic_aberration, edge_glow, ink_outlines, pixel_drift, pixel_sorting, water_refraction, heatmap_recolor |
| `pEffectIndex` | `ofParameter<int>` in the range 0..17. It selects `VideoRegionController::Params::effectName` |
| Write sites | Only `ofApp::syncParamsToSystems()`, every frame, from the GUI value. Nothing changes `pEffectIndex` automatically |
| Trigger | GUI only |
| Per-fragment variation | None. `assignEffectForId()` returns the single global name, captured when the fragment is created |
| Background | Separate GUI slider `pBackgroundEffectIndex`. Default mode 1 (normal, unshaded) |
| Animated amounts/params | None. `effectAmount` is a GUI slider (1.0). Uniforms are catalog defaults |

## 3. Production before this change

`VideoRegionController::Params::effectName = "desaturate"` comes from the struct default. Production never wrote it after setup: there was no write on activate, media change, timer, command, or Regenerate. Regenerate is not advertised. There was no curated pool and no `EffectActivityStatus` producer (`HudFrameData.effects = std::nullopt`), and Blob does not consume Shared Effects selector output. The authored knowledge pack contains no Blob-scoped entries (its only whitelist entry is scoped to `temporal-fields`).

## 4. Parity matrix (defaults)

| Behavior | Standalone | Production (before) | Same? | Classification |
|---|---|---|---|---|
| Fragment effect | desaturate (GUI default) | desaturate | Yes | — |
| Number of usable effects | 18 via GUI | 1 | No | developer-only difference |
| Effect changes over time | No | No | Yes | deferred product behavior |
| Effect-change trigger | Developer slider | None | No | developer-only difference |
| Effect amount | 1.0 | 1.0 | Yes | — |
| Fragment scale | 1.0 | 1.0 | Yes | — |
| Crop padding | 0.0 | 0.0 | Yes | — |
| Draw mode | 0 | 0 | Yes | — |
| Max active fragments | 10 | 10 | Yes | — |
| Background mode | 1 (normal) | 1 (normal) | Yes | — |
| Background effect | desaturate (unused in mode 1) | same | Yes | — |
| Fade behavior | 0.25 s arrive/dissolve | same | Yes | — |
| Per-fragment variation | None | None | Yes | deferred product behavior |
| Scene-level evolution | None | None | Yes | deferred product behavior |

There is no migration regression. At defaults, production matches the standalone sketch exactly.

## 5. Implemented policy

| | |
|---|---|
| Curated set | `desaturate`, `recolor` |
| Trigger | Canonical Shared Video `mediaId` change (option B) |
| Cadence | At most one selection per media item, so it follows Shared Video's auto-advance hold or the operator's Next/Prev |
| Order | Fixed cycle. The first media item keeps `desaturate`, so the startup look is unchanged |
| Owner | `BlobProductionScene` (`blobeffects::MediaEffectPolicy`, `sketches/experience_runtime/src/BlobEffectPolicy.{h,cpp}`) |
| Inactive | No selection while inactive (`update()` is gated). On reactivation, the first update performs exactly one catch-up selection if the media changed |
| Lifecycle | Selection survives `activate()`/`reset()`, so the same media keeps the same effect |
| Fragments in flight | Keep their effect until they dissolve (the effect is captured at creation). No mid-life switch, no flicker |
| Parameters | Catalog defaults only (DEC-007). Only the effect ID is selected |
| Fallback | Unknown or failed shader: the renderer's existing unshaded-crop fallback. The set is a fixed list, so no invalid ID is possible |
| Shared Effects | Untouched. `HudFrameData.effects` stays `std::nullopt`. The compatibility-only `SceneHudStatus.activeEffects` now truthfully reports `region: <selected>` |

The standalone sketch is unchanged and keeps its 18-effect developer slider.

## 6. Curation / BLOB-002 eligibility

`BlobRenderPathAbHarness` now enforces the **unchanged** BLOB-002 production tolerance on every curated effect. It also reports every other effect as within or outside that tolerance.

This tolerance has a direct visual meaning. A fragment fades in on the 3-stage path and switches to the direct path at alpha 1. An effect whose two paths differ therefore pops visibly at arrival and at dissolve.

| Effect | Result | Reason |
|---|---|---|
| desaturate | **Curated** | Within tolerance. Pointwise, 1 tap |
| recolor | **Curated** | Within tolerance (ten_fragments pct>8 0.092% ≤ 0.1%). Pointwise, 1 tap |
| heatmap_recolor | Excluded (closest candidate) | Overlap maxRgb 3 > 2. Fractional/ten_fragments pct>8 0.2–2.2% |
| threshold, solarize | Excluded | Step functions amplify sub-texel interpolation differences. ten_fragments pct>8 3.5–6.6% |
| invert | Excluded | Fractional pct>8 0.155% |
| hue_rotate | Excluded | Identity at catalog defaults (offset 0, speed 0, mults 1). Making it visible would mean inventing params |
| dither | Excluded | Identity at Blob's `effectAmount` = 1 (the shader's alpha arc is "clean" at 1). It also uses `vTexCoord*resolution` |
| channelshift | Excluded | Within tolerance, but `shift` is in scratch-FBO UV units. The grow-only scratch FBO makes the pixel offset session-dependent, and edge taps read cleared texels |
| ascii_solarpunk | Excluded | Fractional output alpha breaks the direct-path equivalence |
| scanlines, nature pack (7) | Excluded | Outside tolerance and/or neighbor/UV-dependent sampling |

Run: 1422/1422 checks, PASS ([log](logs/blob-effect-policy-ab-harness-2026-10-15.log)). The images in [blob-effect-policy-captures/](blob-effect-policy-captures/) (`recolor_*`) show correct crops and no black output. The only difference is the 1 px legacy-composite seam that BLOB-002 already documented for desaturate.

## 7. Runtime proof (ExperienceRuntime, real media)

`BlobLifecycleHarness` gains EffectStep/EffectInactive/EffectResume phases after the existing 20 cycles. Result: **2148/2148 PASS** ([log](logs/blob-effect-policy-blob-lifecycle-2026-10-15.log)). The pre-change count was 895; the extra checks are the per-frame SceneFrame checks during the waits.

- Media 1 → `desaturate` → Next → `recolor` → Next → `desaturate` → Next → `recolor` (wrap exercised). Exactly one selection per media change.
- Deactivated + Next: no selection while inactive. Reactivate: exactly one catch-up selection.
- Zero effect changes without a media change across the whole proof (a per-frame tracker allows one frame of lag).
- Each step: at least one real fragment, `activeEffects` reports the selected effect, `HudFrameData.effects` absent.
- Captures (full HUD + scene-only): `blob-effect-policy-captures/blob_lifecycle_effect_*`. `effect_2_recolor` shows 10 fragments, with one pre-change desaturate fragment still alive, as designed.

## 8. Regressions (desktop, 2026-10-15)

| Check | Result |
|---|---|
| ExperienceRuntime Release build | PASS |
| blob-region-prototype Release build | PASS (no source change) |
| Unit suite `make -C test -f Makefile.tests test` | PASS, including new `blob_effect_policy_tests` 1204/1204 |
| GL restoration harness | ALL CHECKS PASSED |
| RT-003 Blob → Temporal → Blob | 5351 checks / 0 failures (same count as BLOB-002 baseline) |
| BLOB-002 A/B | 1422/1422 |

## 9. Performance

Desktop normal run with `EXPERIENCE_RUNTIME_BLOB_RENDER_PROFILE=1` ([log](logs/blob-effect-policy-desktop-render-profile-2026-10-15.log)), 30 fps cap, direct path dominant:

| Effect | fragments (avg) | regionRenderMs/frame | ≈ ms per fragment |
|---|---|---|---|
| recolor | 9.2 | 0.594 | 0.064 |
| recolor | 3.1 | 0.276 | 0.089 |
| desaturate | 3.2 | 0.159 | 0.050 |
| desaturate | 2.1 | 0.111 | 0.053 |

**Pi: NOT RUN** (no deploy/SSH from this session). `recolor` has the same texture-tap count as `desaturate` and adds one vec3 multiply, so no Pi regression is expected. This needs an operator-run Pi smoke with the profile env var.

## 10. Contract changes

None. No change to IEcopunkScene, SceneFrame, HudFrameData, SceneManager, ExperienceRuntime ownership, Shared Video, Shared Effects, EffectActivityStatus, knowledge schema, slot IDs, HUD geometry, BLOB-002 architecture, or `shared/src`.

## 11. Limitations

- Two curated effects is modest variety, but both are close in character (grey vs amber luma tint). `heatmap_recolor` is the strongest next candidate. It only misses on fractional bounds, where the legacy path's extra resample dominates.
- On warm-toned media (e.g. fire), `recolor` reads as a darker amber and is low-contrast.
- Pi numbers are not recorded.

## 12. Recommended next step

**REVIEW CURATED EFFECT SET WITH USER.**
