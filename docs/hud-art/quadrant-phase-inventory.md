# Quadrant Crosshair — phase inventory and icon brief (v0.3)

Source of truth: `sketches/quadrant-crosshair/src` (QuadrantManager, Quadrant,
ExpansionDirector, CrosshairSystem, TriggerBus, QuadrantSceneCore) and
`sketches/experience_runtime/src/QuadrantSemanticMapping.cpp`. Every phase below
exists in code today; none is invented for the HUD.

## What the HUD shows (v0.3)

The universal state badge (`universal.primary_state`, StatusBadge) reads:

```text
[icon] ONLINE                                   HUNT
 ^ reserved phase-icon slot    ^ phase (value)   ^ detail (caption colour)
```

- **value**: `scene.state.primary`, Quadrant's primary state (table A).
- **detail**: `scene.state.secondary`. During an expansion it is the expanding
  quadrant's active effect (shader name). Otherwise it is the crosshair strategy
  (table E). The strategy is also in the mode panel (NEXT MODE).
- **icon**: `scene.state.primary` again. The icon slot is keyed by the raw state
  ID. Today it draws `drawReservedIcon()`, an outlined square with a diagonal.

## A. Scene primary state (HUD badge value)

| Phase ID | Label | Meaning | Enters when / leaves when | Icon concept | Search keywords | Visual treatment |
|---|---|---|---|---|---|---|
| `scene.quadrant.state.online` | ONLINE | At least one quadrant is playing an effect | Any channel enters PLAYING. Leaves when no channel is PLAYING, or an expansion starts | Four-pane window, one pane lit | quadrant, grid, window, live, signal | Accent icon, steady; the default resting look |
| `scene.quadrant.state.quiet` | QUIET PHASE | No channel playing, at least one in its silence window | Last PLAYING channel goes SILENCING. Leaves on READY → PLAYING (silence 9–15 s per channel; 20% chance of a long 60–120 s rest when fewer than two others are silent) | Four panes, outlined only, with a small pause tick | rest, pause, silence, dormant, breath | Caption-colour icon, slow 0.25 Hz alpha breathing |
| `scene.quadrant.state.standby` | STANDBY | Every channel READY/idle (first frames, after reset) | Activation, before the first kick. Leaves on the first PLAYING | Dotted square with centre dot | standby, idle, power, wait | Muted icon, no animation |
| `scene.quadrant.state.expanding` | EXPANDING | ExpansionDirector TRAVEL_OUT or HOLD: one quadrant grows to fill the canvas | Timer (120–300 s), or a TriggerBus corner trigger. Leaves at TRAVEL_BACK | Arrow from a pane toward the outer corner | expand, maximize, zoom, enlarge, corner | Accent icon; arrow stroke follows `expansion_progress` |
| `scene.quadrant.state.reconfiguring` | RECONFIGURING | TRAVEL_BACK: the expanded quadrant contracts and the grid re-forms | HOLD ends (60–90 s). Leaves at IDLE (travel 6–20 s) | Four panes converging inward, or a circular arrow | contract, minimize, restore, rebuild, cycle | Accent icon at 60% alpha, inward motion |

## B. Per-channel cycle phase (QuadrantManager CyclePhase, ×4, the ChannelStrip)

| Phase ID | Label | Meaning | Transitions | Icon concept | Search keywords | Visual treatment |
|---|---|---|---|---|---|---|
| `scene.quadrant.state.online` (PLAYING) | ONLINE | The channel's shader is fading in, active, or fading out | READY → PLAYING when kickRandomShader runs. → SILENCING when all slots go idle (unless a trigger-interrupted shader resumes) | Filled pip | on, active, play | Accent pip |
| `scene.quadrant.state.quiet` (SILENCING) | QUIET PHASE | Waiting computeSilenceDuration(i) seconds before the next effect | → READY when silenceAcc ≥ silenceDur | Hollow pip | off, rest, gap | Caption pip |
| `scene.quadrant.state.standby` (READY) | STANDBY | Ready; kicked on the next update | → PLAYING on the next frame | Dotted pip | ready, queued | Muted pip |

## C. Per-quadrant layer state (Quadrant::State, internal)

IDLE → FADE_IN (`fadeDur` 1.2 s) → ACTIVE (dwell 6 s + resume) → FADE_OUT → IDLE.
These are drawn as the quadrant's alpha, and they roll up into B. They should not
get separate HUD icons, because that would duplicate B at a granularity visitors
can't read.

## D. Expansion (ExpansionDirector, `scene.quadrant.metric.expansion_state`)

