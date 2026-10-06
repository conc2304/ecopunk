# TEMP-004 — Temporal Stale Playhead Invalidation Fix: Completion Report

**Date:** 2026-10-05
**Work item:** TEMP-004 (found by RT-002)
**Recipient:** Temporal Fields Scene Migration domain manager → ExperienceRuntime & SceneManager (for the full RT-002 rerun)
**Produced by:** Coding agent (Temporal domain)
**Repository:** `blueprint-emergence-updates` @ `59c24f4` + working tree. RT-002 harness/report files from the previous session are also uncommitted; the TEMP-004 changes are listed separately in §4.
**Authoritative inputs:** TEMP-004 authorization prompt; [RT-002 completion report](rt-002-blob-temporal-20-cycle-acceptance-report.md) §5/§7; DEC-014; [Scene/HUD Contract v1](../shared-project-docs/Scene-HUD-Contract-v1.md); [Governance](../shared-project-docs/01-architecture-governance.md); [Handoff Protocol](../shared-project-docs/02-cross-domain-handoff-protocol.md)

> The "TEMP-004 Architecture authorization" artifact is not in the repository. The authorization text in the prompt was treated as authoritative.

---

## 1. Summary

**Fixed.**
- `TimeOffsetVideoBuffer` now releases all six playhead GPU textures every time it resets its history (initial configure, reactivation reload, canonical media change, failed load).
- While history is empty, `getPlayheadTexture()` therefore returns an **unallocated** texture. Every Temporal consumer already treats that as "no valid frame — draw nothing".
- The one consumer that ignored that convention, `TFFragmentTransition::begin()` (the outgoing snapshot), now skips an unallocated outgoing texture.
- No public API, contract, ownership, ExperienceRuntime or SceneManager change.

**Focused proof** (real decode, real canonical `VideoPlaybackService`):
- **0 stale pre-clear playhead frames** over 5 reactivations + 3 canonical media changes;
- six playheads preserved, offsets unchanged, current-media refill every time, no reload loop;
- **139/139 checks**.

Also:
- Standalone Temporal ran 70 s with real rendering and logged **0 `texture is not allocated` warnings**.
- Temporal lifecycle harness: 295/295, 0 warnings.
- All unit suites green.

## 2. Root-cause confirmation

Traced end to end before any change:

1. **History validity is owned by** `TimeOffsetVideoBuffer::history` (a CPU `std::deque<ofPixels>`). Before the fix it was reset in exactly three places:
   - `configure()`;
   - `advanceToNextMedia()` (legacy path);
   - `loadExplicit()` (canonical path).

   Reactivation reaches `loadExplicit()` through `TimeOffsetPlaybackAdapter::invalidateForReactivation()` (clears `loadedMediaId_`) followed by `synchronizeSelectedMedia()`. Canonical media changes reach it through `synchronizeSelectedMedia()` with a new ID.
2. **What survived a reset:** each `Playhead::texture` (GPU, last uploaded frame) plus `uploadedThisFrame`. There was **no readiness flag**.
3. **Lookup:** `getPlayheadTexture()` calls `offsetToHistoryIndex()`, which returns −1 for empty history. In that case it skipped the upload and returned `ph.texture` unchanged (`TimeOffsetVideoBuffer.cpp:209-226` pre-fix).
4. **Presentation:** all direct draw sites guard `if (!tex.isAllocated()) skip`:
   - `TFPatternBSP:104`, `TFPatternBlobGrid:152`, `TFPatternParticleField:198`, `TFPatternTemporalTides:109`;
   - `TFPatternNetworkGrowth:274`, `TFPatternEcologicalSuccession:184`, `TFShapeFragmentRenderer:150`;
   - the standalone debug strip at `ofApp.cpp:368`.

   The design intent was "unallocated ⇔ no history". The buffer broke it by leaving textures allocated across resets, so the guard passed and **stale pre-reset imagery was drawn**.
