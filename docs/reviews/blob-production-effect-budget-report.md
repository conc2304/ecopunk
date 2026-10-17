# Blob Production Effect Budget v0 — Implementation Report

**Date:** 2026-10-17
**Status:** BLOB PRODUCTION EFFECT BUDGET IMPLEMENTED · DESKTOP VERIFIED · PI 3B+ QUALIFICATION PENDING
**Costs:** PROVISIONAL / MEASURED-HARDWARE QUALIFIED (Rock Pi 4B)

## 1. What shipped

Per-fragment treatment selection, Blob-local, built on the existing `assignEffectForId()` seam:

```
eligible palette → weighted candidate (stable per fragment id + palette generation)
→ budget check (sum of live costs ≤ 10) → cheaper eligible shader → Original
```

- **Lifetime.** A fragment's treatment is chosen once, at creation (`VideoRegionController::effectAssigner`). Its cost is released only when the fragment is gone: finished dissolving, or dropped by reset (`effectReleaser`). Fades never change a fragment's cost or treatment.
- **Media change.** A media change starts a new palette generation (`blobeffects::MediaGeneration`). New fragments draw fresh treatments; fragments already on screen keep theirs.
- **Integer costs.** Costs are integer centi-units, so 8 × 1.25 = 10 exactly.
- **Weights override.** `EXPERIENCE_RUNTIME_BLOB_BUDGET_WEIGHTS` overrides weights for art-direction tuning only. It cannot change costs or eligibility.

## 2. Table (design v0 costs and weights; eligibility from this session's review)

| Treatment | Cost | Weight | Eligible | Why |
|---|---|---|---|---|
| Original | 0 | 5 | yes | shader bypass |
| Recolor | 1.0 | 3 | yes | BLOB-002 compatible |
| Heatmap Recolor | 1.25 | 1 | yes | pointwise. At fade end the 3-stage→direct switch shifts colors slightly: 2.2% of frame pixels > 8 levels at 10 fragments with fractional bounds |
| Edge Glow | 1.0 | 2 | **no** | bright glow rims on the right/bottom edges of real tracked fragments |
| Water Refraction | 1.0 | 2 | **no** | black fragments on real tracked fragments |
| Pixel Sorting | 2.5 | 1 | **no** | same sampling defect as the two above |

**Why three effects are blocked.**
- The design keeps visually incompatible effects out regardless of speed (§9, §17).
- All three take pixel offsets as `1/resolution` with `resolution` = the fragment's size. But the crop occupies only `w/scratchW` of the grow-only scratch FBO.
- Once a large fragment has grown the scratch FBO (peak 542×466 in one run), small fragments sample several pixels too far and read cleared texels past the crop.
- The Experiment 2 synthetic load hid this: all of its regions were the same size.
- Evidence: `blob-effect-budget-captures/blocked_*.png`.
- Costs and weights stay in the table. A renderer sampling fix would make them eligible again (§7).

## 3. Observability

The `BlobRenderProfile` line (`EXPERIENCE_RUNTIME_BLOB_RENDER_PROFILE=1`) gains:
- `effectBudgetMax`, `effectBudgetUsed`;
- `activeOriginal`, `activeShader`;
- `activeCostByEffect`, `budgetFallbacks`;
- `paletteGeneration`.

`SceneHudStatus.activeEffects` (compatibility only) lists each distinct live shader treatment, plus the background. No HUD schema change.

## 4. Validation

**Unit tests (§18 cases).** `blob_effect_budget_tests` passes 62593/62593:
- 10 × Recolor is accepted, and the 11th falls back;
- 8 × Heatmap is accepted, and the 9th falls back;
- 4 × Pixel Sorting is accepted, and the 5th falls back;
- the design's 2 PS + 3 HM + 2 RC = 10.75 example sends the last fragment to Original;
- a cheaper shader is tried before Original;
- release frees budget, and a release after a palette change uses the charged cost;
- unknown or costless effects fall back to Original;
- assignment is deterministic and generation-scoped, and weights shape the palette;
- the budget is never exceeded under 20k random assign/release steps.

`blob_effect_policy_tests` (media generations) passes 1192/1192.

**Runtime, controlled load** (capacity bench `budget` mode, 10 fragments, Fern):

| Case | Result | Capture |
|---|---|---|
| Heatmap forced | 8 heatmap + 2 Original, used 10.0 | `budget_forced_heatmap_8_plus_2_original.png` |
| Default weights | 5 Original, 3 Recolor, 2 Heatmap, used 5.5 | `budget_default_weights.png` |

**Runtime, real tracked load** (lifecycle harness): 4937/4938.
- Over the whole effect-policy proof: no live fragment ever changed treatment, the budget was never exceeded, the books matched the renderer's live fragments on every frame, and every assignment was eligible.
- One palette generation per media change; none while inactive; one catch-up generation on resume.
- **The 1 failure:** the resume step landed on *Tenerife Shore*, which produced 0 detections for 30 s at the default detector settings, so the "fragments appeared" precondition timed out. This depends on the clip; it isn't a budget failure. Per-video detection presets (next increment) address it.

**Regressions.**

| Check | Result |
|---|---|
| BLOB-002 A/B | 1514/1514 |
| GL restoration | all passed |
| RT-003 Blob→Temporal→Blob | 5351/0 |
| ExperienceRuntime and standalone Blob Release builds | pass |
| Unit suite | pass |

## 5. Contract changes

None. Additions in shared files used only by Blob:
- `VideoRegionController`: `effectReleaser`, `collectActiveAssignments()`;
- earlier increment: `effectAssigner`, renderer counters, `BlobSceneCore::setRegionOverride()`.

## 6. Pi 3B+ qualification

```sh
timeout 600 env EXPERIENCE_RUNTIME_BLOB_EFFECT_CAPACITY=1 EXPERIENCE_RUNTIME_BLOB_CAPACITY_MEDIA=fern \
  EXPERIENCE_RUNTIME_VIDEO_DWELL=900,900 EXPERIENCE_RUNTIME_BLOB_CAPACITY_SCRIPT=qualify \
  ./bin/experience_runtime > ~/pi-capacity/qualify.log 2>&1
```

Steps: A, recolor@10, edge_glow@10, water_refraction@10, heatmap_recolor@8, pixel_sorting@4, mixed10 (9.5 cost units), then the production `budget` step. Follow the runbook's thermal procedure.

## 7. Recommended follow-ups

1. **Pi 3B+ qualify run.** Freeze the costs, or adjust only the affected ones.
2. **Renderer sampling fix.** Bind `resolution` to the scratch size, and pad the crop capture so neighbor taps read real source pixels. Then re-review edge_glow, water_refraction and pixel_sorting for eligibility.
3. **Fade-end color shift.** Optionally review heatmap_recolor's slight color change at fade end in motion.
