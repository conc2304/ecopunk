# shared/src/video-effects — the canonical video-effect service

**If you're about to add a new shader effect, bind uniforms for an existing
one, or wire up effect selection in a sketch, you almost certainly don't
need to write new code — read this file first.**

This is the single source of truth for video effects (single-pass shaders,
motion extraction/composite, erosion, temporal trails, reaction-diffusion,
ridgeline) across every sketch in this monorepo. It replaced N independent
copies of the same hand-written per-effect uniform-binding cascades — see
[`docs/shader-effect-system-probe.md`](../../../docs/shader-effect-system-probe.md)
for the investigation that found the duplication, and
[`docs/shared-video-effect-architecture.md`](../../../docs/shared-video-effect-architecture.md)
for the full design. This README is the practical "how do I actually use
it" companion to those design docs.

## Directory map

```text
shared/src/video-effects/
├── core/            VideoEffectService, VideoEffectRegistry, VideoEffectDefinition,
│                    VideoEffectTypes, VideoEffectContext, VideoEffectInstance,
│                    VideoEffectParameterSchema, VideoEffectParameters,
│                    VideoEffectAssetRegistry, VideoEffectLoadReport, VideoEffectCapabilities
├── effects/         One VideoEffectInstance subclass per effect *kind* — not one per
│                    effect. SinglePassShaderEffect alone backs all 19 single-pass
│                    effects (desaturate, recolor, heatmap_recolor, ...); the others
│                    (MotionExtractionEffect, MotionCompositeEffect, ErosionEffect,
│                    RidgelineEffect, TemporalTrailsEffect, ReactionDiffusionEffect)
│                    each back exactly one stateful effect family.
├── knowledge/       EffectKnowledgeBase (whitelist/blacklist persistence),
│                    EffectRandomizer (the shared random-parameter-generation pipeline)
├── evolution/       EffectEvolutionController (current->target parameter transitions),
│                    PatternDriftController (continuous low-amplitude drift)
└── catalog/         DefaultVideoEffectCatalog.{h,cpp} — THE place every effect gets
                     registered. If you're adding a new effect, this is where.
```

Shader **assets** (`.glsl`/`.frag`/`.vert` files) live separately, in
[`shared/assets/video-effects/`](../../assets/video-effects/) — see that
directory's own README for the asset-sync mechanism
(`scripts/sync-video-effect-assets.py`). This directory (`shared/src/video-effects/`)
is C++ only.

## Consuming the service from a sketch

```cpp
#include "VideoEffectService.h"

videoeffects::VideoEffectService service;
service.setup(); // reads <sketch>/bin/data/effect-manifest.json, builds a load report

if (service.hasEffect("heatmap_recolor")) {
    auto instance = service.createInstance("heatmap_recolor"); // nullptr if shader load fails
    if (instance) {
        videoeffects::VideoEffectContext ctx;
        ctx.sourceTexture = &myVideoTexture;
        ctx.destinationRect = ofRectangle(0, 0, w, h);
        ctx.time = ofGetElapsedTimef();
        ctx.alpha = 1.0f;

        videoeffects::VideoEffectParameters params; // missing keys fall back to schema defaults
        params.set("gamma", 0.9f);
        params.set("palette", 2);

        instance->update(ctx, params); // no-op for most single-pass effects; real work for Processor-kind ones
        instance->render(ctx, params);
    }
}
```

Every sketch that consumes this needs:

1. `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src` in its `config.make`
   (already true for every sketch that uses `shared/src` at all).
2. An `effect-manifest.json` at the sketch root (see below).
3. A synced copy of whatever shader assets its enabled effects need, in
   `bin/data/shared-video-effects/` — run
   `python3 scripts/sync-video-effect-assets.py sketches/<your-sketch>`
   after writing the manifest.

**Do not** hand-write a per-effect `if (name == "recolor") ...` cascade —
that's exactly the pattern this service replaced. If an effect's uniforms
aren't bound the way you need, either the effect belongs in
`SinglePassShaderEffect`'s generic binder already (check
`DefaultVideoEffectCatalog.cpp`'s parameter schema for that effect id) or it
needs its own `VideoEffectInstance` subclass under `effects/` — see the
existing ones (`ErosionEffect.cpp` is a good template for a stateful,
FBO-owning effect) before writing a new pattern from scratch.

## `effect-manifest.json`

Lives at the sketch root (next to `config.make`), not in `bin/data/` — the
sync script reads it from there, and copies it into `bin/data/` so
`VideoEffectService::setup()` can read it too via `ofToDataPath`.

```json
{
  "schemaVersion": 1,
  "effects": {
    "heatmap_recolor": { "enabled": true },
    "recolor": { "enabled": true }
  }
}
```

Only lists which canonical effect ids this sketch wants; the manifest drives
both asset sync and the service's startup load report. Unknown ids fail the
sync script loudly (`ERROR: manifest references unknown effect id(s)`).

## Adding a brand-new canonical effect

1. Add the shader asset(s) under `shared/assets/video-effects/<family>/` (see
   that directory's README for the family folders and the two supported
   shader contracts — Contract A `tex`/`vTexCoord` is the default; only use
   Contract B `videoTex`/`maskTex`/`texCoordVarying` if you're specifically
   targeting radar-effects-gallery's convention).
2. Add an entry to `shared/assets/video-effects/effect-assets.json` mapping
   the new canonical id to its asset paths.
3. Register a `VideoEffectDefinition` in
   `catalog/DefaultVideoEffectCatalog.cpp` — if it's a plain single-pass
   shader, add it to `registerSinglePassEffects()` using the existing
   `floatParam`/`intParam`/`boolParam`/`vec3Param` helpers; if it needs
   persistent state (FBOs, ping-pong buffers, a CPU-side renderer), write a
   new `VideoEffectInstance` subclass under `effects/` first (see
   `TemporalTrailsEffect.{h,cpp}` for the smallest complete stateful
   example), then register it in its own `register*()` function.
4. Add the new id to any sketch's `effect-manifest.json` that wants it, then
   re-run the sync script for that sketch.
5. Rebuild the sketch (`make Release -j4`) and run it briefly — check the
   log for `ofShader: ... failed to compile` before assuming a new shader is
   correct. `sketches/shader-effect-debugger` is the fastest way to preview
   a new effect in isolation without wiring it into a production sketch
   first.

## Sanity-check your changes

```bash
python3 scripts/sync-video-effect-assets.py --check   # asset drift across every sketch with a manifest
python3 scripts/check-video-effect-drift.py            # + duplicate-id + un-migrated-cascade advisory scan
```

Both are read-only in `--check`/default mode respectively for the drift
checker; only `sync-video-effect-assets.py` without `--check` writes files
(into sketches' `bin/data/shared-video-effects/`, never into
`shared/assets/video-effects/` itself).

## What this service does *not* own

Per the architecture doc's ownership boundary: source video selection,
geometry/layout, masks, final composition, scheduling, which effects a
sketch enables, and when instances are created/reset/destroyed all stay
sketch-owned. This service renders one effect for one context on request —
it does not manage your render loop.
