# hud_elements (moved)

This directory used to hold its own copy of the HUD widget library. That
copy, a second copy under `sketches/quadrant-crosshair/src/hud_elements/`,
and a third under `shared/src/hud/` drifted independently until they were
consolidated into a single canonical implementation.

**The HUD widget library now lives at `shared/src/hud/`.** See
`shared/src/hud/README.md` for the widget list, usage examples, and
integration instructions.

Do not copy the HUD library into a sketch. Add `shared/src/hud` (or
`shared/src`, if the sketch already consumes other `shared/src/` systems) to
the sketch's `PROJECT_EXTERNAL_SOURCE_PATHS` in its `config.make` instead.
`scripts/check-hud-library-uniqueness.sh` (run from the repo root) will fail
if a copy reappears anywhere outside `shared/src/hud/`.
