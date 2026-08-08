# Shared Effect Knowledge — Scoped Extension

**Status:** Implemented, **not yet Architecture-reviewed.** Per
`docs/shared-project-docs/01-architecture-governance.md`'s "Review
checkpoints" ("Mandatory reviews after: ... shared effect-knowledge
design...") and "Frozen shared decisions" ("effect-knowledge schema once
frozen"), this document and the code it describes should be treated as a
**proposal** until an Architecture reviewer signs off — nothing here has
been frozen, and nothing frozen (`SceneContract.h`, `SceneSemanticTypes.h`)
was touched to build it.

## Why "scoped extension," not a new subsystem

An engineering brief for this workstream initially described a "Shared
Effect Knowledge subsystem" as if it needed to be built from nothing:
canonical effect IDs, an `EffectInstance` model, a knowledge schema,
selection weights, Pi-safe flags, a debugger export format. Direct
inspection of `shared/src/video-effects/` before writing any code found
that almost all of this already exists and is in active use:

| Brief's "required deliverable" | Already exists as |
|---|---|
| Canonical effect identifier system | `VideoEffectDefinition::id`, populated for 26 effects in `catalog/DefaultVideoEffectCatalog.cpp` |
| Structured `EffectInstance` model | `core/VideoEffectInstance.h` |
| Knowledge schema (favored/blocked, weights) | `knowledge/EffectKnowledgeBase.h` (whitelist/blacklist + `qualityScore`/`perfObservedFps`), `knowledge/EffectRandomizer.h` (weighted/biased selection) |
| Transition-phase model | `evolution/EffectEvolutionController.h`'s `EvolutionPhase` (`Transitioning`/`Holding`) |
| Preset format | `KnowledgeEntry`'s parameter-snapshot shape |

