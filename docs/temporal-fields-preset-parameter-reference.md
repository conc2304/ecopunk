# Temporal Fields — Preset Parameter Reference

Reference for generating `bin/data/presets/*.json` preset files for the
`temporal-fields` sketch. Written for handoff to an agent that will design
a set of presets — every parameter below is what's actually declared in
`TFParameterPanel.cpp::setup()` (source of truth), not inferred.

This covers the static single-snapshot preset format only. For a preset
that instead describes a sequence of named states (arrival/breathing/
activity/rest, ...) the live dials transition between and hold at over
time, see
[temporal-fields-preset-timeline-format.md](temporal-fields-preset-timeline-format.md)
— it's an additive, optional `"timeline"` object layered on top of exactly
this same base format, and every parameter/range below still applies.

## File format

One preset = one JSON file at `bin/data/presets/<name>.json`. Top-level
shape (see the shipped example, `preset_001.json`):

```json
{
  "pattern": "bsp",                 // which pattern is active; see key list below
  "bsp": { ... },                   // only the active pattern's group is needed —
  "transition": { ... },             // shared hard-cut/crossfade/erosion weights
  "patternregen": { ... },           // shared regen-rate (Bands/ColumnGrid/TelescopingFrames only)
  "background": { ... }              // background layer + nested "Effects" sub-object
}
```

**Every top-level key except `"pattern"` is optional.** `loadNextPreset()`
(`TFParameterPanel.cpp`) only deserializes a group `if (json.contains(key))`
— a missing key is simply skipped and leaves those dials at whatever value
they already had; there's no required-keys check and nothing crashes. So a
preset only needs to include the active pattern's own group (named by
`"pattern"`), plus whichever of `transition`/`patternregen`/`background` it
actually wants to override — **omit the other eight pattern groups
entirely** rather than padding the file with inert defaults. This keeps
preset files small and readable: a Bands preset should contain `pattern`,
`bands`, and whatever subset of `transition`/`patternregen`/`background` it
cares about — not the other eight patterns' full parameter sets.

`"hudvisibility"` is likewise optional (which HUD widgets are shown) and
isn't part of the "composition shape" the patterns/background control —
skip it unless specifically asked to also design HUD visibility.

**Important serialization quirk:** every leaf value is a JSON **string**,
not a native number/bool, e.g. `"Cell_Density": "0.0714402"`,
`"Mask_To_Blob": "1"`. This is `ofSerialize`'s behavior (`parameter.toString()`),
not a bug — generated presets must follow the same convention (quote every
value, including ints and the 0/1 bools).

**Key naming:** each JSON key is the parameter's GUI label with spaces
replaced by underscores (e.g. `"Diagonal Angle"` → `"Diagonal_Angle"`).
The exact label is given as `label` in the tables below — the JSON key is
always `label` with spaces → `_`.

### `pattern` values (which key selects which pattern)

| `pattern` value | Pattern |
|---|---|
| `bsp` | BSP (recursive rectangular subdivision) |
| `blobgrid` | Blob Grid (metaball-masked grid) |
| `bands` | Bands (parallel strips) |
| `columngrid` | Column Grid (masonry/ledger columns) |
| `telescopingframes` | Telescoping Frames (concentric rings) |
| `particlefield` | Particle Field (floating spawned crops) |
| `ecologicalsuccession` | Ecological Succession (aging patch grid) |
| `networkgrowth` | Network Growth (branching tree) |
| `temporaltides` | Temporal Tides (continuous sine sweep) |

Each row below: **label** (→ JSON key) — type, `[min, max]`, default — meaning.

---

## bsp

Recursive rectangular subdivision; each leaf samples drifting Perlin noise
for its playhead offset — gives an "amorphic," organic-blob quality despite
being rectangle-based.

- **Irregularity** — float, `[0, 1]`, 0.55 — spread of split ratio around center; 0 = even bisection, 1 = wildly lopsided splits.
- **Cell Density** — float, `[0.01, 0.3]`, 0.06 — minimum leaf size as a fraction of the shorter canvas edge; lower = finer/denser subdivision.
- **Geometry Reshuffle Rate** — float, `[0.5, 10]` seconds, 3.0 — interval between piecemeal re-partitions of the tree.
- **Regions Touched Per Tick** — int, `[1, 10]`, 1 — how many leaves get re-partitioned per reshuffle tick.
- **Transparency Amount** — float, `[0, 1]`, 0.15 — fraction of leaves suppressed (via a second noise channel) so a background layer shows through.

