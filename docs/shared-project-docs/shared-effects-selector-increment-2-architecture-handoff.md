# Shared Effects — Production Selector / Eligibility Increment 2 Architecture Handoff

**Source domain:** Shared Effects and Shader Debugger  
**Target domain:** Architecture and Program Coordination  
**Handoff type:** Acceptance recommendation / non-blocking follow-up  
**Status:** Increment accepted for selector-policy progress; one runtime observation still pending

## Decision / finding

Shared Effects Production Selector / Eligibility Increment 2 successfully closes the strongest remaining selector-safety gap under the frozen Shared Effect Knowledge v1 contract:

```text
canonical blocked preset
→ excluded before eligibility pool
→ cannot be automatically selected
```

The increment also proves the canonical production data path:

```text
shader-effect-debugger
→ canonical authored knowledge pack
→ canonical sync
→ temporal-fields runtime import
```

No frozen schema, effect ID, preset-ID rule, HUD contract, ExperienceRuntime contract, or Blob implementation was changed.

## What Architecture may accept now

Architecture may accept the following as implemented and proven:

1. canonical preset compatibility precedence is consumed by Temporal;
2. Unclassified presets remain production-ineligible;
3. explicitly incompatible presets are production-ineligible;
4. canonical blocked presets are now hard-excluded before candidate selection;
5. favored/whitelist membership cannot override blocking;
6. stable `presetId` is preserved and used for logging/reference;
7. a real, non-test `temporal-fields`-compatible preset was authored through the real debugger path;
8. the canonical pack was exported to the approved authored root;
9. the canonical sync script delivered the pack byte-identically to Temporal;
10. the real Temporal production picker imported that canonical-derived pack;
11. Shared Effect Knowledge v1 remained unchanged.

## Important remaining gap

The short live Temporal run did **not** happen to select the authored `dither` preset through the scene's actual background picker.

What is proven:

```text
real authored preset exists
+ real scene imports it
+ real TFEffectPicker code path applies structurally identical eligible presets correctly
```

What is not yet directly observed:

```text
the live scene's own picker
→ happens to select dither
→ applies the real authored preset
→ renders it on screen
```

This is a runtime-observation gap, not a schema or selector-policy gap.

## Recommended Architecture disposition

**Accept with minor non-blocking runtime proof follow-up.**

Do not require another broad Shared Effects engineering session.

A narrow validation run is sufficient:

- run Temporal long enough to naturally select the authored effect; or
- temporarily bias the existing effect weights toward that effect for one diagnostic-only run, then revert;
- capture a real log showing the scene's own picker applies the real canonical preset;
- confirm the rendered path uses the authored snapshot;
- make no permanent selector-policy change solely to force the proof.

## Production selector status after Increment 2

```text
preset compatibility filtering        = implemented
Unclassified exclusion                = implemented
blocked canonical preset exclusion    = implemented
favored/whitelist preference          = implemented at current whitelist level
numeric preset weighting              = not present in frozen schema
Pi filtering                          = not active in Temporal; correctly not invented
recent-history/cooldown               = not implemented; no ready shared policy exists
seed/reseed deterministic sequence    = not implemented; no existing Temporal seed path exists
stable preset reference/logging       = implemented
real canonical authored-pack import   = proven
real live authored-preset render      = not yet directly observed
```

## Architecture implications

No schema review is required. No Blob change is implied. No ExperienceRuntime or HUD transport change is implied. No Pi feasibility claim is made.

## Risks to retain

1. One real live application observation remains desirable.
2. Authored compatible knowledge is still sparse.
3. Temporal's coarse effect-name selection remains scene-local; canonical eligibility currently governs preset application, not the entire effect-name candidate set.
4. Recent-history/cooldown remains future production-policy work.
5. Pi filtering remains inactive until a real policy exists.
6. The fallback randomized path still uses the legacy bounded blacklist retry.

## Required Architecture action

Record:

```text
Shared Effects selector Increment 2
= ACCEPTED WITH NON-BLOCKING LIVE-RUNTIME PROOF FOLLOW-UP
```

Do not reopen DEC-016.
Do not delay Blob's narrow migration work.
Do not require another Shared Effects schema session.

## Acceptance evidence

- Shared Effects tests: `93/93`
- Temporal eligibility self-test: `19/19`
- Temporal activity self-test: `25/25`
- Debugger knowledge-pack self-test: `60/60`
- Video-effect drift checks: clean
- Debugger Release build: successful
- Temporal Release build: successful
- Real canonical pack written
- Real sync into Temporal
- Real Temporal runtime import confirmed

## Final recommendation

**ARCHITECTURE RECOMMENDATION: ACCEPT SELECTOR INCREMENT 2 WITH NON-BLOCKING LIVE-RUNTIME PROOF FOLLOW-UP**
