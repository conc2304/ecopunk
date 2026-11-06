# Orbit Console v0.1 - HUD content reallocation: Step 1 occupancy findings

Source: shared/src/hud-compositor/HudRegionCatalog.cpp, HudPresentationProfile.cpp,
HudWireframeRenderer.cpp (production allowlist). Bounds are catalog fractions on the 1280x720
canvas; all six regions are already in the production allowlist, so no membership change is needed.

| # | Region | Bounds (px, x=844.8 w=409.6) | Accepted widgets (max) | Today's binding / source | Scenes |
|---|---|---|---|---|---|
| 4 | universal.activity | y165.6 h43.2 | Numeric, ProgressBar, StatusBadge (1) | universal.primary_state.badge, StatusBadge: scene.state.primary (+ secondary, icon) | all 5, Placeholder if missing |
| 5 | universal.effect_summary | y216.0 h43.2 | EffectChips, StatusBadge, ProgressBar, Numeric (2) | universal.activity.bar, ProgressBar "ACTIVITY": scene.activity.overall, DimLastValid | all 5 |
| 6 | universal.timing | y266.4 h36.0 | Numeric, Label, EffectChips (1) | universal.effect_summary.chips, EffectChips: effects.active + effects.health, Hide if no snapshot | all 5 |
| 7 | universal.activity_trace | y309.6 h72.0 | Sparkline (3) | vitals FPS, FRAME MAX, CPU (runtime.vitals.*), 3 x 24 px | all 5 |
| 8 | universal.secondary_trace | y388.8 h72.0 | Sparkline (2) | vitals GPU TEMP, RSS, 2 x 36 px | all 5 |
| 15 | flexible.primary | y460.8 h115.2 | Metadata, ChannelStrip, Chips, Ring, Sparkline, Timeline, Ambient (1) | per-scene widget (profile id = scene id) | Blob MetadataCard; Temporal Sparkline (32-pt history); Fragment MetadataCard; Quadrant ChannelStrip (4 ch); Blueprint ProgressRing; Contour ProgressRing |

Loading/transition: universal bindings use Placeholder/DimLastValid/Hide per binding; the
flexible profile falls back to ambient (UseAmbientFallback) when scene data is missing. Focus: none
of #4-#8 or #15 is a control; focus/interaction belongs to controls.* and is untouched.
flexible.secondary_a (ACTIVE TIME) and secondary_b (clock) are not part of this reallocation.

## Displacement the plan causes (needs a destination or an accepted omission)
1. #15's six scene-specific widgets are displaced by System Performance. Proposed destination: #5
   as a compact one-line scene status. Cost: 43 px high vs 115 px, so Quadrant's 4-channel strip,
   Temporal's 32-pt sparkline and the rings must be redesigned as compact text/segment forms.
2. #6's effect chips (+ health) are displaced by the activity gauge. No destination named in the
   authorization; candidates: fold into #5's compact status, or accept omission. Needs a ruling.
3. The ACTIVITY ProgressBar moves #5 -> #6; the catalog's #6 does not accept ProgressBar, so
   acceptedWidgetTypes must widen (geometry and IDs unchanged, same precedent as the earlier widening).
4. System Performance in #15: five rows in 115.2 px is about 23 px per row (today 24/36 px); the
   vitals layout reserves a 160 px text column, which leaves about 240 px of plot. Feasible, needs checking.

## Blockers
- Orbit (#7/#8): no SceneColorSnapshot / HudFrameData::sceneColor / runtime.scene_color exists yet
  (scene-sampling-handoff.md sec. 4: awaiting Architecture; linear-vs-encoded ruling also open).
  Nothing to consume, so Orbit cannot be built or verified.
- View selector: none was ever implemented, so there is nothing to retire.
- The scene-specific widget redesign (item 1) is a sizeable design change and was not started
  while #7/#8 cannot yet receive Orbit: moving vitals alone would leave the trace regions empty.
