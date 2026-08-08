# temporal-fields — Shared Effect Knowledge Integration (first proof target)

**Status:** Implemented, narrow, fallback-safe. Not yet Architecture-reviewed (same status as the two Shared Effect Knowledge docs it depends on).

## What changed

`TFEffectPicker` (`sketches/temporal-fields/src/TFEffectPicker.{h,cpp}`) gained:

1. **A `videoeffects::EffectKnowledgeBase` member**, populated once in `setup()` via `importEffectKnowledgePack("shared-video-effects/knowledge/effect-knowledge-pack.json", knowledgeBase, tfCatalogRegistry().allIds())`. Passing the real registered catalog ids (not a hand-curated list) means the unknown-effect-id rejection path is exercised against real data.
2. **Blacklist-avoidance in `pickNext()`:** after picking an effect and randomizing its parameters exactly as before, the resulting parameter snapshot (for the 7 effects this class randomizes at all — see `currentParamSnapshot()`) is checked against that effect's imported blacklist entries via content equality (same rule as `EffectKnowledgeBase::isDuplicate`). A match triggers a bounded re-pick (up to 3 attempts, mirroring `EffectRandomizer`'s own retry-cap precedent), never blocking forever.
3. **`activityStatus()` accessor**, returning a real `videoeffects::EffectActivityStatus` (one slot, or zero slots when "Raw / No Effect" is selected) — exposed and ready to consume, not yet wired to any HUD/status producer since temporal-fields has no `IEcopunkScene`/`SceneHudStatus` adapter yet (still a raw `ofBaseApp` sketch, unmigrated per the roadmap).

## Why this scope, not more

- **No parameter-range replacement.** `randomizeEffectParams()`'s existing hand-tuned `ofRandom(...)` ranges are completely unchanged — the blacklist check runs *after* the existing logic picks values, as an additional filter, not a replacement pipeline. Full replacement with `EffectRandomizer::generate()`-driven sampling (which would also add whitelist-*biased* sampling, not just blacklist avoidance) is a larger, riskier change to this class's core behavior and was judged out of this session's "narrow, preserve existing effect visual behavior" mandate.
- **No effect-selection weighting from knowledge.** `TFEffectPicker::Weights::effectWeights` (which effect names are even eligible, and how likely each is) is a scene-authored, coarse selection-weight concept, distinct from the shared schema's preset-level whitelist/blacklist (see the Session 2 verification doc's §6 effect-vs-preset table). There is no persisted "selection weight" concept in the shared schema to layer onto `effectWeights` at all — inventing one wasn't this session's call to make (Question 5, escalated).
- **`recolor`'s `tint` cannot participate in blacklist checking.** It's `KnowledgeEntry`'s only vector-typed parameter in TF's randomized set, and `KnowledgeEntry::snapshot` is scalar-only by design (see the verification doc §2.2, and `EffectRandomizer.h`'s own pre-existing comment on the same limitation). `currentParamSnapshot("recolor")` returns an empty map — correctly a no-op, not a silently-wrong comparison against fabricated keys.
- **No video/lifecycle changes.** `TimeOffsetVideoBuffer` ownership, `TFBackgroundLayer`'s mode-picking, and every pattern class are untouched.

## Fallback behavior (verified by construction, not by a build-time test)

`knowledgeBase` is populated by `importEffectKnowledgePack()`, which returns `report.ok = true` with zero imports when the pack file is absent (the current real-world case — nothing yet copies the debugger's exported pack into `sketches/temporal-fields/bin/data/`, see the verification doc §12's path table). An empty `EffectKnowledgeBase` means `loadBlacklist()` always returns an empty vector, so `pickNext()`'s new retry loop's `blocked` check is always false on the very first attempt — **byte-for-byte identical behavior to before this change** in the pack's absence. This was not separately unit-tested (no OF-free test harness exists for TF's own classes), but follows directly from `importEffectKnowledgePack`'s own tested behavior (verification doc §9's round-trip evidence, "absent pack is not a failure" case) plus `pickNext()`'s loop structure, which is a small enough diff to review directly.

## Build/test evidence

- `sketches/temporal-fields/config.make` needed the same `PROJECT_EXCLUSIONS` fix as `shader-effect-debugger` and `blob-region-prototype` (three test directories under `shared/src/` collide with `src/main.cpp`'s `main()` otherwise) — applied.
- `make clean && make Release -j1` completes through final link (a `-j4` build hit an unrelated, pre-existing parallel-build race dropping `shared/src/hud-compositor/widgets/HudTextMetricsCache.cpp` from compilation — confirmed unrelated to this integration by reproducing it with `-j1` succeeding cleanly; see the verification doc §8's build-issue note). `TFEffectPicker.cpp` itself compiled without any error or warning in both attempts.
- No standalone unit test exists for `TFEffectPicker` (it has real openFrameworks dependencies — `ofFbo`, `ofShader` — inherent to what it does, not introduced by this change) — validated by the full Release build only, per this session's own fallback allowance for OF-dependent code paths.

## Recommended next steps

1. Wire `scripts/sync-video-effect-assets.py` (or an equivalent) to actually populate `sketches/temporal-fields/bin/data/shared-video-effects/knowledge/effect-knowledge-pack.json` from a debugger export, so the integration has real data to act on rather than always taking the empty-knowledge-base fallback path.
2. When temporal-fields migrates to `IEcopunkScene` (roadmap Phase 8), wire `TFEffectPicker::activityStatus()` into that scene's `SceneHudStatus`/future semantic-snapshot production — the accessor already exists and returns the real shared type.
3. Revisit whether `TFEffectPicker::Weights::effectWeights` should itself become knowledge-pack-driven (a genuine selection-weight feature, not blacklist avoidance) — flagged as a real gap in the shared schema (verification doc §6), not decided here.