5. **First-activation warning:** five transition call sites pass `getPlayheadTexture(lastPlayheadIndex)` into `TFFragmentTransition::begin()`:
   - `TFPatternBSP:244`, `TFPatternBlobGrid:319`, `TFShapeFragmentRenderer:124`, `TFPatternNetworkGrowth:225`, `TFPatternEcologicalSuccession:137`.

   `begin()` drew it into its snapshot FBO without a guard. Before any upload, that produced `ofGLRenderer: draw(): texture is not allocated`; later, it snapshotted stale content.

**Placement decision:** invalidation belongs at **history-reset time**, inside the buffer.
- That is the single place that knows history became invalid, and it covers every path: startup, reactivation, media change and failed load.
- The existing consumer contract (`isAllocated()`) then holds naturally.
- Private state suffices, so no public/shared API addition is needed (no DEC-014 review).

## 3. Files inspected

- **`shared/src`:** `TimeOffsetVideoBuffer.{h,cpp}`; `video-playback/adapters/TimeOffsetPlaybackAdapter.h`
- **`sketches/temporal-fields/src`:**
  - every `getPlayheadTexture()` consumer: `TFPatternBSP`, `TFPatternBlobGrid`, `TFPatternParticleField`, `TFPatternTemporalTides`, `TFPatternNetworkGrowth`, `TFPatternEcologicalSuccession`, `TFShapeFragmentRenderer`, `ofApp`;
  - `TFFragmentTransition.{h,cpp}`, `TemporalSceneCore.*`, `TFVideoAdapterSelfTest.{h,cpp}`, `test/`
- **`sketches/experience_runtime/src`:** `TemporalProductionScene.cpp` (activate → `invalidateForReactivation()`), `TemporalLifecycleHarness.cpp`; symlinks `TimeOffsetVideoBuffer.cpp` and `TFFragmentTransition.cpp` (both point at the canonical files)
- **`sketches/fragment-trail/config.make`:** that sketch excludes `shared/src` and uses its own `TimeOffsetVideoBuffer` fork, so it is not affected by this change.

## 4. Files changed (TEMP-004 only)

| File | Change |
|---|---|
| `shared/src/TimeOffsetVideoBuffer.h` | New **private** helper `resetHistoryAndPlayheadTextures()`. Comment on the existing public `getPlayheadTexture()` documents "unallocated while history is empty". No public signature added or changed. |
| `shared/src/TimeOffsetVideoBuffer.cpp` | Helper clears `history` and, for each playhead, `texture.clear()` + `uploadedThisFrame = false`. All three reset sites (`configure`, `advanceToNextMedia`, `loadExplicit`) now call it; `history.clear()` exists only inside the helper. |
| `sketches/temporal-fields/src/TFFragmentTransition.cpp` | `begin()`: draw the outgoing texture into the snapshot only `if (oldTex.isAllocated())`; otherwise the snapshot stays cleared (the transition reveals new content from empty). |
| `sketches/temporal-fields/src/TFVideoAdapterSelfTest.{h,cpp}` | Focused TEMP-004 proof (test-only): initial-empty, media-change, reactivation, failed-load, refill checks, plus a 5-reactivation / 3-media-change repetition phase with per-frame stale detection. |
| `docs/reviews/logs/temp004-*.log`, this report | Evidence. |

Not changed:
- `TimeOffsetPlaybackAdapter`, `VideoPlaybackService`, `TemporalProductionScene`, `TemporalSceneCore`, and all patterns;
- ExperienceRuntime and SceneManager;
- HUD, Shared Effects, every frozen contract;
- Temporal pattern/effect selection policy.

## 5. Implementation approach

```text
any history reset (configure / loadExplicit / advanceToNextMedia)
→ resetHistoryAndPlayheadTextures(): history.clear(); every playhead texture released
→ while history empty: getPlayheadTexture() returns an unallocated texture
→ consumers skip it (existing guard); transition begin() snapshots nothing
→ first new history frame: getPlayheadTexture() re-allocates + uploads current-media pixels (existing path)
```