## blobgrid

Square grid masked by a drifting metaball field; cells outside the field
are true holes, cells near the boundary feather, cells deep inside merge
into larger blocks.

- **Grid Resolution** — int, `[4, 40]`, 18 — base grid cols/rows.
- **Blob Centers** — int, `[1, 8]`, 3 — number of metaball centers.
- **Drift Speed** — float, `[0, 3]`, 1.0 — how fast the blob field moves.
- **Blob Radius** — float, `[0.05, 0.6]`, 0.28 — normalized radius (fraction of shorter canvas edge).
- **Edge Softness** — float, `[0, 1]`, 0.30 — feather width at the blob boundary.
- **Size Variation** — float, `[0, 1]`, 0.50 — 0 = uniform grid cells, 1 = merged blocks up to ~7x base cell size.
- **Fragment Refresh Rate** — float, `[0.5, 10]` seconds, 3.0 — interval between quadtree re-quantizations.
- **Mask To Blob** — bool, 1 — whether cells outside the blob field are hidden (true holes) vs. always drawn.
- **Transparent Background** — bool, 0 — whether the pattern itself is transparent where masked out.

## bands

Parallel strips — vertical, horizontal, an axis-flip split (one zone of
each), or diagonal — with randomized widths; regenerates wholesale on
`patternregen`'s clock.

- **Orientation** — enum int, `[0, 3]`, 0 — 0 Vertical / 1 Horizontal / 2 Axis Flip / 3 Diagonal.
- **Band Count** — int, `[2, 40]`, 10 — bands per zone (relevant for Axis Flip too).
- **Width Variation** — float, `[0, 1]`, 0.3 — 0 = even widths, 1 = highly uneven.
- **Diagonal Angle** — float, `[0, 90]` degrees, 30.0 — only matters when Orientation = Diagonal.
- **Offset Mode** — enum int, `[0, 1]`, 0 — 0 Dynamic (offsets drift/reassign continuously) / 1 Strata (offset fixed at regen time, proportional to band index — a layered/sedimentary look).

## columngrid

Fixed equal-width vertical columns, each independently subdivided into
randomized-height rows; regenerates wholesale on `patternregen`'s clock.

- **Column Count** — int, `[1, 20]`, 6.
- **Rows Per Column** — int, `[1, 20]`, 6 — target row count (Brick Offset can add one partial extra row).
- **Row Height Variation** — float, `[0, 1]`, 0.35 — 0 = even rows, 1 = highly uneven.
- **Brick Offset** — bool, true — true: rows stagger between adjacent columns by half a row-height (masonry/running-bond look); false: each column is fully independent (ledger/spreadsheet look).

## telescopingframes

Concentric rectangular rings nested from full-canvas size inward to a solid
center; regenerates wholesale on `patternregen`'s clock.

- **Ring Count** — int, `[1, 20]`, 6.
- **Thickness Variation** — float, `[0, 1]`, 0.35 — 0 = even ring thickness, 1 = highly uneven.

## particlefield

Structurally distinct: free-floating, overlapping, spawning/aging/dying
square crops (Resolume-particle-style), not a persistent reassigning grid.

- **Spawn Rate** — float, `[0.1, 20]` particles/sec, 3.0.
- **Max Particle Count** — int, `[1, 200]`, 60 — hard cap on live particles.
- **Min Size** / **Max Size** — float, `[0.01, 0.5]`, 0.05 / 0.18 — fraction of shorter canvas edge.
- **Min Life** — float, `[0.5, 20]` sec, 3.0. **Max Life** — float, `[0.5, 30]` sec, 8.0.
- **Drift Speed** — float, `[0, 5]`, 1.0.
- **Drift Direction** — enum int, `[0, 2]`, 0 — 0 Omnidirectional / 1 Upward / 2 Downward.
- **Rotation Speed** — float, `[0, 180]` deg/sec, 0 — 0 = no rotation.
- **Depth Order** — enum int, `[0, 1]`, 0 — 0 Newest On Top / 1 Largest Behind.

## ecologicalsuccession

