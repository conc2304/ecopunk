# Architecture Freeze — HUD-006 Canonical Blueprint Identity v1

**Decision owner:** Architecture & Program Coordination  
**Decision date:** 2026-10-08  
**Disposition:** APPROVED — canonical identity and compatibility contract frozen; repository durability pending  
**Implementation authorization:** None

## Decision

- `blueprintId`: `ecopunk-hud-blueprint`
- `blueprintVersion`: `1`
- Compatibility: exact match of both values; no ranges, aliases, implicit defaults, or fallback identity.
- `skinId` and `skinVersion` independently identify the skin package and its release; skin changes do not implicitly change Blueprint compatibility.
- The accepted tools-only manifest validator continues to require the caller to supply expected Blueprint identity/version explicitly and to fail closed if policy inputs are unset.

## Canonical Blueprint source baseline

- Logical canvas: **1280 × 720**.
- Canonical region inventory: **19 distinct regions**, as registered in `shared/src/hud-compositor/HudRegionCatalog.cpp` (last source revision `557edc3`). The prior claim of 20 regions is an erratum.
- Authoritative geometry: `docs/reviews/hud-wireframe-bounds-v1.md` (revision `d25cc13`).
- Authoritative wireframe typography: `docs/reviews/hud-typography-metrics-v1.md` (revision `d25cc13`); this does **not** freeze final skin typography.
- Runtime viewport geometry/clipping: `shared/src/hud-compositor/MediaViewportMesh.{h,cpp}` (last source revision `557edc3`); rounded upper corners and lower-left corner, lower-right 45° bevel, mesh-based clipping; no runtime raster media mask required.
- Presentation scope: the Blueprint defines all 19 regions. Production currently renders 13 (`MinimalSlice`: 12 binding regions and `media_viewport`); this is a permitted presentation subset, not a different Blueprint. `FullProfile` remains an available presentation scope.
- Safe-area policy: the accepted region rectangles and their separation gaps are the v1 exclusion geometry; no additional safe-area schema is required.

## Compatibility-change policy

Architecture review and an explicit Blueprint compatibility-version decision are required for changes to region IDs, geometry, region semantics/bindings, viewport shape/clipping contract, or the meaning of presentation scopes. A presentation configuration enabling or disabling existing regions does not by itself require a version bump if the existing semantics and geometry remain unchanged. Cosmetic skin artwork, textures, palettes, and independently versioned skin-package releases do not by themselves change Blueprint compatibility.

No skin may redefine Blueprint geometry or semantic region IDs. No production default is authorized for the expected Blueprint pair.

## Deferred and separate gates

- `universal.timing` inset and text-budget documentation remains open as a nonblocking documentation follow-up.
- Pi skin texture, residency, loading and frame budgets do **not** gate HUD-006 identity freeze; they remain required for later skin production acceptance.
- HUD-004 full Skin Package Specification remains open; accepted 394/394 test evidence covers only the tools-only manifest foundation.
- HUD-005 production loader, image decoding, fallback behavior, Validation Studio loader parity, GPU residency, and asset-root delivery are **not authorized** by this decision.
- Historic art-guide dotted IDs and runtime raster mask requirements are superseded as canonical construction requirements; preserve historical documents with annotations.

## Evidence and provenance

Source verification report: `HUD-006 / HUD-004 Source Verification Report`, dated 2026-10-08, read-only inspection of branch `blueprint-emergence-updates` at HEAD `dfbe16959911121cc44d738326a366b5edfc17a1`, clean worktree. The catalog and mesh files were last changed in `557edc3`; the geometry and typography reviews were last changed in `d25cc13`. The report found no earlier approved literal Blueprint ID/version. No builds or tests were run in that verification pass.

Local commits `4bc1be7` (manifest foundation and acceptance record) and `dfbe169` (HUD-004 registry reconciliation) were reported committed but **not pushed**. Do not describe them as remote-durable.

## Registry disposition and required handoffs

**HUD-006:** Architecture identity/compatibility freeze **APPROVED**. Registry closure/durable source state **PENDING** until the authorized repository owner files this decision in the canonical repository, reconciles HUD-006 evidence/status, and records commit/push provenance. Do not claim the repository has already been changed.

**HUD Runtime:** Preserve catalog, bounds and production `MinimalSlice`; no implementation authorized. After durable freeze, a separate, narrowly scoped validator compatibility reconciliation may be proposed, not automatically executed.

**HUD Art:** Prepare a documentation-only 19-region construction crosswalk, using ratified bounds, wireframe text protection, runtime-owned viewport clipping and safe-area gaps. Do not begin final Dark Moss production.

**Repository owner:** Review and include this decision, update Decision Log/Work Registry under governance, and preserve accurate local-versus-remote provenance. Coding agents must not stage, commit or push.
