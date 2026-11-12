# Blueprint Emergence — EDITORIAL_COLLAGE (continuously evolving)

A composition grammar inside `blueprint_emergence`, not a seventh scene. In the Ecopunk
runtime it is the **third entry of the Blueprint mode selection**
(`ghost_layers → perpetual → editorial_collage` via NEXT MODE); the standalone sketch
starts in it (key `c` toggles back to the classic cycle).

## Why the first version felt static

The first implementation generated one `CollageLayout` per cycle and drew every
element as a pure function of a single clock (`visibility(stage, delay, dur)`).
Everything entered by ~t=12 s, then nothing changed except a hard crop swap every
13–24 s: a long frozen dwell. There was no per-element lifecycle and nothing could
be added, moved, split or removed after entry.

## Model

| Piece | Where | Knows about Blueprint? |
|---|---|---|
| Palette + primitives (`GraphicBlock`, `GraphicFrame`, `TechnicalGrid`, `TickRail`, `RegistrationMark`, `DataBar`, `CollagePalette`) | `shared/src/hud/graphics/` (umbrella `GraphicPrimitives.h`) | No |
| UV transform math (`QuarterTurn`, `makeCollageUvQuad`) | `BECollageTransform.h` (GL-free) | Sketch-local |
| Tuning surface + profiles | `BECollageTuning.h` | Sketch-local |
| Registry, director, mutations, drawing | `BECollage.{h,cpp}` | Yes |
| Hooks | `BEComposition` (forced mode, spawn suspension, dissolve gate), `BlueprintSceneCore` | Yes |

`hud/HudElements.h` and every runtime contract (`IEcopunkScene`, `SceneFrame`,
`HudFrameData`, HUD compositor, video/effect selection) are untouched. The only shared
HUD file touched is one additive vocabulary entry (`editorial_collage`) in
`HudVocabularyResolver.cpp`.

### Persistent element registry

`CollageElement` (video or graphic): stable id, role/type, animated rect, `CollageCrop`
(source centre, scale, quarter-turn, flipX/flipY) with the previous crop for wipe swaps,
frame/overlay/opacity tweens, z-layer, group, anchor flag and its own lifecycle:
`Pending → Entering → Active → (mutate ×N) → Exiting → retired`. Elements are never
regenerated wholesale; templates only seed roles.

### Video sampling

Source crop and destination rect are independent. One quad per fragment, UV corners
permuted (TL,TR,BR,BL; rotation is clockwise, flips in screen space) — no per-fragment
FBO. Crop aspect is derived from the destination (swapped for 90/270). Crop centres are
chosen best-of-10 far from the destination and from crops in use, inside the central band
of the frame. The one canonical playing texture is sampled every frame, so video motion
continues inside every living fragment.
Transform mix: 0° 40 / 90° 20 / 180° 10 / 270° 15 / flipX 7.5 / flipY 5 / rot+flip 2.5 (%).
Orientation changes happen only through a wipe cover (`TransformChange`), never free spin.

### Director (single seeded RNG, dt-driven)

* **Tier timers** (seeded jitter, scaled by activity level): micro ~1–3 s, local ~4–8 s,
  swell ~18–35 s, recomposition ~60–100 s (BALANCED).
* **Activity envelope**: `QUIET / ACTIVE / SWELL / SETTLE` (internal) with asymmetric
  rise/decay. A swell enqueues a staggered choreography (colour mass grows, an image
  splits/moves, another replaced, grid + tick wipes, merge/crop-swap, transform change);
  SETTLE thins excess graphics and consolidates crowded imagery.
* **Weighted, context-aware selection**: recency penalty per event, sparse layouts favour
  additions, crowded ones favour removal/merge; region and side-balance aware placement;
  a rejected event retries within ~0.4 s so scheduling never stalls.
* **Anchors**: a hero image and a large colour mass are exempt from replace/split/merge/move;
  the image anchor rotates every ~25–45 s; recomposition retains it.
* **Mutation vocabulary** — micro: overprint fade/coverage, frame edge draw/retract,
  grid reveal, tick extension, mark pulse/move, decorative data-bar update, add/remove
  micro graphics. Local: crop swap (wipe), move/resize, replace (new image enters before
  the old one leaves), transform change, **split** into 2–3 (children start in the parent
  footprint then diversify), **merge** of neighbours, block slide/expand/contract, strip
  replace, staggered montage. Swell: colour-mass growth + the above.
* **Honest data**: one `DATA_STRIP` is bound to live video coverage; others are
  decorative (labelled so here and in code).

### Templates (5)

`VERTICAL SPINE`, `SCIENTIFIC PLATE`, `BROKEN POSTER`, `ASYMMETRIC CROSS`,
`STACKED REGISTRATION` (index = `hash(seed) % 5` at the first plate; recomposition picks a
different grammar). Each generates under an overlap/in-canvas validity check.

### Lifecycle mapping (Blueprint phases)

The composition is pinned to `GHOST_LAYERS` (timer-driven arc) with spawning suspended.
The collage follows the phases but never waits on them:

* **cycle start** → first plate builds; later cycles **recompose** (anchor retained, rest
  retired in a 0–3.5 s stagger, new grammar's roles enter around it; palette cross-fades).
* **PLACEMENT** → establishing layers enter staggered (initial build ≈ 9 s).
* **DENSITY** → continuous evolution (the director runs in every phase).
* **DISSOLVE** → selective clear-out (≈70 % of non-anchor imagery, ≈55 % of non-residue
  graphics, staggered); spawning events are suppressed; gate opens after 4.5 s.
* **RESET_HOLD** → no special state; the collage keeps evolving at QUIET.

### Palette

`pickCollagePalette(seed)` — five curated families, ≤2 chromatic accents each. Elements
store `PaletteToken`s only, so no mutation can introduce a third accent; palette changes
cross-fade over 1.5 s.

## Tuning (developer-only)

`BECollageTuning.h` is the single surface: interval ranges, activity levels/dwells,
caps (`maxFragments/minFragments/maxGraphics/maxConcurrent/maxQueue`), cover thresholds,
transition durations, minimum holds, per-event weights, `lowQuality` (Pi fallback).
Profiles: **QUIET**, **BALANCED** (default), **KINETIC** — all keep evolving.
`BECollage::setProfile(name)`; standalone: `BE_COLLAGE_PROFILE=<name>` and key `p`.

## Dev tooling (standalone `ofApp` only)

* `BE_COLLAGE_SELFTEST=1` — logic tests (UV orientations, palette accents, seeded
  determinism, bounds/ids/cleanup/queue, no-stall cadence, profile ordering, low-quality
  caps, dt hitch clamp, lock/restart, dissolve gate, anchor retention, transform mix).
* `BE_COLLAGE_LIVE_DIR=<dir>` (+ `BE_COLLAGE_LIVE_INTERVAL`, `BE_COLLAGE_LIVE_SECONDS`,
  `BE_TIME_SCALE`) — runs the live lifecycle, saves PNGs, prints a timestamped event log.
* `BE_COLLAGE_CAPTURE_DIR=<dir>` — frozen seeds (3 per template).

## Not done / open

* Pi 3B performance is **unmeasured** (desktop only); `lowQuality` is a fallback, not a result.
* No per-fragment shared video-effect treatment (brief §8 step 9, optional).
* Runtime NEXT MODE shows the new `editorial_collage` mode (additive vocabulary entry).