Uniform grid of patches that age independently; a patch's maturity
(age / Maturity Time) drives its reassignment rate, its transition style
(young→hard cut, climax→erosion), and its render size (grows from ~25% at
sprout to 100% at climax). Below Sprout Threshold, patches don't draw at
all. Periodic disturbances reset patches within a radius back to age zero.

- **Grid Resolution** — int, `[4, 30]`, 12.
- **Maturity Time** — float, `[5, 120]` sec, 45 — seconds to reach full climax.
- **Young Turnover** — float, `[0.2, 10]` sec, 2.0 — reassignment interval at maturity 0.
- **Climax Turnover** — float, `[2, 60]` sec, 20 — reassignment interval at maturity 1.
- **Young Hard Cut Weight** — float, `[0, 100]`, — hard-cut transition weight at maturity 0 (lerps toward 0 as it matures).
- **Climax Erosion Weight** — float, `[0, 100]`, — erosion transition weight at maturity 1 (lerps up from 0).
- **Crossfade Weight** — float, `[0, 100]`, — constant across maturity.
- **Sprout Threshold** — float, `[0, 0.5]` — maturity below which a patch doesn't draw at all (true hole).
- **Min Render Scale** — float, `[0.05, 1]` — inset size of a patch at the sprout threshold (grows to 1.0 at full climax).
- **Disturbance Interval** — float, `[2, 60]` sec — time between disturbance events.
- **Disturbance Radius** — float, `[0.02, 0.6]` — normalized radius of each disturbance.
- **Disturbance Flash Duration** — float, `[0.1, 3]` sec — how long the "Quarantine Hatch" flash visual lasts on affected patches.

## networkgrowth

Tree-like branching structure: small node fragments connected by persistent
edge lines. Grows from 1-3 seed nodes near center; on a growth clock, a
random living node spawns a child at a jittered distance/angle.

- **Seed Node Count** — int, `[1, 3]`, 2.
- **Growth Interval** — float, `[0.1, 10]` sec — seconds between branch attempts.
- **Max Node Count** — int, `[2, 200]`, 60 — cap on currently-alive nodes; growth pauses above this.
- **Branch Distance** — float, `[0.01, 0.3]` — normalized distance from parent to new child.
- **Branch Distance Jitter** — float, `[0, 1]` — randomization of that distance.
- **Branch Angle Jitter** — float, `[0, 180]` degrees — spread around the parent's outward-facing direction.
- **Min Size** / **Max Size** — float, `[0.01, 0.3]` — each node's size is randomized in this range at spawn and fixed for its lifetime.
- **Node Reassign Interval** — float, `[0.5, 30]` sec — how often a node's own playhead reassigns.
- **Node Lifespan Enabled** — bool — whether nodes die of old age.
- **Max Node Age** — float, `[2, 120]` sec — only relevant if lifespan is enabled.
- **Edge Thickness** — float, `[0.5, 6]` px.
- **Rectangular Chance** — float, `[0, 100]` % — chance a node spawns rectangular instead of square.
- **Min Aspect Ratio** / **Max Aspect Ratio** — float, `[1, 5]` — long-side:short-side ratio for rectangular nodes, randomized at spawn; which dimension is the long side is a per-node coin flip.
- **Crop Chance** — float, `[0, 100]` % — for square nodes only, chance of a positional crop of the source frame (same mechanism as BSP/Blob Grid) instead of the whole buffered frame scaled to fit. Rectangular nodes ignore this and always crop.

## temporaltides

No discrete transitions at all — a cell's playhead offset is a continuous
function of position and time (a sine sweep). Cells below Exposed Threshold
are dry/exposed and are true holes (hard cutoff, no crossfade).

- **Grid Resolution** — int, `[4, 40]`, 16.
- **Tide Speed** — float, `[0, 3]`, 0.3 — sweep speed over time.
- **Wave Length** — float, `[0.1, 5]`, 1.0 — spatial frequency of the sweep.
- **Amplitude** — float, `[0, 1]`, 1.0.
- **Wave Direction** — enum int, `[0, 2]`, 0 — 0 Horizontal / 1 Vertical / 2 Diagonal.
- **Exposed Threshold** — float, `[0, 1]`, 0.35 — below this wave value, a cell is exposed/dry and doesn't draw.

---

## Shared groups (present in every preset regardless of active pattern)

### transition

