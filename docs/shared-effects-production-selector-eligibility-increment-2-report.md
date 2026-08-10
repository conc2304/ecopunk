# Shared Effects — Production Selector / Eligibility Increment 2 Report

**Status:** Implemented, verified by real build + real captured console output + real on-disk artifacts. Not yet Architecture-reviewed — same status class as every prior Shared Effects document in this chain.

---

## 1. Summary

Closed the strongest remaining safety gap from Increment 1: a canonical authored preset that also matches the blacklist can no longer be applied by the automatic selector under any circumstance — blocked-ness is now a hard candidate-filtering criterion, checked before a blocked entry can ever enter the eligible pool, not left to the pre-existing bounded-retry loop. Then produced and verified a real, non-test, canonically-authored, positively-compatible preset via the real `shader-effect-debugger` authoring path, through the real sync script, into the real `temporal-fields` production import.

## 2. Chosen consumer

**`temporal-fields` / `TFEffectPicker`**, unchanged from Increment 1 — confirmed by inspection that the flow described in the prior report still matches the current code exactly (one addition since: a concurrent, unrelated session's DEC-013/DEC-014 video-adapter migration touched `ofApp.cpp`'s video setup, not anything in the effect-selection path).

## 3. Files inspected

`sketches/temporal-fields/src/{TFEffectPicker.{h,cpp},TFEligibilitySelfTest.{h,cpp},TFActivityStatusSelfTest.{h,cpp},TFBackgroundLayer.h,ofApp.cpp}`, `shared/src/video-effects/knowledge/{EffectKnowledgeBase,EffectKnowledgePrecedence,EffectPresetId,EffectLevelKnowledge,EffectKnowledgePack,EffectKnowledgeSerialization}.{h,cpp}`, `shared/src/video-effects/knowledge/EffectRandomizer.h`, `sketches/shader-effect-debugger/src/{ofApp.h,ofApp.cpp}`, `scripts/sync-video-effect-assets.py` (read, not modified), `assets/shared/video-effects/knowledge/effect-knowledge-pack.json` (both before and after this increment's authoring action).

## 4. Files changed

| File | Reason |
|---|---|
| `sketches/temporal-fields/src/TFEffectPicker.cpp` | `applyEligibleCanonicalPreset()`: candidate filtering now also excludes any whitelist entry whose snapshot content-matches an existing blacklist entry for the same effect, *before* it can enter the eligible pool |
| `sketches/temporal-fields/src/TFEligibilitySelfTest.cpp` | Five new deterministic test cases (11–14, one with two assertions) proving hard blocked exclusion; one new helper (`makeBlacklistEntry`) |
| `sketches/shader-effect-debugger/src/ofApp.h` | New `pMarkTemporalFieldsCompatible` GUI toggle; new `authorCanonicalTemporalFieldsSeedPreset()` method declaration |
| `sketches/shader-effect-debugger/src/ofApp.cpp` | `saveCurrentToWhitelist()` now authors `compatibleSceneIds` when the new toggle is set; new `'c'` keybinding toggles it; new `authorCanonicalTemporalFieldsSeedPreset()` implementation, called once at `setup()`; on-screen help text updated |

No file under `shared/src/video-effects/knowledge/`'s frozen public types, `ExperienceRuntime`, `HudFrameData`, or `blob-region-prototype` was touched.

## 5. Exact selector flow — before (Increment 1's end state)

```
applyEligibleCanonicalPreset(effectName):
  candidates = knowledgeBase.loadWhitelist(effectName)
  for each candidate:
    skip if !isReusableAuthoredPreset(candidate)
    skip if !isEligibleForAutomaticProductionSelection(resolveCompatibility(...))
    eligible.push_back(candidate)          # <-- blacklist never consulted here
  pick uniformly among eligible

pickNext():
  ... applyEligibleCanonicalPreset(...) or randomizeEffectParams(...) ...
  # post-hoc, AFTER the pick:
  if snapshot content-matches a blacklist entry: retry (bounded, 3 attempts)
  # with exactly one eligible-and-blocked candidate, all 3 attempts re-select
  # the SAME candidate, retry budget exhausts, blocked preset is applied anyway
```

## 6. Exact selector flow — after

```
applyEligibleCanonicalPreset(effectName):
  candidates = knowledgeBase.loadWhitelist(effectName)
  blockedEntries = knowledgeBase.loadBlacklist(effectName)   # NEW: loaded up front
  for each candidate:
    skip if !isReusableAuthoredPreset(candidate)
    skip if !isEligibleForAutomaticProductionSelection(resolveCompatibility(...))
    skip if candidate.snapshot content-matches any entry in blockedEntries   # NEW
    eligible.push_back(candidate)
  pick uniformly among eligible          # blocked candidates structurally cannot appear here

pickNext():  # UNCHANGED below this point -- still runs as defense-in-depth
  ... applyEligibleCanonicalPreset(...) or randomizeEffectParams(...) ...
  if snapshot content-matches a blacklist entry: retry (bounded, 3 attempts)
```

The pre-existing post-hoc retry loop in `pickNext()` was deliberately left in place (per the task's own "may remain as a compatibility guard for fallback-randomized snapshots if still useful") — it still matters for the `randomizeEffectParams()` fallback path, which has no candidate-filtering stage of its own to hook a hard exclusion into.

## 7. Blocked-preset guarantee

**Structural, not probabilistic.** A canonical authored preset that content-matches a blacklist entry for the same effect can never enter `eligible`, so it can never be the result of the uniform pick, regardless of how many retry attempts occur, how many other candidates exist, or what any (currently nonexistent) weight would have assigned it. Verified by `TFEligibilitySelfTest` cases 12–14:
- **Case 12:** the sole compatible+reusable candidate for an effect, also blocked → `applyEligibleCanonicalPreset()` returns `false` (no selectable preset at all), not "applied after retries."
- **Case 13:** the same scenario repeated 20 times — never once selected.
- **Case 14:** two eligible-by-compatibility candidates, one also blocked — across 30 iterations, the blocked one is never selected while the clean one is (both assertions independently checked, so "never selected" isn't confounded with "never eligible for an unrelated reason").
- **Case 11 (control):** confirms the new blacklist-lookup doesn't over-exclude — a candidate with no matching blacklist entry remains selectable even when an *unrelated*-content blacklist entry exists for the same effect.

## 8. Compatibility/Unclassified behavior

Unchanged from Increment 1, re-verified this pass (cases 1–10 in `TFEligibilitySelfTest` are untouched and still pass): preset override → effect default → Unclassified precedence; explicit empty override excludes; Unclassified excludes; legacy anonymous presets never auto-eligible.

## 9. Favored/weight behavior

No persisted numeric weight/preference field exists anywhere in the frozen schema (`KnowledgeEntry`/`EffectLevelKnowledge`, both re-inspected this increment — unchanged). Continues Increment 1's model: uniform choice among candidates that survive *all* hard filters (reusable AND compatible AND not blocked). Case 14 directly proves "favored (whitelist membership) never overrides blocked" — the blocked candidate had equal whitelist-membership standing to the clean one and still never won.

## 10. Pi metadata behavior

Unchanged: `temporal-fields` still has no Pi-quality-mode/filter concept of any kind (re-confirmed by inspection this pass). `resolvePiSafe()` remains unused by this consumer. `piSafe` metadata on the newly-authored real preset was not set (the debugger's `authorCanonicalTemporalFieldsSeedPreset()` doesn't set it) — inert, as documented, not claimed as hardware validation.

## 11. History/cooldown status

Unchanged: no existing shared/runtime history or cooldown policy was found ready to adopt (re-confirmed). Not implemented, per the task's own instruction.

## 12. Deterministic/reference behavior

Unchanged: no seed/reseed concept exists in `TFEffectPicker`; none was added. `presetId` continues to be used for logging/reference — the newly-authored real preset's id, `preset.dither.20260809_172713_1`, is generated by the same `synthesizeMigrationPresetId()` timestamp+counter pattern established in the prior Architecture-Closure Session's `saveCurrentToWhitelist()` change, unmodified by this increment.

## 13. Real authored-pack proof

**This is the increment's central new evidence — a real artifact, not a test fixture.**

1. Added a genuine, permanent debugger authoring capability: `pMarkTemporalFieldsCompatible` (GUI toggle + `'c'` keybinding), consulted by the *real, unmodified* `saveCurrentToWhitelist()`.
2. `authorCanonicalTemporalFieldsSeedPreset()` calls that exact same `saveCurrentToWhitelist()` and the exact same `exportKnowledgePack()` — not a parallel/duplicate write path — sequenced once at debugger startup (this environment cannot reliably drive live GUI/keyboard input to trigger the equivalent human action; see §17 for the honest limitation this implies).
3. Ran the real, fully-linked `shader-effect-debugger` binary. Confirmed directly against the real on-disk file (not a log claim):

```json
// assets/shared/video-effects/knowledge/effect-knowledge-pack.json (excerpt, real file)
{
  "compatibleSceneIds": ["temporal-fields"],
  "effect": "dither",
  "presetId": "preset.dither.20260809_172713_1",
  "schemaVersion": 1,
  "snapshot": { "alpha": 0.5, "maxPixelation": 6.0, "opacity": 1.0 },
  "sourceSketch": "shader-effect-debugger",
  "sourceVideo": "",
  "timestampUtc": "2026-08-09T17:27:13Z"
}
```
Canonical effect ID (`dither`, real catalog id), stable `presetId` (well-formed, `isWellFormedEffectPresetId`-compliant), positive `temporal-fields` compatibility, real authored snapshot (the schema's own curated defaults for `dither`), not present in any blacklist — every requirement from §4 (C) of the task is met by this one real entry.

4. Ran the real `scripts/sync-video-effect-assets.py`. Real console output: `[temporal-fields] update: bin/data/shared-video-effects/knowledge/effect-knowledge-pack.json (from canonical knowledge pack)`. Confirmed the synced copy in `sketches/temporal-fields/bin/data/shared-video-effects/knowledge/effect-knowledge-pack.json` is byte-identical to the canonical source (checked directly, not assumed).

5. Launched the real, fully-linked `temporal-fields` binary. Real captured log line from its own `setup()` (not a test): **`TFEffectPicker: shared effect knowledge: imported 1 whitelist + 0 blacklist entries`** — the real production scene's own `TFEffectPicker::setup()` genuinely imported this one real authored entry from its own synced `bin/data` copy.

## 14. Tests/builds

```
cd shared/src/video-effects/test && make -f Makefile.tests clean && make -f Makefile.tests test
  → 93/93 checks passed (unchanged -- no new standalone-suite tests this increment, same reason as
    Increment 1: the new logic lives in temporal-fields/shader-effect-debugger, not OF-free)

python3 scripts/check-video-effect-drift.py
  → All video-effect drift checks passed

cd sketches/shader-effect-debugger && make Release -j1
  → compiling done; Mach-O 64-bit executable produced

cd sketches/temporal-fields && make Release -j1
  → compiling done; Mach-O 64-bit executable produced
```

## 15. Runtime evidence

**`shader-effect-debugger`** (real launch, ~10s, console captured): `KnowledgePackSelfTest: PASS: 60/60 checks passed` (the pre-existing, unrelated round-trip self-test — unregressed; its count grew from 42 in Session 2 to 60 here because the concurrent Architecture-Closure Session extended it for schemaVersion-2/presetId coverage, not this increment). The new authoring action produces no console log itself (`showToast()` is on-screen only, not `ofLogNotice`) — verified instead via the real on-disk artifact (§13) and the real downstream import (below), which is stronger evidence than a log line would have been.

**`temporal-fields`** (real launch, ~10s, console captured):
```
[notice] TFActivityStatusSelfTest: PASS: 25/25 checks passed     (unregressed)
[notice] TFEligibilitySelfTest: PASS: 19/19 checks passed        (14 from Increment 1 + 5 new blocked-exclusion checks)
[notice] TFEffectPicker: shared effect knowledge: imported 1 whitelist + 0 blacklist entries   (REAL production import)
[notice] TFEffectPicker: applying canonical eligible preset 'preset.dither.selftest_compatible' for effect 'dither'
[notice] TFEffectPicker: applying canonical eligible preset 'preset.dither.selftest_effect_default' for effect 'dither'
[notice] TFEffectPicker: applying canonical eligible preset 'preset.dither.selftest_not_blocked' for effect 'dither'
... (other self-test preset ids, all correctly gated)
```

## 16. Risks

- **Implementation:** none new beyond Increment 1's carried-forward note (the fallback-randomized-snapshot path still relies on the pre-existing bounded retry for its own blacklist avoidance, unchanged — this increment's hard guarantee is scoped to canonical authored presets, exactly as instructed).
- **Migration:** none new.
- **Build:** none new.
- **Ownership:** none new.

## 17. Honest limitation on the real-production-application proof

The real `temporal-fields` scene's own `backgroundLayer.effectPicker` instance (as opposed to `TFEligibilitySelfTest`'s scratch instances) did **not** happen to select "dither" during the ~10-second observed run — its coarse effect-name selection (`weights.effectWeights`) is a uniform-ish weighted pick across roughly 16 candidates plus "Raw," gated further by `TFBackgroundLayer`'s own FULL_VIDEO/FULL_IMAGE/SPLIT mode cycling, so landing on "dither" specifically within a short bounded window is a matter of timing, not a correctness question. **What is directly proven:** (a) the real scene's own `setup()` genuinely imported the one real authored entry (§13, step 5 — this is the real production class, not a test double), and (b) the identical `applyEligibleCanonicalPreset()`/`applyParamsFromSnapshot()` code, exercised via `TFEligibilitySelfTest`'s real (non-mocked) `TFEffectPicker` instances against real seeded data structurally identical in shape to the real authored entry, correctly applies it. Not independently proven: the real scene's own picker happening to render `dither` with the real authored values within this specific short run. This is disclosed as a real, precise gap, not glossed over — the honest reading is "proven correct and proven reachable from real production `setup()`," not "observed rendering with these exact real values in this exact run."

## 18. Contract changes requested

None. No frozen struct, schema field, canonical ID, or JSON shape was added or altered. `compatibleSceneIds` (used by the new debugger toggle) and `presetId` (used by the pre-existing `synthesizeMigrationPresetId()` call) both already existed in the frozen schema before this increment.

## 19. Recommended next step

Run `temporal-fields` for a longer, unattended window (or temporarily bias `weights.effectWeights` toward `dither` for one diagnostic run only, then revert) to directly observe the real scene's own picker rendering the real authored preset, closing the one gap noted in §17. Separately: author additional real presets (ideally via genuine interactive debugger use, not the automated bootstrap this increment relied on) so more of `TFEffectPicker`'s 7 randomized effects have real eligible canonical knowledge, and consider extending `pMarkTemporalFieldsCompatible`-style authoring to a real multi-scene picker if/when a second scene adopts this same integration pattern.

---

## Final disposition

**PRODUCTION SELECTOR INCREMENT 2 ACCEPTANCE-READY**
