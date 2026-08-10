# Architecture Authorization — Temporal as Production Scene #2

**Owner:** Architecture & Program Coordination  
**Status:** AUTHORIZED

## Basis

Architecture has now received and accepted the two final planning prerequisites:

1. **Temporal Production Migration / Two-Scene Runtime Acceptance Requirements**
2. **Blob → Temporal → Blob HUD Acceptance Matrix**

The previously accepted technical prerequisites also remain in place:

- Blob accepted as production scene #1;
- Shared Video Temporal specialized adapter seam accepted;
- `VideoPlaybackService::currentAbsolutePath() const` accepted as an additive canonical read-only API;
- Temporal media cadence owned by canonical Shared Video timing;
- `TFEffectPicker` accepted as Temporal's canonical production effect-activity producer;
- Shared Effects Selector Increment 2 accepted with only a non-blocking live authored-preset observation pending;
- ExperienceRuntime infrastructure accepted;
- HUD Runtime infrastructure accepted.

## Architecture decision

```text
TEMPORAL FULL PRODUCTION MIGRATION:
AUTHORIZED
```

Temporal is now the second production scene candidate.

## Migration target

```text
Temporal
→ IEcopunkScene
→ SceneManager
→ ExperienceRuntime-owned scene FBO
→ SceneFrame
→ HudFrameData
→ production HUD
```

The migration must also prepare for the first real two-scene sequence:

```text
Blob
→ Temporal
→ Blob
```

## Non-blocking follow-up carried into migration

The Shared Effects live authored-preset observation remains desirable:

```text
Temporal picker
→ naturally selects canonical authored preset
→ applies it
→ renders it live
```

This should be captured during migration/acceptance if practical, but it is not a migration gate.

## Architecture stop conditions

Stop and return to Architecture if Temporal requires changing:

- `IEcopunkScene`;
- lifecycle semantics;
- `SceneFrame`;
- `HudFrameData`;
- SceneManager ownership;
- command ownership;
- canonical video ownership;
- canonical effect ownership/status;
- frozen semantic slot IDs;
- canonical HUD geometry;
- production HUD ownership;
- the accepted dual-decoder Shared Video boundary.

No scene-specific branch may be added inside shared HUD rendering.

## Next acceptance milestone

Temporal migration is not complete until the coding-agent return proves:

1. real Temporal production launch;
2. real lifecycle/reactivation;
3. canonical video behavior under ExperienceRuntime;
4. canonical `TFEffectPicker` activity transport;
5. honest Temporal semantic mapping;
6. legacy Temporal HUD/debug presentation suppressed in production;
7. no camera path;
8. GL/FBO isolation;
9. no shared-contract drift;
10. readiness for the full 20-cycle Blob ↔ Temporal switching proof.

After Temporal itself is accepted, Architecture will authorize/execute the repeated two-scene switching acceptance milestone.
