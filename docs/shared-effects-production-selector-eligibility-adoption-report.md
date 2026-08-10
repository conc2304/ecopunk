# Shared Effects — Production Selector / Eligibility Adoption Report

**Status:** Implemented, verified by real build + real captured console output. Not yet Architecture-reviewed — same status class as every other Shared Effects document in this chain.

---

## 1. Summary

`TFEffectPicker::pickNext()` now consults the real, frozen `EffectKnowledgePrecedence.h` API (`resolveCompatibility()`, `isEligibleForAutomaticProductionSelection()`, `EffectPresetId.h`'s `isReusableAuthoredPreset()`) before applying an authored preset's parameter values, instead of relying solely on its pre-existing post-hoc, content-match blacklist avoidance. No schema changed. No new fields were added. Which *effect* gets chosen remains `TFEffectPicker::Weights`' own local policy, unaffected — only which *parameter values* an already-chosen effect uses is now eligibility-gated.

## 2. Chosen consumer and why

**`temporal-fields` / `TFEffectPicker`**, per the task's own stated preference, confirmed correct by inspection:
- Already imports the canonical shared pack (`setup()`'s existing `importEffectKnowledgePack()` call, from the prior Engineering Session 2 increment).
- Real production effect chooser/renderer (`pickNext()`/`drawCurrent()` are genuinely on the render path, not tooling).
- Its canonical activity producer seam (`activityStatus()`) was already proven in the prior "Final Canonical Activity Producer Seam Patch."
- It already had blacklist-aware behavior (content-match avoidance) — but, confirmed by direct inspection of `pickNext()` before this change, it **never called any function from `EffectKnowledgePrecedence.h`** — the exact bypass this task's brief predicted.

No other consumer was considered: `blob-region-prototype` is explicitly out of scope (§4 "Blob boundary"); `shader-effect-debugger` is tooling, explicitly disallowed as the proof consumer; `fragment-trail`/`quadrant-crosshair` are explicitly disallowed.

## 3. Files inspected

`shared/src/video-effects/knowledge/{EffectKnowledgeBase,EffectKnowledgePrecedence,EffectPresetId,EffectLevelKnowledge,EffectKnowledgePack,EffectKnowledgeSerialization}.{h,cpp}`, `shared/src/video-effects/knowledge/EffectRandomizer.h`, `shared/src/video-effects/catalog/DefaultVideoEffectCatalog.cpp`, `sketches/temporal-fields/src/TFEffectPicker.{h,cpp}`, `sketches/temporal-fields/src/TFBackgroundLayer.h`, `sketches/temporal-fields/src/ofApp.cpp` (also found substantially rewritten by a concurrent, unrelated DEC-013/DEC-014 video-adapter migration session — read, not modified beyond the two new `#include`/call-site lines this increment needed), `docs/shared-effect-knowledge-schema-v1.md`.

## 4. Files changed

| File | Reason |
|---|---|
| `sketches/temporal-fields/src/TFEffectPicker.h` | New `applyEligibleCanonicalPreset()`/`applyParamsFromSnapshot()` private methods; new test-support accessors (`knowledgeBaseForTest()`, `forceEffectForTest()`, `lastPickUsedCanonicalPreset()`, `lastAppliedPresetId()`, `currentParamSnapshotForTest()`) |
| `sketches/temporal-fields/src/TFEffectPicker.cpp` | `pickNext()` now calls `applyEligibleCanonicalPreset()` before falling back to the pre-existing `randomizeEffectParams()`; new method implementations; new includes for `EffectKnowledgePrecedence.h`/`EffectLevelKnowledge.h`/`EffectPresetId.h` |
| `sketches/temporal-fields/src/TFEligibilitySelfTest.{h,cpp}` | **New.** Deterministic focused tests against the real compiled class (§9) |
| `sketches/temporal-fields/src/ofApp.cpp` | Wired the new self-test to run automatically at `setup()`, alongside the existing activity-status self-test |

No file under `shared/src/video-effects/knowledge/`'s frozen types, `EffectActivityStatus`, `ExperienceRuntime`, `HudFrameData`, or `blob-region-prototype` was touched.

## 5. Exact selection flow — before

```
pickNext():
  currentEffect = tfWeightedPick(weights.effectWeights)   // scene-local coarse policy, unchanged
  randomizeEffectParams(currentEffect)                     // hand-tuned ofRandom() ranges, always
  snapshot = currentParamSnapshot(currentEffect)
  if snapshot matches a blacklist entry (content-equality): retry (bounded, 3 attempts)
```
Never consulted `compatibleSceneIds`, `presetId`, effect-level defaults, or any `EffectKnowledgePrecedence.h` function — confirmed by grep before editing (zero matches for those symbols anywhere in the pre-change file).

## 6. Exact selection flow — after

```
pickNext():
  currentEffect = tfWeightedPick(weights.effectWeights)   // UNCHANGED -- still the scene-local policy
  if currentEffect == "": randomizeEffectParams(""); done  // "Raw" -- unchanged

  usedPreset = applyEligibleCanonicalPreset(currentEffect):
    candidates = knowledgeBase.loadWhitelist(currentEffect)
    for each candidate:
      skip if !isReusableAuthoredPreset(candidate)                       // legacy anonymous -> never auto-eligible
      classification = resolveCompatibility(candidate, effectDefault, "temporal-fields")
      skip if !isEligibleForAutomaticProductionSelection(classification) // Unclassified/Disallowed -> ineligible
      add to eligible[]
    if eligible empty: return false
    pick uniformly among eligible[]                        // "existing whitelist-membership behavior," no new weight field
    applyParamsFromSnapshot(currentEffect, chosen.snapshot) // paramX..W <- real authored values
    log chosen.presetId; return true

  if !usedPreset: randomizeEffectParams(currentEffect)      // UNCHANGED fallback

  snapshot = currentParamSnapshot(currentEffect)
  if snapshot matches a blacklist entry: retry (bounded, 3 attempts, UNCHANGED)
```

## 7. Compatibility/Unclassified behavior

Implemented exactly per `EffectKnowledgePrecedence.h`'s frozen precedence (preset override → effect-level default → Unclassified), consumed, not re-derived: `applyEligibleCanonicalPreset()` calls `resolveCompatibility()` directly and never re-implements the precedence logic itself. Verified by `TFEligibilitySelfTest` cases 1–7 (§9): a positively-compatible preset with no default is eligible; an incompatible preset is excluded regardless of what an effect-level default would have said (both directions — override-wins-positive and override-wins-negative are both proven, not just one); Unclassified (no override, no default) is excluded; an explicit empty override is excluded (Disallowed, not Unclassified — a real, distinct classification, proven distinctly); an effect-level default correctly applies when a preset authors no override of its own.

## 8. Blocked/favored/weight behavior

- **Blocked:** blacklist membership was, and remains, checked entirely separately from eligibility (`applyEligibleCanonicalPreset()` never reads the blacklist — it only reads `loadWhitelist()`). The pre-existing post-hoc content-match retry loop still runs unconditionally after either path (canonical-preset or fallback-random), so a canonical preset that happens to also content-match a blacklist entry still triggers the existing bounded retry — this increment did not weaken that guarantee, and did not attempt to build a stronger one beyond what already existed (see §11, risk 1, for the one honest limitation this leaves).
- **Favored/weight:** no persisted per-preset numeric weight field exists anywhere in the frozen schema (confirmed by inspection of `KnowledgeEntry`/`EffectLevelKnowledge`) — per the task's own Step 3 ("If only whitelist/favored membership exists, use the existing behavior... If no authored weight exists, do not add schema"), this increment adopts exactly that: a uniform random pick among all *eligible* whitelist entries for the chosen effect. No new field was invented. `TFEligibilitySelfTest` case 10 proves this with real captured evidence (§9's log excerpt): across 40 forced re-picks with two eligible presets and one deliberately-ineligible one present, both eligible ids were selected in real, varying proportion and the ineligible one **never** was.
- **Favored never overrides hard exclusions:** proven by the same case 10 — the ineligible (incompatible) third preset was never applied despite being present in the same whitelist and eligible for equal random-pick weight if eligibility weren't checked first.

## 9. Tests/builds

```
cd shared/src/video-effects/test && make -f Makefile.tests clean && make -f Makefile.tests test
  → 93/93 checks passed (unchanged from before this increment -- this increment added no
    new standalone-suite tests, since the new logic lives in temporal-fields, which is not
    OF-free)

python3 scripts/check-video-effect-drift.py
  → All video-effect drift checks passed

cd sketches/temporal-fields && make Release -j1
  → compiling done; Mach-O 64-bit executable produced, no warnings/errors from any file
    this increment touched
```

**Runtime proof** (`bin/temporal-fields.app/Contents/MacOS/temporal-fields`, real launch, ~10s, console captured):
```
[notice] TFActivityStatusSelfTest: PASS: 25/25 checks passed     (prior increment, unregressed)
[notice] TFEligibilitySelfTest: PASS: 14/14 checks passed        (this increment, new)
```
Real log lines from that run (excerpted, not fabricated):
```
[notice] TFEffectPicker: applying canonical eligible preset 'preset.dither.selftest_compatible' for effect 'dither'
[notice] TFEffectPicker: applying canonical eligible preset 'preset.pixel_sorting.selftest_override_good' for effect 'pixel_sorting'
[notice] TFEffectPicker: applying canonical eligible preset 'preset.dither.selftest_effect_default' for effect 'dither'
[notice] TFEffectPicker: applying canonical eligible preset 'preset.channelshift.selftest_multi_a' for effect 'channelshift'
[notice] TFEffectPicker: applying canonical eligible preset 'preset.channelshift.selftest_multi_b' for effect 'channelshift'
... (alternating a/b across 40 iterations; 'selftest_multi_ineligible' never appears)
```

`TFEligibilitySelfTest`'s 14 checks: compatible-eligible-applied (4 checks: used-flag, presetId, two snapshot values); incompatible-excluded; Unclassified-excluded; override-beats-worse-default; override-loses-despite-better-default; explicit-empty-override-excludes; effect-default-applies-with-no-override; legacy-anonymous-never-eligible; forced-selection-honored-regardless; multi-eligible-never-selects-ineligible (2 checks).

## 10. Pi metadata behavior

The active `temporal-fields` production policy has **no Pi-quality-mode/filter concept at all today** (confirmed by inspection — nothing in `TFEffectPicker`, `TFBackgroundLayer`, or `ofApp` reads a quality-profile setting of any kind). Per the task's own instruction ("Only apply Pi filtering if the active production policy already has a defined Pi-quality mode/filter... Do not invent a new global quality policy"), **no Pi-based filtering was added.** `resolvePiSafe()` was inspected and left completely unused by this increment — it exists and is tested at the `shared/src/video-effects/test/` level already (pre-existing, unmodified), but `TFEffectPicker` does not call it, and this report does not claim otherwise. `piSafe` metadata on any seeded/real preset is inert for this consumer today.

## 11. History/cooldown ownership and implementation status

**Not integrated — no existing shared/runtime history or cooldown policy was found ready to adopt.** Confirmed by inspection: `EffectRandomizer` has no history/cooldown state of any kind (only a per-call `seed` and `whitelistBiasProbability`, neither of which is a cross-call history); `TFEffectPicker` itself has never had any recent-selection memory (its own `pickNext()`'s 3-attempt retry loop is single-call, immediate-avoidance only — confirmed still true after this increment, since `applyEligibleCanonicalPreset()` adds no state of its own beyond `lastPickUsedCanonicalPreset_`/`lastAppliedPresetId_`, which are diagnostic-only, reset every call, and consulted by nothing). **Ownership, if this is built later:** most naturally `TFEffectPicker` itself (it already owns the per-call selection loop this would extend) or a new shared `EffectRandomizer`-adjacent helper if multiple scenes eventually need the same recency window — not decided here, per the task's explicit "do not build a new history system in this session."

## 12. Deterministic seed/reference behavior

`TFEffectPicker` has **no existing seed/reseed concept** (confirmed — `pickNext()` uses the global `ofRandom()`/`tfWeightedPick()` stream throughout, both before and after this increment; `EffectRandomizer::RandomizeRequest::seed` exists but is never constructed or passed anywhere in this class, since this class never calls `EffectRandomizer::generate()` at all). Per the task's own instruction ("If the current picker/randomizer has seed/reseed/reference behavior, preserve it... Do not design a new reseed contract"), **no seed/reseed capability was added.** Determinism in this increment's own tests is achieved a different, already-sanctioned way — by controlling the *candidate set* deterministically (seeding exactly the knowledge entries a test needs) rather than by seeding the RNG, which is why `TFEligibilitySelfTest`'s single-eligible-candidate cases (1, 2, 3, 4, 5, 6, 7, 8) are 100% deterministic despite using the real, unseeded `ofRandom()` internally — with only one eligible option, "uniform pick among eligible" has only one possible outcome. Case 10 (multiple eligible candidates) is the one genuinely probabilistic case, and is asserted accordingly (never-selects-the-ineligible-one, not a specific sequence) — documented as such, not disguised as deterministic.

`presetId` is used for logging/reference exactly as the task requested (`ofLogNotice` cites the applied `presetId` by its real stable string on every canonical-preset application — see §9's captured log excerpt), and is exposed to callers/tests via `lastAppliedPresetId()`.

## 13. Deviations

- Effect-*selection* itself (which of the 7 randomized effect names gets chosen) was deliberately left ungated by eligibility, even though a literal reading of "route through canonical eligibility" could be argued to apply there too. Reason, with evidence: no `EffectLevelKnowledge` record exists yet for any of the 7 effects in the real canonical pack, so gating selection itself would make all of them Unclassified-and-ineligible today, collapsing the picker to constant "Raw" — a large, real, sudden regression the task's own "empty eligible set" risk guidance (§9 of the brief) warns against causing silently. This is documented in code (`applyEligibleCanonicalPreset()`'s own header comment) and here, not silently decided.
- `EffectRandomizer::generate()` itself was not adopted wholesale in place of `randomizeEffectParams()`'s hand-tuned ranges, to avoid changing the *artistic* (non-eligibility) visual behavior for the common case where no eligible preset exists yet — preserving "existing visual behavior as much as possible" per the task's own repeated instruction.

## 14. Risks

- **Implementation:** the blacklist-avoidance retry loop, when the *only* eligible whitelist candidate for an effect happens to also content-match a blacklist entry, will deterministically re-select that same single candidate on every retry attempt, exhaust the retry budget, and apply it anyway (matches the pre-existing "never blocks forever" contract exactly, but is worth naming explicitly: with exactly one eligible option, "avoid the blacklisted one" and "there is only one option" are in tension, and the pre-existing design already resolves that tension by accepting after the bound — this increment did not change that resolution, just makes it reachable via a canonical preset in addition to a random roll).
- **Migration:** none new — `temporal-fields` remains unmigrated to `IEcopunkScene`, unchanged by this increment.
- **Build:** none new — the `-j4` parallel-build race documented in the prior Session 2 report is unrelated to this increment's files and was not re-triggered (`-j1` used throughout, per the task's own instruction not to require `-j4`).
- **Ownership:** effect-level knowledge (`EffectLevelKnowledge`) remains unauthored for all of `temporal-fields`' 7 randomized effects in the real canonical pack — meaning today, in real production use (not this report's seeded tests), `applyEligibleCanonicalPreset()` will only ever find an eligible candidate once someone authors a `presetId`-bearing, positively-compatible whitelist entry via `shader-effect-debugger`. Until then, this increment's real-world behavior is identical to before it (verified: the real canonical pack's one existing entry, for `ascii_solarpunk`, isn't even one of the 7 effects this class randomizes at all).

## 15. Contract changes requested

None. No frozen struct, schema field, canonical ID, or JSON shape was added or altered.

## 16. Recommended next step

Author at least one real, `presetId`-bearing, `temporal-fields`-compatible whitelist preset via `shader-effect-debugger` for one of the 7 randomized effects (e.g. `dither`), export, sync, and confirm the real (non-test) production picker applies it — the seeded self-test proves the code path works; this would be the first real-world exercise of it outside a test.

---

## Final disposition

**PRODUCTION SELECTOR INCREMENT ACCEPTANCE-READY**