Properties:
- **Offsets untouched:** `currentOffset`, `targetOffset` and motion are not modified.
- **Six-playhead structure preserved.**
- **No retained fallback image:** empty history renders as omission (background-only), the existing "no frame yet" behavior.
- **Cost:** one GL texture release per playhead per reset, re-allocated on refill. Resets only happen on load, media change and reactivation.

## 6. Tests/builds run

| Command | Result |
|---|---|
| `make Release -j8` — `temporal-fields` | exit 0, 0 errors |
| `make Release -j8` — `experience_runtime` (incremental, picks up both symlinked files) | exit 0, 0 errors |
| `temporal-fields` standalone, 70 s real run — `TFVideoAdapterSelfTest` | **PASS: 139/139 checks**; `TEMP-004 SUMMARY reactivations=5 mediaChanges=3 staleAfterReactivation=0 staleAfterMediaChange=0` |
| same run — real Temporal rendering | **0 `texture is not allocated` warnings**; only warning is the self-test's intentional bad-path load |
| `EXPERIENCE_RUNTIME_TEMPORAL_LIFECYCLE_HARNESS=1` | **295/295 PASS**, 0 warning/error lines |
| `temporal-fields/test` (`tf_timeline_tests`) | 96/96 |
| ExperienceRuntime unit suites | 78/78 · 42/42 · 68/68 · temporal semantic 34/34 · switch controller 127/127 |
| Shared Video tests | 38/38 · 113/113 |
| Shared Effects tests | 93/93 (linkage unaffected; run as a regression) |
| HUD tests | 713 · 140 · 357 · 14 — all pass |
| **Not run (by instruction)** | the full RT-002 20-cycle acceptance — returns to ExperienceRuntime |

An earlier background chain in this session had queued the RT-002 harness. It was **stopped during the `temporal-fields` compile, before RT-002 started** (no RT-002 log was produced).

Logs:
- [`logs/temp004-temporal-adapter-selftest-2026-10-05.log`](logs/temp004-temporal-adapter-selftest-2026-10-05.log)
- [`logs/temp004-temporal-lifecycle-harness-2026-10-05.log`](logs/temp004-temporal-lifecycle-harness-2026-10-05.log)
- [`logs/temp004-unit-suites-2026-10-05.log`](logs/temp004-unit-suites-2026-10-05.log)
- [`logs/temp004-build-summary-2026-10-05.log`](logs/temp004-build-summary-2026-10-05.log)

## 7. Results

| Case | Stale frames before fix | Stale frames after fix | Result |
|---|---|---|---|
| Initial empty history | Invalid presentation: unallocated texture drawn on first activation (1 frame, 2–7 warnings in RT-002 runs 1, 2, 4) | 0 presentable playheads (all six unallocated after first load); 0 warnings in a 70 s real render | **PASS** |
| Reactivation | 4–16 per reactivation, 19/19 reused previous-activation imagery (RT-002 run 4) | **0** over 5 focused reactivations (+1 single-case check); Temporal lifecycle harness 0 warnings | **PASS** |
| Canonical media change | Previous-media imagery presented (e.g. 3 frames after the intentional change in RT-002 run 4 cycle 1; old media in 3/19 reactivations) | **0** over 3 focused canonical changes (+ the `next()`/`previous()` single-case checks) | **PASS** |
| Explicit clear/invalidation (`invalidateForReactivation()` + resync; failed `loadExplicit()`) | Same mechanism as reactivation (stale texture retained) | 0 presentable playheads immediately after every clear and after the failed load | **PASS** |
| Refilled history | n/a | n/a | **PASS** — all six re-allocate from current media, offsets unchanged, content = new media |

