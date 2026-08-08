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
