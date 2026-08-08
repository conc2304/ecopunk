# blob-region-prototype — Shared Video-Effect Catalog Migration Status

**Conclusion: fully migrated for uniform binding, via a shared (not blob-local) component. Effect *selection* is manual/developer-GUI only — no shared `EffectRandomizer`/`EffectKnowledgeBase` usage of any kind exists yet. No migration performed this session — inspection only, per this task's explicit instruction.**

## Why Session 1 flagged this as unconfirmed

Session 1's compatibility matrix (`shared/src/video-effects/knowledge/EffectSceneCompatibility.cpp`) listed `blob-region-prototype` as `SharedServiceConsumer` per `CLAUDE.md`'s own migration table, but flagged it as unconfirmed because a grep for `videoeffects::`/`VideoEffect*` under `sketches/blob-region-prototype/src/` returned zero matches. That grep was too narrow — it only looked at the sketch's own `src/` directory.

## What actually consumes the catalog

`sketches/blob-region-prototype/src/ofApp.h` includes `"ShaderLibrary.h"`, which resolves (confirmed — no local `ShaderLibrary.h` exists in this sketch) to `shared/src/ShaderLibrary.h`, a **shader-loading-only** utility (name → `.glsl` path → `ofShader`, explicitly documented in its own header comment as *not* the canonical effect registry: *"If you're adding a new effect or need to bind its uniforms, do not hand-write another per-effect dispatch cascade here... see shared/src/video-effects/README.md"*). Loading a shader by name is not, by itself, evidence of catalog consumption.

The real evidence is one level further in: `ofApp.cpp:291` calls `backgroundEffectRenderer.render(req, shaderLib)`, where `backgroundEffectRenderer` is a `VideoRegionEffectRenderer` (`shared/src/VideoRegionEffectRenderer.{h,cpp}` — a **shared** component, not sketch-local). That file:

```cpp
#include "DefaultVideoEffectCatalog.h"
#include "VideoEffectRegistry.h"
...
const videoeffects::VideoEffectRegistry & catalogRegistry() {
    static videoeffects::VideoEffectRegistry registry = [] {
        videoeffects::VideoEffectRegistry r;
        videoeffects::registerSinglePassEffects(r);
        return r;
    }();
    return registry;
}
...
const VideoEffectDefinition * def = catalogRegistry().getDefinition(name);
```

— i.e. every effect's uniforms are bound from the canonical `VideoEffectDefinition`'s parameter schema defaults, the exact same pattern `temporal-fields/src/TFEffectPicker.cpp`'s own fallback branch and `shared/src/VideoRegionEffectRenderer.cpp` both use (see the Engineering Session 2 verification doc's own note that this is the established, repeated pattern across migrated scenes). `VideoRegionController` (also shared, also used by blob-region-prototype per `ofApp.cpp:62`) governs the per-fragment/per-region path the same way.

**This confirms `CLAUDE.md`'s "fixed-default uniforms source from the shared catalog" characterization exactly** — "fixed-default" is literally what happens: no per-instance parameter randomization occurs anywhere in this sketch's effect path, every bound value is the catalog's own `defaultValue`.

## Effect ID list

`ofApp.cpp`'s local `kEffectChoices` (18 entries: `desaturate`, `invert`, `recolor`, `threshold`, `dither`, `solarize`, `scanlines`, `channelshift`, `hue_rotate`, `ascii_solarpunk`, `bioluminescence`, `chromatic_aberration`, `edge_glow`, `ink_outlines`, `pixel_drift`, `pixel_sorting`, `water_refraction`, `heatmap_recolor`) — every id matches a real canonical `VideoEffectDefinition::id` in `DefaultVideoEffectCatalog.cpp` exactly (checked directly, not assumed). No renaming, no aliasing, no local id namespace.

## Selection mechanism: manual, not shared/automatic

Two independent `ofParameter<int>` GUI sliders select **which** canonical effect id runs — `pEffectIndex` (foreground/region fragments) and `pBackgroundEffectIndex` (full-background pass), both indexing into the same fixed `kEffectChoices` array:

```cpp
ofParameter<int> pEffectIndex { "effectIndex (see log)", 0, 0, 17 };
...
int idx = ofClamp(pEffectIndex.get(), 0, static_cast<int>(kEffectChoices.size()) - 1);
rp.effectName = kEffectChoices[idx];
```

This is developer-tuning-shaped (an `ofxGui`-bound index slider), the same category `HUD-Initiative-Scope-and-Product-Direction-Addendum.md` §9 describes as *"exclusively a developer-tuning interface... not the source of truth for cinematic HUD controls."* Confirmed by grep: **zero** references to `reseed`/`regenerate`/`randomize` (case-insensitive) exist anywhere in `blob-region-prototype/src/ofApp.cpp` — there is no automatic variation of any kind today, curated or otherwise, for this sketch's effect selection.

**No `EffectRandomizer`, `EffectKnowledgeBase`, or `EffectKnowledgePack` symbol appears anywhere in `blob-region-prototype`'s own source or in `VideoRegionEffectRenderer`/`VideoRegionController`.** The shared-knowledge subsystem this session hardens is entirely unconsumed by this scene today.

## Why no migration was attempted this session

Unlike `temporal-fields` (which already had `pickNext()`/`randomizeEffectParams()` — an existing automatic-selection seam to layer blacklist-avoidance onto narrowly), blob-region-prototype has **no existing automatic-selection seam at all** to hook into. Giving it one — even just "avoid a blacklisted combination when the GUI slider lands on it" — doesn't apply either, since this sketch never randomizes per-instance parameters in the first place (every value is the catalog default; there is no "combination" to check against a blacklist). Adding automatic/curated selection to blob-region-prototype would be a genuine new capability and product/architecture decision (should Blob gain autonomous effect cycling at all, and under what curated-reseed semantics — `DEC-007`'s territory), not a "trivial hookup on an already-shared path." Per this task's own instruction (*"If only a trivial knowledge-pack hookup is required on an already-shared path, describe it; otherwise stop at a migration recommendation"*), this document stops here.

## Recommendation

If/when blob-region-prototype gains curated reseed (roadmap DEC-007, not yet implemented for this scene), that is the natural point to introduce `EffectKnowledgeBase`-informed selection (e.g. weighting `kEffectChoices` by imported whitelist entries, avoiding blacklisted ones) — at that point the integration shape would closely resemble what this session built for `temporal-fields`, since both would be choosing among the same canonical id space via the same shared catalog. Not implemented here.
