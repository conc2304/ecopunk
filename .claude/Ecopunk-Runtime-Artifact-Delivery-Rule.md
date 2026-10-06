# Ecopunk Runtime — Artifact Delivery Rule

## Purpose

Ensure that all substantial cross-chat and cross-agent work products are easy to preserve, share, review, and reuse.

## Rule

Any non-trivial artifact intended for another domain chat, coding agent, reviewer, or future project phase must be delivered as a downloadable Markdown file.

This includes, but is not limited to:

- implementation prompts,
- coding-agent prompts,
- discovery prompts,
- probe instructions,
- investigation reports,
- architecture reviews,
- design reviews,
- surveys,
- questionnaires,
- handoff documents,
- migration plans,
- implementation plans,
- decision proposals,
- acceptance criteria,
- test plans,
- risk assessments,
- status reports,
- specifications,
- checklists,
- domain summaries.

## Inline response requirement

The chat may include a short inline summary, but the complete artifact must be provided as a downloadable `.md` file.

The inline summary must not replace the downloadable artifact.

## Trivial-content exception

A downloadable file is not required for:

- brief confirmations,
- one- or two-sentence answers,
- simple clarifying questions,
- short status updates,
- minor corrections that do not need to be handed to another agent.

When uncertain, create the Markdown file.

## File naming

Use clear, descriptive filenames in kebab-case or title-style filenames.

Examples:

```text
experience-runtime-implementation-prompt.md
shared-video-playback-probe.md
hud-semantic-slot-review.md
blob-scene-migration-plan.md
pi-performance-validation-report.md
```

Avoid generic names such as:

```text
notes.md
prompt.md
report.md
document.md
```

## Artifact quality

Every downloadable artifact should:

- include a descriptive title,
- state its purpose,
- identify its intended recipient or owner,
- reference the authoritative project documents it depends on,
- distinguish facts, decisions, recommendations, and open questions,
- include required outputs and acceptance criteria when applicable,
- preserve the Cross-Domain Handoff Protocol where relevant,
- be complete enough to use without relying on surrounding chat context.

## Project-wide application

This rule applies to:

- Architecture and Program Coordination,
- all domain-manager chats,
- all scene-migration chats,
- all coding-agent prompt generation,
- all review and handoff work.

Domain chats should treat downloadable Markdown artifacts as the default unit of collaboration.

Work Registry updates and reconciliation artifacts are project-state artifacts and should be preserved in repository/project documentation (see `docs/shared-project-docs/03-work-registry.md`).