| Phase ID | Label | Meaning | Transitions | Icon concept | Search keywords | Visual treatment |
|---|---|---|---|---|---|---|
| `idle` | (none) | Normal four-quadrant split | → travel_out after 120–300 s, or `triggerQuadrant()` | — | — | No icon (A shows ONLINE/QUIET) |
| `travel_out` | EXPANDING | Crosshair travels to the target corner (eased, 6–20 s) | → hold at t = 1 | Arrow to corner | expand, travel, move | `expansion_progress` drives the arrow length |
| `hold` | EXPANDING | The quadrant fills the canvas for 60–90 s | → travel_back | Maximized pane | fullscreen, hold, focus | Steady accent |
| `travel_back` | RECONFIGURING | Crosshair returns; the quadrant crossfades back (contraction) | → idle; the crosshair resumes noise over 2 s | Arrow from corner | return, restore, collapse | 60% accent |

## E. Strategy (CrosshairSystem preset; NEXT MODE; badge detail when not expanding)

| Phase ID | Label | Meaning | Transitions | Icon concept | Search keywords | Visual treatment |
|---|---|---|---|---|---|---|
| `motion_drift` / `…movement_preset.drift` | DRIFT | Slow, wide noise path (freq 0.04/0.009) | NEXT MODE ('m' or the mode panel + Enter) cycles DRIFT → SCAN → HUNT → NERVOUS → ORBIT → DRIFT | Wavy line | drift, wave, float, wander | Text detail; future icon in the mode panel |
| `motion_scan` | SCAN | Medium sweep (0.07/0.020) | (cycle) | Horizontal sweep line with ticks | scan, sweep, radar, search | 〃 |
| `motion_hunt` | HUNT | Fast, tight search (0.12/0.035) | (cycle) | Crosshair with an arrowhead | hunt, target, seek, track | 〃 |
| `motion_nervous` | NERVOUS | High-frequency jitter (0.18/0.004) | (cycle) | Zig-zag | jitter, nervous, erratic, shake | 〃 |
| `motion_orbit` | ORBIT | Looping path (0.05/0.051) | (cycle) | Ellipse with a dot | orbit, loop, circle, satellite | 〃 |

ENERGY (installation control) scales how fast each strategy's noise path
advances: 0.35× at ENERGY 0, 1× at 0.5 and 2.2× at 1. It doesn't change the
strategy or add a phase.

## F. Trigger events (TriggerBus; momentary, not phases)

EDGE_PROXIMITY (100 px), VELOCITY_HIGH (speed > 4 px per 60 Hz frame),
VELOCITY_LOW (< 0.8 for 3 s, 45 s cooldown), QUADRANT_CENTER (80 px),
CORNER_NEAR (150 px), DWELL (< 25 px movement in 6 s).

These re-deal effects. If the HUD ever shows them, it should be as a 0.3 s
accent flash on the badge icon, not as states.

## Icon recommendations (Ecopunk)

- Draw on a 24×24 grid, 1.5 px stroke, square caps, no fills except the "lit"
  pane or pip. Use the Dark Moss tokens: `accent` for live, `caption` for
  quiet, `muted` for standby. The HUD tints the icon, so the SVG should be
  monochrome (`currentColor`).
- Use organic and technical hybrids: lichen-like rounded corners on pane
  outlines, and survey/blueprint tick marks on arrows. Keep them legible at
  13 px, the badge slot size.
- Required now: the five from table A. Nice to have: the five strategies from
  table E, for the mode panel.

## Placeholder locations and SVG integration format

- Placeholder renderer: `shared/src/hud-compositor/widgets/HudWidgetDrawUtils.h`
  `drawReservedIcon(iconId, square, color)`. `"immersive"` draws corner brackets
  (the IMMERSIVE control). Any other ID draws the outlined square with a diagonal.
- Callers:
  - `widgets/StatusBadgeWidget.cpp`: the phase icon, keyed by the
    `scene.state.primary` ID.
  - `widgets/LabelWidget.cpp`: the `icon:<id>` vocabulary values; the IMMERSIVE
    control is `icon:immersive`.
- Proposed delivery format: one SVG per ID, at
  `shared/assets/hud-icons/<icon-id>.svg`. Use viewBox `0 0 24 24`, stroke only,
  `stroke="currentColor"`, no text, no transforms, and paths or
  lines/rects/circles only. IDs match the state IDs verbatim, for example
  `scene.quadrant.state.expanding.svg` and `immersive.svg`.
- Integration plan (not implemented): pre-tessellate each SVG once at load
  (`ofxSvg` or a tiny path parser) into an `ofPath`/`ofMesh` cache keyed by ID.
  `drawReservedIcon` then looks up the cache and scales it into `square`,
  falling back to today's placeholder for unknown IDs. No widget changes are
  needed.
