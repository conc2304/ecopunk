# Temporal Fields — Multi-State Transition Presets (Timeline Format)

Extends the preset format described in
[temporal-fields-preset-parameter-reference.md](temporal-fields-preset-parameter-reference.md)
with an optional `"timeline"` object that turns a preset from a single fixed
snapshot into a small authored composition: a sequence of named states
(arrival, breathing, activity, rest, ...) that the live dials smoothly
transition between and hold at, on a loop or once through.

Implemented by `TFPresetTimeline` (`src/TFPresetTimeline.h/.cpp`), owned by
`TFParameterPanel` the same way it already owns the Phase 7 wobble/waypoint
evolution system — see that class's header comments for the broader
preset-loading picture.

## Legacy static presets are unaffected

A preset with no `"timeline"` key loads exactly as it always has: dials jump
straight to the stored values, no transitions, no timeline state. This is
the existing format described in the parameter reference doc — nothing
about it changed. Every preset that predates this feature keeps working
with zero modification.

## The `"timeline"` object

```json
{
  "pattern": "blobgrid",
  "blobgrid": { "Drift_Speed": "0.15", "Blob_Radius": "0.42", "...": "..." },
  "transition": { "Erosion_Weight": "60", "...": "..." },
  "background": { "Image_Fade_Duration": "3.0", "...": "..." },

  "timeline": {
    "Loop": "1",
    "Start_State": "arrival",
    "States": [
      {
        "Name": "arrival",
        "Hold_Duration": "20.0",
        "Transition_Duration": "30.0",
        "Easing": "smoothstep",
        "Overrides": {
          "blobgrid": { "Drift_Speed": "0.03", "Edge_Softness": "0.85" },
          "background": { "Image_Fade_Duration": "8.0" }
        }
      }
    ]
  }
}
```

Everything outside `"timeline"` — `pattern`, and the pattern/transition/
patternregen/background sections — is the preset's **base values**, exactly
as in a static preset. `"timeline"` is additive: if it's missing, absent,
or fails validation, the preset behaves as a static snapshot of those base
values and nothing else changes.

- **`Loop`** — `"1"`/`"0"` (same string-bool convention as everywhere else
  in this format). `"1"`: after the final state's hold completes, the
  timeline transitions back to the first state and continues indefinitely.
  `"0"`: the timeline stops and stays parked on the final state's held
  values.
- **`Start_State`** — name of the state to begin at. Empty or unmatched
  falls back to the first state in `States[]` (with a warning if it was
  non-empty but didn't match anything).
- **`States[]`** — ordered list of states, each:
  - **`Name`** — required, must be unique within this timeline. Missing or
    duplicate names cause that one state to be dropped (warned, not fatal
    to the whole preset).
  - **`Hold_Duration`** — seconds to hold this state's resolved values once
    the transition into it completes.
  - **`Transition_Duration`** — seconds the transition *into* this state
    takes. `0` means an instant cut.
  - **`Easing`** — one of `linear`, `smoothstep`, `smootherstep`,
    `easeInOutSine` (see below). An unrecognized name logs one warning and
    falls back to `linear` — it never invalidates the state.
  - **`Overrides`** — sparse, grouped exactly like the top-level preset
    (`blobgrid`/`bands`/`transition`/`patternregen`/`background`, with
    `background.Effects` nested one level deeper). Only list the
    parameters this state actually changes.

## Transition and hold semantics

Each state has two phases, run in order:

1. **Transitioning** — dials interpolate from wherever they currently sit
   toward this state's resolved target values, over `Transition_Duration`
   seconds, shaped by `Easing`.
2. **Holding** — dials sit at the resolved target values for
   `Hold_Duration` seconds.

When the hold completes, the timeline advances to the next state in
`States[]` (or loops back to the first, or stops, per `Loop`).

**The very first state's transition starts from the preset's own base
values.** `TFParameterPanel::loadNextPreset()` always deserializes a
preset's base values into the live dials *before* the timeline is loaded
and started, so by the time the first transition begins and snapshots
"the current live values" as its starting point, those values already
equal the base preset — there's no separate bootstrapping path or special
case for this; it falls out naturally from load order.

## Sparse override inheritance

Each state's full target snapshot is resolved once, at load time, as:

```
resolved target for state N = base preset values, with state N's own
                               Overrides applied on top
```

**Never** `previous state's target + this state's Overrides` — each state
inherits independently from the base preset, not from whatever an earlier
state happened to set. So if `arrival` overrides `Drift_Speed` to `0.03`
but `breathing` doesn't mention `Drift_Speed` at all, `breathing`'s
resolved target for `Drift_Speed` is the **base preset's** value (say,
`0.15`) — the transition into `breathing` will actively move `Drift_Speed`
from wherever `arrival` left it back toward `0.15`, not freeze it at
`0.03` forever.

