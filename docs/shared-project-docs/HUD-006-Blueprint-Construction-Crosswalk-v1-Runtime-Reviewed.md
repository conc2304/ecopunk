# HUD-006 Blueprint Construction Crosswalk v1 — Runtime-Reviewed Documentation Revision

**Recipient:** Architecture & Program Coordination; CC: HUD Runtime & Validation Studio  
**Owner:** Ecopunk — HUD Art Pipeline and Skin Production  
**Date:** 2026-10-08  
**Disposition:** DOCUMENTATION REVISION COMPLETE — HUD Runtime review ALIGNED, minor production-scope correction incorporated; submitted for Architecture review. No new Blueprint freeze or compatibility decision.  
**Authorization:** Documentation only; no artwork, implementation, staging, commits or pushes.

## TL;DR

HUD Runtime confirmed the exact production membership at HEAD `dfbe169` (underlying HUD source unchanged from `61a4acf`). All 19 canonical regions belong to `ecopunk-hud-blueprint` / `1`; `MinimalSlice` renders 13 and `FullProfile` additionally includes six. The source-backed mapping below replaces the prior "membership unverified" status. **Allowed widget classes are catalog eligibility, not proof that a particular widget or semantic binding is actively selected by a presentation profile.** Exact active binding IDs/configuration still require a profile-binding extract before they may be asserted. The accepted geometry, wireframe typography and `MediaViewportMesh` ownership are unchanged.

## Source hierarchy and status

| Item | Status | Source |
|---|---|---|
| Identity/version/compatibility | **Approved** | *Architecture Freeze — HUD-006 Canonical Blueprint Identity v1*, 2026-10-08; repo durability pending |
| Region count, region IDs, geometry, safe-area gaps | **Approved** | `shared/src/hud-compositor/HudRegionCatalog.cpp` at `557edc3` as documented in `docs/reviews/hud-wireframe-bounds-v1.md` at `d25cc13`; supplied text |
| Wireframe typography | **Approved** | `docs/reviews/hud-typography-metrics-v1.md` at `d25cc13`; supplied attachment |
| Final skin typography | **Draft / Not frozen** | Separate later skin-design decision |
| 13-region `MinimalSlice` and six `FullProfile` additions | **Verified by HUD Runtime source review** | `HudWireframeRenderer.cpp:33–41, 365–370, 388–395`; `HudWireframeRenderer.h:197`; `HudCompositorBridge.cpp:3–14`; HEAD `dfbe169`, underlying source `61a4acf` |
| `universal.timing` text inset | **Missing (deferred)** | Wireframe bounds §1, Architecture freeze record |
| Pi skin budgets / production loader | **Not Evidenced as approved** | Separately gated HUD-004/HUD-005 and Pi work |

## 1. Canonical 19-region construction table

Coordinates are **(x,y,w,h)** in logical pixels; normalized values are **(x/1280,y/720,w/1280,h/720)**. The precision shown is a representation of the supplied source geometry, **not** a proposed geometry change. All regions belong to the single canonical Blueprint. `MinimalSlice` membership is **verified by HUD Runtime**; `FullProfile` additionally exposes six existing regions, not a second Blueprint. "Allowed widget / role" is catalog eligibility; "Binding / semantic purpose" is a descriptive purpose, **not a verified active profile-binding ID**. No Art-owned geometry is implied.

