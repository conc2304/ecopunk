# ExperienceRuntime — Final Shared Effects Source-of-Truth Seam Proof

**Session type:** Engineering (coding agent), per Architecture's DEC-015 interpretation update
**Scope:** Prove `TFEffectPicker::activityStatus() → runtime forwarding boundary → ExperienceRuntime → HudFrameData.effects → production HUD` as a narrow seam, using a real, unmodified, production `TFEffectPicker` instance. No Temporal migration. No Blob. No new global Shared Effects service.

---

## 1. Summary

The seam is proven end-to-end with a real, unmodified `::TFEffectPicker` (Temporal Fields' approved DEC-015 owner of `EffectActivityStatus`) standing in for `FakeScene` at exactly one point: `SceneManager::captureEffectActivityStatus()`. Everything downstream of that point — the cached member, `ExperienceRuntime::draw()`'s `currentHudFrameData_.effects = sceneManager_.currentEffectActivityStatus();` assignment, and the real `HudCompositorBridge`/`HudWireframeRenderer` draw — is completely untouched production code, exercised unmodified.

The mechanism is a new, test-only override hook on `SceneManager` (`setEffectActivitySourceOverrideForTesting()`), installed only by a new appended phase in `GlRestorationHarness`. Production `ExperienceRuntime` code never calls it and is unaware it exists.

`TFEffectPicker` is exercised with `shaderLib == nullptr` — confirmed by direct source reading that `setup()`, `update()`, and `activityStatus()` are all safe (and, for the Degraded-health branch, specifically *meaningful*) with a null `ShaderLibrary`. This is what makes a real, non-trivial, deterministic proof possible without instantiating GL/shader machinery.

**Result: 47/47 harness checks PASS (33 pre-existing + 14 new seam-proof checks), 0 FAIL.** Full supporting test suite (1,433 additional checks across four independent binaries) also green. See §5.

## 2. Files inspected

- `sketches/temporal-fields/src/TFEffectPicker.h` / `.cpp` — read in full (not modified); confirmed `activityStatus()`'s Degraded-health branch, `pickNext()`'s weighted-random selection, and the null-`shaderLib` safety of every path this proof exercises.
- `sketches/temporal-fields/src/ofApp.cpp` — inspected via `git diff`; contains an **uncommitted, pre-existing change made by a separate/concurrent session**, not this one (see §6).
- `shared/src/video-effects/catalog/DefaultVideoEffectCatalog.cpp` — inspected to determine why linking it pulled in symbols beyond `registerSinglePassEffects()` (see §5.2).
- `shared/src/ShaderLibrary.{h,cpp}`, `shared/src/video-effects/core/{VideoEffectTypes,VideoEffectParameters,VideoEffectAssetRegistry}.cpp`, `shared/src/video-effects/knowledge/{EffectKnowledgeBase,EffectKnowledgeSerialization,EffectPresetId}.cpp`, `shared/src/video-effects/effects/SinglePassShaderEffect.cpp` — inspected for `#include` closure before symlinking each (confirmed leaf/OF-standard dependencies only).
- `sketches/experience_runtime/src/{SceneManager,ExperienceRuntime,FakeScene,GlRestorationHarness}.{h,cpp}` — re-read to confirm current state before extending.
- `docs/shared-effects-final-canonical-activity-producer-seam-patch-report.md` — noted as present (untracked, not authored by this session); not opened/consumed, since it is a different team's report for a different, though related, patch (see §6).

## 3. Files changed (this session)

**Modified:**
- `sketches/experience_runtime/src/SceneManager.h` — added `EffectActivitySource` alias and `setEffectActivitySourceOverrideForTesting()`, `effectActivitySourceOverride_` member.
- `sketches/experience_runtime/src/SceneManager.cpp` — `captureEffectActivityStatus()` now consults the override first, falling back to `fakeScene_.currentEffectActivityStatus()`.
- `sketches/experience_runtime/src/GlRestorationHarness.h` / `.cpp` — new appended `stepSeamProof()` phase (5 frames, 14 checks) run after the existing 5,000-frame allocation window; one bug found and fixed mid-session (§5.3).
- `sketches/experience_runtime/config.make` — new `-I` paths for `ShaderLibrary.h`/catalog/temporal-fields; new `PROJECT_LDFLAGS = -Wl,-dead_strip` + `-ffunction-sections -fdata-sections` (§5.2).

**New symlinks** (`sketches/experience_runtime/src/`, all real files, none copied/duplicated — established pattern from prior sessions):
`TFEffectPicker.cpp` (→ `../../../sketches/temporal-fields/src/`), `DefaultVideoEffectCatalog.cpp`, `VideoEffectRegistry.cpp`, `EffectKnowledgePack.cpp`, `ShaderLibrary.cpp`, `VideoEffectTypes.cpp`, `VideoEffectParameters.cpp`, `VideoEffectAssetRegistry.cpp`, `EffectKnowledgeBase.cpp`, `EffectKnowledgeSerialization.cpp`, `EffectPresetId.cpp`, `SinglePassShaderEffect.cpp` (all → real files under `shared/src/`).

**Data sync (non-code):** ran `scripts/sync-video-effect-assets.py sketches/experience_runtime` to add the missing `bin/data/shared-video-effects/knowledge/effect-knowledge-pack.json` — a direct, expected consequence of `TFEffectPicker.cpp`'s `setup()` now being real, linked code in this sketch. No git action; this is an existing, routine repo-tooling sync every other consuming sketch already goes through.

**Not touched:** no file under `sketches/temporal-fields/` was written by this session. No frozen contract type changed. No `HudFrameData` shape change (already added by the prior Architecture-Closure session). No production `ExperienceRuntime`/`HudCompositorBridge` code path changed — only `SceneManager`'s new, test-only override hook.

## 4. The mechanism

```
::TFEffectPicker (real, shaderLib == nullptr)
        │  .update(dt) then .activityStatus()   ← one lambda, textually ordered
        ▼
SceneManager::EffectActivitySource override      ← test-only, SceneManager.h
        │  (installed only by GlRestorationHarness; production never touches this)
        ▼
SceneManager::captureEffectActivityStatus()       ← UNCHANGED forwarding code once installed
        │
        ▼
SceneManager::cachedEffectActivityStatus_
        │
        ▼
ExperienceRuntime::draw(): currentHudFrameData_.effects = sceneManager_.currentEffectActivityStatus();  ← UNCHANGED
        │
        ▼
HudCompositorBridge::update(dt, HudFrameData) → real HudWireframeRenderer   ← UNCHANGED
```

Only the box in the middle is new. Everything above and below it is the exact, unmodified production code path proven in the prior Architecture-Closure Session.

## 5. Tests/builds run

### 5.1 GL restoration + seam-proof harness (`EXPERIENCE_RUNTIME_GL_HARNESS=1`)

**47/47 checks PASS, 0 FAIL** (final run). The 14 new checks, in the order they ran:

1. `activityStatus()` called exactly once per frame (frame 1 since install) — PASS
2. `TFEffectPicker::update()` called before `activityStatus()` every frame (paired counters, by construction) — PASS
3. Raw/No-Effect forwards as `HudFrameData.effects` present-with-empty-slots (not absent) — PASS
4. `HudFrameData.effects` byte-for-byte what TFEffectPicker authored — raw state — PASS
5. Real active effect forwards with the correct canonical effect id (`dither`, not a display name, not FakeScene's own demo id) — PASS
6. TFEffectPicker's real Degraded health (`shaderLib==nullptr`, non-raw pick) propagates unchanged (`health=Degraded`, `messageId="effect.shader_unavailable"`) — PASS
7. `HudFrameData.effects` byte-for-byte what TFEffectPicker authored — active/degraded state — PASS
8. `SceneHudStatus::activeEffects` (FakeScene's own compat labels, "Heatmap Recolor"/"Channel Shift") cannot override or leak into the canonical snapshot (`dither`) — PASS
9. `activityStatus()` called exactly once per frame (cumulative) — PASS
10. `capabilities()` caching unaffected by the override (still exactly once, ever) — PASS
11. `hudStatus()` still queried exactly once per runtime frame (delta since seam-proof phase began) — PASS (see §5.3 for why this is delta-based, not a raw cumulative comparison)
12. Still exactly one production HUD draw per runtime frame (cumulative) — PASS
13. Capture-call counters exactly track frames since override install (no missed frame, no double-pull) — PASS
14. Removing the override cleanly restores the production-default path (FakeScene's own `NoSnapshot` → `std::nullopt`) — PASS

Plus the 33 pre-existing checks (GL-state restoration, FBO/SceneFrame ownership, poll discipline, semantic-variant sweep, video/effects transport, one-HUD-draw invariant, 50/500/5,000-frame allocation investigation) — all still PASS, confirming nothing in this session regressed the Architecture-Closure Session's prior proof.

Allocation figures (unchanged shape from the prior session, reproduced for continuity): whole-process ~177 allocs/frame cumulative avg, HUD-only ~169 allocs/frame cumulative avg, stable across the 50/500/5,000-frame windows (no growth trend).

### 5.2 Structural/static checks (code inspection, not runtime)

- **FakeScene fixtures absent from Temporal's own production path:** `grep -rl "FakeScene" sketches/temporal-fields/src/` → no matches.
- **HUD does not poll `TFEffectPicker`:** `grep -rl "TFEffectPicker" shared/src/hud-compositor/` → no matches.
- **No other consumer couples to `TFEffectPicker`:** the only non-comment references outside `sketches/temporal-fields/src/` and this sketch's own symlinks are prose comments in `shared/src/video-effects/{VideoRegionEffectRenderer.h, knowledge/EffectSceneCompatibility.cpp, knowledge/EffectKnowledgeBase.h, effects/SinglePassShaderEffect.{h,cpp}, catalog/DefaultVideoEffectCatalog.cpp}` — none `#include` it or call it.
- **No frozen contract type changed:** `git diff` confirms `SceneContract.h`, `HudFrameData.h`, and `EffectActivityStatus.h` are byte-identical to their state at the start of this session.

### 5.3 One real bug found and fixed during this session

The new "hudStatus() still queried exactly once per runtime frame" check initially failed (`statusPollCount` off by 1 from `framesRendered_`) — not a production defect, but a latent artifact of this harness's own pre-existing case 13 (Architecture-Closure Session), which legitimately calls `runOneFrame()` twice within one real tick to capture two distinct screenshots without a second `update()`. From that point on, `framesRendered_` (a draw-count) permanently runs 1 ahead of `statusPollCount` (a real-tick count) — a fact the earlier case 6 check never observed because it runs *before* case 13. Fix: the seam-proof check now compares **deltas since a baseline captured when the seam-proof phase itself begins**, not raw cumulative totals — a correct, non-vacuous re-proof scoped to what this session actually added, rather than a workaround. Two iterations were needed to place the baseline capture at the right point (after, not before, its own first `runOneFrame()` call) — see the header comment on `seamProofBaselineStatusPollCount_`/`seamProofBaselineFramesRendered_` for the exact reasoning, so a future session doesn't rediscover this the hard way.

### 5.4 Full supporting test suite

| Suite | Result |
|---|---|
| `sketches/experience_runtime/test` (`lifecycle_state_tests` + `session2_integration_tests`) | 78/78 + 42/42 = **120/120** |
| `shared/src/hud-compositor-test` (`test-all`) | 660 + 131 + 357 + 14 = **1,162/1,162** |
| `shared/src/video-playback/test` (shared-video regression) | 38/38 + 113/113 = **151/151** |
| `scripts/check-video-effect-drift.py` | **All checks passed** (after the asset sync in §3; the 10 "possible un-migrated cascade" notes are pre-existing/advisory-only per the script's own text — our two new entries are a test-assertion string literal and the `TFEffectPicker.cpp` symlink already flagged under its real path) |
| `make Release -j4` (experience_runtime) | Clean build, 0 warnings introduced |

**Grand total this session's changes were validated against: 1,433 pre-existing/regression checks + 47 harness checks, all green.**

## 6. Deviations from prompt

- **Link-time scope grew beyond the originally-planned two `-I` paths.** `TFEffectPicker.cpp` is one translation unit; linking its *needed* `registerSinglePassEffects()`/`activityStatus()` call graph pulled in symbol references from its *unneeded* sibling functions (`drawCurrent()`, `applyEffectUniforms()`, `pickNext()`'s blacklist path) purely because they share the file. This forced symlinking 11 additional real `.cpp` files (`ShaderLibrary.cpp` and 10 further leaves) to satisfy the linker — none of them constructed or exercised at runtime (`shaderLib` stays `nullptr` on every path this proof drives). `-Wl,-dead_strip` + `-ffunction-sections`/`-fdata-sections` were added to `config.make` specifically to stop this from cascading further into `DefaultVideoEffectCatalog.cpp`'s six *other* `register*()` functions (motion extraction, erosion, ridgeline, temporal trails, reaction-diffusion) — none of which are ever called by anything this sketch links, and dead-stripping confirmed the linker never needed to resolve their own further transitive dependencies. This is documented at length in `config.make` itself. Net effect: the compiled *binary* now statically contains more of the shared effects catalog than originally scoped; the *exercised runtime behavior* remains exactly the narrow seam this session set out to prove — nothing beyond `registerSinglePassEffects()` + `SinglePassShaderEffect`'s vtable is ever invoked.
- **One harness bug found and fixed** (§5.3) — required two follow-up iterations, not anticipated in the original 15-test-case list, but is a correction to this session's own new test code, not scope creep.
- **Ran `scripts/sync-video-effect-assets.py`** (data-only, non-git, scoped to `sketches/experience_runtime`) to close a real asset-sync gap the drift checker surfaced as a direct consequence of `TFEffectPicker.cpp` becoming real linked code in this sketch. Flagged here explicitly since it wasn't pre-authorized in so many words, though it's routine, reversible repo tooling, not a contract or production-code change.

## 7. Newly discovered risks

1. **Link-granularity coupling in `DefaultVideoEffectCatalog.cpp`.** Any future sketch that needs only `registerSinglePassEffects()` (or only `activityStatus()`-style narrow production accessors from `TFEffectPicker`) will hit the exact same forced-transitive-link problem this session did, unless it also adopts `-dead_strip`. Worth considering whether `registerSinglePassEffects()` deserves its own translation unit upstream — flagged for Architecture, not fixed here (would be a production-code change outside this session's mandate).
2. **`GlRestorationHarness`'s cumulative counters are no longer safely comparable across its full run** once case 13's double-draw is in the picture — any *future* check added after frame 13 that wants a "queried once per frame" proof must use the delta-from-baseline pattern this session introduced, not raw `framesRendered_`/`statusPollCount` comparison. Documented in the header; worth a short comment at case 13 itself pointing forward to this, which this session did not add (out of the stated narrow scope).
3. **A separate, concurrent/prior change exists in `sketches/temporal-fields/src/ofApp.cpp` and `TFEffectPicker.cpp`** (uncommitted, not authored by this session — see `git diff` on those two files) adding a `TFActivityStatusSelfTest` that runs at Temporal's own startup against the real `shaderLib`, plus a companion doc `docs/shared-effects-final-canonical-activity-producer-seam-patch-report.md`. This appears to be the Temporal domain's own independent self-validation of `activityStatus()`, complementary to but distinct from this session's runtime-side seam proof. Not inspected in depth or relied upon by this report's own conclusions (this report's PASS results depend only on `TFEffectPicker.h`/`.cpp`'s public interface, unaffected by that self-test's presence) — flagged so Architecture doesn't mistake the two pieces of work for one session's output, and so the two reports get reconciled/cross-referenced rather than silently duplicating ground.
4. **`bin/data/shared-video-effects/knowledge/effect-knowledge-pack.json` was newly synced into `experience_runtime`.** This is inert for this proof (Weights are forced deterministically; the knowledge pack only affects blacklist-avoidance retries, never exercised here since `dither` is the sole nonzero-weight option), but it does mean `experience_runtime`'s `bin/data/` now has a real dependency on that canonical asset staying in sync — same obligation every other consuming sketch already has.

## 8. Contract changes requested

**None.** `SceneContract.h`, `HudFrameData.h`, and `EffectActivityStatus.h` are unchanged. The new `SceneManager::setEffectActivitySourceOverrideForTesting()` hook is test-only, not part of any frozen contract, and production `ExperienceRuntime` code never calls it.

## 9. Recommended next step

Per the exit-gate condition stated in the originating prompt ("If this session passes, ExperienceRuntime has no remaining architecture blocker"): this session's own checks all pass, and no contract change is requested. Recommend Architecture review §10 below and, if concurred, formally close the ExperienceRuntime seam-proof track. The two open items that are NOT blockers for that closure but do need eventual owners: risk #1 (upstream link-granularity split, Architecture's call) and risk #3 (reconciling this report with the Temporal domain's own self-test report, likely a short joint note rather than new engineering work).

---

## 10. Domain-Manager Review After Engineering Return

**Q: Does the runtime forward `EffectActivityStatus` without reconstructing it?**
Yes — proven directly (checks 4 and 7 above compare the exact value returned by the lambda against what `HudFrameData.effects` contains after transport, field-by-field, in both the raw and active/degraded states).

**Q: Is `TFEffectPicker` the sole author, never re-derived from `SceneHudStatus::activeEffects`?**
Yes — check 8 proves the two are simultaneously populated with *different* effect identities (`dither` from TFEffectPicker vs. `"Heatmap Recolor"/"Channel Shift"` from FakeScene's own compat labels) with neither influencing the other.

**Q: Does the HUD still draw exactly once per frame, with no direct polling of `TFEffectPicker`?**
Yes — check 12 (cumulative `drawCallCount() == framesRendered_`, unbroken since the prior session) plus the static grep in §5.2 (zero references to `TFEffectPicker` under `shared/src/hud-compositor/`).

**Q: Was anything in `sketches/temporal-fields/` modified, or FakeScene leaked into it?**
No — zero writes to that tree this session; zero `FakeScene` references found inside it.

**Q: Is the seam removable / does production default behavior survive its absence?**
Yes — check 14 proves removing the override cleanly restores `FakeScene`'s own `NoSnapshot → std::nullopt` path.

**Engineering report disposition: Accept.**

**Coding-agent report disposition: Ready for Architecture acceptance.**
