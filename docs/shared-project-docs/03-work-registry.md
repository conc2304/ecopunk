# Ecopunk Runtime — Canonical Work Registry

**Owner:** Architecture & Program Coordination (records); all domains (propose updates)
**Purpose:** The authoritative record of *accepted current project reality* — what work exists, what state it is in, what evidence supports that state, what is NOW / NEXT / LATER, what is blocked, and how durable it is in source control.
**Last reconciled:** 2026-10-05 (initial population — see [`../reviews/work-registry-protocol-implementation-report.md`](../reviews/work-registry-protocol-implementation-report.md))

> **Plans describe intent. Reports describe evidence. The Work Registry describes accepted current reality.**
>
> **Status is evidence-derived, never conversation-derived.** Absence of an acceptance/evidence artifact means absence of proof. Never infer completion from elapsed time, authorization, or a generated prompt.

This is not a roadmap. Planned sequence lives in the [Master Roadmap](../Ecopunk-HUD-System-Master-Roadmap.md); approved decisions live in the [Decision Log](Decision-Log-v3.md). Ownership of project truth, conflict precedence, and the return-from-hiatus rule are defined in [Architecture Governance](01-architecture-governance.md#project-state-and-work-registry). Session-close reporting is defined in the [Cross-Domain Handoff Protocol](02-cross-domain-handoff-protocol.md#session-close-report).

---

# Project Frontier

```text
Current Phase:
Roadmap Phase 7 — Integrate the Second Scene and Prove Scene Switching.
Temporal (scene #2) is implemented and desktop-verified; the first real
two-scene switching proof has not been executed.

Last Architecture-Accepted Milestone:
Blob as production scene #1, together with ExperienceRuntime and HUD Runtime
infrastructure (BLOB-001, RT-001, HUD-001).

Furthest Implemented Milestone:
Temporal production scene #2 + Temporal runtime asset reproducibility
(TEMP-001, TEMP-002) — commit d25cc13, 2026-08-10.

Furthest Verified Milestone:
Temporal production scene #2 under ExperienceRuntime on macOS desktop —
295/295 lifecycle/reactivation checks (TEMP-001). Not re-run since 2026-08-10.

Next Architecture Gate:
TEMP-001 Architecture acceptance → then authorization of RT-002
(Blob ↔ Temporal 20-cycle switching acceptance).

Current Blocking Uncertainty:
1. Whether Temporal scene #2 was accepted by Architecture outside the
   repository (no acceptance artifact exists in-repo or in ~/Downloads).
2. Raspberry Pi 3B+ feasibility — nothing has ever been built or measured on
   Pi hardware (PI-001).

Current NOW Item:
TEMP-001 — Architecture acceptance review of Temporal production scene #2.
```

---

# Registry Rules

## Lifecycle

```text
BACKLOG → READY → AUTHORIZED → IN_PROGRESS → IMPLEMENTED → VERIFIED → ACCEPTED → CLOSED

Side/terminal: BLOCKED · DEFERRED · SUPERSEDED · CANCELLED
```

| Status | Meaning |
|---|---|
| `BACKLOG` | Known work, not yet actionable. |
| `READY` | Scope and prerequisites understood enough to begin. |
| `AUTHORIZED` | The required owner / Architecture gate approved implementation. |
| `IN_PROGRESS` | Implementation or evidence gathering underway. |
| `IMPLEMENTED` | Repository change reported present; verification may remain. |
| `VERIFIED` | Required tests / runtime evidence passed. |
| `ACCEPTED` | Responsible domain (or Architecture, for Architecture-gated work) reviewed and accepted the evidence. |
| `CLOSED` | Accepted **and** required documentation / durability / source-control gates complete. |
| `BLOCKED` | Cannot proceed because a named dependency or gate is unmet. |
| `DEFERRED` | Intentionally postponed. |
| `SUPERSEDED` | A later decision made this work obsolete. |
| `CANCELLED` | Intentionally abandoned. |
| `UNKNOWN` | *Reconciliation placeholder, not a lifecycle state.* Current state cannot be determined from available evidence. Must name what evidence would resolve it. |

Work not requiring an Architecture gate may skip `AUTHORIZED` once its domain manager has approved it. No other step may be skipped.

## Evidence-derived status

| Event | Maximum resulting status |
|---|---|
| "We should do X" | `BACKLOG` / `READY` |
| "Architecture authorizes X" | `AUTHORIZED` |
| A coding-agent prompt was generated | **No change** |
| Coding agent reports code changed | `IMPLEMENTED` |
| Required tests / runtime proof pass | `VERIFIED` |
| Domain manager accepts evidence (no Architecture gate) | `ACCEPTED` |
| Architecture accepts milestone (Architecture-gated) | `ACCEPTED` |
| Acceptance + docs + required durability/source-control complete | `CLOSED` |

The last generated prompt is never proof of current state.

## Gates

When one status is too coarse, an item's gates are tracked in the [Gate Matrix](#gate-matrix):

`Implementation` · `Desktop Verification` · `Integration Verification` · `Architecture Acceptance` · `Pi Verification` · `Documentation` · `Source Control`

Allowed values: `NOT_STARTED` · `PENDING` · `PASS` · `FAIL` · `NOT_REQUIRED` · `UNKNOWN`

`Documentation: PENDING` is used when the acceptance or evidence artifact that justifies a status exists only outside the repository (e.g. a chat or `~/Downloads`), or a shared doc still contradicts the accepted state.

## Source State

| Value | Meaning |
|---|---|
| `UNTRACKED` | Exists only outside Git (local files, `~/Downloads`, chat). |
| `WORKTREE_ONLY` | In the working tree but not committed. |
| `COMMITTED` | In a commit on a branch. State the branch and whether it is pushed. |
| `MERGED` | Merged into the designated integration branch. |
| `RELEASED` | Part of a deployed / tagged release. |
| `NOT_APPLICABLE` | No repository artifact is involved. |
| `UNKNOWN` | Not determinable from evidence. |

- `IMPLEMENTED` does not imply `COMMITTED`; `VERIFIED` does not imply `MERGED`.
- Architecture must not treat required work as durable while it is `UNTRACKED` or `WORKTREE_ONLY`.
- An item may state the durability its dependents require (`Required Durability:`). **Interim default** until BUILD-001 is decided: `COMMITTED` on a branch pushed to `origin`.

**Current repository fact (2026-10-05):** all committed work lives on `blueprint-emergence-updates` (pushed to `origin`, HEAD `d25cc13`, clean working tree, no stashes). Local `main` is at `e12b39b`, 14 commits behind, and has never been pushed. No integration branch is designated, so no item is `MERGED` — see BUILD-001.

## Work IDs

`<DOMAIN>-<###>`, never reused, never renumbered. Prefixes: `ARCH` · `BUILD` · `RT` · `HUD` · `ART` · `VIDEO` · `FX` · `PI` · `BLOB` · `TEMP` · `BLUE` · `CONTOUR` · `FRAG` · `QUAD`. A superseded item keeps its ID with status `SUPERSEDED` and a pointer to its replacement.

Off-repo evidence is cited as `OFF-REPO: ~/Downloads/<file>` and is never sufficient for `CLOSED`.

---

# NOW / NEXT / LATER

## Project-wide

| | Items |
|---|---|
| **NOW** | TEMP-001 Architecture acceptance |
| **NEXT** | RT-002 Blob ↔ Temporal 20-cycle switching · PI-001 Pi build + baseline · ARCH-003 import off-repo evidence · BUILD-001 integration-branch policy |
| **LATER** | PI-002/PI-003 Pi measurements · HUD Blueprint / Skin Package freeze (HUD-004, HUD-006) · scene #3 (BLUE-001) · remaining scene migrations · Dark Moss and remaining skins |

## Per domain

| Domain | NOW | NEXT | LATER |
|---|---|---|---|
| Architecture & Program Coordination | TEMP-001 acceptance | ARCH-002 accept protocol · ARCH-003 · BUILD-001 · authorize RT-002 | ARCH-004 doc reconciliation |
| ExperienceRuntime & SceneManager | — (awaiting RT-002 authorization) | RT-002 | Scene #3 hosting (BLUE-001) |
| HUD Runtime & Validation Studio | — | HUD-003 profile binding · HUD-002 domain acceptance | HUD-004 · HUD-005 · HUD-006 |
| Shared Video Playback | — | — | VIDEO-003 retire compatibility paths |
| Shared Effects & Shader Debugger | — | FX-004 (optional) | — |
| Raspberry Pi Runtime & Performance | PI-001 | PI-002 · PI-003 | Quality profiles, thermal soak |
| HUD Art Pipeline & Skin Production | HUD-004 (locate state) | — | ART-001 · ART-002 · ART-003 |
| Blob Scene Migration | — (accepted) | Support RT-002 | — |
| Temporal Fields Scene Migration | TEMP-001 acceptance support | Support RT-002 | TEMP-003 (deferred) |
| Blueprint Emergence Scene Migration | — | BLUE-001 planning (after RT-002) | — |
| Contour Portrait Scene Migration | — | — | CONTOUR-001 |
| Fragment Trail Scene Migration | — | — | FRAG-001 |
| Quadrant Crosshair Scene Migration | — | — | QUAD-001 |

---

# Gate Matrix

`Impl` Implementation · `Desk` Desktop Verification · `Integ` Integration Verification · `Arch` Architecture Acceptance · `Pi` Pi Verification · `Docs` Documentation · `SC` Source Control

| ID | Status | Impl | Desk | Integ | Arch | Pi | Docs | SC |
|---|---|---|---|---|---|---|---|---|
| ARCH-001 | ACCEPTED | PASS | NOT_REQUIRED | NOT_REQUIRED | PASS | NOT_REQUIRED | PENDING | COMMITTED |
| ARCH-002 | IMPLEMENTED | PASS | NOT_REQUIRED | NOT_REQUIRED | PENDING | NOT_REQUIRED | PASS | WORKTREE_ONLY |
| ARCH-003 | READY | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | NOT_REQUIRED | NOT_REQUIRED | NOT_STARTED | UNTRACKED |
| ARCH-004 | READY | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | PENDING | NOT_REQUIRED | NOT_STARTED | NOT_APPLICABLE |
| BUILD-001 | READY | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | PENDING | NOT_REQUIRED | NOT_STARTED | NOT_APPLICABLE |
| RT-001 | ACCEPTED | PASS | PASS | PASS | PASS | NOT_STARTED | PENDING | COMMITTED |
| RT-002 | READY | NOT_STARTED | NOT_STARTED | NOT_STARTED | PENDING | NOT_STARTED | PENDING | UNTRACKED |
| HUD-001 | ACCEPTED | PASS | PASS | PASS | PASS | NOT_STARTED | PASS | COMMITTED |
| HUD-002 | VERIFIED | PASS | PASS | UNKNOWN | NOT_REQUIRED | NOT_STARTED | PENDING | COMMITTED |
| HUD-003 | READY | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | NOT_STARTED | NOT_APPLICABLE |
| HUD-004 | UNKNOWN | UNKNOWN | NOT_REQUIRED | NOT_REQUIRED | PENDING | NOT_REQUIRED | UNKNOWN | UNKNOWN |
| HUD-005 | BACKLOG | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_APPLICABLE |
| HUD-006 | BLOCKED | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_APPLICABLE |
| ART-001 | BLOCKED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_APPLICABLE |
| ART-002 | BACKLOG | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | NOT_STARTED | NOT_REQUIRED | NOT_STARTED | NOT_APPLICABLE |
| ART-003 | BACKLOG | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_APPLICABLE |
| VIDEO-001 | ACCEPTED | PASS | PASS | PASS | PASS | NOT_STARTED | PASS | COMMITTED |
| VIDEO-002 | ACCEPTED | PASS | PASS | PASS | PASS | NOT_STARTED | PENDING | COMMITTED |
| VIDEO-003 | BACKLOG | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | NOT_STARTED | NOT_APPLICABLE |
| FX-001 | ACCEPTED | PASS | PASS | PASS | PASS | NOT_STARTED | PASS | COMMITTED |
| FX-002 | ACCEPTED | PASS | PASS | NOT_REQUIRED | PASS | NOT_STARTED | UNKNOWN | COMMITTED |
| FX-003 | ACCEPTED | PASS | PASS | PENDING | PASS | NOT_STARTED | PASS | COMMITTED |
| FX-004 | READY | NOT_REQUIRED | NOT_STARTED | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | NOT_STARTED | NOT_APPLICABLE |
| BLOB-001 | ACCEPTED | PASS | PASS | PASS | PASS | NOT_STARTED | PENDING | COMMITTED |
| TEMP-001 | VERIFIED | PASS | PASS | PASS | PENDING | NOT_STARTED | PASS | COMMITTED |
| TEMP-002 | VERIFIED | PASS | PASS | NOT_REQUIRED | PENDING | NOT_REQUIRED | PENDING | COMMITTED |
| TEMP-003 | DEFERRED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | NOT_STARTED | NOT_APPLICABLE |
| PI-001 | READY | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | PENDING | NOT_STARTED | NOT_STARTED | NOT_APPLICABLE |
| PI-002 | BLOCKED | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | PENDING | NOT_STARTED | NOT_STARTED | NOT_APPLICABLE |
| PI-003 | BLOCKED | NOT_STARTED | NOT_REQUIRED | NOT_REQUIRED | PENDING | NOT_STARTED | NOT_STARTED | NOT_APPLICABLE |
| BLUE-001 | BACKLOG | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_APPLICABLE |
| CONTOUR-001 | BACKLOG | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_APPLICABLE |
| FRAG-001 | BACKLOG | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_APPLICABLE |
| QUAD-001 | BACKLOG | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_STARTED | NOT_APPLICABLE |

---

# Work Items

## Architecture & Program Coordination

### ARCH-001 — Shared architecture and runtime contracts

Status: ACCEPTED
Owner: Architecture & Program Coordination
Architecture Gate: YES

Depends On:
- None

Current Evidence:
- [Scene-HUD-Contract-v1.md](Scene-HUD-Contract-v1.md), [01-architecture-governance.md](01-architecture-governance.md)
- [Decision-Log-v3.md](Decision-Log-v3.md) DEC-001 – DEC-018 (DEC-009 freezes the semantic slot model; DEC-012 the canonical HUD Blueprint policy)
- [../HUD-Semantic-Slot-Model-v1.md](../HUD-Semantic-Slot-Model-v1.md)

Next Gate:
None. Changes only through the contract-change process.

Next Action:
ARCH-004 — the slot-model and contract file headers still read "draft" / "ready for freeze"; the later DEC-009/DEC-012 decisions govern.

Blocked By:
None

Source State:
COMMITTED (`blueprint-emergence-updates`, pushed)

Last Updated:
2026-10-05

### ARCH-002 — Workspace-state protocol and Canonical Work Registry

Status: IMPLEMENTED
Owner: Architecture & Program Coordination
Architecture Gate: YES

Depends On:
- None

Current Evidence:
- This file; governance/handoff/chat-index updates
- [../reviews/work-registry-protocol-implementation-report.md](../reviews/work-registry-protocol-implementation-report.md)

Next Gate:
Architecture acceptance of the protocol.

Next Action:
Architecture reviews the protocol and the initial population; resolves the UNKNOWN items.

Blocked By:
None

Source State:
WORKTREE_ONLY

Last Updated:
2026-10-05

### ARCH-003 — Bring off-repo Architecture evidence into the repository

Status: READY
Owner: Architecture & Program Coordination
Architecture Gate: NO

Depends On:
- None

Current Evidence:
- The following accepted/authoritative artifacts exist only in `~/Downloads` (UNTRACKED): `experience-runtime-architecture-closure-review.md`, `blob-post-acceptance-hardening-architecture-handoff.md`, `shared-video-temporal-adapter-architecture-handoff.md`, `temporal-production-migration-two-scene-runtime-acceptance-requirements.md`, `temporal-runtime-asset-reproducibility-architecture-handoff.md`, `temporal-pattern-vocabulary-architecture-handoff.md`, `blob-temporal-20-cycle-production-switching-acceptance.md`, `Pre-Blob-Cross-Domain-Current-State-Baseline.md`, `Ecopunk-Architecture-Program-Coordination-Domain-Restart-Status-Report.md`, `experience-runtime-domain-restart-status-report.md`
- The "Shared Effect Knowledge v1 Freeze Specification" cited by the restart audit was not found in the repository or `~/Downloads`.

Next Gate:
None (documentation durability only).

Next Action:
Architecture designates which of these are canonical; a coding agent copies them unmodified into `docs/shared-project-docs/` and the registry evidence links are updated.

Blocked By:
None

Source State:
UNTRACKED

Last Updated:
2026-10-05

### ARCH-004 — Reconcile stale shared planning documents

Status: READY
Owner: Architecture & Program Coordination
Architecture Gate: YES

Depends On:
- ARCH-002 [IMPLEMENTED]

Current Evidence:
- Master Roadmap §3 "Current State" lists ExperienceRuntime, SceneManager, HudCompositor, shared video service, and the wireframe harness as "Not yet implemented"; all are ACCEPTED here.
- DEC-016 is headed "Approved / Frozen" but its Consequences text still describes the freeze as pending.
- `HUD-Semantic-Slot-Model-v1.md` header: "Architecture draft"; `Scene-HUD-Contract-v1.md` header: "Ready for freeze … pending final sign-off".
- `docs/blueprint_emergence_engineering_plan.md` (2026-06-22) §11 open questions all `_pending_`; seek-and-capture superseded by DEC-018.

Next Gate:
Architecture approves header/status-line corrections (no contract content changes).

Next Action:
Decide whether to annotate these docs with "superseded by / see registry" notes. Historical reports are not rewritten.

Blocked By:
None

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

### BUILD-001 — Integration branch and merge policy

Status: READY
Owner: Architecture & Program Coordination
Architecture Gate: YES

Depends On:
- None

Current Evidence:
- `git branch -a -vv` (2026-10-05): `blueprint-emergence-updates` = `origin/blueprint-emergence-updates` = `d25cc13`; local `main` = `e12b39b` (2026-06-29), 14 commits behind, no `origin/main`.

Next Gate:
Architecture decision: integration branch, and whether `CLOSED` requires `MERGED`.

Next Action:
Decide the policy; until then the interim durability default (`COMMITTED`, pushed) applies.

Blocked By:
None

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

## ExperienceRuntime & SceneManager

### RT-001 — ExperienceRuntime / SceneManager infrastructure

Status: ACCEPTED
Owner: ExperienceRuntime & SceneManager
Architecture Gate: YES

Depends On:
- ARCH-001 [ACCEPTED]

Current Evidence:
- [Architecture-Authorization-Temporal-Production-Scene-2.md](Architecture-Authorization-Temporal-Production-Scene-2.md) — "ExperienceRuntime infrastructure accepted"
- OFF-REPO: `~/Downloads/experience-runtime-architecture-closure-review.md` (recommended disposition: Accept)
- Code: `sketches/experience_runtime/src/ExperienceRuntime.*`, `SceneManager.*`

Next Gate:
None for infrastructure; Pi Verification via PI-001.

Next Action:
ARCH-003 — commit the closure review.

Blocked By:
None

Source State:
COMMITTED (`557edc3`, `d25cc13`; pushed)

Last Updated:
2026-10-05

### RT-002 — Blob ↔ Temporal 20-cycle production switching acceptance

Status: READY
Owner: ExperienceRuntime & SceneManager
Architecture Gate: YES

Depends On:
- BLOB-001 [ACCEPTED]
- TEMP-001 [VERIFIED] — must be ACCEPTED before authorization
- RT-001 [ACCEPTED]
- HUD-001 [ACCEPTED]

Current Evidence:
- OFF-REPO: `~/Downloads/blob-temporal-20-cycle-production-switching-acceptance.md` — §1: "PENDING EXECUTION EVIDENCE"
- [blob-temporal-blob-hud-acceptance-matrix.md](blob-temporal-blob-hud-acceptance-matrix.md)
- No switching harness exists: `sketches/experience_runtime/src` contains only `BlobLifecycleHarness` and `TemporalLifecycleHarness` (searched 2026-10-05).

Next Gate:
AUTHORIZED — per the Temporal authorization: "After Temporal itself is accepted, Architecture will authorize/execute the repeated two-scene switching acceptance milestone."

Next Action:
Commit the acceptance spec (ARCH-003); after TEMP-001 acceptance, Architecture authorizes; then generate the harness implementation prompt.

Blocked By:
TEMP-001 Architecture acceptance

Source State:
UNTRACKED (spec only; no implementation)

Last Updated:
2026-10-05

## HUD Runtime & Validation Studio

### HUD-001 — HUD Runtime and canonical 1280×720 wireframe

Status: ACCEPTED
Owner: HUD Runtime & Validation Studio
Architecture Gate: YES

Depends On:
- ARCH-001 [ACCEPTED]

Current Evidence:
- [../reviews/hud-runtime-architecture-closure-review.md](../reviews/hud-runtime-architecture-closure-review.md) §30–31
- [../reviews/hud-final-narrow-closure-patch-report.md](../reviews/hud-final-narrow-closure-patch-report.md) — 1307/1307 checks
- [Architecture-Authorization-Temporal-Production-Scene-2.md](Architecture-Authorization-Temporal-Production-Scene-2.md) — "HUD Runtime infrastructure accepted"; DEC-017

Next Gate:
Pi Verification via PI-003.

Next Action:
None.

Blocked By:
None

Source State:
COMMITTED (`d25cc13`; pushed)

Last Updated:
2026-10-05

### HUD-002 — Temporal pattern vocabulary

Status: VERIFIED
Owner: HUD Runtime & Validation Studio
Architecture Gate: NO

Depends On:
- TEMP-001 [VERIFIED]

Current Evidence:
- `shared/src/hud-compositor/HudVocabularyResolver.cpp` (Temporal pattern IDs, `d25cc13`)
- OFF-REPO: `~/Downloads/temporal-pattern-vocabulary-architecture-handoff.md` — all nine pattern IDs mapped, HUD tests green

Next Gate:
Domain acceptance by the HUD Runtime manager (not recorded).

Next Action:
HUD Runtime manager records acceptance or rejection.

Blocked By:
None

Source State:
COMMITTED (`d25cc13`; pushed)

Last Updated:
2026-10-05

### HUD-003 — Temporal HUD profile binding correction

Status: READY
Owner: HUD Runtime & Validation Studio
Architecture Gate: NO

Depends On:
- HUD-002 [VERIFIED]

Current Evidence:
- `shared/src/hud-compositor/HudPresentationProfile.cpp:213` binds `scene.temporal.metric.field_activity`, which production Temporal leaves absent
- `shared/src/hud-compositor-test/hud_presentation_tests.cpp:477` — `scene.temporal.metric.pattern` is not bound
- OFF-REPO: `~/Downloads/temporal-pattern-vocabulary-architecture-handoff.md` (presentation-profile mismatch section)

Next Gate:
Domain acceptance.

Next Action:
HUD Runtime manager writes a narrow binding-patch prompt. Escalate to Architecture only if a slot ID would change.

Blocked By:
None

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

### HUD-004 — Skin Package Specification and Skin Loader contract

Status: UNKNOWN
Owner: HUD Art Pipeline & Skin Production (spec); HUD Runtime (loader contract)
Architecture Gate: YES

Depends On:
- HUD-001 [ACCEPTED]

Current Evidence:
- No specification or loader-contract artifact in the repository or `~/Downloads`.
- OFF-REPO: restart audit §4 — "Skin Package Specification still in draft/freeze work"; `MediaViewportMesh` is the runtime clipping authority (no raster media mask).

Next Gate:
Architecture freeze review (Governance review checkpoint "Skin Package Spec").

Next Action:
HUD Art / HUD Runtime managers locate the latest draft and record its real state here. Resolves to IN_PROGRESS or BACKLOG.

Blocked By:
None (evidence gap)

Source State:
UNKNOWN

Last Updated:
2026-10-05

### HUD-005 — Skin Loader implementation

Status: BACKLOG
Owner: HUD Runtime & Validation Studio
Architecture Gate: YES

Depends On:
- HUD-004 [UNKNOWN]

Current Evidence:
- No skin-loader code in `shared/` or `sketches/` (searched 2026-10-05). Roadmap Phase 12.

Next Gate:
READY once HUD-004 is frozen.

Next Action:
None until HUD-004 resolves.

Blocked By:
HUD-004

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

### HUD-006 — HUD Blueprint v1 freeze

Status: BLOCKED
Owner: HUD Art Pipeline & Skin Production
Architecture Gate: YES

Depends On:
- HUD-004 [UNKNOWN]
- PI-003 [BLOCKED]

Current Evidence:
- DEC-012 (one canonical Blueprint policy); [../reviews/hud-runtime-architecture-closure-review.md](../reviews/hud-runtime-architecture-closure-review.md) — "No Blueprint freeze claimed"

Next Gate:
Architecture freeze review (checkpoint "HUD Blueprint").

Next Action:
None until dependencies resolve.

Blocked By:
HUD-004, PI-003

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

## HUD Art Pipeline & Skin Production

### ART-001 — Dark Moss skin production

Status: BLOCKED
Owner: HUD Art Pipeline & Skin Production
Architecture Gate: YES

Depends On:
- HUD-006 [BLOCKED]
- HUD-004 [UNKNOWN]
- PI-003 [BLOCKED]

Current Evidence:
- DEC-008; Roadmap Phase 13. No production assets in the repository.

Next Gate:
READY once Blueprint and Skin Package Spec are frozen and Pi budgets are measured.

Next Action:
None.

Blocked By:
HUD-006, HUD-004, PI-003

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

### ART-002 — Art-production pipeline formalization

Status: BACKLOG
Owner: HUD Art Pipeline & Skin Production
Architecture Gate: YES

Depends On:
- ART-001 [BLOCKED]

Current Evidence:
- Roadmap Phase 14. No artifacts.

Next Gate:
READY after ART-001.

Next Action:
None.

Blocked By:
ART-001

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

### ART-003 — Remaining four skins

Status: BACKLOG
Owner: HUD Art Pipeline & Skin Production
Architecture Gate: YES

Depends On:
- ART-002 [BACKLOG]

Current Evidence:
- DEC-008 (Bioluminescent Tech, Botanical Drafting, Sunlit Solarpunk Glass, Plywood + Marble); Roadmap Phase 15.

Next Gate:
READY after ART-002.

Next Action:
None.

Blocked By:
ART-002

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

## Shared Video Playback

### VIDEO-001 — Canonical VideoPlaybackService

Status: ACCEPTED
Owner: Shared Video Playback
Architecture Gate: YES

Depends On:
- ARCH-001 [ACCEPTED]

Current Evidence:
- DEC-011, DEC-013
- [../shared-video-playback-system-implementation-report.md](../shared-video-playback-system-implementation-report.md), [../shared-video-playback-engineering-session-2-report.md](../shared-video-playback-engineering-session-2-report.md) — 240/240 checks

Next Gate:
Pi Verification via PI-001.

Next Action:
None.

Blocked By:
None

Source State:
COMMITTED (`557edc3`; pushed)

Last Updated:
2026-10-05

### VIDEO-002 — Temporal specialized playback-history adapter seam

Status: ACCEPTED
Owner: Shared Video Playback
Architecture Gate: YES

Depends On:
- VIDEO-001 [ACCEPTED]

Current Evidence:
- DEC-014; [Architecture-Authorization-Temporal-Production-Scene-2.md](Architecture-Authorization-Temporal-Production-Scene-2.md) — "Shared Video Temporal specialized adapter seam accepted"
- OFF-REPO: `~/Downloads/shared-video-temporal-adapter-architecture-handoff.md` — 38/38 real GL adapter self-test

Next Gate:
Pi Verification via PI-002 (dual-decoder cost).

Next Action:
ARCH-003 — commit the handoff.

Blocked By:
None

Source State:
COMMITTED (`d25cc13`; pushed)

Last Updated:
2026-10-05

### VIDEO-003 — Retire temporary media compatibility paths

Status: BACKLOG
Owner: Shared Video Playback
Architecture Gate: NO

Depends On:
- VIDEO-001 [ACCEPTED]

Current Evidence:
- DEC-011 ("scene-local media folders and symlinks are temporary compatibility paths only"); e.g. `sketches/blueprint_emergence/bin/data/media/` symlinks.

Next Gate:
READY once remaining scene migrations no longer use local media paths.

Next Action:
None.

Blocked By:
None

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

## Shared Effects & Shader Debugger

### FX-001 — Canonical shared effect-activity transport

Status: ACCEPTED
Owner: Shared Effects & Shader Debugger
Architecture Gate: YES

Depends On:
- ARCH-001 [ACCEPTED]

Current Evidence:
- DEC-015; [../experience-runtime-final-shared-effects-seam-proof-report.md](../experience-runtime-final-shared-effects-seam-proof-report.md) — 47/47 checks
- [Architecture-Authorization-Temporal-Production-Scene-2.md](Architecture-Authorization-Temporal-Production-Scene-2.md) — "`TFEffectPicker` accepted as Temporal's canonical production effect-activity producer"

Next Gate:
None.

Next Action:
None.

Blocked By:
None

Source State:
COMMITTED (`d25cc13`; pushed)

Last Updated:
2026-10-05

### FX-002 — Shared Effect Knowledge v1 (frozen)

Status: ACCEPTED
Owner: Shared Effects & Shader Debugger
Architecture Gate: YES

Depends On:
- FX-001 [ACCEPTED]

Current Evidence:
- DEC-016 ("Approved / Frozen"); [../shared-effect-knowledge-schema-v1.md](../shared-effect-knowledge-schema-v1.md)
- `assets/shared/video-effects/knowledge/effect-knowledge-pack.json`

Next Gate:
None. Documentation gate UNKNOWN: the "Freeze Specification" cited by the restart audit could not be located.

Next Action:
ARCH-003 — locate and commit the Freeze Specification.

Blocked By:
None

Source State:
COMMITTED (`d25cc13`; pushed)

Last Updated:
2026-10-05

### FX-003 — Production selector / eligibility increment 2

Status: ACCEPTED
Owner: Shared Effects & Shader Debugger
Architecture Gate: YES

Depends On:
- FX-002 [ACCEPTED]

Current Evidence:
- [shared-effects-selector-increment-2-architecture-handoff.md](shared-effects-selector-increment-2-architecture-handoff.md) — "ACCEPTED WITH NON-BLOCKING LIVE-RUNTIME PROOF FOLLOW-UP"
- [../shared-effects-production-selector-eligibility-increment-2-report.md](../shared-effects-production-selector-eligibility-increment-2-report.md)

Next Gate:
Integration Verification completes via FX-004 (non-blocking).

Next Action:
None.

Blocked By:
None

Source State:
COMMITTED (`d25cc13`; pushed)

Last Updated:
2026-10-05

### FX-004 — Live authored-preset observation in Temporal

Status: READY
Owner: Shared Effects & Shader Debugger
Architecture Gate: NO

Depends On:
- TEMP-001 [VERIFIED]

Current Evidence:
- [Architecture-Authorization-Temporal-Production-Scene-2.md](Architecture-Authorization-Temporal-Production-Scene-2.md) — "desirable … not a migration gate"
- [../temporal-production-scene-2-migration-report.md](../temporal-production-scene-2-migration-report.md) — "A live authored-preset selection was not specifically observed"

Next Gate:
Domain acceptance.

Next Action:
Capture opportunistically during RT-002.

Blocked By:
None

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

## Blob Scene Migration

### BLOB-001 — Blob as production scene #1

Status: ACCEPTED
Owner: Blob Scene Migration
Architecture Gate: YES

Depends On:
- RT-001 [ACCEPTED]
- HUD-001 [ACCEPTED]
- VIDEO-001 [ACCEPTED]

Current Evidence:
- [blob-scene-migration-first-production-scene-acceptance-review.md](blob-scene-migration-first-production-scene-acceptance-review.md) — initial disposition "REQUIRES NARROW BLOB PATCH"
- [../reviews/blob-first-production-acceptance-narrow-patch-report.md](../reviews/blob-first-production-acceptance-narrow-patch-report.md) — "PASS — READY FOR FINAL BLOB ACCEPTANCE REVIEW"
- [../reviews/blob-post-acceptance-hardening-report.md](../reviews/blob-post-acceptance-hardening-report.md); parity captures in `docs/reviews/blob-parity-captures*/`
- Acceptance recorded in [Architecture-Authorization-Temporal-Production-Scene-2.md](Architecture-Authorization-Temporal-Production-Scene-2.md) ("Blob accepted as production scene #1") and OFF-REPO `~/Downloads/blob-post-acceptance-hardening-architecture-handoff.md` ("KEEP BLOB ACCEPTED")

Next Gate:
CLOSED — needs the final acceptance record in the repository (ARCH-003) and the BUILD-001 durability rule.

Next Action:
ARCH-003.

Blocked By:
None

Source State:
COMMITTED (`557edc3`, `d25cc13`; pushed)

Last Updated:
2026-10-05

## Temporal Fields Scene Migration

### TEMP-001 — Temporal as production scene #2

Status: VERIFIED
Owner: Temporal Fields Scene Migration
Architecture Gate: YES

Depends On:
- BLOB-001 [ACCEPTED]
- VIDEO-002 [ACCEPTED]
- FX-001 [ACCEPTED]
- TEMP-002 [VERIFIED]

Current Evidence:
- [Architecture-Authorization-Temporal-Production-Scene-2.md](Architecture-Authorization-Temporal-Production-Scene-2.md) — AUTHORIZED; lists the 10 acceptance proofs
- [../temporal-production-scene-2-migration-report.md](../temporal-production-scene-2-migration-report.md) — real launch, 295/295 lifecycle/reactivation checks, semantic mapping 34/34, Release builds green (macOS, reported 2026-08-10; not re-run since)
- `sketches/experience_runtime/src/TemporalProductionScene.*`, `TemporalLifecycleHarness.*` in `d25cc13`
- OFF-REPO: restart audit — "Temporal manager recommended conditional acceptance … unconditional Architecture acceptance not safely proven"; its open condition was source-control persistence, now confirmed (see Source State)

Next Gate:
Architecture acceptance.

Next Action:
Architecture reviews the migration report against the authorization's 10 acceptance proofs. Optionally re-run the Temporal lifecycle harness on the current checkout first.

Blocked By:
None

Source State:
COMMITTED (`d25cc13`, 2026-08-10; pushed to `origin/blueprint-emergence-updates`)

Last Updated:
2026-10-05

### TEMP-002 — Temporal runtime asset reproducibility

Status: VERIFIED
Owner: Temporal Fields Scene Migration
Architecture Gate: YES

Depends On:
- None

Current Evidence:
- `scripts/sync-temporal-runtime-assets.py`; canonical sources `sketches/temporal-fields/data/{shaders,backgrounds}/` (both first committed in `d25cc13`)
- `python3 scripts/sync-temporal-runtime-assets.py --check` on 2026-10-05: `temporal-fields` and `experience_runtime` "up to date", exit 0
- OFF-REPO: `~/Downloads/temporal-runtime-asset-reproducibility-architecture-handoff.md` — "PASS — FUNCTIONALLY CLOSED"; noted files "not yet committed" (written 09:07, superseded by commit `d25cc13` at 09:09 on 2026-08-10)

Next Gate:
Architecture acceptance (recommended in the handoff).

Next Action:
Architecture accepts with TEMP-001; ARCH-003 commits the handoff.

Blocked By:
None

Source State:
COMMITTED (`d25cc13`; pushed)

Last Updated:
2026-10-05

### TEMP-003 — `TFPresetTimeline` production extraction

Status: DEFERRED
Owner: Temporal Fields Scene Migration
Architecture Gate: NO

Depends On:
- TEMP-001 [VERIFIED]

Current Evidence:
- OFF-REPO: restart audit — Architecture accepted omission of `TFPresetTimeline` from production v1 as non-blocking.

Next Gate:
None until reprioritized.

Next Action:
None.

Blocked By:
None

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

## Raspberry Pi Runtime & Performance

### PI-001 — Pi build, deployment path, and baseline profiling

Status: READY
Owner: Raspberry Pi Runtime & Performance
Architecture Gate: YES (checkpoint "Pi baseline")

Depends On:
- RT-001 [ACCEPTED]

Current Evidence:
- No Pi build has ever existed: `PLATFORM_PI` "is never defined by any build in this repo" (`sketches/blueprint_emergence/src/BESettings.h:395`)
- [../hud-layout-integration-probe.md](../hud-layout-integration-probe.md) — "No sketch in this repo has ever been profiled on real Raspberry Pi 3B hardware"
- `shared/src/video-effects/knowledge/EffectSceneCompatibility.h` — `piValidated = false` everywhere
- No deploy script, systemd unit, or telemetry beyond fps/frameTimeMs

Next Gate:
Architecture "Pi baseline" checkpoint.

Next Action:
Pi domain manager writes the Roadmap Phase 9 build/deploy/baseline prompt. Highest feasibility risk in the project.

Blocked By:
None

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

### PI-002 — Temporal dual-decoder cost on Pi

Status: BLOCKED
Owner: Raspberry Pi Runtime & Performance
Architecture Gate: YES

Depends On:
- PI-001 [READY]
- VIDEO-002 [ACCEPTED]

Current Evidence:
- DEC-014 follow-up: "Measure the dual-decoder cost on Pi hardware before feasibility claims." No measurement exists.

Next Gate:
READY once PI-001 produces a running Pi build.

Next Action:
None.

Blocked By:
PI-001

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

### PI-003 — Pi HUD/skin texture, atlas, and residency budgets

Status: BLOCKED
Owner: Raspberry Pi Runtime & Performance
Architecture Gate: YES

Depends On:
- PI-001 [READY]

Current Evidence:
- OFF-REPO: restart audit — Blueprint freeze and Dark Moss are gated on "measured Pi texture/atlas/residency/emissive budgets". No measurement exists.

Next Gate:
READY once PI-001 produces a running Pi build.

Next Action:
None.

Blocked By:
PI-001

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

## Remaining scene migrations

### BLUE-001 — Blueprint Emergence production migration (scene #3 candidate)

Status: BACKLOG
Owner: Blueprint Emergence Scene Migration
Architecture Gate: YES

Depends On:
- RT-002 [READY]

Current Evidence:
- Standalone sketch already consumes `VideoPlaybackService` (`sketches/blueprint_emergence/src/ofApp.cpp:53-56`); DEC-018 retired the seek-and-capture requirement.
- Known risks: `bypassErosion = true` temp diagnostic (`ofApp.h:69`); nested-FBO hazard under an outer runtime FBO ([../hud-layout-integration-probe.md](../hud-layout-integration-probe.md)).

Next Gate:
READY after RT-002 (restart audit: "best after first two-scene milestone is proven").

Next Action:
None.

Blocked By:
None

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

### CONTOUR-001 — Contour Portrait production migration

Status: BACKLOG
Owner: Contour Portrait Scene Migration
Architecture Gate: YES

Depends On:
- RT-002 [READY]

Current Evidence:
- Still contains `ofVideoGrabber` camera path (`sketches/contour-portrait/src/ContourSource.h:35`); DEC-002 requires prerecorded-only. Not on the shared effects service ([../video-effect-second-wave-evaluation.md](../video-effect-second-wave-evaluation.md)).

Next Gate:
READY after RT-002.

Next Action:
None.

Blocked By:
None

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

### FRAG-001 — Fragment Trail production migration

Status: BACKLOG
Owner: Fragment Trail Scene Migration
Architecture Gate: YES

Depends On:
- RT-002 [READY]

Current Evidence:
- Local `ShaderLibrary`/`LFOBank`/`TriggerBus` forks collide with `shared/src` (root `CLAUDE.md`); chat index gates it on "shared-crosshair/fork prerequisites".

Next Gate:
READY after fork prerequisites are resolved.

Next Action:
None.

Blocked By:
None

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

### QUAD-001 — Quadrant Crosshair production migration

Status: BACKLOG
Owner: Quadrant Crosshair Scene Migration
Architecture Gate: YES

Depends On:
- RT-002 [READY]

Current Evidence:
- Production path migrated to shared effect catalog; `DebugMode` deliberately not migrated (root `CLAUDE.md`); same fork prerequisites as FRAG-001; [../quadrant-crosshair-reuse-analysis.md](../quadrant-crosshair-reuse-analysis.md).

Next Gate:
READY after fork prerequisites are resolved.

Next Action:
None.

Blocked By:
None

Source State:
NOT_APPLICABLE

Last Updated:
2026-10-05

---

# Registry Update Template

Submit with the session-close report when `REGISTRY UPDATE REQUIRED: YES`. The approver applies it here; historical reports are never edited to match.

```markdown
## Work Registry Update

Work Item:
Date:
Owner:

Previous Status:
Proposed Status:

Evidence:
- ...

Gate Changes:
- ...

Source State:
...

Dependencies Changed:
- ...

Next Action:
...

Architecture Review Required:
YES / NO
```

New items use the same field set as the entries above, take the next unused ID for their prefix, and start no higher than `READY` unless evidence says otherwise.

# Validation

No Markdown/registry validator exists in this repository. Checks for duplicate IDs, status vocabulary, required fields, and local link targets are optional future work (a small script under `scripts/`, alongside `check-video-effect-drift.py`).
