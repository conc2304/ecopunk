# Ecopunk Runtime — Architecture Governance

## Purpose

Protect shared architecture while allowing multiple domain-manager chats and coding agents to work in parallel.

## Decision classes

### Frozen shared decisions

Require Architecture review before changing:

- runtime scene scope,
- `IEcopunkScene`,
- `SceneFrame`,
- render/FBO ownership,
- scene lifecycle semantics,
- command ownership,
- semantic slot IDs,
- canonical HUD layout once frozen,
- skin package schema once frozen,
- canonical media-root policy,
- effect-knowledge schema once frozen.

### Domain-owned decisions

May be made within a domain if they do not alter shared contracts:

- private implementation details,
- internal helper classes,
- test structure,
- scene-local adapters,
- internal caching,
- logging details,
- domain-specific file organization.

### Experimental decisions

May be explored behind a prototype or branch but cannot become shared dependencies without review.

## Domain-manager responsibilities

Every domain-manager chat must:

1. Keep its scope narrow.
2. Ground claims in code or approved project docs.
3. Distinguish facts, recommendations, and unresolved questions.
4. Avoid duplicating ownership already assigned elsewhere.
5. Produce coding-agent prompts with explicit boundaries.
6. Require coding agents to report deviations before changing shared APIs.
7. Maintain a short domain decision log.
8. Send cross-domain questions through a written handoff.
9. Read the [Work Registry](03-work-registry.md) before assuming any work status, and close meaningful sessions with the [session-close report](02-cross-domain-handoff-protocol.md#session-close-report).

## Architecture review triggers

A proposal requires Architecture review if it:

- adds or changes a shared public type,
- changes render ownership,
- changes lifecycle,
- changes scene/HUD status shape,
- changes command semantics,
- introduces a new runtime service,
- changes canonical asset roots,
- creates a scene-specific branch inside shared HUD rendering,
- creates skin-specific C++ logic,
- creates a second source of truth for video or effects,
- changes Pi quality targets.

## Contract-change process

1. Domain manager writes a change proposal.
2. Proposal includes motivation, current limitation, alternatives, migration cost, and affected domains.
3. Architecture chat reviews.
4. Approved changes update the shared source document/header.
5. Dependent domain managers receive a handoff.
6. Coding work begins only after the artifact is updated.

## Review checkpoints

Mandatory reviews after:

- semantic slot model,
- shared video-service design,
- shared effect-knowledge design,
- ExperienceRuntime skeleton,
- wireframe HUD,
- first real scene,
- first two-scene switch,
- Pi baseline,
- HUD Blueprint,
- Skin Package Spec,
- Dark Moss skin,
- art-production pipeline.

## Project state and Work Registry

> **Plans describe intent. Reports describe evidence. The Work Registry describes accepted current reality.**

The [Canonical Work Registry](03-work-registry.md) is the single authoritative record of current work state, gates, dependencies, evidence, and source-control durability. It defines the lifecycle, status vocabulary, gate values, Source State values, and update template; this section defines who owns project truth and how conflicts resolve.

**Status is evidence-derived, never conversation-derived.** Absence of an acceptance/evidence artifact means absence of proof. Never infer completion from elapsed time, authorization, or a generated prompt.

### Ownership of project truth

```text
Coding agents
→ implementation + test/runtime evidence

Domain managers
→ interpret evidence + recommend status transitions

Architecture & Program Coordination
→ approves Architecture-gated transitions

Canonical Work Registry
→ records accepted current project reality
```

- Coding agents must not declare Architecture acceptance. They may propose `IMPLEMENTED` / `VERIFIED` with evidence; they edit the registry only to apply an approved Registry Update.
- Domain managers must not mark implementation complete merely because they generated a prompt. They may approve `ACCEPTED` for work without an Architecture gate.
- Architecture approves every transition of Architecture-gated work to `AUTHORIZED`, `ACCEPTED`, or `CLOSED`.

### Precedence for current-state reconciliation

When artifacts disagree:

```text
frozen shared contract
→ later Architecture decision
→ accepted evidence
→ Work Registry
→ roadmap / planning documents
```

1. Frozen/shared contracts win over proposals.
2. Later Architecture decisions win over older planning artifacts.
3. Accepted evidence wins over stale roadmap checkboxes.
4. The Work Registry must then be reconciled.

Update the registry when accepted evidence proves it stale. Do not rewrite historical reports to match current state.

### Return from hiatus

If a domain has been inactive long enough that repository or dependency state may have changed, do not resume from the last generated prompt. Instead:

1. read the Work Registry;
2. read the latest accepted evidence;
3. read relevant frozen contracts / Architecture decisions;
4. inspect current repository state where necessary;
5. reconcile a restart status before implementation.

**The last generated prompt is never proof of current state.**