| Canonical ID | Normalized (x,y,w,h) | Pixels (x,y,w,h) | Allowed widget / role | Binding / semantic purpose | Production scope | Wireframe text protection | Art construction / gap rule |
|---|---|---|---|---|---|---|---|
| `universal.scene_title` | `0.66, 0.03, 0.32, 0.06` | `844.8, 21.6, 409.6, 43.2` | Label; BindingPlaceholder (studio only) | Scene title | **MinimalSlice** (also FullProfile) | 406.72 px; 1 line | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `universal.media_title` | `0.66, 0.1, 0.32, 0.05` | `844.8, 72.0, 409.6, 36.0` | Label | Media title | **MinimalSlice** (also FullProfile) | 407.2 px; 1 line | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `universal.primary_state` | `0.66, 0.16, 0.32, 0.06` | `844.8, 115.2, 409.6, 43.2` | StatusBadge | Primary scene state | **MinimalSlice** (also FullProfile) | 406.72 px; 1 line | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `universal.activity` | `0.66, 0.23, 0.32, 0.06` | `844.8, 165.6, 409.6, 43.2` | NumericValue / ProgressBar | Activity value/progress | **MinimalSlice** (also FullProfile) | 406.72 / 409.6 px; 1 line | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `universal.effect_summary` | `0.66, 0.3, 0.32, 0.06` | `844.8, 216.0, 409.6, 43.2` | EffectChips + StatusBadge (2 vertical slots) | Effect and health summary | **FullProfile only** (not MinimalSlice) | 408.16 px per 21.6px slot; 1 line/chip | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `universal.timing` | `0.66, 0.37, 0.32, 0.05` | `844.8, 266.4, 409.6, 36.0` | NumericValue / Label | Timing | **FullProfile only** (not MinimalSlice) | INSET/TEXT BUDGET OPEN | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `universal.activity_trace` | `0.66, 0.43, 0.32, 0.1` | `844.8, 309.6, 409.6, 72.0` | Sparkline | Activity trace | **FullProfile only** (not MinimalSlice) | No region text budget; sparkline caption if rendered | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `universal.secondary_trace` | `0.66, 0.54, 0.32, 0.1` | `844.8, 388.8, 409.6, 72.0` | Sparkline | Secondary trace | **FullProfile only** (not MinimalSlice) | No region text budget; sparkline caption if rendered | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `media_viewport` | `0.02, 0.16, 0.6, 0.7` | `25.6, 115.2, 768.0, 504.0` | MediaViewportMesh (no binding) | Scene image viewport | **MinimalSlice** (also FullProfile) | N/A; mesh clipping | Mesh silhouette and bevel; do not raster-mask or cover media content |
| `controls.scene_previous` | `0.02, 0.9, 0.08, 0.06` | `25.6, 648.0, 102.4, 43.2` | Label | Previous scene control | **MinimalSlice** (also FullProfile) | 99.52 px; 1 line | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `controls.scene_next` | `0.12, 0.9, 0.08, 0.06` | `153.6, 648.0, 102.4, 43.2` | Label | Next scene control | **MinimalSlice** (also FullProfile) | 99.52 px; 1 line | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `controls.media_previous` | `0.24, 0.9, 0.08, 0.06` | `307.2, 648.0, 102.4, 43.2` | Label | Previous media control | **MinimalSlice** (also FullProfile) | 99.52 px; 1 line | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `controls.media_next` | `0.34, 0.9, 0.08, 0.06` | `435.2, 648.0, 102.4, 43.2` | Label | Next media control | **MinimalSlice** (also FullProfile) | 99.52 px; 1 line | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `controls.reseed` | `0.46, 0.9, 0.1, 0.06` | `588.8, 648.0, 128.0, 43.2` | Label | Reseed control | **MinimalSlice** (also FullProfile) | 125.12 px; 1 line | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `flexible.primary` | `0.66, 0.64, 0.32, 0.16` | `844.8, 460.8, 409.6, 115.2` | MetadataCard / ChannelStrip / EffectChips / ProgressRing / Sparkline / Timeline / AmbientField | Mutually exclusive flexible content | **MinimalSlice** (also FullProfile) | MetadataCard 398.08 px; ChannelStrip 99.52 px/cell; other widget-dependent | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `flexible.secondary_a` | `0.66, 0.81, 0.15, 0.06` | `844.8, 583.2, 192.0, 43.2` | NumericValue / ProgressBar / ProgressRing / StatusBadge / Sparkline / AmbientField | Flexible secondary A | **FullProfile only** (not MinimalSlice) | 189.12–192 px by widget; 1 line | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `flexible.secondary_b` | `0.83, 0.81, 0.15, 0.06` | `1062.4, 583.2, 192.0, 43.2` | NumericValue / ProgressBar / ProgressRing / StatusBadge / Sparkline / AmbientField | Flexible secondary B | **FullProfile only** (not MinimalSlice) | 189.12–192 px by widget; 1 line | Keep artwork/ornaments out of widget live content; preserve rectangle and neighbor gaps |
| `overlay.health` | `0.02, 0.03, 0.2, 0.06` | `25.6, 21.6, 256.0, 43.2` | StatusBadge / Label | Health overlay | **MinimalSlice** (also FullProfile) | 253.12 px; 1 line | Transparent overlay; protect live text and compositing order |
| `overlay.transition` | `0.02, 0.1, 0.2, 0.08` | `25.6, 72.0, 256.0, 57.6` | ProgressBar + Label + Label (3 slots) | Transition overlay | **MinimalSlice** (also FullProfile) | 256 / approx 254.7 px; 1 line per slot | Transparent overlay; protect live text and compositing order |

