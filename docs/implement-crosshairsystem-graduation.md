# Task Outline: Graduate CrosshairSystem to shared/src/

**Status: outline only — not started.** This scopes the work agreed in
[`docs/scene-consolidation-decisions.md`](./scene-consolidation-decisions.md)
(Decision 1). No `CrosshairSystem` code has been touched. This document is
for review before implementation begins.

## Why this task exists

`CrosshairSystem` currently exists as two independently-forked copies:
`quadrant-crosshair/src/CrosshairSystem.{h,cpp}` (the original) and
`fragment-trail/src/CrosshairSystem.{h,cpp}` (a superset fork, plus one
deliberate behavioral divergence on line width — see the decisions doc for
the full diff analysis). The decision was to graduate it to `shared/src/`,
using fragment-trail's version as the base, with line width made
configurable so quadrant-crosshair doesn't silently inherit fragment-trail's
hardcoded value.

## Key technical finding: where in shared/src/ it lands matters

Both sketches currently keep `shared/src`'s **bare top-level loose files**
excluded, for unrelated name-collision reasons:

```make
# quadrant-crosshair/config.make
PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src
PROJECT_EXCLUSIONS = ../../shared/src

# fragment-trail/config.make
PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src
PROJECT_EXCLUSIONS = ../../shared/src ../../shared/src/video-effects%
```

Per this Makefile system's documented behavior (see top-level `CLAUDE.md`
and fragment-trail's own `config.make` comment): a bare `../../shared/src`
exclusion entry only blocks files sitting **directly** in `shared/src`'s
top level — it does **not** cover subdirectories unless a `%` wildcard is
added. That's exactly why both sketches already successfully consume
`shared/src/hud/`, `shared/src/hud_overlay/`, and (quadrant-crosshair only)
`shared/src/video-effects/` — those are subdirectories, not top-level loose
files, so the bare exclusion doesn't touch them.

**If `CrosshairSystem.{h,cpp}` graduates directly into `shared/src`'s bare
top level (alongside `GridSystem.cpp`, `Fragment.cpp`, etc.), both
sketches' existing bare exclusion entries would swallow it too** — the
exact same mechanism that's currently (correctly) blocking `GridState`,
`LFOBank`, `MotionExtraction`, `ShaderLibrary`, and `TriggerBus` from
colliding with each sketch's own local forks of those classes would also
block the new shared `CrosshairSystem`, defeating the entire point of
graduating it.

**Resolution: give it its own subdirectory**, `shared/src/crosshair/`,
mirroring the existing precedent (`shared/src/hud/`, `shared/src/hud_overlay/`,
`shared/src/ridgeline/`, `shared/src/video-effects/` are all organized this
way specifically so they can be individually included/excluded). With
`CrosshairSystem` in its own subdirectory:

- **No `PROJECT_EXCLUSIONS` changes are needed in either sketch.** Both
  already pull in all of `shared/src` recursively
  (`PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src`); a new subdirectory
  is automatically swept in, and the existing bare top-level exclusions
  don't touch it.
- The other colliding classes each sketch still forks locally (`GridState`,
  `LFOBank`, `MotionExtraction`, `ShaderLibrary`, `TriggerBus` for
  quadrant-crosshair; `ShaderLibrary`, `LFOBank`, `TriggerBus`,
  `TimeOffsetVideoBuffer` for fragment-trail) keep needing their current
  exclusions exactly as-is — this task doesn't touch that.