Regardless of which state is transitioning, the **starting point** of
every transition is always whatever the live dial's actual current value
is at that moment (via each parameter's own live getter) — never a
remembered "previous target." This is what keeps a `pause()`/`resume()` or
a `jumpToState()` continuous instead of popping.

## Supported parameter types and interpolation policy

| Type (as declared on the live `ofParameter`) | Policy |
|---|---|
| float | Continuous eased lerp every frame. |
| int | Lerp as a float internally, `std::round()` at write time (deterministic, rounds half away from zero). |
| bool | Switches from the "from" value to the "to" value at 50% eased transition progress. |
| enum int (`Orientation`, `Offset_Mode`, `Drift_Direction`, `Depth_Order`, `Wave_Direction`, `Split_Axis_Choice`) | Same 50%-switch policy as bool — one consistent rule for every discrete choice, rather than two different ones for "bool" vs "enum." |
| string / non-numeric | Not currently exercised by any parameter in this panel (every animatable dial is float/int/bool) — reserved as `TFTimelineBinding::Kind::Unsupported` for a future non-numeric parameter; such a path is simply left untouched by the timeline rather than guessed at. |

An override value that fails to parse as a number for a numeric binding is
warned about and skipped (that one path keeps whatever value the base
preset/previous state left it at) — it never invalidates the whole state.
An override outside the parameter's own declared range (the same min/max
`TFParameterPanel::setup()` already gives every dial) is clamped to that
range, with one warning.

### Which parameter classes are safe to animate continuously

- **Continuous dials** (Drift Speed, Blob Radius, Edge Softness, all the
  weight/interval floats, ...) animate exactly as you'd expect — smooth
  motion for the whole transition.
- **Discrete/regeneration-driving parameters** (`Grid_Resolution`,
  `Band_Count`, `Ring_Count`, `Column_Count`, `Seed_Node_Count`,
  `Max_*_Count`, ...) can be listed in `Overrides` and will be rounded and
  written every frame like any other int — but every pattern class in this
  codebase only ever *reads* these inside its own regen/rebuild function,
  which itself only runs on that pattern's own timer tick or on a fresh
  `reset()` (confirmed by reading `TFPatternBlobGrid::rebuildQuadtree()`,
  `TFPatternTemporalTides::rebuildGrid()`, `TFPatternEcologicalSuccession::
  rebuildGrid()`, `TFPatternBands::regenerateBands()`). So animating one of
  these doesn't visibly ramp smoothly — the pattern's geometry only jumps
  to whatever the interpolated value happens to be at that pattern's next
  scheduled regen tick. Treat these as "fine to nudge between states, not
  useful for a smooth continuous sweep."
