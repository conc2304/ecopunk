# Canonical Work Registry & Workspace State Rules — Implementation Report

**Date:** 2026-10-05
**Intended recipient:** Architecture & Program Coordination (acceptance of ARCH-002); all domain managers (new startup/session-close rules)
**Produced by:** Coding agent (documentation/workspace-governance task; no runtime C++ changed)
**Authoritative inputs:** `docs/shared-project-docs/00`–`04`, Decision Log v3, Master Roadmap, Scene/HUD Contract v1, `.claude/Ecopunk-Runtime-Artifact-Delivery-Rule.md`, and the Architecture restart audit (`~/Downloads/Ecopunk-Architecture-Program-Coordination-Domain-Restart-Status-Report.md`, used as a starting hypothesis only)

---

## 1. Summary

The project has a canonical, evidence-derived Work Registry at:

```text
docs/shared-project-docs/03-work-registry.md
```

It contains:

- a Project Frontier block;
- the lifecycle, gate, Source State and evidence rules;
- project-wide and per-domain NOW / NEXT / LATER;
- a Gate Matrix;
- 34 work items with stable IDs;
- a Registry Update template.

Governance, the handoff protocol, the chat index, shared context, the roadmap, the Decision Log, the Artifact Delivery Rule and the agent instruction file now point to it. Each addition is small, and no policy text is duplicated.

The registry was populated from repository evidence. It was not populated from roadmap checkboxes or from the existence of prompts. Repository inspection also resolved the restart audit's main unknown: **the Temporal migration and its asset-sync changes are committed (`d25cc13`, pushed)**.

## 2. Documentation structure found

| Artifact | Path | Authority afterward |
|---|---|---|
| Shared project context | `docs/shared-project-docs/00-shared-project-context.md` | Product scope and principles |
| Architecture Governance | `docs/shared-project-docs/01-architecture-governance.md` | Decision classes, review triggers, **ownership of project truth, precedence, return-from-hiatus** |
| Cross-Domain Handoff Protocol | `docs/shared-project-docs/02-cross-domain-handoff-protocol.md` | Handoff template, coding-agent report, **session-close report** |
| *(slot 03 was unused)* | `docs/shared-project-docs/03-work-registry.md` | **Current work state (new)** |
| Project Chat Index | `docs/shared-project-docs/04-project-chat-index.md` | Domain list and startup reading order |
| Decision Log | `docs/shared-project-docs/Decision-Log-v3.md` | Approved project-level decisions only |
| Master Roadmap | `docs/Ecopunk-HUD-System-Master-Roadmap.md` | Planned sequence only |
| Artifact Delivery Rule | `.claude/Ecopunk-Runtime-Artifact-Delivery-Rule.md` (tracked) | Unchanged rule, plus one sentence |
| Agent instructions | `.claude/CLAUDE.md` (tracked; the agent workflow file); root `CLAUDE.md` (shader/effects guidance, not touched) | Agent workflow |
| Acceptance/review evidence | `docs/reviews/`, `docs/shared-project-docs/*review*`, `docs/*-report.md` | Evidence |

No existing status tracker, `AGENTS.md`, or Markdown validator was found. `docs/blob-region-prototype-shared-catalog-migration-status.md` is a single-increment report, not a tracker.

**Significant finding:** about ten Architecture-level artifacts exist only in `~/Downloads`, outside Git. These are closure reviews, handoffs, the 20-cycle acceptance spec and both restart audits. They are cited in the registry as `OFF-REPO` and are tracked as ARCH-003.

## 3. Files changed

| File | Change | What it owns afterward |
|---|---|---|
| `docs/shared-project-docs/03-work-registry.md` | **New** | Current state, lifecycle and vocabulary, gates, Source State, NOW/NEXT/LATER, items, update template |
| `docs/shared-project-docs/01-architecture-governance.md` | New section "Project state and Work Registry": invariant, evidence rule, ownership of project truth, precedence chain, return-from-hiatus. Domain-manager responsibility #9 added | Policy on who decides state and how conflicts resolve |
| `docs/shared-project-docs/02-cross-domain-handoff-protocol.md` | New "Session-close report" section appended after the coding-agent report format, with rules for when each `UPDATE REQUIRED` flag is YES | Session-close requirements |
| `docs/shared-project-docs/04-project-chat-index.md` | Startup list now includes the Work Registry before the roadmap; registry-before-roadmap and hiatus rules added | Startup reading order |
| `docs/shared-project-docs/00-shared-project-context.md` | Work Registry added to "Required source documents" | — |
| `docs/Ecopunk-HUD-System-Master-Roadmap.md` | Authority notice at top; "project tracker" purpose bullet changed to "planned milestone sequence"; one-line superseded note on §3 "Current State" (section content untouched) | Planned sequence only |
| `docs/shared-project-docs/Decision-Log-v3.md` | One-line role clarification | Decisions only |
| `.claude/Ecopunk-Runtime-Artifact-Delivery-Rule.md` | One appended sentence: registry updates and reconciliation artifacts are project-state artifacts to be preserved in repository documentation. Existing rule unchanged | — |
| `.claude/CLAUDE.md` | Default workflow step 0: read the registry; refers to Governance for the rules. Completion report now ends with the session-close block; agents never report `ACCEPTED`/`CLOSED` | — |
| `docs/reviews/work-registry-protocol-implementation-report.md` | **New** (this report) | — |

