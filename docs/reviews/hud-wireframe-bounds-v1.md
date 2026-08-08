# HUD Wireframe Bounds v1

Status: Architecture-Closure Session deliverable. Exact region geometry
for the canonical 1280×720 wireframe, plus the programmatic containment
checks that validate it. Canonical wireframe geometry (region shapes,
`MediaViewportMesh`) is preserved unchanged from Engineering Session 2 —
this document records it precisely, it does not redesign it. No
containment defect was found; nothing here required escalation.

## 1. Region table

All bounds computed from `HudRegionCatalog`'s normalized `[0,1]` fractions
at the canonical 1280×720 canvas. "Live-content inset" is each widget's
own padding convention at that region's pixel size (see
`hud-typography-metrics-v1.md` §3 for the per-widget formulas this derives
from). "Overlap policy" states this region's role-class exemption from
the no-overlap check in §3.

| Region | x | y | w | h | Live-content inset (L/T/R/B, px) | Allowed widget(s) | Max occupancy | Typography tier | Z/order | Overlap policy |
|---|---|---|---|---|---|---|---|---|---|---|
| `universal.scene_title` | 844.8 | 21.6 | 409.6 | 43.2 | 1.44 / 0 / 1.44 / 0 | Label (+BindingPlaceholder) | 1 | Fixed native (see typography doc) | Chrome | No same-role overlap |
| `universal.media_title` | 844.8 | 72.0 | 409.6 | 36.0 | 1.2 / 0 / 1.2 / 0 | Label | 1 | Fixed native | Chrome | No same-role overlap |
| `universal.primary_state` | 844.8 | 115.2 | 409.6 | 43.2 | 1.44 / 0 / 1.44 / 0 | StatusBadge | 1 | Fixed native | Chrome | No same-role overlap |
| `universal.activity` | 844.8 | 165.6 | 409.6 | 43.2 | 0 (ProgressBar) | NumericValue, ProgressBar | 1 | Fixed native | Chrome | No same-role overlap |
| `universal.effect_summary` | 844.8 | 216.0 | 409.6 | 43.2 | per-slot, see below | EffectChips, StatusBadge | 2 (vertically sliced) | Fixed native | Chrome | No same-role overlap |
| `universal.timing` | 844.8 | 266.4 | 409.6 | 36.0 | not yet audited | NumericValue, Label | 1 | Fixed native | Chrome | No same-role overlap |
| `universal.activity_trace` | 844.8 | 309.6 | 409.6 | 72.0 | N/A (Sparkline, no text budget) | Sparkline | 1 | N/A | Chrome | No same-role overlap |
| `universal.secondary_trace` | 844.8 | 388.8 | 409.6 | 72.0 | N/A | Sparkline | 1 | N/A | Chrome | No same-role overlap |
| `media_viewport` | 25.6 | 115.2 | 768.0 | 504.0 | N/A (mesh, not text) | (none — `MediaViewportMesh`, drawn directly, not through the binding pipeline) | 0 bindings / 1 mesh | N/A | Content (drawn first) | Overlaid by `overlay.*` regions by design |
| `controls.scene_previous` | 25.6 | 648.0 | 102.4 | 43.2 | 1.44 / 0 / 1.44 / 0 | Label | 1 | Fixed native | Chrome | No same-role overlap |
| `controls.scene_next` | 153.6 | 648.0 | 102.4 | 43.2 | 1.44 / 0 / 1.44 / 0 | Label | 1 | Fixed native | Chrome | No same-role overlap |
| `controls.media_previous` | 307.2 | 648.0 | 102.4 | 43.2 | 1.44 / 0 / 1.44 / 0 | Label | 1 | Fixed native | Chrome | No same-role overlap |
| `controls.media_next` | 435.2 | 648.0 | 102.4 | 43.2 | 1.44 / 0 / 1.44 / 0 | Label | 1 | Fixed native | Chrome | No same-role overlap |
| `controls.reseed` | 588.8 | 648.0 | 128.0 | 43.2 | 1.44 / 0 / 1.44 / 0 | Label | 1 | Fixed native | Chrome | No same-role overlap |
| `flexible.primary` | 844.8 | 460.8 | 409.6 | 115.2 | 5.76 / — (MetadataCard) | MetadataCard, ChannelStrip, EffectChips, ProgressRing, Sparkline, Timeline, AmbientField | 1 (mutually exclusive — `HudProfileCompiler`'s own occupancy rule) | Fixed native | Chrome | No same-role overlap |
| `flexible.secondary_a` | 844.8 | 583.2 | 192.0 | 43.2 | not yet audited | NumericValue, ProgressBar, ProgressRing, StatusBadge, Sparkline, AmbientField | 1 | Fixed native | Chrome | No same-role overlap |
| `flexible.secondary_b` | 1062.4 | 583.2 | 192.0 | 43.2 | not yet audited | NumericValue, ProgressBar, ProgressRing, StatusBadge, Sparkline, AmbientField | 1 | Fixed native | Chrome | No same-role overlap |
| `overlay.health` | 25.6 | 21.6 | 256.0 | 43.2 | 1.44 / 0 / 1.44 / 0 | StatusBadge, Label | 1 | Fixed native | Overlay (drawn atop content) | **Exempt** — designed to sit atop `media_viewport` |
| `overlay.transition` | 25.6 | 72.0 | 256.0 | 57.6 | per-slot (3-way vertical split, 19.2px each) | ProgressBar, Label | 3 (vertically sliced) | Fixed native | Overlay | **Exempt** — designed to sit atop `media_viewport` |

`universal.effect_summary`'s 2-way vertical split: chips slot
`(844.8, 216.0, 409.6, 21.6)`, health slot `(844.8, 237.6, 409.6, 21.6)`.

`overlay.transition`'s 3-way vertical split: progress-bar slot
`(25.6, 72.0, 256.0, 19.2)`, phase-label slot `(25.6, 91.2, 256.0, 19.2)`,
message-label slot `(25.6, 110.4, 256.0, 19.2)`.

## 2. `MediaViewportMesh` geometry (preserved, not redesigned)

Unchanged from Engineering Session 2: rounded corners (default
`cornerRadius = 18.0f`, clamped to ≤40% of the shorter bounds side),
45° lower-right bevel (default `bevelSize = 42.0f`, same clamp), arc
tessellation at `degreesPerArcStep = 6.0f`. At `media_viewport`'s
768.0×504.0 bounds, both clamps are inactive (18 and 42 are well under
40% of 504). No defect was found in this geometry this session — the two
real bugs found and fixed (ARB-texture UV sampling, inert-`ofScale()`
truncation) were both rendering/measurement bugs, not geometry defects;
the triangulated shape itself (54 vertices / 52 triangles at these
bounds) is unchanged.

## 3. Programmatic validation

`shared/src/hud-compositor-test/hud_presentation_tests.cpp` —
`test_region_bounds_within_canvas_and_no_same_role_overlap()`:

- Every region's pixel bounds (at 1280×720) fall fully within the canvas
  — no region defined partly or fully off-canvas.
- No two regions of the **same role** (`Universal`, `Controls`,
  `Flexible`) overlap each other. `Overlay`-role regions are exempt by
  design (see §1's overlap-policy column) — verified separately by visual
  inspection (every screenshot with `overlay.health`/`overlay.transition`
  content shows it correctly composited atop the media viewport, never
  clipped or corrupted by it).

`test_truncation_never_exceeds_budget()` — the structural proof behind
"text measured bounds ⊆ text live bounds": fuzzes
`HudTextMetricsCache::truncateToWidth()` across 7 representative strings
(empty, single-char, short caption, two deliberately-overlong strings,
an identifier-shaped string, a long dotted identifier) × 11 budgets
(0 through 1000px, including the exact 406.72/253.12/99.52px budgets real
regions actually use), asserting the returned width never exceeds the
requested budget. 77 checks, all passing.

Both are part of the dependency-free suite (`make -f Makefile.tests test`)
— 660/660 checks passing, up from 515 before this session's bounds/
truncation-fuzz additions (145 new checks: region containment/overlap
pairs + the truncation fuzz matrix).

No widget-bounds-vs-region-bounds violation was found. No canonical
wireframe geometry change was required or made.
