# Architecture Acceptance — First Two-Scene Milestone (Blob ↔ Temporal)

**Owner:** Architecture & Program Coordination
**Status:** TECHNICAL ACCEPTANCE GRANTED · DURABLE MILESTONE PENDING
**Date recorded:** 2026-10-06
**Recorded by:** Repository / project-artifact closure session (records the grant; makes no acceptance decision of its own)

> **Provenance.** Architecture relayed this grant in the 2026-10-06 durability-closure session. Before this file, it existed only in chat. No copy existed in the repository or `~/Downloads`. The decision block below reproduces that text verbatim. Every other section is reconciliation against in-repo evidence, not new Architecture content.

## Architecture decision

```text
FIRST-TWO-SCENE TECHNICAL ACCEPTANCE: GRANTED

RT-003: ACCEPTED
TEMP-004: VERIFIED — FOCUSED + INTEGRATED
RT-002: ACCEPTED
FX-004: VERIFIED

CONTRACT IMPACT: NONE
DURABLE MILESTONE: PENDING
```

### Remainder of the grant (second relay, 2026-10-06)

The first relay was truncated after the decision block above. The full text was relayed later in the same durability-closure work. Its grant content is reproduced verbatim here; the closure instructions that followed it are not reproduced.

> Your job is to make that accepted state durably reproducible and correctly represented by the repository and Work Registry.
>
> Authoritative technical result
> The accepted RT-002 rerun completed:

```text
20 / 20 Blob → Temporal → Blob cycles
115,726 checks
0 failures
```

> The accepted runtime includes:
> - real Blob + Temporal in-process SceneManager switching;
> - production NextScene / PreviousScene routing;
> - lifecycle ownership and inactive-scene isolation;
> - activation-time capability replacement;
> - static outgoing-frame transitions;
> - runtime-owned scene FBO behavior;
> - one SceneHudStatus pull per live frame;
> - one immutable HudFrameData per presented runtime frame;
> - one production HUD presentation path;
> - Blob canonical effects = nullopt;
> - Temporal canonical EffectActivityStatus forwarding;
> - canonical Shared Video continuity and live selection following;
> - TEMP-004 stale-playhead invalidation;
> - real Temporal GL/FBO handoff;
> - real mid-TFFragmentTransition observation;
> - bounded desktop resources;
> - clean runtime shutdown;
> - natural canonical authored Shared Effects preset observation.
>
> No frozen shared contract changed.
> Do not reopen or redesign any of these systems.

Registry states Architecture specified in the same relay, now applied in the [Work Registry](03-work-registry.md):

```text
RT-003   Status: ACCEPTED · Domain Acceptance: PASS · Architecture Acceptance: PASS
TEMP-004 Status: VERIFIED · Focused Verification: PASS · Integration Verification: PASS · Architecture Review: NOT REQUIRED
RT-002   Status: ACCEPTED · Desktop Verification: PASS · Integration Verification: PASS · Architecture Acceptance: PASS
         Pi Verification: NOT REQUIRED FOR THIS DESKTOP ACCEPTANCE
FX-004   Status: VERIFIED (evidence: RT-002 clean acceptance; TFEffectPicker naturally selected
         preset.dither.20260809_172713_1 and HudFrameData.effects became present-active)
```

Governing rulings that previously existed only in chat are recorded in [Architecture-Rulings-Relay-First-Two-Scene-Milestone.md](Architecture-Rulings-Relay-First-Two-Scene-Milestone.md).

## What was accepted

The first real two-scene sequence runs under the production stack:

```text
Blob → Temporal → Blob   (20 cycles, in-process, production InputRouter path)
```

| Item | Decision | Evidence |
|---|---|---|
| RT-003 — in-process SceneManager switching for Blob + Temporal | ACCEPTED | [RT-003 report](../reviews/rt-003-blob-temporal-scenemanager-switching-report.md): controller tests 127/127; SceneSwitchHarness 3 cycles 5,351/5,351; all suites green; `docs/reviews/logs/rt003-*.log` |
| TEMP-004 — Temporal stale-playhead invalidation | VERIFIED (focused + integrated) | Focused: [TEMP-004 report](../reviews/temp-004-temporal-stale-playhead-fix-report.md), 139/139, 0 stale frames. Integrated: [RT-002 rerun](../reviews/rt-002-rerun-after-temp004-report.md), 0 stale playhead presentations across 20 Temporal activations |
| RT-002 — Blob ↔ Temporal 20-cycle production switching acceptance | ACCEPTED | [RT-002 rerun](../reviews/rt-002-rerun-after-temp004-report.md) clean run B: 20/20 cycles, 115,726 checks, 0 failures, harness unchanged since original run. Original FAIL record kept as history: [RT-002 report](../reviews/rt-002-blob-temporal-20-cycle-acceptance-report.md) |
| FX-004 — live authored-preset observation in Temporal | VERIFIED | RT-002 rerun: `preset.dither.20260809_172713_1` naturally selected, applied and rendered under ExperienceRuntime |

**Contract impact:** none. All three reports state `CONTRACT IMPACT: NONE`, and the RT-003 report confirms that `git diff` is empty for the frozen shared directories.

## What this acceptance does not cover

- **Pi behaviour.** No item here has Pi evidence (PI-001/PI-002 remain open).
- **Durability.** `DURABLE MILESTONE: PENDING`. The milestone code and evidence are committed on `blueprint-emergence-updates` (`59c24f4`, `597ee65`), but at the time of recording those commits are **not pushed to `origin`**. Under the registry's interim durability default, the milestone is not durable until they are.
- **Carried-forward risks** are non-blocking and recorded in the [Work Registry](03-work-registry.md):
  - Temporal `shutdown()` releases nothing (TEMP-005 proposed);
  - `fragment-trail`'s buffer fork likely has the same stale-playhead pattern (FRAG-001);
  - HUD scene-switch controls stay hidden: no scene-count signal, so a contract proposal would be needed;
  - no recovery from the `Failed` transition phase;
  - the acceptance harness does not lock out real input.

## Conditions for `DURABLE MILESTONE` → complete

1. This record, the reconciled Work Registry, and the closure report are committed.
2. `blueprint-emergence-updates` is pushed to `origin`, and `git status -sb` shows no `ahead`.
3. Architecture then moves RT-003 and RT-002 to `CLOSED` (BUILD-001 has not decided an integration branch, so `MERGED` is not required).
