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

## Escalation rule

If an agent discovers that the task cannot be completed without changing a shared contract, it must stop at a proposal and not silently implement the change.