- **Rate/interval parameters that drive an existing timer** (`Pattern_Regen_
  Rate`, `Fragment_Refresh_Rate`, `Geometry_Reshuffle_Rate`, `Growth_
  Interval`, `Disturbance_Interval`, `Mode_Change_Interval`, `Cycle_
  Interval`, ...) are always safe to animate: every one of them is compared
  against a plain accumulator (`timer += dt; if (timer >= rate) { timer -=
  rate; ... }`) using the *current* live value each frame — changing the
  rate mid-flight just changes the next comparison threshold, it never
  resets the timer and never fires a burst of events from one frame's tiny
  change.

## Easing names

`linear`, `smoothstep`, `smootherstep`, `easeInOutSine` — see
`TFTimelineEasing.h`. All four satisfy `f(0)=0`/`f(1)=1` exactly, so a
transition's very last frame always lands precisely on target with no
separate snap-to-target step.

## Looping

`Loop: "1"` cycles indefinitely: last state's hold → transition back to
the first state → repeat. `Loop: "0"` stops and stays parked on the final
state's held values once its hold completes; further frames don't move
anything further (checked once via an internal `finished` flag, not by
repeatedly comparing timers that no longer matter).

## Pattern-switch limitation

A timeline only ever animates the **one pattern** named by the preset's
top-level `"pattern"` field, plus the shared `transition`/`patternregen`/
`background` groups — it cannot switch which pattern is active
state-to-state. A `"pattern"` key found inside a state's `Overrides` is
warned about and ignored; the preset's top-level pattern stays in effect
for the whole timeline. A genuinely safe pattern-switching timeline would
need deeper integration with `TFComposition` and is out of scope here.

Related: `TFComposition`'s own independent ~90-second auto-cycle (which
normally rotates through every pattern regardless of any preset) is
suspended for as long as a multi-state timeline preset is active
(`TFComposition::setAutoCycleSuspended()`, wired from `ofApp::update()`),
so an authored timeline can actually hold its one pattern for its full
designed duration instead of getting interrupted every 90 seconds. Static
presets are unaffected — the auto-cycle behaves exactly as before whenever
no timeline is running.

## Runtime controls

C++ API (`TFParameterPanel`): `isTimelineActive()`, `isTimelinePaused()`,
`restartTimeline()`, `pauseTimeline()`, `resumeTimeline()`,
`advanceTimelineState()`, `jumpTimelineState(name)`,
`getTimelineDebugStatus()`. `TFPresetTimeline` itself additionally exposes
`getCurrentStateName()`, `getPhase()`, `getPhaseProgress()`,
`getTotalElapsed()`, `isLooping()`.

Keyboard, mirroring this sketch's existing single-letter debug convention
(`'r'` restart cycle, `'t'` force next pattern, `'S'` save preset, `TAB`
load next preset): **`'P'`** pauses/resumes the active timeline, **`']'`**
advances it to its next state immediately.

Debug overlay (`'d'` toggles `showDebugGui`, same as every other debug
readout in this sketch) shows, only while a timeline is active:

```
Timeline: air_and_time_quiet_canopy
State: breathing (2/4)
Phase: transition
Progress: 63%
Next: activity
Loop: on
Paused: no
```

## Saving presets while a timeline is loaded

The panel's Save button always serializes the **current live dial
values** — while a timeline is running, that's whatever instant of the
animated blend this happens to be (same principle as saving live
wobble/evolution values today, not something new to this feature). The
loaded preset's original, unmodified `"timeline"` object is preserved
verbatim only when re-saving back to that exact same filename; saving
under any other name produces a plain static snapshot with one clear
`ofLogWarning` so the timeline's disappearance is never silent. There is
no GUI authoring for timeline content itself (hand-author the JSON).

## Full example

See `bin/data/presets/air_and_time_quiet_canopy.json` — Blob Grid base
(two large blob centers, transparent masked background, long history,
erosion-heavy transitions) cycling arrival (20s hold / 30s transition) →
breathing (180s / 45s) → activity (120s / 35s) → rest (90s / 50s, drifting
back toward the base character since it only overrides drift speed and
refresh rate) → loops back to arrival.
