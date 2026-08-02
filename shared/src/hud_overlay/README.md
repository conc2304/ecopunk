# hud_overlay (HUD Glitch Overlay System)

Standalone ambient overlay layer, built per *Blueprint Emergence — Engineering
Handoff Addendum: HUD Glitch Overlay System v1.0*. Composite organism
behavior (Lock Sequence, Fault Cascade, Radar Ping, Handshake) plus
always-on ambient atoms (scanline, radar station, breathing tick clusters,
live telemetry readouts), driven by four master dials.

## Isolation boundary

Per the addendum's Section 08, nothing in this module knows about — and
nothing should be added here that references — the Composition Manager's
cycle lifecycle, `Fragment` placement events, `VideoSampler`, or the
composition's own random seed. `HudOverlayLayer` draws over a solid
near-black background only. If a class here starts needing one of those,
that's scope creep into the (future) integration phase, not this one.

## What it reuses vs. what's new

Atoms conceptually already covered by the canonical `shared/src/hud/`
widget library are reused directly, composed by the organisms in
`organisms/`:

- **Reticle** (Lock Sequence, Radar Ping, Handshake) — `hud::ReticleWidget`,
  via its additive `triggerAt(nx, ny, label, holdSeconds)` one-shot method
  (Standard preset, ambient pool disabled). See `ReticleWidget.h`'s comment
  for why this needed a small additive extension rather than reuse as-is:
  the ambient widget's target pool is driven by internal timers, not
  externally triggerable at an exact instant.
- **Tear + accent-line flash** (Fault Cascade) — `hud::GlitchTearWidget`,
  via its existing `trigger()`, with its own ambient random-interval loop
  disabled (see `FaultCascadeOrganism::setup()`) so it only ever fires on
  this organism's schedule.

Everything else the design doc describes has no existing counterpart in
`shared/src/hud/` and was added there as new widgets: `TickBurstWidget`,
`RadarStationWidget`, `BreathingTickClusterWidget`,
`TelemetryReadoutWidget`, `HalftonePatchWidget`, `TextCalloutWidget`,
`DashedLineWidget`. See `shared/src/hud/README.md` for their full API —
they're canonical library widgets, not private to this module, and are
covered by `scripts/check-hud-library-uniqueness.sh` like every other
widget there.

## Layout

```text
shared/src/hud_overlay/
  HudOverlayDialState.h    — plain data: the four master dials
  HudOverlayDialPanel.h/.cpp — ofxGui panel, writes into a HudOverlayDialState
  HudOverlayTheme.h        — Palette -> hud::HudTheme mapping
  HudOverlayScheduler.h/.cpp — per-organism random-interval timers
  HudOverlayOrganism.h     — base class: armSequence()/tickSequence() stagger helper
  HudOverlayLayer.h/.cpp   — top-level owner (ambient atoms + organisms + scheduler)
  organisms/
    LockSequenceOrganism.h/.cpp
    FaultCascadeOrganism.h/.cpp
    RadarPingOrganism.h/.cpp
    HandshakeOrganism.h/.cpp
```

## Standalone test harness

Each of `quadrant-crosshair`, `blueprint_emergence`, and `temporal-fields`
gates a `HudOverlayLayer` + `HudOverlayDialPanel` behind the `o` key — press
it to swap that sketch's normal draw for a solid `#0D0D0D` canvas with the
HUD system alone, dials live-adjustable via the on-screen panel. Press `o`
again to return to the sketch's normal output. Nothing else on screen is
touched while the overlay is active. See each sketch's `ofApp.cpp` for the
toggle.

## Deviations from the reference prototype worth knowing about

- **Radar Station**'s tick-flash timing doesn't literally replicate the
  prototype's CSS `animation-delay` mechanism (fixed per-tick delay
  offsets). Instead, each tick's brightness is computed every frame
  directly from its angular distance behind the wedge's *current* rotation
  angle. Same phase-locked result (ticks flash exactly as the wedge passes,
  same rotation period drives both), computed differently for a real-time
  immediate-mode renderer.
- **Event Coupling** is implemented as the design doc's Section 05
  describes the reference prototype itself working: narrowing each
  organism's independent random-interval range toward its mean as the dial
  increases, not a genuine shared-heartbeat scheduler. The design doc
  Section 08 calls that out as a suggested future improvement, not a
  requirement for this phase.
- **Radar Ping**'s trigger point is an independent timer picking a random
  point near the radar ring's edge — it does not read the wedge's live
  rotation angle. Section 08 leaves that coupling as an open integration-
  phase question.