All changes are `WORKTREE_ONLY`; nothing was staged or committed.

## 4. Work Registry design

- **Lifecycle:** `BACKLOG → READY → AUTHORIZED → IN_PROGRESS → IMPLEMENTED → VERIFIED → ACCEPTED → CLOSED`, plus `BLOCKED / DEFERRED / SUPERSEDED / CANCELLED`.
  - `UNKNOWN` is allowed in the Status field as an explicit **reconciliation placeholder, not a lifecycle state**. It must name the evidence that would resolve it.
  - Non-Architecture-gated work may skip `AUTHORIZED` once its domain manager approves.
- **Evidence-derived status:** the prompt's event → maximum-status table, stated prominently together with "absence of evidence = absence of proof".
- **IDs:** `<DOMAIN>-<###>`, never reused or renumbered. Prefixes: `ARCH BUILD RT HUD ART VIDEO FX PI BLOB TEMP BLUE CONTOUR FRAG QUAD`.
- **Project Frontier:** current phase, last Architecture-accepted milestone, furthest implemented and verified milestones, next Architecture gate, blocking uncertainty, NOW item.
- **NOW / NEXT / LATER:** one project-wide table and one per-domain table covering all 13 chats in the Chat Index.
- **Gate Matrix:** one table, with seven gates per item and the prompt's gate values. I used a single matrix instead of per-item gate blocks so the 34 entries stay readable.
- **Source State:** the prompt's seven values. `COMMITTED` entries name the branch and whether it is pushed.
  - **Interim durability default** (until BUILD-001 is decided): `COMMITTED` on a branch pushed to `origin`.
- **Off-repo evidence:** may support a status, but sets `Documentation: PENDING` and can never support `CLOSED`.

## 5. Initial project-state population

