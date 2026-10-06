# Ecopunk Runtime — Project Chat Index

## Required project-manager chats

1. Architecture and Program Coordination
2. ExperienceRuntime and SceneManager
3. HUD Runtime and Validation Studio
4. Shared Video Playback
5. Shared Effects and Shader Debugger
6. Raspberry Pi Runtime and Performance
7. HUD Art Pipeline and Skin Production
8. Blob Scene Migration
9. Temporal Fields Scene Migration
10. Blueprint Emergence Scene Migration
11. Contour Portrait Scene Migration
12. Fragment Trail Scene Migration
13. Quadrant Crosshair Scene Migration

## Recommended creation order

Create immediately:

- Architecture
- HUD Runtime
- ExperienceRuntime
- Shared Video
- Shared Effects
- Pi Runtime
- HUD Art Pipeline

Create scene-migration chats when their prerequisites approach readiness:

- Blob and Temporal first
- Blueprint and Contour second
- Fragment and Quadrant after shared-crosshair/fork prerequisites are resolved

## Chat naming convention

`Ecopunk — <Domain Name>`

## Every chat should begin with

- its domain startup prompt,
- the shared project context ([00](00-shared-project-context.md)),
- Architecture Governance ([01](01-architecture-governance.md)),
- Cross-Domain Handoff Protocol ([02](02-cross-domain-handoff-protocol.md)),
- the Canonical Work Registry ([03](03-work-registry.md)),
- the Master Roadmap,
- relevant frozen and domain-specific source documents.

The Work Registry must be read before assuming any roadmap status. The roadmap describes planned sequence, not current completion.

A chat resuming after inactivity follows the return-from-hiatus rule in [Architecture Governance](01-architecture-governance.md#return-from-hiatus): reconcile a restart status from the registry, accepted evidence, and the repository before generating implementation prompts. The last generated prompt is never proof of current state.
