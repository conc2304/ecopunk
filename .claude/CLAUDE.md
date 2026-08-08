# Ecopunk Runtime — Claude Code Instructions

## Mission

Work as a safety-first autonomous engineering agent. Complete routine repository inspection, implementation, builds, tests, and verification with minimal interruption.

## Default workflow

1. Read the task and relevant project documents.
2. Inspect before editing.
3. Establish the smallest safe implementation plan.
4. Make only task-scoped changes.
5. Build and test the narrowest affected target first.
6. Inspect `git diff` and `git status`.
7. Report evidence, deviations, risks, and the next step.

Continue autonomously through routine searches, loops, pipelines, builds, tests, harnesses, and corrective iterations. Do not ask for approval merely because a command is compound.

## Absolute prohibitions

Never commit, push, stage, merge, rebase, tag, stash, reset, clean, switch branches, or otherwise mutate Git history or the Git index. Read-only Git commands are allowed.

Never deploy, use SSH/SCP/rsync, change system services, use privilege escalation, install packages, or download and execute remote code.

Never silently alter a frozen shared contract. Stop at a written change proposal when a contract change is required.

## Project architecture

The approved top-level model is:

```text
ExperienceRuntime
├── SceneManager
├── HudCompositor
├── InputRouter
└── RuntimeServices
```

Preserve ownership boundaries. Avoid duplicate video/effect systems, scene-specific branches in shared HUD rendering, skin-specific C++ logic, and raw `ofxGui` exposure in the runtime HUD.

## Completion report

Every implementation report must include:

1. Summary
2. Files inspected
3. Files changed
4. Tests/builds run
5. Results
6. Deviations from prompt
7. Newly discovered risks
8. Contract changes requested
9. Recommended next step
