# Ecopunk Runtime — Cross-Domain Handoff Protocol

## Purpose

Make collaboration artifact-driven rather than relying on chats sharing implicit context.

## Handoff template

Every cross-domain handoff should contain:

### 1. Source domain
The domain producing the handoff.

### 2. Target domain
The domain expected to act.

### 3. Decision or finding
A concise statement of what changed or was discovered.

### 4. Evidence
Code paths, probe findings, test results, or approved project documents.

### 5. Required action
What the receiving domain must do.

### 6. Contract impact
- None
- Proposed additive change
- Breaking change
- Requires Architecture review

### 7. Dependencies
What must happen first.

### 8. Acceptance criteria
How the receiving domain knows the handoff is complete.

## Coding-agent report format

Coding agents must return:

1. Summary
2. Files inspected
3. Files changed
4. Tests/builds run
5. Results
6. Deviations from prompt
7. Newly discovered risks
8. Contract changes requested
9. Recommended next step

followed by the session-close report below.

## Session-close report

Every meaningful engineering or domain-manager session ends with this block. It augments the coding-agent report above and any handoff; it does not replace them. One block per affected [Work Registry](03-work-registry.md) item.

```text
WORK ITEM:
PREVIOUS STATUS:
NEW STATUS:

WHAT CHANGED:
WHAT WAS PROVEN:
WHAT WAS NOT PROVEN:

NEW EVIDENCE:
NEW RISKS:
NEW BLOCKERS:

NEXT ACTION:
NEXT GATE:

REGISTRY UPDATE REQUIRED: YES / NO
ROADMAP UPDATE REQUIRED: YES / NO
DECISION LOG UPDATE REQUIRED: YES / NO
ARCHITECTURE HANDOFF REQUIRED: YES / NO
```

- `NEW STATUS` follows the registry's evidence-derived rules; a coding agent never reports `ACCEPTED` or `CLOSED`.
- `REGISTRY UPDATE REQUIRED: YES` whenever the status, any gate value, the Source State, dependencies, blockers, or the evidence list of an item changes, or a new item is discovered. Attach a completed Registry Update (template in the registry).
- `ROADMAP UPDATE REQUIRED` is `YES` only when planned sequence or milestone scope changes — never for routine status progress.
- `DECISION LOG UPDATE REQUIRED` is `YES` only for a new or changed approved project-level decision.
- `ARCHITECTURE HANDOFF REQUIRED` is `YES` when the item is Architecture-gated and ready for a gate, or when an Architecture review trigger was hit.

## Escalation rule

If an agent discovers that the task cannot be completed without changing a shared contract, it must stop at a proposal and not silently implement the change.
