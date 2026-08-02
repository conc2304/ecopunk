# Canonical video-effect shader assets

Source of truth for every shader consumed by `shared/src/video-effects/`.
Populated per [`docs/video-effect-promotion-inventory.md`](../../../docs/video-effect-promotion-inventory.md)
(Phase 0) and [`docs/shared-video-effect-architecture.md`](../../../docs/shared-video-effect-architecture.md)
(§2.1). Not consumed directly at runtime by any sketch — `scripts/sync-video-effect-assets.py`
copies the subset each sketch's `effect-manifest.json` enables into that
sketch's own `bin/data/shared-video-effects/`, per the project's existing
no-runtime-cross-sketch-sharing policy.

## Deviations from the architecture doc's illustrative layout

- **`erosion/`** uses `erosion_accumulation.glsl` and
  `erosion_history_blend.{frag,vert}` instead of the architecture doc §2.1
  sketch's placeholder `erosion_update.glsl`/`erosion_composite.glsl` names.
  Those placeholders predated the actual side-by-side comparison; §4.4's
  authoritative instruction ("rename them to unambiguous IDs") is what these
  names satisfy, per the promotion inventory's §3 recommendation.
- **`utility/passthrough.glsl` was intentionally not promoted.** The
  promotion inventory (§1) found it isn't a fragment effect at all — it's an
  unregistered, unused *vertex* shader duplicate of `common/vert.glsl`'s
  concept, sitting in `quadrant-crosshair`'s nature-pack directory by
  mistake. Promoting a mislabeled dead file would just relocate dead weight.
- **`single-pass/caustics.glsl` was promoted despite having zero current
  callers anywhere in the monorepo** (confirmed in the promotion inventory)
  — unlike `passthrough.glsl`, it's a complete, correctly-formed fragment
  effect that was simply never wired into any `ShaderLibrary`. The registry
  (Phase 2) marks it `safeForAutomaticSelection = false` until it gets real
  usage/validation, but it's real, promotable content, not dead weight.

## Erosion: two supported variants, not a merge

`erosion_accumulation.glsl` (from `quadrant-crosshair`) and
`erosion_history_blend.{frag,vert}` (from the `shared/src/ErosionFBO`
lineage used by blob-region-prototype/blueprint_emergence) are **two
different effects that happen to share a family name** — different uniform
sets, different visual results (the history-blend variant desaturates and
has explicit first-frame priming; the accumulation variant does neither).
They are registered as distinct canonical IDs, not consolidated into one
shader. See `docs/video-effect-promotion-inventory.md` §3 for the full
comparison.