"Before" figures come from the RT-002 acceptance harness, which measured "playhead texture presentable while history empty" on the unfixed code. "After" figures come from this session's focused self-test plus warning logs.

### Focused reactivation evidence (from the self-test log)

| Event | Type | Canonical media | Stale frames | First valid history frame | History | Playheads / allocated after refill | Reload count |
|---|---|---|---|---|---|---|---|
| 2 | reactivation | `media.auto.97b545fd` | 0 | 7 | 1/72 | 6/6 | 5 |
| 3 | reactivation | `media.auto.97b545fd` | 0 | 7 | 1/72 | 6/6 | 6 |
| 5 | reactivation | `media.auto.9a8649a1` | 0 | 6 | 1/72 | 6/6 | 8 |
| 6 | reactivation | `media.auto.9a8649a1` | 0 | 7 | 1/72 | 6/6 | 9 |
| 8 | reactivation | `media.auto.cba6721d` | 0 | 7 | 1/72 | 6/6 | 11 |

Each event also checked:
- history size 0 and **0 eligible playheads immediately after the clear**;
- exactly one reload, then no further reloads over 10 hold frames;
- offsets unchanged;
- decoder path == canonical path.

### Focused canonical-media-change evidence

| Event | Media A (pre-clear file) | Media B (canonical after change) | Media B ID | Stale frames | First valid frame | Refilled content ≠ media A | Reload count |
|---|---|---|---|---|---|---|---|
| 1 | `vecteezy_ladybug-among-plants-in-green-nature_23796343.mp4` | `vecteezy_beautiful-rain-drops-falling-on-to-tree-slow-motion-video_21921750.mp4` | `media.auto.97b545fd` | 0 | 4 | yes | 4 |
| 4 | `…beautiful-rain-drops…21921750.mp4` | `vecteezy_turquoise-waves-rolled-on-the-rocks-beach-of-koh-miang_11120414.mp4` | `media.auto.9a8649a1` | 0 | 5 | yes | 7 |
| 7 | `…turquoise-waves…11120414.mp4` | `vecteezy_arashiyama-bamboo-grove-which-is-a-natural-forest-of-bamboo_12932507.mp4` | `media.auto.cba6721d` | 0 | 6 | yes | 10 |

All paths are under `assets/shared/media/`. Every change went through `VideoPlaybackService::next()`/`previous()`; Temporal only followed via `synchronizeSelectedMedia()`. No Temporal-local selector was used.

### Six-playhead regression result
**PASS.**
- `getNumPlayheads() == 6` before and after every clear.
- All six re-allocate on refill every time (6/6), and none stay invalid.
- Distinct per-playhead offsets (0, 0.2, …, 1.0) were set before the repetition phase and stayed byte-identical through all 8 clear/refill events.
- No change to playhead semantics or count.

### Warning result
**0 `ofGLRenderer: draw(): texture is not allocated`** in the 70 s standalone render and in the Temporal lifecycle harness (295/295). That was the same presentation path, now removed: unguarded `TFFragmentTransition::begin()` on unallocated or stale textures. Warning suppression was not the criterion; presentation validity was, and it is measured above.

## 8. Deviations

1. **`TFFragmentTransition.cpp` changed** (Temporal rendering). It is the one consumer that owned presentation eligibility for the outgoing snapshot and lacked the existing `isAllocated()` guard. The change is a single guard.
2. **Focused proof lives in the existing Temporal self-test** (`TFVideoAdapterSelfTest`, standalone `temporal-fields`), which already drives real decode and the canonical service. A new test target was not needed. The test runs no Temporal rendering, so warning evidence comes from the same standalone run's real scene rendering and from the Temporal lifecycle harness.
3. **"Before" numbers come from the RT-002 harness**, not a pre-fix run of the new focused test. That would have required reverting the fix in the working tree, and stash/reset are prohibited.
4. **Stopped background run.** A background chain queued before the full prompt arrived would have run RT-002. It was terminated before RT-002 started.