This should be verified with a real build, not just reasoned about from the
docs — the Makefile exclusion-matching behavior has already surprised this
repo once (fragment-trail's `config.make` comment documents an individual-
file exclusion that looked like it should work and didn't).

## Task list

1. **Create `shared/src/crosshair/CrosshairSystem.h` and `.cpp`**, based on
   `fragment-trail/src/CrosshairSystem.{h,cpp}` (the superset version, per
   the graduation decision).

2. **Make line width configurable rather than hardcoded either way.**
   Fragment-trail's fixed `lineWidth = 2.0f` and quadrant-crosshair's
   speed+LFO-driven `ofLerp(4.0f, 12.0f, speedNorm * 0.7f + pulse * 0.3f)`
   are both real, intentional behaviors for their respective sketches —
   neither should become the silent default for both. Concretely: add a
   `LineWidthMode` (or similar) concept to the shared class — e.g. a fixed
   value vs. a speed+LFO-driven range — with quadrant-crosshair configured
   to reproduce its existing 4–12px dynamic behavior and fragment-trail
   configured to reproduce its existing fixed 2px. The exact shape (enum +
   params struct, a small strategy callback, or a couple of constructor
   options) is an implementation-time call, not decided here — but the
   constraint is firm: after this change, both sketches' on-screen line
   width behavior should be visually unchanged from today.

3. **Update quadrant-crosshair to consume the shared class:**
   - Delete `sketches/quadrant-crosshair/src/CrosshairSystem.{h,cpp}`.
   - No include-path changes needed in `ofApp.h`/`ofApp.cpp` — OF's
     Makefile system adds every external-source subdirectory to the
     include search path, so the existing `#include "CrosshairSystem.h"`
     resolves to the new shared location automatically once the local file
     is gone.
   - Configure line width per the item above so current behavior is
     preserved.
   - Confirm `config.make` needs no changes (per the finding above) — but
     verify with a real build rather than assuming.

4. **Update fragment-trail to consume the shared class:**
   - Delete `sketches/fragment-trail/src/CrosshairSystem.{h,cpp}`.
   - Same include-path reasoning as above — should resolve automatically.
   - Configure line width to preserve the current fixed-2px behavior.
   - Confirm `config.make` needs no changes; verify with a real build.

5. **Search both sketches for every `CrosshairSystem` reference** (not just
   the `ofApp` member) before assuming the swap is complete — e.g.
   `FTModeController`'s `TriggerBus`-driven mode switching reads
   `crosshair.getState()` via `ofApp`, and quadrant-crosshair's
   `ExpansionDirector`/`triggerBus` wiring also touches crosshair state.
   None of these are expected to need changes (they consume the public
   `CrosshairState`/`getState()` interface, which isn't changing), but
   confirm rather than assume.

6. **Build and run both sketches**, not just compile-check. Visually
   compare crosshair motion and line width against a pre-change build of
   each (or against the screenshots taken during the pre-req live
   verification pass, where applicable) to confirm no behavior change
   beyond what's intended.

7. **Run `scripts/check-video-effect-drift.py` and consider whether
   `scripts/check-hud-library-uniqueness.sh`-style drift protection is
   worth adding for `shared/src/crosshair/`** — not required for this task,
   but worth a one-line decision (yes/no, and why) when this is actually
   implemented, given this repo's history of exactly this kind of class
   drifting into multiple copies before being consolidated.

## Explicitly out of scope for this task

- **`LFOBank` consolidation.** `CrosshairSystem` depends on `LFOBank`
  (`#include "LFOBank.h"`), and both sketches currently resolve that
  against their own local `LFOBank` forks, not the canonical
  `shared/src/LFOBank`. Whether/how to consolidate `LFOBank` is a related
  but separate decision (per `docs/scene-consolidation-decisions.md`) that
  hasn't been made — this task should graduate `CrosshairSystem` against
  whichever `LFOBank` each sketch already uses (i.e., no `LFOBank` changes),
  not bundle a second graduation into the same pass. If `LFOBank`
  consolidation is decided separately later, it's a follow-up task of its
  own.
- **Any other local fork** (`GridState`, `MotionExtraction`,
  `ShaderLibrary`, `TriggerBus`, `TimeOffsetVideoBuffer`). None of these are
  touched by this task.
- **Broader SceneManager migration work.** This is a prerequisite
  cleanup task, not part of the SceneManager implementation itself.