**Scope distinction (HUD Runtime verified):** 19 canonical regions total. Production `MinimalSlice` renders 13 (12 binding regions + `media_viewport`); six additional regions are available in `FullProfile`. Presentation membership is not evidence of a specific active binding configuration, nor does `FullProfile` define a separate Blueprint.

**Verified `MinimalSlice` (13):** `universal.scene_title`, `universal.media_title`, `universal.primary_state`, `universal.activity`, `controls.scene_previous`, `controls.scene_next`, `controls.media_previous`, `controls.media_next`, `controls.reseed`, `flexible.primary`, `overlay.health`, `overlay.transition`, `media_viewport`.

**Additional `FullProfile` regions (6):** `universal.effect_summary`, `universal.timing`, `universal.activity_trace`, `universal.secondary_trace`, `flexible.secondary_a`, `flexible.secondary_b`.

**Profile/binding distinction:** The table lists catalog-permitted widget classes and existing region purposes. Which concrete widget instance, binding ID, value source, or optional configuration is active in a given profile is a separate profile-configuration question. Do not infer these from catalog eligibility or production membership.

### Region-specific subslots and geometry

- `universal.effect_summary`: chips `(844.8,216.0,409.6,21.6)` and health `(844.8,237.6,409.6,21.6)`; preserve both live-content areas.
- `overlay.transition`: progress `(25.6,72.0,256.0,19.2)`, phase `(25.6,91.2,256.0,19.2)`, message `(25.6,110.4,256.0,19.2)`; overlay may intentionally composite over the media viewport.
- `flexible.primary`: one of the allowed widgets at a time, per `HudProfileCompiler` occupancy rules. ChannelStrip uses four 102.4px cells; MetadataCard has 5.76px side padding and 398.08px live width.
- `flexible.secondary_a/b`: 25.6px horizontal separation between A and B; 7.2px vertical separation above them to `flexible.primary`. NumericValue/StatusBadge 189.12px, ProgressBar/Sparkline 192px, ProgressRing 190.56px, AmbientField 190.92px. No new decoration inset is required beyond widget-owned insets.
- Same-role Universal, Controls and Flexible rectangles must not overlap; Overlay regions are intentional exceptions. Preserve all gaps as the v1 safe-area policy, with no new safe-area schema.

## 2. Viewport shape and visual protection

`media_viewport` is `(25.6,115.2,768.0,504.0)` logical pixels. `MediaViewportMesh` owns shape and clipping, not HUD Art: three rounded corners (upper-left, upper-right, lower-left), 45° lower-right bevel, default corner radius 18px, bevel size 42px, arc step 6°, with 40%-of-short-side clamps inactive at this size. Source reports 54 vertices / 52 triangles. Art may build shell ornament around the viewport but must not bake in a raster media mask or introduce panel masks as runtime construction requirements. Respect intentional `overlay.*` compositing.

## 3. Typography protection — wireframe accepted, final skin typography open

The accepted wireframe strategy is one native-size OpenFrameworks bitmap tier, fixed **8px glyph pitch × 13px cell height**, ASCII, single-line text, no letter spacing. Dynamic text uses `HudTextMetricsCache::truncateToWidth()` with ASCII `...`; below 24px available width it returns empty. Text is positioned by widget offsets, not a separately verified baseline. Avoid ornamentation and emission in live-content areas; do not replace runtime typography with art-baked captions or infer a new font system.