`CLAUDE.md` for this repo says directly: *"Do not write a new... registry...
Recreating it is exactly the mistake this system exists to prevent."*
Building a second, parallel system would have been exactly that mistake.
This document instead scopes the real gap: everything in
`EffectKnowledgeBase` is **per-sketch by design** (its own header comment:
"no cross-sketch sharing"), so there was no scene-compatibility matrix, no
HUD-facing activity type, no dominance rule for multi-slot scenes (e.g.
`quadrant-crosshair`'s four quadrants), and no way for the debugger's
curated knowledge to reach a production scene without hand-editing JSON.

## What this pass added

All additive, all under `shared/src/video-effects/knowledge/`, none of it
touching `SceneContract.h`, `SceneSemanticTypes.h`, `RuntimeServices`
ownership, or the debugger's role as the authoring tool:

1. **`KnowledgeEntry`** gained two optional fields — `piSafe` and
   `compatibleSceneIds`. Both are additive; any file written before this
   change parses identically (absent = "unknown/unclassified," never
   coerced to `false`/empty-means-no).
2. **`EffectSceneCompatibility.{h,cpp}`** — a static table of the six
   roadmap scenes' relationship to this service
   (`SharedServiceConsumer`/`LocalForkOnly`/`NotIntegrated`), transcribed
   from `CLAUDE.md`'s own migration table and
   `docs/video-effect-second-wave-evaluation.md`. Every entry carries an
   `evidence` string citing its source document — nothing in the table was
   invented. One entry (`blob-region-prototype`) is flagged rather than
   silently resolved: `CLAUDE.md` documents it as migrated, but this pass's
   own grep of its `src/` found no direct call into the service. Left as
   documented, flagged as unconfirmed — see "Open questions" below.
3. **`EffectActivityStatus.{h,cpp}`** — lets a scene turn live effect-slot
   state into the short label list `SceneHudStatus::activeEffects` already
   expects (that field's type — `std::vector<std::string>` — did not
   change). `resolveDominantEffectLabels()` implements the
   dominance-resolution rules:
   - drop slots below a configurable prominence floor,
   - group by effect id (same effect across multiple slots collapses to one
     label — group prominence is the *max*, not the sum, of member slots,
     so running one effect in four quadrants doesn't out-rank one
     dramatically prominent different effect),
   - sort by prominence descending, tie-break by smallest `slotId`
     (deterministic — no reliance on map iteration order),
   - cap to `maxLabels` (default 2),
   - annotate a group as "shifting" if any member slot is
     `EvolutionPhase::Transitioning`.

   No shader parameter of any kind crosses this function's boundary — only
   a caller-supplied `displayName` string.
4. **`EffectKnowledgePack.{h,cpp}`** — bundles several effects' whitelist/
   blacklist entries into one file (`exportEffectKnowledgePack`) and
   idempotently imports one back into any `EffectKnowledgeBase`
   (`importEffectKnowledgePack`, reusing `EffectKnowledgeBase`'s own
   duplicate check — safe to call on every scene startup). `shader-effect-debugger`
   gained a `[k]` key bound to `exportKnowledgePack()`, which writes every
   known effect's accumulated knowledge to
   `bin/data/shared-video-effects/knowledge/effect-knowledge-pack.json` —
   the same synced-assets directory `scripts/sync-video-effect-assets.py`
   already populates for shader files, chosen for naming consistency, not
   because this pass modified that script (it did not — see "Proposed
   follow-ups").
5. **`EffectKnowledgeSerialization.{h,cpp}`** — a small internal refactor:
   the JSON↔`KnowledgeEntry` mapping was duplicated between what would have
   become `EffectKnowledgeBase.cpp` and `EffectKnowledgePack.cpp`; it's now
   one function pair both call, so the two file formats cannot silently
   drift apart. `EffectKnowledgeBase.cpp`'s own behavior is unchanged —
   this is a pure extraction, verified by identical JSON output.
6. **`test/`** — a standalone, OF-free unit-test binary (following the
   `sketches/experience_runtime/test/` pattern) covering dominance
   resolution and compatibility-matrix lookups: 40 checks, all passing.
   Verified this pass that `EffectActivityStatus.*`/`EffectSceneCompatibility.*`
   compile and link with nothing but a bare `c++17` compiler + glm — no
   openFrameworks headers anywhere in that chain.

## Pi-safety

No canonical effect or scene has been measured on real Pi 3B hardware yet
(`Scene-HUD-Contract-v1.md` §16, restated directly: "nothing in this
contract has been measured on real Pi 3B hardware"). `KnowledgeEntry::piSafe`
exists as a place to *record* a judgment once one is made (by a human
reviewing `perfObservedFps` against a Pi run, or by future automated
profiling) — this pass does not set it to `true` anywhere, and
`EffectSceneCompatibility`'s `piValidated` field is `false` for all six
scenes, enforced by its own unit test
(`test_no_scene_reports_pi_validated_yet`). Treat both fields as inputs
future Pi-validation work (roadmap Phase 9) will populate, not something
this pass could honestly claim now.

## Debugger export/import format

```jsonc
{
  "schemaVersion": 1,
  "sourceTool": "shader-effect-debugger",
  "exportedAtUtc": "2026-...Z",
  "whitelist": [ /* KnowledgeEntry[], same shape EffectKnowledgeBase already writes, plus optional piSafe/compatibleSceneIds */ ],
  "blacklist": [ /* same */ ]
}
```

A pack is a full snapshot (re-exporting overwrites), not an append log.
Importing is additive and duplicate-checked against the target
`EffectKnowledgeBase`'s existing entries, so re-importing the same pack
(e.g. on every scene `setup()`) is a safe no-op after the first time.

## Migration risks

- **No scene currently calls `importEffectKnowledgePack()`.** This pass
  built the primitive and the debugger-side export action; wiring an
  import call into any of the four `SharedServiceConsumer` scenes' `setup()`
  is unstarted. Until that happens, exporting a pack from the debugger has
  no observable effect on any production scene.
- **The pack's physical location is a convention, not a contract.** It
  currently lives under each sketch's own `bin/data/shared-video-effects/`
  the same way synced shader assets do, but nothing keeps that copy
  identical across sketches the way `scripts/sync-video-effect-assets.py`
  does for shaders — a debugger export on one machine only lands in the
  debugger's own `bin/data/`, not in every consuming sketch's. Promoting
  the canonical copy to `shared/assets/video-effects/knowledge/` and
  syncing it out (mirroring the shader-asset flow) is the natural next
  step and is *not* done here — this pass did not modify
  `scripts/sync-video-effect-assets.py`, since that's real build tooling
  this pass did not fully inspect and did not want to guess at breaking.
- **`blob-region-prototype`'s "migrated" status is unconfirmed by direct
  code inspection this pass** (see `EffectSceneCompatibility.cpp`'s own
  comment on that entry). If it turns out not to actually consume the
  shared service, its compatibility entry needs correcting.

## Proposed contract changes

**None.** `SceneHudStatus`, `SceneServices`, and every other type in
`shared/src/scene/SceneContract.h`/`SceneSemanticTypes.h` are unchanged.
`SceneServices::sharedEffectAssetRoot` already anticipates exactly the kind
of shared, cross-scene asset path a promoted knowledge pack would need —
no new field was required to express that.

## Open questions / recommended next steps

1. Confirm (or correct) `blob-region-prototype`'s compatibility entry with a
   direct read of its source, not just `CLAUDE.md`'s table.
2. Wire `importEffectKnowledgePack()` into one real scene's `setup()`
   (`SceneServices::sharedEffectAssetRoot` is the natural path source) as a
   proof of the round trip.
3. Decide whether the knowledge pack becomes a synced asset via
   `scripts/sync-video-effect-assets.py`, or stays a manually-promoted file
   — and update that script accordingly, in its own reviewed change.
4. Once real Pi 3B measurements exist (roadmap Phase 9), start populating
   `KnowledgeEntry::piSafe` and `EffectSceneCompatibilityEntry::piValidated`
   from actual data rather than leaving both at their honest "not yet"
   defaults.
5. Architecture review of this document, per the governance doc's mandatory
   checkpoint, before treating any of the above as frozen.