| ID | Item | Status | Source State |
|---|---|---|---|
| ARCH-001 | Shared architecture & contracts | ACCEPTED | COMMITTED |
| ARCH-002 | This protocol | IMPLEMENTED | WORKTREE_ONLY |
| ARCH-003 | Import off-repo evidence | READY | UNTRACKED |
| ARCH-004 | Reconcile stale shared docs | READY | NOT_APPLICABLE |
| BUILD-001 | Integration branch / merge policy | READY | NOT_APPLICABLE |
| RT-001 | ExperienceRuntime / SceneManager infra | ACCEPTED | COMMITTED |
| RT-002 | Blob ↔ Temporal 20-cycle switching | READY | UNTRACKED |
| HUD-001 | HUD Runtime / wireframe | ACCEPTED | COMMITTED |
| HUD-002 | Temporal pattern vocabulary | VERIFIED | COMMITTED |
| HUD-003 | Temporal profile binding fix | READY | NOT_APPLICABLE |
| HUD-004 | Skin Package Spec / Loader contract | **UNKNOWN** | **UNKNOWN** |
| HUD-005 | Skin Loader implementation | BACKLOG | NOT_APPLICABLE |
| HUD-006 | HUD Blueprint v1 freeze | BLOCKED | NOT_APPLICABLE |
| ART-001 | Dark Moss | BLOCKED | NOT_APPLICABLE |
| ART-002 | Art pipeline | BACKLOG | NOT_APPLICABLE |
| ART-003 | Remaining four skins | BACKLOG | NOT_APPLICABLE |
| VIDEO-001 | VideoPlaybackService | ACCEPTED | COMMITTED |
| VIDEO-002 | Temporal adapter seam | ACCEPTED | COMMITTED |
| VIDEO-003 | Retire media compatibility paths | BACKLOG | NOT_APPLICABLE |
| FX-001 | Effect-activity transport | ACCEPTED | COMMITTED |
| FX-002 | Shared Effect Knowledge v1 | ACCEPTED | COMMITTED |
| FX-003 | Selector increment 2 | ACCEPTED | COMMITTED |
| FX-004 | Live authored-preset observation | READY | NOT_APPLICABLE |
| BLOB-001 | Blob production scene #1 | ACCEPTED | COMMITTED |
| TEMP-001 | Temporal production scene #2 | VERIFIED | COMMITTED |
| TEMP-002 | Temporal asset reproducibility | VERIFIED | COMMITTED |
| TEMP-003 | `TFPresetTimeline` extraction | DEFERRED | NOT_APPLICABLE |
| PI-001 | Pi build / deploy / baseline | READY | NOT_APPLICABLE |
| PI-002 | Temporal dual-decoder on Pi | BLOCKED | NOT_APPLICABLE |
| PI-003 | Pi skin/texture budgets | BLOCKED | NOT_APPLICABLE |
| BLUE-001 | Blueprint migration (scene #3) | BACKLOG | NOT_APPLICABLE |
| CONTOUR-001 | Contour migration | BACKLOG | NOT_APPLICABLE |
| FRAG-001 | Fragment migration | BACKLOG | NOT_APPLICABLE |
| QUAD-001 | Quadrant migration | BACKLOG | NOT_APPLICABLE |

**No item is `CLOSED`.** Every `ACCEPTED` item still has either an off-repo acceptance record (`Documentation: PENDING`) or no designated integration branch (BUILD-001).

**UNKNOWN states:**

- HUD-004: status, implementation, documentation and source state are all unknown. No spec or loader-contract artifact was found anywhere.
- FX-002 Documentation: the "Freeze Specification" cited by the restart audit was not found in the repo or `~/Downloads`.
- HUD-002 Integration Verification: not recorded.

**Pi Verification is `NOT_STARTED` for every item.**

## 6. Reconciliations

| Conflict | Resolution |
|---|---|
| Restart audit: "Temporal asset Git persistence: UNKNOWN / previously pending commit" | **Resolved: COMMITTED.** The asset handoff (`~/Downloads`, written 2026-08-10 09:07) said the files were "not yet committed". Commit `d25cc13` (09:09 the same day, pushed) contains `scripts/sync-temporal-runtime-assets.py`, `sketches/temporal-fields/data/**`, `TemporalProductionScene.*` and `TemporalLifecycleHarness.*`. The working tree is clean, and `sync-temporal-runtime-assets.py --check` passes today. |
| Restart audit: "whether the 20-cycle harness exists/ran" | **Resolved: it does not exist.** Only the Blob and Temporal lifecycle harnesses are present. The spec in `~/Downloads` says "PENDING EXECUTION EVIDENCE". |
| Restart audit: Temporal Architecture acceptance "not safely assumable" | **Kept as VERIFIED, with Architecture Acceptance PENDING.** No acceptance artifact exists. The open condition (source-control persistence) is now satisfied, so TEMP-001 is the NOW item. |
| Master Roadmap §3 lists ExperienceRuntime, HUD and the video service as "Not yet implemented" | Superseded note added. Content not rewritten. Correction tracked under ARCH-004. |
| `blob-scene-migration-…-acceptance-review.md` says "REQUIRES NARROW BLOB PATCH" | Superseded by the narrow-patch PASS report and Architecture's later "Blob accepted" (Temporal authorization). The review itself is unchanged. |
| `shared-effects-canonical-activity-producer-seam-acceptance-review(1).md` says "Requires Architecture ownership decision" | Answered by DEC-015 and the Temporal authorization ("`TFEffectPicker` accepted"). |
| DEC-016 is headed "Approved / Frozen" but its Consequences text describes the freeze as pending | Header wins (the later decision). Tracked under ARCH-004; the Decision Log entry was not edited. |
| `HUD-Semantic-Slot-Model-v1.md` "Architecture draft"; `Scene-HUD-Contract-v1.md` "Ready for freeze" | DEC-009/DEC-012 govern. Header correction tracked under ARCH-004. Contract files not edited. |
| `blueprint_emergence_engineering_plan.md` (2026-06-22): all §11 questions `_pending_`; seek-and-capture | Superseded by DEC-018. Recorded as BLUE-001 evidence and under ARCH-004. |
| Local `main` 14 commits behind `blueprint-emergence-updates`, never pushed | Raised as BUILD-001. Interim durability default defined. |

## 7. Verification performed

- **Registry structure:** an ad-hoc script in the session scratchpad (not added to the repo) parsed all 34 items. Results: **0 duplicate IDs**; every Status, gate value and Source State is in the defined vocabulary; all 10 required fields are present on every item; every `[status]` written next to a dependency matches that item's own status; every referenced ID is defined; the Gate Matrix rows match the items one-to-one. **Result: no errors.**
- **Links:** every relative link and `#anchor` in the six modified docs/ files resolves. The only exception was the link to this report, which now exists.
- **Cited code lines** re-checked: `HudPresentationProfile.cpp:213`, `BESettings.h:395`, harness file list.
- **Existing repo checks** (read-only):
  - `python3 scripts/sync-temporal-runtime-assets.py --check` → both sketches up to date, exit 0.
  - `python3 scripts/check-video-effect-drift.py` → "All video-effect drift checks passed" (11 advisory flags, unchanged).
- **Manual cross-read:**
  - Registry, Governance, Handoff Protocol and Chat Index use the same status vocabulary and precedence chain.
  - Policy text lives in one place each, with links elsewhere: rules in Governance, vocabulary in the Registry, the session-close block in the Handoff Protocol.
- **Roadmap:** the authority notice is the first content under the title. The purpose bullet no longer calls it a tracker.
- **Startup guidance:** the Chat Index, `00` and `.claude/CLAUDE.md` all require reading the registry before the roadmap.
- **Artifact Delivery Rule:** the original text is byte-identical. One sentence was appended.
- **Frozen contracts:** `git status` shows no change to `Scene-HUD-Contract-v1.md`, `HUD-Semantic-Slot-Model-v1.md`, the shared headers, or any code.
- **Not run:** builds and runtime harnesses (out of scope). The "Furthest Verified" milestone rests on the 2026-08-10 report and was not re-executed.

## 8. Deviations

1. **`UNKNOWN` as a Status value.** The prompt asks for UNKNOWN where state is ambiguous but gives no lifecycle slot for it. It is defined as a reconciliation placeholder, not a lifecycle state.
2. **Gate Matrix as one table** instead of a gate block in every item, to keep the registry readable. The gates are the same, as is the value vocabulary.
3. **Interim durability default** (`COMMITTED`, pushed) added, because no integration branch exists. The final rule is left to Architecture (BUILD-001).
4. **Registry Update template lives inside the registry**, not in a separate file, to avoid another document home.
5. **No validator script added.** None existed in the repo. The IDE's markdownlint flags only pre-existing heading-style warnings and does not check registry semantics. A `scripts/check-work-registry.py` is listed as optional future work.

## 9. Risks / remaining work

- **Off-repo evidence (highest documentation risk).** Several ACCEPTED states rest on files in `~/Downloads` (ARCH-003). They could be lost, and an agent working from the repo cannot see them.
- **Temporal acceptance may have happened off-record.** If Architecture accepted TEMP-001 in a chat with no artifact, the registry understates progress until that is recorded.
- **HUD-004 is fully unknown.** HUD/Art managers must locate the Skin Package Spec draft.
- **Pi feasibility is completely unmeasured** (PI-001). This is the largest technical risk; DEC-014 explicitly requires measuring dual-decoder cost before feasibility claims.
- **Source-control durability:** all work lives on one feature branch, and `main` is stale and unpushed (BUILD-001).
- **The desktop verification evidence is 8 weeks old.** Re-running the Temporal lifecycle harness on the current checkout would refresh it.
- **Optional automation:** a small registry checker (duplicate IDs, vocabulary, required fields, matrix and link consistency) modelled on the ad-hoc check used here.

## 10. Contract impact

```text
CONTRACT IMPACT: NONE
```

No runtime, HUD, Shared Video or Shared Effects contract, semantic slot ID, or Pi quality target was touched. No governance conflict required escalation.

## 11. Recommended next step

1. **Architecture accepts the workspace-state protocol (ARCH-002).** Then a coding agent commits these docs together with the off-repo evidence (ARCH-003), so the registry's citations become durable.
2. **Resume the highest-priority technical milestone: TEMP-001 Architecture acceptance.** Its only open condition, source-control persistence, is now proven. That acceptance unlocks authorization of RT-002, the 20-cycle switching proof.
3. In parallel, the Pi domain starts **PI-001**. It has no dependency on RT-002 and is the project's biggest unknown.

---

### Session-close report

```text
WORK ITEM: ARCH-002 — Workspace-state protocol and Canonical Work Registry
PREVIOUS STATUS: (new item)
NEW STATUS: IMPLEMENTED

WHAT CHANGED: Registry created (34 items); governance, handoff protocol, chat index,
  shared context, roadmap, decision log, delivery rule, .claude/CLAUDE.md updated.
WHAT WAS PROVEN: Registry structural consistency (0 errors); links resolve; frozen
  contracts untouched; Temporal assets committed in d25cc13 and in sync (--check).
WHAT WAS NOT PROVEN: Current checkout builds/runs; any Architecture acceptance not
  recorded in an artifact; Skin Package Spec state; any Pi behaviour.

NEW EVIDENCE: d25cc13 contents; git branch state; sync --check output; absence of a
  switching harness.
NEW RISKS: ~10 Architecture artifacts exist only in ~/Downloads.
NEW BLOCKERS: None.

NEXT ACTION: Architecture reviews ARCH-002; then TEMP-001 acceptance.
NEXT GATE: Architecture acceptance of ARCH-002.

REGISTRY UPDATE REQUIRED: NO (initial population is this change)
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: YES
```