Per-region widths in the table are **wireframe truncation budgets**, not minimum readable dimensions for a final font. `universal.timing` inset and budget remain **deferred and open**. For `universal.activity_trace` / `universal.secondary_trace`, the bounds document labels text budget N/A, but Sparkline caption behavior exists: obtain precise instance binding and caption budget from HUD Runtime before claiming no text exists. `BindingPlaceholderWidget` is tooling-only and is not a production text path.

Source-reported validation: 676/676 dependency-free checks passing, including region containment, same-role non-overlap, dedicated flexible-secondary geometry and truncation fuzz; 14/14 typography tests. These are **reported results from supplied documentation**, not rerun by HUD Art.

## 4. Historical guidance supersession

| Historical claim | Status | Replacement authority |
|---|---|---|
| Dotted `media.viewport`, `hud.title`, `hud.media`, `hud.controls`, `hud.status`, `hud.scene`, `hud.shell`, `hud.emissive` as canonical region IDs | **SUPERSEDED** | 19 exact catalog names in §1; `HudRegionCatalog.cpp` at `557edc3` |
| `MEDIA_VIEWPORT`, `TOP_STATUS`, `SCENE_GRAPH` illustrative art labels as runtime IDs | **SUPERSEDED** | Same catalog |
| Independent raster media mask or runtime panel-mask assets required for clipping | **SUPERSEDED** | `MediaViewportMesh.{h,cpp}` at `557edc3`; Architecture HUD-006 freeze |
| A second Blueprint for `MinimalSlice` vs `FullProfile` | **SUPERSEDED** | One 19-region Blueprint; scopes differ only in presentation |
| Additional independent safe-area schema required for v1 | **SUPERSEDED** | Accepted region geometry and separation gaps |
| Blueprint ID/version were unapproved placeholders | **SUPERSEDED by later decision** | Architecture Freeze — HUD-006, `ecopunk-hud-blueprint` / `1`, exact matching |
| Final skin typography frozen by wireframe metrics | **NOT APPROVED** | Wireframe-only typography acceptance; final skin typography remains separate |

Retain and annotate older documents; do not overwrite their historical evidence.

## 5. Outstanding questions and verification requests

1. **HUD Runtime — region-by-region `MinimalSlice` membership: CLOSED.** Exact 13+6 mapping supplied and entered in §1. Source: `HudWireframeRenderer.cpp:33–41, 365–370, 388–395`; `HudWireframeRenderer.h:197`; `HudCompositorBridge.cpp:3–14`, HEAD `dfbe169`, underlying HUD source `61a4acf`.
2. **HUD Runtime — optional active-binding fidelity follow-up:** if art-production guidance later needs exact binding IDs or concrete widget instances, provide the presentation-profile binding configuration. Catalog eligibility and region purpose are documented; active bindings are **not asserted** by this crosswalk. This does not block the documentation-only 19-region crosswalk.
3. **HUD Runtime — `universal.timing`:** document its widget inset and text budget when scheduled; currently deferred and nonblocking for Blueprint identity.
4. **HUD Runtime — visual review:** confirm shell/decorative/emissive layering does not obscure protected text, controls, overlay regions or viewport bevel. Art should use a region-outline and text-live-area overlay, not invent extra geometry.
5. **Architecture — provenance:** record the approved identity freeze in the canonical repository/Work Registry and accurately track local-vs-remote durability; no HUD Art commit is authorized.
6. **HUD-004/Pi (separate):** reconcile package export profiles, atlas and emissive budget after Runtime package contract and measured Pi limits are available. These do not reopen HUD-006 identity.

## 6. Review disposition / next increment

**HUD Runtime alignment incorporated; completed documentation-only crosswalk returned to Architecture for review, copying HUD Runtime.** All 19 geometry rectangles, 13+6 presentation membership, wireframe text budgets, media clipping and safe-area rules are documented. Exact active binding IDs remain deliberately unasserted, and `universal.timing` inset/text budget remains deferred. Architecture should not treat this document as a new Blueprint identity or compatibility contract.

**No asset production, runtime code, validator, loader, deployment, staging, commits or pushes.**
