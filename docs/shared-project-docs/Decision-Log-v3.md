# Ecopunk Runtime — Decision Log

Use one entry per approved project-level decision.

**Role:** Decision Log → approved project-level decisions. [Work Registry](03-work-registry.md) → current work state, gates, dependencies, and evidence. Do not record work status here; when a decision changes work state, the registry references the decision by its DEC ID.

## Template

### DEC-XXX — Title

- **Status:** Proposed / Approved / Superseded
- **Date:**
- **Owner:**
- **Affected domains:**
- **Decision:**
- **Rationale:**
- **Alternatives considered:**
- **Consequences:**
- **Source documents:**
- **Follow-up actions:**

---

## Existing decisions to seed

### DEC-001 — Runtime scene scope

- **Status:** Approved
- **Decision:** Runtime includes Blob, Contour, Temporal, Fragment, Quadrant, and Blueprint only.

### DEC-002 — No camera input

- **Status:** Approved
- **Decision:** All runtime scenes use prerecorded media; camera modes are disabled or removed during migration.

### DEC-003 — Developer-only tuning surfaces

- **Status:** Approved
- **Decision:** `ofxGui` and `shader-effect-debugger` remain developer tooling and are not mirrored into the cinematic HUD.

### DEC-004 — Fixed canonical HUD layout

- **Status:** Approved
- **Decision:** All scenes and skins use one canonical layout with flexible scene-specific telemetry regions.

### DEC-005 — Native-resolution scene rendering for v1

- **Status:** Approved
- **Decision:** Scenes render at native resolution into a runtime-owned FBO; the HUD scales and clips the resulting texture.

### DEC-006 — Standardized video playback required

- **Status:** Approved
- **Decision:** All runtime scenes converge on one canonical media root and shared playback behavior.

### DEC-007 — Curated reseed

- **Status:** Approved
- **Decision:** Runtime reseed/reconfigure uses safe presets and ranges, never arbitrary raw parameters.

### DEC-008 — Five HUD skins

- **Status:** Approved
- **Decision:** Bioluminescent Tech, Dark Moss, Botanical Drafting, Sunlit Solarpunk Glass, and Plywood + Marble share one runtime package format.

## DEC-009

**Title:** HUD Semantic Slot Model v1 Approved

Freeze the semantic slot model as the shared contract between runtime and HUD. Stable slot IDs require Architecture review for incompatible changes.

## DEC-010

**Title:** HudFrameData Ownership

ExperienceRuntime assembles one immutable HudFrameData per frame. HudCompositor consumes it read-only. Runtime telemetry, scene status, video status, and effect status remain independently owned.

## DEC-011

**Title:** Canonical Shared Media Root

The canonical physical media root is assets/shared/media/. Scene-local media folders and symlinks are temporary compatibility paths only.

## DEC-012

**Title:** Canonical HUD Blueprint Policy

There shall be exactly one canonical HUD Blueprint. All HUD skins implement the same geometry, region IDs, safe areas, layer ordering, and widget layout. Skins may vary only in materials, textures, ornamentation, emissive treatment, iconography, and vocabulary packs. Runtime geometry and semantic bindings remain unchanged.

## DEC-013 — Canonical Shared Video Playback API

- **Status:** Approved
- **Owner:** Architecture + Shared Video
- **Affected domains:** Shared Video, ExperienceRuntime, HUD Runtime, all scene migrations
- **Decision:** `VideoPlaybackService` is the canonical shared ordinary playback API. It owns the canonical media catalog, selection policy, session history, Previous/Next semantics, hold behavior, ordinary decoder lifecycle, and `VideoPlaybackStatus`. `VideoPlaybackStatus` is transported through `HudFrameData.video`.
- **Rationale:** The architecture is implemented and validated by ExperienceRuntime plus the Blob and Blueprint playback migrations.
- **Consequences:** Scene-local ordinary playlist/selection ownership is transitional. Specialized temporal history remains outside the service.
- **Follow-up:** Migrate Temporal, Contour, Fragment, and Quadrant without creating competing selection authorities.

---

## DEC-014 — Temporal Playback / History Boundary

