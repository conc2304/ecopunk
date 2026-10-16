# Blob Visual Model — Desaturated World + Original / Recolor Reveals

**Date:** 2026-10-16
**Domain:** Blob Scene Migration / Blob Rendering
**Status:** IMPLEMENTED — VISUAL MODEL CHANGED. Desktop-verified, Pi not run.
**Builds on:** [blob-production-effect-policy-report.md](blob-production-effect-policy-report.md). That report's media-change policy is kept. Its curated set `{desaturate, recolor}` is replaced here.

## 1. Composition

**Before**

```
full-frame source → cover-fit drawSubsection (unshaded)          → scene FBO   [BlobSceneCore background mode 1]
tracked crop      → desaturate | recolor (per media)             → fragment   [VideoRegionEffectRenderer, BLOB-002 direct/legacy]
```

**Now**

```
full-frame source → desaturate, same cover-fit crop rect          → scene FBO   [background mode 2, existing background renderer, direct path]
tracked crop      → Original: source crop straight to destination → fragment   [new unshaded direct draw, no FBO, any alpha]
                  → Recolor:  BLOB-002 direct at alpha 1, 3-stage while fading (unchanged)
```

- **Background.** `BlobSceneCore::drawBackground()` mode 2 was already there. It feeds the same `computeCropFillSourceRect` rect into BlobSceneCore's separate background `VideoRegionEffectRenderer`. The background always draws at alpha 1 and amount 1, so it takes the BLOB-002 direct path. Geometry is identical to mode 1, and there is no new overlay path. Production now sets `mode = 2, effectName = "desaturate"` in `BlobProductionScene::setup()`.
- **Original.** The renderer's existing contract is "empty `effectName` ⇒ unshaded crop". Its old implementation copied the crop into the scratch FBO and composited it back out. When direct output is enabled, the unshaded case now draws `sourceTexture->drawSubsection(dest, crop)` with the fragment alpha. That is one resample, no FBO and no shader.
  - It is valid at every alpha. The scratch copy of an opaque source is itself opaque, so both versions do the same single alpha blend. Fades therefore stay on one path, and there is no pop when a fade ends.
  - It is counted as a direct draw.
  - With `EXPERIENCE_RUNTIME_BLOB_RENDER_LEGACY` set, the old scratch route still runs and is now counted as a legacy draw.
- **Recolor.** Unchanged BLOB-002 routing.

## 2. Files changed

| File | Change |
|---|---|
| `shared/src/VideoRegionEffectRenderer.{h,cpp}` | Unshaded direct draw. Stage counters now include unshaded draws. Only Blob uses this class: the fragment renderer and the background renderer |
| `sketches/experience_runtime/src/BlobEffectPolicy.{h,cpp}` | `FragmentTreatment {Original, Shader(effectId)}`, curated `{Original, Recolor}`, `backgroundEffectId()` |
| `sketches/experience_runtime/src/BlobProductionScene.{h,cpp}` | Desaturated background. Treatment → `effectName`. `activeEffects` omits a region entry for Original. Profile line gains `treatment`, `background`, `backgroundMs`. Adds `EXPERIENCE_RUNTIME_BLOB_BACKGROUND_NORMAL` (profiling A/B only) |
| `sketches/experience_runtime/src/BlobRenderPathAbHarness.cpp` | Original pass. The production set is now recolor + desaturate (background) + Original. Fractional Original fragments are checked against the source itself |
| `sketches/experience_runtime/src/BlobLifecycleHarness.{h,cpp}` | Background check, background-only capture, treatment-based checks, captures wait for ≥3 fragments |
| `sketches/experience_runtime/test/blob_effect_policy_tests.cpp` | Treatment model tests |

No changes to `BlobSceneCore`, `VideoRegionController`, the standalone sketch, or any contract.

## 3. Treatment representation and selection

`blobeffects::FragmentTreatment` is Blob-local.
- **Original** has `kind = Original` and an empty `effectId`. It maps to an empty `VideoRegionController::Params::effectName`, which the renderer draws unshaded. It is not a fake shader.
- **Shader** treatments carry a canonical effect ID and use catalog-default uniforms.

Selection keeps the media-change policy:
- The first media item gets Original. Each later media change alternates: Original → Recolor → Original → …
- There is no per-frame reselection and no selection while inactive. Reactivation does exactly one catch-up selection.
- Fragments keep their treatment until they fade out, because it is captured at creation.
- `desaturate` is excluded as a fragment treatment because it would be invisible on the desaturated background.

## 4. Visual results — [blob-desaturated-world-captures/](blob-desaturated-world-captures/)

