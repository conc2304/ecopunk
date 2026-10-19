# HUD Blueprint v1 — HUD Art reference exports

These are reference images for authoring skins against `ecopunk-hud-blueprint` / `1` (HUD-006, DEC-019). Nothing here is runtime-loaded. They are **not** skin assets and do not belong in `assets/hud/skins/`.

All geometry is taken from the **live production renderer** at export time:
- `HudWireframeRenderer::regionCatalog()`
- `pixelBoundsFor()`
- `regionInMinimalSlice()` / `productionProvenance()` (one production allowlist, split by provenance: historical MinimalSlice, Living HUD v0.1, HUD Clock v0.1, Mode Navigation v0.1)
- `mediaViewportMesh()`

No layout is re-typed. `regions.json` is a **derived** export of that catalog, not an independent definition. If it ever disagrees with `shared/src/hud-compositor/HudRegionCatalog.cpp`, the source wins and these files must be regenerated.

## Files

| File | Format | What it is |
|---|---|---|
| `hud_reference_wireframe_1280x720.png` | 1280×720 RGBA, transparent | Clean structural outlines: the 18 region rectangles plus the real `MediaViewportMesh` silhouette (green). No scene content, skin or text. Layer it over artwork. |
| `hud_reference_schematic_1280x720.png` | 1280×720 RGB, opaque | All 19 regions with index, stable ID, exact px bounds and role. Key: **teal** = historical 13-region MinimalSlice, **magenta** = Overlay (part of the historical 13), **green** = Living HUD v0.1 (`universal.effect_summary`, `universal.timing`), **blue** = HUD Clock v0.1 (`flexible.secondary_b`), **yellow** = Mode Navigation v0.1 (`flexible.secondary_a`), **amber** = FullProfile only. |
| `hud_reference_production_1280x720.png` | 1280×720 RGB, opaque | **Current production presentation**: the 17 regions production actually draws, filled by provenance, with the 2 FullProfile-only regions greyed and marked "not drawn". Notes what live text each new region carries. |
| `hud_reference_constraints_1280x720.png` | 1280×720 RGBA, translucent | Artwork constraint map (key below). Layer it over artwork. |
| `regions.json` | JSON | Derived catalog: id, role, normalized and px bounds, scope, `production` flag, viewport mesh facts, and `presentation` counts (19 canonical / 13 historical MinimalSlice / 17 production = 13 + 2 Living HUD + 1 clock + 1 mode / 2 FullProfile only) |
| `runtime_wireframe_reference_1280x720.png` | 1280×720 | A real ExperienceRuntime frame (no skin) captured during the export run, used for comparison |
| `verification.txt` | text | The export run's PASS/FAIL log |

## Current production presentation (17 of 19)

The historical **13-region MinimalSlice** is the first vertical slice (12 binding regions + the always-drawn `media_viewport`). Production has since added:

| Addition | Region | Live content |
|---|---|---|
| Living HUD v0.1 | `universal.effect_summary` | active effect chips + `EFFECTS <health>` badge |
| Living HUD v0.1 | `universal.timing` | `ACTIVE TIME` / scene active time |
| HUD Clock v0.1 | `flexible.secondary_b` | local date + zone (`THU 15 OCT 2026 MDT`) / `HH:MM` |
| Mode Navigation v0.1 | `flexible.secondary_a` | `MODE i/n` / mode name (up to 23 glyphs, e.g. `EFFECT PARAM VARIANT`); also the NEXT MODE focus stop (focus brackets 3 px outside the rect) |

`universal.scene_title` also carries a live `  i/n` scene position after the scene name. `universal.activity_trace` and `universal.secondary_trace` remain FullProfile only.

**All of this text is drawn live by the runtime. Never bake any of it into skin art.** Keep the 25.6 px gap between `flexible.secondary_a` and `_b` free of ornament: focus brackets on the mode panel extend into it.

## Regenerate

```bash
cd sketches/experience_runtime
make Release
env -u EXPERIENCE_RUNTIME_HUD_SKIN EXPERIENCE_RUNTIME_HUD_ART_EXPORT=1 \
  ./bin/experience_runtime.app/Contents/MacOS/experience_runtime
# optional: EXPERIENCE_RUNTIME_HUD_ART_EXPORT_DIR=/some/other/dir
```

- **The run needs an active desktop session.** GLFW cannot open a window while the session is idle or locked.
- **Read the result from `verification.txt`.** The `RESULT:` line there is authoritative. The process exit code is always 0, a pre-existing issue in `main.cpp`.

## Constraint map key, and its sources

| Colour | Zone | Rule | Source |
|---|---|---|---|
| Red | Media silhouette (`MediaViewportMesh`: radius 18 at top-left, top-right and bottom-left; 42 px 45° bevel at bottom-right) | Keep fully transparent; no raster media mask | HUD-004 spec §1, §4; HUD-006 crosswalk §2 |
| Orange | Live-content regions (16: every region except the media viewport and the 2 overlays) | Runtime widgets draw here. A background panel is fine, because it sits under the widgets; no ornament or emission | Crosswalk §1 ("keep artwork/ornaments out of widget live content") |
| Magenta | Overlay regions (`overlay.health`, `overlay.transition`) | Transparent over media; protect live text | Crosswalk §1 |
| Yellow | Inside the viewport bounds but outside the mesh (bevel and corner wedges) | **No explicit rule exists**; see below | — |
| Unmarked | Margins, gutters, the gap between the viewport and the right column | Only rule: preserve the region rectangles and the gaps between them | DEC-019 (v1 safe area = region rects + separation gaps) |

**Measured separation gaps** (from `regions.json`):

| Between | Gap (px) |
|---|---|
| Right-column regions | 7.2 each, **except `universal.secondary_trace` → `flexible.primary`: 0** (they share an edge) |
| `flexible.primary` → secondary row | 7.2 |
| `flexible.secondary_a` → `_b` | 25.6 |
| Controls | 25.6 / 51.2 / 25.6 / 51.2 |
| Viewport → right column | 51.2 |
| Viewport → controls row | 28.8 |
| `overlay.health` → `overlay.transition` | 7.2 |

`overlay.transition` overlaps the viewport top by 14.4 px, by design. Canvas margins are left 25.6, top 21.6, right 25.6 and bottom 28.8.

## Missing authoring rules (not invented here)

1. **No safe-area insets beyond the region rects.** DEC-019 defines the v1 safe area as the region rectangles plus their gaps. No inset, padding or "keep-clear" margin exists for art around a region.
2. **No rule for the bevel and corner wedges** inside the viewport bounds but outside the mesh. Media never draws there. Nothing says whether frame art may fill them; the accepted Dark Moss candidate does fill them.
3. **No rule for the gaps themselves.** "Preserve the gaps" does not say whether art may sit *in* a gap (frame lines, ornament) or must leave it visually open. At `secondary_trace` / `flexible.primary` the gap is 0, so any separator art must sit inside one of the two regions.
4. **Live-content insets vary by widget type** (`hud-wireframe-bounds-v1.md` §1 and §1a, typically about 1.44 px). They are not drawn here, because a region can host several widget types. The orange zone is therefore the full region rectangle, which is the conservative choice.
5. **`universal.timing` text budget** is still undocumented: a deferred, nonblocking HUD-006 follow-up.
6. **Scope-dependent treatment.** Four regions (`universal.activity_trace`, `universal.secondary_trace`, `flexible.secondary_a`, `flexible.secondary_b`) are drawn only in FullProfile; Living HUD v0.1 moved `universal.effect_summary` and `universal.timing` into production. There is no rule for how art should treat regions that are empty in production; this is presentation policy, not geometry.