## 9. Newly discovered risks

1. **Shutdown resource release (separate follow-up, not fixed).** `TemporalSceneCore::shutdown()` still releases nothing; the decoder and history live until destruction (RT-002 Risk 2). It is not part of this invalidation mechanism. A safe fix would touch `TemporalSceneCore`/adapter lifecycle, which is outside TEMP-004's scope. Recommend a small TEMP-005.
2. **Background-only window remains by design.** For 4–7 frames after each clear, Temporal fragments are omitted (background/ambient layers only) until history refills. That is the intended "no valid frame" behavior. The RT-002 rerun should accept it as valid FadingIn content, not stale content.
3. **`fragment-trail` keeps its own `TimeOffsetVideoBuffer` fork,** which likely has the same stale-texture pattern. It is not in this pair's runtime scope; note it for FRAG-001.

## 10. Contract changes requested

```text
CONTRACT IMPACT: NONE
```

No public `TimeOffsetVideoBuffer` API was added (only a private helper and a doc comment). DEC-014 is unchanged: canonical selection still lives in `VideoPlaybackService`, and Temporal still follows. No stop condition was reached.

## 11. Recommended next step

Return to ExperienceRuntime for the **full RT-002 rerun, unchanged** (`EXPERIENCE_RUNTIME_TWO_SCENE_ACCEPTANCE=1`). Its existing `§7/§12 no stale playhead texture presented while Temporal history is empty` check is the end-to-end gate and should report 0 failures.

Separately:
- authorize TEMP-005 (Temporal `shutdown()` resource release) if wanted;
- commit TEMP-004 together with, or separately from, the uncommitted RT-002 harness and report;
- push `59c24f4`.

### Work Registry update

```markdown
## Work Registry Update
Work Item: TEMP-004 — Temporal stale playhead invalidation
Date: 2026-10-05
Owner: Temporal Fields Scene Migration
Previous Status: AUTHORIZED
Proposed Status: VERIFIED
Evidence:
- docs/reviews/temp-004-temporal-stale-playhead-fix-report.md
- docs/reviews/logs/temp004-*.log
Gate Changes: Implementation PASS; Desktop Verification PASS (focused); Integration Verification PENDING (RT-002 rerun); Architecture Acceptance PENDING; Pi NOT_STARTED; Documentation PASS; Source Control WORKTREE_ONLY
Source State: WORKTREE_ONLY
Dependencies Changed: RT-002 blocker cleared pending rerun
Next Action: ExperienceRuntime reruns RT-002 unchanged
Architecture Review Required: NO
```

---

### Session-close report

```text
WORK ITEM: TEMP-004
PREVIOUS STATUS: AUTHORIZED
NEW STATUS: VERIFIED (focused; proposed)

WHAT CHANGED: TimeOffsetVideoBuffer releases playhead textures on every history reset (private helper);
  TFFragmentTransition::begin() skips unallocated outgoing textures; focused self-test extended.
WHAT WAS PROVEN: 0 stale playhead frames over 5 reactivations + 3 canonical media changes; six playheads,
  offsets, current-media refill, single reload preserved; 0 unallocated-texture warnings in real rendering;
  all regressions green.
WHAT WAS NOT PROVEN: full 20-cycle two-scene behavior (RT-002 rerun, not run by instruction); any Pi behavior.

NEW EVIDENCE: docs/reviews/logs/temp004-*.log
NEW RISKS: §9 items 1-3
NEW BLOCKERS: none

NEXT ACTION: ExperienceRuntime full RT-002 rerun
NEXT GATE: RT-002 PASS -> Architecture first-two-scene acceptance

REGISTRY UPDATE REQUIRED: YES
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: NO (handoff to ExperienceRuntime)
```

**TEMP-004 VERIFIED — RETURN TO EXPERIENCERUNTIME FOR FULL RT-002 RERUN**