Global fragment-transition tuning — shared "spirit" copy is duplicated per
pattern above (each pattern's own hard-cut/crossfade/erosion weights), but
this top-level group is the one Bands/ColumnGrid/TelescopingFrames/BSP/
BlobGrid transition system reads besides its own per-pattern copy.

- **Transition Duration** — float, `[0.1, 3]` sec — how long a fragment's transition animation takes.
- **Hard Cut Weight** — float, `[0, 100]` — relative weight of instant hard-cut style.
- **Crossfade Weight** — float, `[0, 100]` — relative weight of crossfade style.
- **Erosion Weight** — float, `[0, 100]` — relative weight of erosion (dissolve) style.
- **Quantize Bands** — int, `[8, 16]` — number of discrete time-offset bands the playhead pool quantizes to.
- **Max History Seconds** — float, `[2, 20]` — how far back in time the most-delayed slice can reach (CPU RAM ring buffer; bigger = more pronounced time-lag effect, smaller = subtler).

The three weights (Hard Cut / Crossfade / Erosion) are relative, not
required to sum to 100 — they're normalized at use time.

### patternregen

- **Pattern Regen Rate** — float, `[0.5, 30]` sec — shared wholesale-regeneration clock for Bands, Column Grid, and Telescoping Frames only (BSP has its own Geometry Reshuffle Rate, Blob Grid its own Fragment Refresh Rate, Particle Field doesn't use this at all).

### background

Background layer: a rotating mix of full video / full image / split
video+image, plus a nested `Effects` sub-object of visual-effect weights.

- **Full Video Weight** — float, `[0, 100]` — relative chance of full-canvas video mode.
- **Full Image Weight** — float, `[0, 100]` — relative chance of full-canvas still-image mode.
- **Split Weight** — float, `[0, 100]` — relative chance of a split video/image mode.
- **Mode Change Interval** — float, `[5, 120]` sec — how often the background re-picks a mode.
- **Split Ratio** — float, `[0.2, 0.8]` — fraction of canvas given to one side of a split.
- **Split Axis Choice** — enum int, `[0, 2]` — 0 Random / 1 Vertical / 2 Horizontal.
- **Image Cycle Interval** — float, `[1, 60]` sec — how often the still image changes.
- **Image Fade Duration** — float, `[0.1, 10]` sec — crossfade duration between images.

#### background → Effects (nested object under `"background"."Effects"`)

- **Cycle Interval** — float, `[1, 30]` sec — how often the active effect is re-picked.
- **Raw Weight** — float, `[0, 100]` — relative chance of no effect (passthrough).
- Then one weight per effect, all float `[0, 100]`, all relative (picked via weighted random, not required to sum to 100): **Desaturate, Invert, Recolor, Threshold, Dither, Solarize, Scanlines, Channelshift, Hue Rotate, Ascii Solarpunk, Bioluminescence, Chromatic Aberration, Edge Glow, Ink Outlines, Pixel Drift, Pixel Sorting, Water Refraction**.

---

## Practical notes for generating presets

1. Only include the active pattern's own group (the one named by
   `"pattern"`), plus whichever of `transition`/`patternregen`/`background`
   the preset actually wants to set. Leave out the other eight pattern
   groups — a missing key is skipped on load (dials just keep their prior
   value), so there's no need to pad every preset with all nine patterns'
   full parameter sets.
2. Corollary of (1): omitted groups aren't reset to defaults, they just
   keep whatever value was last loaded (from an earlier preset, or the
   in-code defaults if nothing's touched them yet) — since only the
   *active* pattern's group and the shared groups affect what's actually
   drawn, this is harmless, but it does mean two loads of a minimal preset
   aren't guaranteed byte-identical in their inactive-pattern dial state.
   Don't design around inactive-pattern values persisting or resetting —
   treat them as "don't care."
3. All values are quoted strings in the JSON, including ints/enums/bools
   (bools as `"0"`/`"1"`).
4. Enum ints must stay within their listed range — e.g. `bandsOrientation`
   is `[0,3]`, not a boolean.
5. The three transition-style weights (Hard Cut / Crossfade / Erosion) are
   relative weights, not percentages that must sum to 100 — same for the
   background-effect weights.
6. See `bin/data/presets/preset_001.json` for one concrete, valid example
   of the full schema (note it predates this minimal-file guidance and
   still includes an inactive `blobgrid` group — fine as a schema
   reference, just not itself a minimal example).
