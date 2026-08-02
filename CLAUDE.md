# CLAUDE.md

Guidance for any coding agent (Claude Code or otherwise) working in this
repository. Read this before touching shaders, video effects, or anything
under `shared/src/`.

## What this repo is

Multi-sketch openFrameworks monorepo. macOS dev, **Raspberry Pi 3B**
deployment target. See `README.md` for the sketch layout and build
instructions.

```text
shared/src/             # code shared by every sketch, pulled in via PROJECT_EXTERNAL_SOURCE_PATHS
shared/assets/          # canonical data assets shared by every sketch (currently: video-effects/)
sketches/<name>/        # one openFrameworks project per sketch, generated via projectGenerator
scripts/                # repo-wide tooling (asset sync, drift checks)
docs/                   # architecture docs and engineering plans
```

## ⚠️ Before adding or modifying ANY shader/video effect

**A shared video-effect service already exists. Do not write a new
per-effect uniform-binding cascade, a new `ShaderLibrary`-like registry, or
copy a shader file into a sketch's `bin/data/` by hand.** This monorepo
spent a long time with every sketch independently reinventing the same
handful of effects (`recolor`, `desaturate`, `heatmap_recolor`,
`bioluminescence`, ...) with hand-duplicated literal uniform values — that
duplication was consolidated into one canonical system. Recreating it is
exactly the mistake this system exists to prevent.

**Start here:** [`shared/src/video-effects/README.md`](shared/src/video-effects/README.md)
— practical instructions for consuming the service and adding a new canonical
effect. Read that file before writing any shader-related code in this repo.

**Full design background** (read if you need to understand *why*, or if
you're touching the service's own architecture, not just consuming it):

- [`docs/shared-video-effect-architecture.md`](docs/shared-video-effect-architecture.md) — the canonical architecture spec
- [`docs/shader-effect-system-probe.md`](docs/shader-effect-system-probe.md) — the investigation that motivated it (what was duplicated, where, and why)
- [`docs/video-effect-promotion-inventory.md`](docs/video-effect-promotion-inventory.md) — per-effect promotion decisions (canonical IDs, uniform contracts)
- [`docs/video-effect-second-wave-evaluation.md`](docs/video-effect-second-wave-evaluation.md) — why radar-effects-gallery, radar-pulse, and contour-portrait are *not* on the shared service, and shouldn't be forced onto it without re-reading this first
- [`docs/implement-shader-effect-debugger-and-service.md`](docs/implement-shader-effect-debugger-and-service.md) — the original staged implementation plan (historical reference; the work it describes is done)

**Quick sanity check before committing shader-related changes:**

```bash
python3 scripts/check-video-effect-drift.py
```

Flags duplicate-ID registrations, stale/drifted synced shader assets, and
(advisory) per-effect uniform cascades outside `shared/src/video-effects/`
that look like un-migrated duplication.

## Which sketches use the shared service, and which deliberately don't

| Sketch | Status |
|---|---|
| blob-region-prototype, blueprint_emergence, temporal-fields, quadrant-crosshair (production path) | Migrated — fixed-default uniforms source from the shared catalog |
| shader-effect-debugger | Built directly against the service; the reference example for how to consume it |
| fragment-trail | **Deliberately not migrated** — local `ShaderLibrary`/`LFOBank`/etc. forks collide by name with `shared/src`'s top-level classes; see `sketches/fragment-trail/config.make`'s own comments before attempting this |
| quadrant-crosshair `DebugMode` | **Deliberately not migrated** — every parameter there is live-tunable via GUI-bound state (the whole point of a debug panel), not a duplicate of fixed literals; forcing it onto the fixed-default catalog pattern would silently break live tuning. Its real migration path is retirement in favor of `shader-effect-debugger`, not a mechanical refactor |
| radar-effects-gallery, radar-pulse, contour-portrait | **Not migrated, second-wave candidates** — see `docs/video-effect-second-wave-evaluation.md` for why each one doesn't fit the current service without its own design pass |

If you're about to touch any sketch in the second row, read the doc/comment
cited before assuming "just migrate it like the others" is safe — each has a
specific, documented reason it's excluded.

## Build/validate a sketch

```bash
cd sketches/<name>
make Release -j4
make RunRelease
```

Every sketch in the "migrated" row above pulls in `shared/src/` (including
`shared/src/video-effects/`) via `PROJECT_EXTERNAL_SOURCE_PATHS` — a change
to `shared/src/video-effects/` should be validated by rebuilding at least one
consuming sketch, not just checked for syntax. `fragment-trail` specifically
excludes `shared/src/video-effects%` via `PROJECT_EXCLUSIONS` — see that
file's comments if you need to touch project-level Makefile config here,
since this Makefile system's exclusion matching has real, non-obvious
behavior (exact-string match per discovered subdirectory, not recursive path
matching — a bare directory exclusion does not cover its own subdirectories
without a `%` wildcard).
