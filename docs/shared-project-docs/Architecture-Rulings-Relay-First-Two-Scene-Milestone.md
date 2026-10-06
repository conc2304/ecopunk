# Architecture Rulings Relay — First Two-Scene Milestone (RT-003 · RT-002 · TEMP-004)

**Owner:** Architecture & Program Coordination
**Recorded:** 2026-10-06, by the first-two-scene durability-closure session (ARCH-003 mechanism)
**Nature of this file:** a **verbatim relay record**. It is not a reconstruction of the original Architecture documents.

> **Provenance.** The original Architecture rulings governing this milestone were never saved as files, either in the repository or in `~/Downloads`. Architecture's decisions reached the coding agent only as decision blocks quoted at the top of the work prompts that the project owner relayed into the coding-agent session on 2026-10-05.
>
> Each block below is copied **exactly as relayed**. Nothing has been paraphrased, merged or updated. Text outside the blocks only identifies which relay each block came from.
>
> If the original ruling documents are later recovered, they supersede this record wherever the two differ, and the difference must be reported. They must not be silently merged.

Related canonical records:
- [Architecture-Acceptance-First-Two-Scene-Milestone.md](Architecture-Acceptance-First-Two-Scene-Milestone.md) — final technical acceptance grant (2026-10-06)
- [Architecture-Authorization-Temporal-Production-Scene-2.md](Architecture-Authorization-Temporal-Production-Scene-2.md) — Temporal scene #2 authorization
- Governing specifications, imported 2026-10-06 unmodified from `~/Downloads`:
  - [temporal-production-migration-two-scene-runtime-acceptance-requirements.md](temporal-production-migration-two-scene-runtime-acceptance-requirements.md)
  - [blob-temporal-20-cycle-production-switching-acceptance.md](blob-temporal-20-cycle-production-switching-acceptance.md)
- [blob-temporal-blob-hud-acceptance-matrix.md](blob-temporal-blob-hud-acceptance-matrix.md) (already in-repo; byte-identical to the `~/Downloads` copy)

---

## 1. RT-003 authorization — Blob + Temporal Stage B / in-process switching

**Relay:** the "RT-003 — Blob + Temporal In-Process SceneManager Switching" work prompt, 2026-10-05. It cites "Architecture Ruling — Blob + Temporal Stage B / RT-003 Authorization" as authoritative. That ruling document itself was not available.

Quoted decision text:

> Architecture has explicitly authorized in-process SceneManager switching for the BlobProductionScene + TemporalProductionScene pair only.

```text
TEMP-001: ACCEPTED
Blob + Temporal Stage B collision gate: CLEARED FOR THIS PAIR ONLY
Switching strategy: IN-PROCESS SCENEMANAGER SWITCHING
RT-003: AUTHORIZED
RT-002: BLOCKED BY RT-003
Contract change required: NO
```

> Do not reopen these decisions.

> Do not mark global Stage B collision resolution complete.
> This authorization applies only to Blob + Temporal.

Evidence that acted on it: [RT-003 completion report](../reviews/rt-003-blob-temporal-scenemanager-switching-report.md) (commit `59c24f4`).

## 2. RT-003 acceptance / RT-002 readiness

**Relay:** the "RT-002 — Blob ↔ Temporal 20-Cycle Production Switching Acceptance" work prompt, 2026-10-05.

Quoted decision text:

> RT-003 has been technically accepted. Blob and Temporal now switch in-process through the production SceneManager path.

```text
RT-003: ACCEPTED
RT-002: READY
Blob + Temporal Stage B gate: CLEARED FOR THIS PAIR ONLY
Switching strategy: IN-PROCESS
Contract change required: NO
```

> Do not reopen those decisions.

The relay listed "RT-003 ExperienceRuntime domain acceptance review" as an authoritative input, but its content was **not relayed and is not recoverable**. Only the decision above is recorded.

## 3. TEMP-004 authorization — Temporal stale playhead invalidation

**Relay:** the "TEMP-004 — Temporal Stale Playhead Invalidation Fix" work prompt, 2026-10-05. It cites "TEMP-004 Architecture authorization" as authoritative; that document itself was not available.

Quoted decision text:

> Architecture has authorized **TEMP-004**, a narrow defect fix discovered by the full RT-002 Blob ↔ Temporal 20-cycle acceptance.
>
> This is a **Temporal-owned behavior correction**, not a runtime redesign.

Established by RT-002, as relayed:

```text
RT-003 switching: PASS
canonical Shared Video ownership: PASS
Temporal adapter follows canonical media: PASS
CPU history clear/refill: PASS
GL/FBO/HUD switching: PASS

Temporal rendered playhead state while history empty: FAIL
```

Required behavioral invariant, as relayed:

```text
Temporal history is empty / invalid
        ↓
previous playhead imagery is not eligible
for presentation
```

Public-API stop condition, as relayed:

> If the correct solution requires adding or changing a **public/shared `TimeOffsetVideoBuffer` API**, stop implementation before making that public change.

Evidence that acted on it: [TEMP-004 report](../reviews/temp-004-temporal-stale-playhead-fix-report.md). The fix used only a private helper, so the stop condition was not reached.

## 4. RT-002 rerun instruction (post-TEMP-004)

**Relay:** ExperienceRuntime/Architecture routing message, 2026-10-05.

Quoted text, beginning as received (the opening was truncated in transmission):

> e same existing RT-002 acceptance harness unchanged:
> `EXPERIENCE_RUNTIME_TWO_SCENE_ACCEPTANCE=1`

```text
20 × Blob → Temporal → Blob
        ↓
every Temporal activation/history clear
        ↓
history empty
        ↓
0 stale/pre-clear playhead presentations
        ↓
valid current-media history refills
        ↓
all other RT-002 requirements remain PASS
```

> If that clean rerun passes, the return should come directly here for the Architecture first-two-scene acceptance review. TEMP-004 itself remains WORKTREE_ONLY, so durability/source-artifact closure still follows technical acceptance rather than blocking this rerun.

Evidence that acted on it: [RT-002 rerun report](../reviews/rt-002-rerun-after-temp004-report.md). The authoritative record is clean run B: 20/20 cycles, 115,726 checks, 0 failures.

---

## Not recoverable

| Artifact | State |
|---|---|
| Original "Architecture Ruling — Blob + Temporal Stage B / RT-003 Authorization" document | Not found; decision content relayed above (§1) |
| RT-003 ExperienceRuntime domain acceptance review | Not found; content never relayed |
| Original "TEMP-004 Architecture authorization" document | Not found; decision content relayed above (§3) |
