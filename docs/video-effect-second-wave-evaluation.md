# Second-Wave Evaluation: radar-effects-gallery, radar-pulse, contour-portrait

Per [`shared-video-effect-architecture.md`](./shared-video-effect-architecture.md)
§9 Phase 9: "Evaluate RG, RP, and CP. Treat these as second-wave
integrations. Do not force them into Contract A." This is a decision record,
not an implementation — none of these three sketches were modified as part
of this migration.

## radar-effects-gallery (RG)

**Not migrated. Recommendation: leave as its own Contract B compositor.**

- Uses `videoTex`/`maskTex` + `texCoordVarying` (Contract B), not `tex`/
  `vTexCoord` (Contract A) — a genuinely different uniform contract, not a
  variant of the effects-pool convention every migrated sketch shares.
- No `ShaderLibrary` at all — `GalleryCompositor` owns a flat
  `std::array<ofShader, NUM_MODES>` indexed by mode number, not a
  name-keyed registry. Folding it into the canonical `VideoEffectRegistry`
  would mean rewriting `GalleryCompositor` into a name-based dispatcher
  first, which is a real architectural change to a working sketch, not a
  refactor-in-place like the four sketches that were migrated.
- `heatmap_recolor`'s Contract B port (`gallery_mode10_heatmaprecolor.frag`,
  added earlier this session) demonstrates the two contracts can coexist
  side by side without unifying them — that pattern (hand-port a shader
  into Contract B when RG specifically wants it) is cheaper and lower-risk
  than generalizing the shared service to a second contract it doesn't
  otherwise need.
- If RG's contract is ever added to the shared service, `VideoEffectDefinition::contract`
  (`ShaderContract::A` / `B` / `None`) already has a `B` value reserved for
  exactly this — see `shared/src/video-effects/core/VideoEffectTypes.h`. No
  other code changes would be needed to represent it; only `GalleryCompositor`
  itself would need to change to consume the registry, which this evaluation
  recommends against doing as part of this effort.

## radar-pulse (RP)

**Not migrated. Recommendation: leave as its own fixed two-stage pipeline.**

- No effect registry of any kind — `RPCompositor` and `RPRevealMask` each
  own exactly one shader, always on, with no swappable-effect concept.
- `RPRevealMask.h`'s own header comment states this is deliberate:
  "Kept local to this sketch per the engineering handoff — this risk...
  shouldn't leak into shared/src/hud and silently affect sketches that
  never touch this feature." Forcing a registry entry onto a
  single-fixed-shader pipeline would work against that documented intent,
  not fulfil it.
- `color_reveal.frag` already does its own luma-based recolor (binary
  desaturate↔color reveal driven by `maskValue`) — conceptually adjacent to
  the effects-pool family but implemented as part of the reveal-mask
  compositing itself, not a general-purpose effect. There is no clean
  extraction point that wouldn't also require restructuring `RPCompositor`.

## contour-portrait (CP)

**Not migrated. Recommendation: leave as its own fixed preprocessing pass.**

- One shader (`contour_preprocess.frag`/`.vert`), no registry, no swappable
  effects — `ContourDisplacementEffect` feeds its single GPU pass into a
  CPU-side line-displacement algorithm. The "effect" in the everyday sense
  here is the whole class's displacement/geometry pipeline, not a
  recolor/stylize pass the shared catalog's `SinglePassShaderEffect`
  contract is shaped for.
- `ContourPresets` already provides five named starting points, but they
  write directly into `ContourDisplacementEffect`'s `ofParameter`s — there
  is no shader-swapping concept to migrate onto the shared registry at all.

## Why "second-wave" rather than "excluded permanently"

All three are legitimate `ofShader` consumers (they were correctly included
in the original [`shader-effect-system-probe.md`](./shader-effect-system-probe.md)
inventory) — they're deferred, not out of scope by definition. Each would
require its own dedicated design pass (RG: generalize `VideoEffectRenderer`
to Contract B and rewrite `GalleryCompositor` as a registry consumer; RP/CP:
decide whether a single-fixed-shader sketch benefits at all from a registry
built for many-effects-pick-one) before touching their working code — the
architecture doc's own instruction ("do not force them into Contract A") is
the correct call for this pass, and forcing a rushed migration onto any of
the three risks exactly the kind of regression the safe, scoped migrations
of BRP/BE/TF/QC (this session) were careful to avoid.