- **Status:** Approved
- **Owner:** Architecture + Shared Video
- **Affected domains:** Shared Video, Temporal Fields, Raspberry Pi Runtime
- **Decision:** `VideoPlaybackService` remains the sole media-selection/status authority for Temporal Fields. A Temporal-specific `TimeOffsetVideoBuffer` decoder/history pipeline may remain, but it must accept the service-selected media explicitly and may not independently scan, shuffle, or select media.
- **Rationale:** Temporal history is a specialized scene requirement and should not distort the ordinary playback service.
- **Consequences:** A minimal additive shared `TimeOffsetVideoBuffer` API may be implemented to support explicit selected-media loading and correct cleanup/reset semantics.
- **Follow-up:** Measure the dual-decoder cost on Pi hardware before feasibility claims.

---

## DEC-015 — Canonical Shared Effect Activity Transport

- **Status:** Approved
- **Owner:** Architecture + Shared Effects + ExperienceRuntime
- **Affected domains:** Shared Effects, ExperienceRuntime, HUD Runtime, scene migrations
- **Decision:** Canonical shared effect activity is transported as one immutable optional `EffectActivityStatus` sibling snapshot in `HudFrameData.effects`. `SceneHudStatus::activeEffects` remains compatibility-only during migration.
- **Rationale:** Shared Effects owns effect activity; scenes and HUD should not create parallel sources of truth.
- **Consequences:** HUD effect slots must prefer `HudFrameData.effects` whenever present. Missing snapshot and present-empty snapshot are distinct states.
- **Follow-up:** Apply after the Shared Effects closing schema increment.

---

## DEC-016 — Shared Effect Knowledge v1 Freeze Policy

- **Status:** Approved / Frozen
- **Owner:** Architecture + Shared Effects
- **Affected domains:** Shared Effects, HUD Runtime, ExperienceRuntime, Temporal, Blob, Blueprint, Fragment, Quadrant, Raspberry Pi Runtime
- **Decision:**
  - absent/empty `compatibleSceneIds` means **Unclassified**;
  - Unclassified knowledge is excluded from automatic production selection until positively classified;
  - `prominence` is a dominance-ranking value and is not `effects.intensity`;
  - `effects.intensity` may remain absent in v1;
  - reusable authored/favored presets require stable canonical preset IDs before schema freeze;
  - effect-level metadata provides defaults, preset-level metadata provides authored knowledge/overrides;
  - canonical authored knowledge lives under `assets/shared/video-effects/knowledge/`;
  - per-sketch copies are derived deployment/build artifacts.
- **Rationale:** These decisions close the ambiguities found by Engineering Session 2 without forcing false telemetry or multiple sources of truth.
- **Consequences:** Shared Effects must publish a final schema/precedence/version artifact and close canonical pack distribution before the schema is marked frozen.
- **Follow-up:** Determine the minimal effect-health representation and explicitly locate selection-weight/recent-history policy ownership in the closing schema artifact.

---

## DEC-017 — Production HUD Presentation Boundary

- **Status:** Approved
- **Owner:** Architecture + HUD Runtime + ExperienceRuntime
- **Affected domains:** HUD Runtime, ExperienceRuntime
- **Decision:** The architecture requires exactly one production HUD presentation path consuming one immutable `HudFrameData` per frame. A thin runtime bridge plus HUD-owned renderer satisfies the `HudCompositor` responsibility; a specific concrete class name is not required.
- **Rationale:** Domain reports used different terminology for the same boundary. The architectural requirement is ownership and data flow, not class spelling.
- **Consequences:** No second compositor should be invented merely to match a name. Stub/fake paths must remain tooling-only and inactive in production.
- **Follow-up:** Joint HUD/ExperienceRuntime acceptance test before wireframe approval.

---

## DEC-018 — Blueprint Emergence Capture Assumption Retired

- **Status:** Approved
- **Owner:** Architecture + Shared Video
- **Affected domains:** Shared Video, Blueprint Emergence
- **Decision:** Blueprint Emergence's production behavior does not depend on the unused `VideoSampler::requestCapture()` seek/callback path. Future Blueprint migration work should preserve continuous live texture/pixel sampling and crop behavior, not rebuild an unused capture subsystem unless a new product requirement intentionally reintroduces it.
- **Rationale:** Repository-wide call-site inspection found no production callers.
- **Consequences:** Future Blueprint prompts should remove still-frame capture preservation from required scope.
