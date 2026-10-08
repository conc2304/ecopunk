# Architecture Acceptance — HUD-006 Blueprint Construction Crosswalk v1

**Owner:** Architecture & Program Coordination
**Status:** ACCEPTED — REVIEW COMPLETE
**Date recorded:** 2026-10-08
**Recorded by:** A HUD Runtime coding-agent session, which prepared this filing for the repository owner. The session makes no acceptance decision of its own.
**Work item:** [HUD-006](03-work-registry.md#hud-006--hud-blueprint-v1-freeze)

> **Provenance.** Architecture issued this disposition in the project conversation. Before this file, no copy existed in the repository or `~/Downloads`. The repository owner relayed it on 2026-10-08.
>
> The block below is copied **exactly as relayed**, including its tab-separated table and its opening fragment. Nothing has been paraphrased. Every other section only reconciles the block against in-repository evidence.
>
> If the original Architecture document is later recovered, it supersedes this record wherever the two differ, and the difference must be reported. It must not be silently merged.

## Architecture decision (verbatim relay)

```text
: Only the repository owner needs the accepted documents for durable filing. No additional HUD Runtime or HUD Art review is required.
Architecture final disposition
ACCEPTED — REVIEW COMPLETE
Based on the reported HUD Runtime acceptance and completed corrections, I accept HUD-006 Blueprint Construction Crosswalk v1 — Runtime-Reviewed Documentation Revision as the authoritative HUD Art construction reference.
Acceptance criterion	Disposition
All 19 canonical regions	Accepted
13 production MinimalSlice regions	Accepted
Six additional FullProfile regions	Accepted
Widget eligibility versus active bindings	Accepted
Deferred universal.timing gap	Accepted as nonblocking
Frozen Blueprint identity and geometry	Preserved
HUD Runtime technical review	Complete
Architecture crosswalk review	Complete
This acceptance is based on the reported completed revision and Runtime approval. The specifically named Runtime-Reviewed file has not been independently inspected in this turn.
```

## Accepted document, as filed

- [HUD-006-Blueprint-Construction-Crosswalk-v1-Runtime-Reviewed.md](HUD-006-Blueprint-Construction-Crosswalk-v1-Runtime-Reviewed.md)
  - Imported unmodified from `~/Downloads`, 2026-10-08.
  - SHA-256 prefix `eb345cf2db1c2557`.
  - The owner's `~/Downloads` held two copies, `…-Runtime-Reviewed.md` and `… (1).md`; they are byte-identical.
- Not filed: the `-Source-Enriched` and `-Reconciled` variants in `~/Downloads`. They are predecessor drafts of the accepted revision.

## Reconciliation against the repository

Architecture states it did not inspect the named file itself. As part of this filing, the coding agent checked the filed copy against the current source at HEAD `dfbe169`, by static inspection only:

- **Region table (§1):** all 19 IDs, normalized rectangles and 1280×720 pixel rectangles match `shared/src/hud-compositor/HudRegionCatalog.cpp:32-119` (revision `557edc3`) and [../reviews/hud-006-hud-004-source-verification-report.md](../reviews/hud-006-hud-004-source-verification-report.md) §2.1.
- **Presentation scope:**
  - The 13 `MinimalSlice` regions and 6 `FullProfile`-only regions match `HudWireframeRenderer.cpp:33-41, 365-370, 388, 395`, `HudWireframeRenderer.h:197` and `HudCompositorBridge.cpp:3-14`.
  - The crosswalk's "underlying source `61a4acf`" means the HUD source is unchanged since that probe revision. The files themselves were last changed in `557edc3`.
- **Viewport (§2):** rounded corners at upper-left, upper-right and lower-left, a 45° lower-right bevel, and radius 18 / bevel 42 / 6° steps match `MediaViewportMesh.{h,cpp}`. "54 vertices / 52 triangles" agrees with the arc-step arithmetic: three 90° arcs → 16 points each, plus 4 edge points, plus center and closing vertex.
- **Typography (§3):** consistent with `hud-typography-metrics-v1.md` (`d25cc13`) and `HudTextMetricsCache`. The 13 px cell height remains document-asserted, not source-measured.
- **Minor note, no correction made:** the §1 table lists `BindingPlaceholder (studio only)` only for `universal.scene_title`. In source it is eligible in every region (`HudRegionCatalog.cpp:127-129`). It is tooling-only either way, so this does not affect construction guidance.

## Related records

- [Architecture-Freeze-HUD-006-Canonical-Blueprint-Identity-v1.md](Architecture-Freeze-HUD-006-Canonical-Blueprint-Identity-v1.md): canonical identity `ecopunk-hud-blueprint` / `1`. Imported unmodified.
- [Decision-Log-v3.md](Decision-Log-v3.md) DEC-019.
- [../reviews/hud-006-hud-004-source-verification-report.md](../reviews/hud-006-hud-004-source-verification-report.md): the source evidence cited by the freeze record. Imported unmodified.
