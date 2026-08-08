# Scene/SceneManager Consolidation — Decisions Record

Decisions made in conversation (2026-08-01), following the pre-req audit in
[`docs/scene-consolidation-probe.md`](./scene-consolidation-probe.md) and the
subsequent pre-req fix pass (radar-pulse build fix, quadrant-crosshair doc
corrections, live-verified fragment-trail bug fix). Recorded here because
they weren't written down anywhere durable at the time they were made, and
future work on the SceneManager consolidation will depend on both.

No existing ADR-style convention was found elsewhere in `docs/` — the
closest precedent is [`docs/video-effect-second-wave-evaluation.md`](./video-effect-second-wave-evaluation.md),
a similarly-shaped "decision record, not an implementation" doc for a
different set of sketches. This doc follows that same shape.

---

## Decision 1: CrosshairSystem graduates to `shared/src/`

**Decision**: `CrosshairSystem` moves from being two independently-forked
sketch-local copies (`quadrant-crosshair/src/CrosshairSystem` and
`fragment-trail/src/CrosshairSystem`) to a single canonical implementation
in `shared/src/`, consumed by both sketches.

**Rationale**:

- A full diff of the two current copies shows fragment-trail's version is a
  **strict superset** of quadrant-crosshair's, with one exception. FT adds:
  an `applyLiveSettings(...)` live-tuning API (speed multiplier, smoothing,
  speed-variance/rate, pattern index, auto-cycle enable/interval — all
  driven from `FTParameterPanel`); a `stretchNoise()` contrast-stretch fix
  that corrects a real bug in the shared core algorithm (`ofNoise()`
  empirically only reaches ~[0.40, 0.66] over a real run, not the
  theoretical [0,1] range, so raw samples compress the crosshair's wander
  into a narrow center band — FT's fix genuinely improves the noise-driven
  motion, it isn't just an additive feature); and a `smoothing` low-pass
  filter on the resulting position. None of this removes or changes
  quadrant-crosshair's core noise-driven motion model
  (`p.freqA/freqB/ampA/ampB` per preset) — it layers on top of it.
- **The one exception**: quadrant-crosshair computes line width from speed
  + an LFO pulse (`ofLerp(4.0f, 12.0f, ...)`, a 4–12px dynamic range).
  Fragment-trail hardcodes `lineWidth = 2.0f`, with a comment noting this
  was "a user preference — thinner than quadrant-crosshair's original
  speed-scaled range," not a bug fix or an accidental regression. This is a
  genuine behavioral divergence, not a missing feature, and needs to be
  preserved rather than silently dropped when FT's version becomes
  canonical.
  - **Resolution**: line width becomes a configurable/themeable parameter
    on the shared class (not a hardcoded constant either way), so
    quadrant-crosshair can restore its speed+LFO-driven 4–12px behavior
    against the shared implementation instead of silently inheriting
    fragment-trail's fixed 2px. See the task outline in
    [`docs/implement-crosshairsystem-graduation.md`](./implement-crosshairsystem-graduation.md)
    for how this gets threaded through.
- The original decision to keep these as separate copies (recorded in
  fragment-trail's own engineering plan) was a **build-mechanics choice**,
  not a product/behavior one: "mirroring how quadrant-crosshair itself is a
  self-contained copy — not a promotion into shared/src." The concern at
  the time was avoiding a Makefile name-collision problem quadrant-crosshair
  already had (`shared/src`'s top level has unrelated blueprint_emergence-
  owned classes with the same names as several of quadrant-crosshair's own
  local classes). That constraint doesn't argue for keeping `CrosshairSystem`
  specifically forked forever — it was never a claim that the two sketches
  need genuinely different crosshair behavior, just a pragmatic workaround
  at the time. Once both sketches are scenes inside one binary, two
  drifting copies of the same class is exactly the failure mode
  `shared/src/hud/`'s own README documents happening once already (two
  sketch-local copies + one canonical copy, all drifting, later
  consolidated) — worth heading off now rather than after the fact.

**Flagged wrinkle — do not fold into the graduation work without a
separate decision**: `CrosshairSystem` depends on `LFOBank`
(`#include "LFOBank.h"`, used via `update(dt, lfoBank)`). Both
quadrant-crosshair and fragment-trail currently resolve that include
against their own **local** `LFOBank` forks, not the already-generalized
canonical `shared/src/LFOBank` (which supports arbitrary lane counts and
caller-owned lane semantics, vs. quadrant-crosshair's original fixed-enum
version and fragment-trail's single-lane trimmed copy of it). Graduating
`CrosshairSystem` cleanly will eventually want `LFOBank`'s own fork
situation resolved too — but that's a related, separate question with its
own tradeoffs (it isn't `CrosshairSystem`-specific; other classes reference
each sketch's local `LFOBank` too), and no decision has been made about it.
The graduation task outline explicitly scopes `CrosshairSystem` alone and
calls out `LFOBank` as follow-up, not included work.

---

## Decision 2: radar-effects-gallery and radar-pulse are cut from the SceneManager scope

**Decision**: both sketches are excluded from the SceneManager
consolidation entirely — not deprioritized to a second wave, not
"decide after the build fix" as the original scope table had it. This
overrides that earlier fast-follow language.

**This is a scope/product decision, not a technical one.** Both sketches
build and run correctly:

- `radar-pulse`'s build was broken (missing `ofxOpenCv` addon — see the
  pre-req fix pass) and is now fixed; verified live, it runs stably
  (~85fps, correct reveal-mask compositing, clean quit). The build fix
  itself was worth doing regardless of this scope decision — it was a
  real, isolated, low-risk bug, not entangled with the scope question.
- `radar-effects-gallery` already built and ran cleanly before this pass.

Neither sketch is excluded because of a defect. They're excluded because
they're judged weaker additions to the consolidated composition cycle than
the other five sketches going into the SceneManager — a creative/product
call, not a bug report.

**Note for posterity**: `radar-effects-gallery`'s `RPVideoSampler`/
`RPRevealMask` were already documented in their own header comments as
throwaway forks of `radar-pulse`'s classes — explicitly temporary, meant to
be deleted "once a mode wins" (`radar-effects-gallery` was built as an
11-mode shader-treatment gallery to help decide `radar-pulse`'s eventual
visual treatment, not as a competing production sketch). This scope
decision effectively closes that open question by removing **both**
sketches from further consolidation work — there's no more "which mode
wins" question to answer, because neither sketch is going into the
SceneManager regardless of which treatment might have looked best.

**This does not mean deleting either sketch from the repo.** That's a
separate call this decision doesn't make one way or the other — for now,
both sketches simply stop being planned for inclusion in the consolidated
binary. Nothing has been deleted.

---

## Related housekeeping

The original SceneManager consolidation engineering prompt (referenced as
having a scope table listing `radar-pulse` as "Not yet" pending the build
fix and `radar-effects-gallery` as "Decide after fixing radar-pulse") could
not be located anywhere in this repo (`docs/`) or in the locally-accessible
Claude plan files searched during this pass. If that document lives
somewhere else, its scope table should be updated to mark both sketches
**Excluded — cut from scope**, with a pointer back to this document.