| State | Scene-only | Full HUD |
|---|---|---|
| Background only (after Reset, 0 fragments) | `blob_lifecycle_background_only_scene_only.png` | `blob_lifecycle_background_only.png` |
| Original (fern clip, 10 fragments) | `blob_lifecycle_effect_1_original_scene_only.png` | `blob_lifecycle_effect_1_original.png` |
| Original (resume, 3 fragments) | `blob_lifecycle_effect_resume_original_scene_only.png` | `blob_lifecycle_effect_resume_original.png` |
| Recolor | `blob_lifecycle_effect_{0,2}_recolor_scene_only.png` | `blob_lifecycle_effect_{0,2}_recolor.png` |

On the fern clip, the reveals are true green and their edges continue exactly into the grey background.

The A/B harness proves alignment at the pixel level. Fractional-bound Original fragments over the opaque source reproduce the source with **maxRgb 0** (ten fragments: maxRgb 1).

The four `*_desaturate*` files in that folder are stale copies from 2026-10-15 and should be deleted (deletion was blocked in this session).

## 5. BLOB-002

A/B harness: **1514/1514 PASS** ([log](logs/blob-desaturated-world-ab-harness-2026-10-16.log)).
- recolor and desaturate are within the unchanged tolerance.
- Original takes the direct path in every case, fades and reduced amount included. Integer-bound and fade cases are within ±2 of legacy, with alpha never lost.
- Fractional Original differs from legacy only by legacy's extra resample. That difference is informational, because Original never uses legacy in production.

Normal-run profile: Original windows show `legacy=0`.

## 6. Performance (desktop, 30 fps cap; CPU-side timings, not GPU time)

| Config | backgroundMs | regionRenderMs | fragments | sceneDrawMs |
|---|---|---|---|---|
| NEW desaturated bg + Original | 0.072 | 0.073 | 10.0 | 0.171 |
| NEW desaturated bg + Recolor | 0.072 | 0.196 | 6.85 | 0.289 |
| Unshaded bg + Original | 0 (inline) | 0.043 | 10.0 | 0.099 |
| Unshaded bg + Recolor | 0 (inline) | 0.093 | 9.5 | 0.150 |

The "OLD" rows use `EXPERIENCE_RUNTIME_BLOB_BACKGROUND_NORMAL`, which keeps the new treatments.

Desktop cannot answer the Pi question. The background effect adds two full-frame fill passes at 1280×720: a crop blit into the scratch FBO, then the shader pass. On a VideoCore IV that GPU fill cost is the thing to measure.

**Pi operator smoke** (from the deployed `experience_runtime` binary directory, ~2 min each; compare `fps`, `frameMs`, `backgroundMs`, `regionRenderMsPerFrame`, `fragments`, `treatment`, `directDraws`/`legacyDraws` in the `BlobRenderProfile` lines):

```sh
# NEW: desaturated world (alternates Original / Recolor on media change)
EXPERIENCE_RUNTIME_BLOB_RENDER_PROFILE=1 ./experience_runtime 2>&1 | tee blob-pi-new.log
# OLD background for comparison (same treatments, unshaded background)
EXPERIENCE_RUNTIME_BLOB_RENDER_PROFILE=1 EXPERIENCE_RUNTIME_BLOB_BACKGROUND_NORMAL=1 ./experience_runtime 2>&1 | tee blob-pi-old-bg.log
grep BlobRenderProfile blob-pi-*.log
```

If the background pass proves costly, there is a Blob-local fix that wasn't made here because it would be premature optimization. desaturate is pointwise, so the background could be shaded straight from the source texture, skipping the full-frame scratch blit.

## 7. Regressions (desktop, 2026-10-16)

| Check | Result |
|---|---|
| ExperienceRuntime / standalone Blob Release builds | PASS |
| Unit suite (10 binaries incl. blob_effect_policy 1214/1214) | PASS |
| Blob lifecycle (20 cycles + background + treatment proof) | 3279/3279 |
| GL restoration | ALL CHECKS PASSED |
| RT-003 Blob → Temporal → Blob | 5351 / 0 failures |
| BLOB-002 A/B | 1514/1514 |

## 8. Contract changes

None. `HudFrameData.effects` stays `std::nullopt`. The compatibility `activeEffects` field reports `background: desaturate`, plus `region: recolor` only while Recolor is active.

## 9. Limitations

- Pi cost of the full-frame background pass is unmeasured.
- The standalone dev sketch still defaults to the old model (background mode 1, GUI effect slider). It has no Original choice.
- Original is subtle on low-saturation media (rain, surf). That's inherent to "reveal the original color".
- A fragment born under the previous treatment finishes its life with it, so the two treatments briefly mix at a media change.

## 10. Recommended next step

**RUN MULTI-TREATMENT PI CAPACITY EXPERIMENT** (commands in §6).
