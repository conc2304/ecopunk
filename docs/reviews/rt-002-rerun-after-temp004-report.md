# RT-002 Rerun after TEMP-004 — Blob ↔ Temporal 20-Cycle Acceptance: Completion Report

**Date:** 2026-10-05
**Work item:** RT-002 (rerun) — first real two-scene milestone
**Recipient:** ExperienceRuntime & SceneManager domain manager → Architecture & Program Coordination (first-two-scene acceptance review)
**Produced by:** Coding agent (acceptance/verification only; no production code changed in this session)
**Builds on:**
- [RT-002 completion report](rt-002-blob-temporal-20-cycle-acceptance-report.md) (original run: FAIL on §7/§12, Temporal stale playheads)
- [TEMP-004 report](temp-004-temporal-stale-playhead-fix-report.md) (fix VERIFIED)

**Code under test:** `blueprint-emergence-updates` @ `59c24f4` + working tree:
- TEMP-004 fix: `shared/src/TimeOffsetVideoBuffer.{h,cpp}`, `sketches/temporal-fields/src/TFFragmentTransition.cpp`;
- RT-002 harness: `TwoSceneAcceptanceHarness.*`, `ofApp.*`.

**Harness:** `EXPERIENCE_RUNTIME_TWO_SCENE_ACCEPTANCE=1`, **unchanged** since the original run 4 (source last modified 21:22, before run 4). The binary was built 22:40, after both TEMP-004 changes (22:25).

---

## 1. Summary

**The clean rerun passed: 20/20 cycles, 115,726 checks, 0 failures, RESULT: PASS.**

The decisive regression holds in every Temporal activation:
- **0 stale / pre-clear playhead presentations** (20/20 activations);
- 0 reuse of previous-activation imagery;
- **0 `texture is not allocated` warnings**;
- current-media history refills every time (first valid history frame 4–8 frames after activation).

Every other RT-002 requirement still passes, and the authored Shared Effects preset was observed naturally for the first time.

**Run history in this rerun session** (all disclosed; the harness was never modified):

| Attempt | Result | Disposition |
|---|---|---|
| Rerun 1 | 165,111 checks, 52 failures | **Contaminated — not a valid record.** Two real `]` key presses reached the app window during the cycle-10 Temporal leg while it was being inspected. Each produced an unscripted, correctly handled scene switch (accepted while Idle, full transition, other input rejected mid-transition). All 52 failures trace to those two switches (50 "dwell stays Idle", 1 activation total 22/21 instead of 21/20, 1 "no inactive updates" on that leg). Stale-playhead check 0/20 even here. |
| Rerun 2 (clean A) | 188,536 checks, 1 failure | **§11 not naturally observed.** All 4 natural pattern transitions in the extended Temporal activation were 1-frame transitions (they complete within the update that starts them, so no in-transition frame exists). The harness hit its 11,000-frame cap. Every other requirement passed. |
| **Rerun 3 (clean B)** | **115,726 checks, 0 failures** | **PASS — the acceptance record.** Declared beforehand as the last retry. |

Transition length is chosen at random by Temporal's production style weights, and the harness observes rather than forces it. Natural transitions across all runs were either 1 frame or 9 frames (runs 2/3/4, rerun 1, clean A, clean B: 9 / 1,1,9 / 9 / 1,1,9 / 1,1,1,1 / 1,9).

## 2. Files inspected / changed

- **Inspected:** the run logs, harness source (unchanged), TEMP-004 diff, captures.
- **Changed in this session:** none in source. Evidence was added under `docs/reviews/logs/` and `docs/reviews/rt002-captures/rerun-after-temp004/`, plus this report.

## 3. Tests/builds run

| Run | Result | Log |
|---|---|---|
| RT-002 rerun 1 | contaminated (external input) | [`logs/rt002-rerun-after-temp004-contaminated-external-input-2026-10-05.log`](logs/rt002-rerun-after-temp004-contaminated-external-input-2026-10-05.log) |
| RT-002 rerun 2 (clean A) | 1 failure (§11 not naturally observed) | [`logs/rt002-rerun-after-temp004-clean-a-2026-10-05.log`](logs/rt002-rerun-after-temp004-clean-a-2026-10-05.log) |
| **RT-002 rerun 3 (clean B)** | **PASS, 0 failures** | [`logs/rt002-rerun-after-temp004-clean-b-PASS-2026-10-05.log`](logs/rt002-rerun-after-temp004-clean-b-PASS-2026-10-05.log) |

The TEMP-004 session already re-ran the supporting regressions on this code (Temporal lifecycle 295/295, Temporal self-test 139/139, all unit suites); see the TEMP-004 report. RT-003 switch, GL restoration and Blob lifecycle harnesses passed on the pre-TEMP-004 build (RT-002 report). TEMP-004 touched neither scene switching nor Blob nor the GL guard path.

## 4. Results (clean B — the acceptance record)

### Acceptance table

| Requirement | Result | Evidence |
|---|---|---|
| Two real scenes registered | **PASS** | `[blob-region-prototype, temporal-fields]`, both 1280×720; FakeScene not registered |
| Deterministic Blob startup | **PASS** | active=Blob, Idle; Blob setup=1/activate=1/capQ=1; Temporal not set up |
| NextScene | **PASS** | 20/20 via `]` → InputRouter |
| PreviousScene | **PASS** | 20/20 via `[` → InputRouter |
| 20 complete cycles | **PASS** | 20/20; 40 accepted switches, all scripted |
| ≥60 active frames/scene/activation | **PASS** | minimum 73 |
| Lifecycle ordering | **PASS** | every leg: FadingOut 12 → Loading 1 → FadingIn 12 → Idle, ordered as accepted |
| No inactive updates | **PASS** | outgoing counters frozen across every transition + dwell |
| Capability replacement | **PASS** | capQ == activations: Blob 21/21, Temporal 20/20; no merged sets; unsupported not advertised |
| One SceneHudStatus pull / live frame | **PASS** | 8,329 live frames = 8,329 pulls (Blob 1,530 + Temporal 6,799); 520 static frames = 0 pulls |
| One HudFrameData / presented frame | **PASS** | 8,849 = 8,849 (see §6, Limitation 1, on "presented") |
| One HUD draw / presented frame | **PASS** | 8,849 runtime = 8,849 bridge `drawCallCount()` |
| Blob effects missing | **PASS** | 0 Blob frames with effects |
| Temporal present-empty effects | **PASS** | 2,780 frames (first @74) |
| Temporal present-active effects | **PASS** | 4,019 frames (first @2321): `dither`, `ink_outlines`, `scanlines` |
| No stale Temporal effects on Blob | **PASS** | missing on every first Blob frame and every Blob frame |
| Semantic/profile clearing | **PASS** | per-frame sceneId == activeSceneId == HUD profile; scene-prefixed semantics only |
| Canonical video continuity | **PASS** | 11 canonical changes; adapter mismatch ≤ 1 frame |
| Mandatory live media change | **PASS** | frame 116/117: `media.auto.9fc43161 → media.auto.080b175e`; adapter + history clear @117; refilled @134 |
| **Temporal history resync** | **PASS** (was FAIL) | **stale playhead frames 0 / reuse 0 in all 20 activations**; Blob-dwell change `→ media.auto.61933905` followed at next activation (frame 418) |
| Static-frame transitions | **PASS** | retained frame byte-identical through FadingOut/Loading; replaced at FadingIn |
| GL/FBO isolation | **PASS** | 0 GL-failure frames / 8,849; scene FBO 1 allocation, 0 reallocations |
| Mid-TFFragmentTransition capture | **PASS** | frame 7032 (§4 below) |
| Temporal warning instrumented | **PASS** | 0 warnings in all 20 activations |
| Bounded resources | **PASS** | RSS 389–437 MB (437 = 90 s Temporal leg), net allocations 3,782–4,254, no cycle-proportional growth; 6 playheads; 1 decoder; 1 FBO |
| Clean shutdown | **PASS** (runtime) | Blob 21/21, Temporal 20/20 deactivations; one shutdown each; post-shutdown commands/switches rejected. Temporal resources still released only at destruction (TEMP-005, non-blocking) |
| Authored preset natural observation | **OBSERVED** | frame 2321: `TFEffectPicker: applying canonical eligible preset 'preset.dither.20260809_172713_1' for effect 'dither'`; first present-active `HudFrameData.effects` frame = 2321 |
| Frozen-contract drift | **NONE** | TEMP-004 is private to `TimeOffsetVideoBuffer` plus a Temporal-local guard; no contract/runtime/SceneManager change |
| Durable source/artifact state | **WORKTREE_ONLY / COMMITTED (unpushed)** | §5 |

### Temporal stale-playhead / warning table (clean B)

| Activations | Warning frames | First valid history frame (live idx) | Stale playhead frames | Reused previous-activation imagery | Canonical media changed since previous |
|---|---|---|---|---|---|
| 1 | 0 | 5 | 0 | 0 | — |
| 2–3 | 0 | 8 | 0 | 0 | 1 of these (cycle 3) |
| 4–20 | 0 | 4 | 0 | 0 | — |

Background-only frames while history refills (4–8 frames per activation) are the intended "no valid frame yet" behavior, not stale content.

### Mid-TFFragmentTransition (clean B)

| Field | Value |
|---|---|
| Frame | 7032 (extended cycle-10 Temporal activation, 2nd natural pattern change) |
| Active scene / state | `temporal-fields` / `scene.temporal.state.transitioning`, pattern type 2 |
| Progress | in-transition frame 8 of 9 (elapsed 0.267 s) |
| Scene FBO / viewport / framebuffer | 1280×720 / 1280×720 / 0 |
| HUD | profile `temporal-fields`, 19 bindings, geometry `54v [25.6,115.2]-[793.6,619.2]` = baseline; HUD draws 7032 = frame |
| GL | scissor 0, stencil 0, program 0, tex2D 0, fbo 0, blend 1/770/771, viewport/matrix/style = baseline |
| Next frame | valid |
| No stale Blob | scene FBO ≠ last Blob static frame |

Screenshots: [`rt002-captures/rerun-after-temp004/rt002_mid_tffragmenttransition.png`](rt002-captures/rerun-after-temp004/rt002_mid_tffragmenttransition.png) (erosion-style fragment transition inside the canonical viewport; HUD intact; Temporal TRANSITIONING; manager IDLE) and its `_scene_only` variant. Also first-incoming frames both directions and the final Blob frame in the same folder.

### Per-cycle resources (clean B; sampled at the end of each Blob return)

| Cycles | RSS MB | Net live allocs | FBO allocs / reallocs | Blob fragment scratch | Temporal history / cap | Playheads | Adapter reloads |
|---|---|---|---|---|---|---|---|
| 1 | 389 | 3,782 | 1 / 0 | 316×244 | 22/72 | 6 | 2 |
| 2–9 | 410–418 | 3,814–3,910 | 1 / 0 | 980×364 (on-demand high water) | 65–69/72 | 6 | 3–10 |
| 10 (90 s+ Temporal leg) | 437 | 4,254 | 1 / 0 | 980×364 | 72/72 | 6 | 17 |
| 11–20 | 386–421 | 4,000–4,070 | 1 / 0 | 980×364 | 18–69/72 | 6 | 18–29 |

Allocations plateau after the long leg, with no cycle-proportional growth. The reload count rises by one per Temporal activation or canonical change, so there is no decoder accumulation.

## 5. Durability

| Artifact group | SOURCE STATE |
|---|---|
| RT-003 source, tests, report, logs; Work Registry/governance docs | **COMMITTED** (`59c24f4`), **not pushed** |
| TEMP-004 fix + focused self-test + report + logs | **WORKTREE_ONLY** |
| RT-002 harness (`TwoSceneAcceptanceHarness.*`, `ofApp.*`) | **WORKTREE_ONLY** |
| RT-002 reports (original + this rerun), logs, `docs/reviews/rt002-captures/` | **WORKTREE_ONLY** |
| Full capture set `bin/data/captures/` | **UNTRACKED** (git-ignored) |
| Architecture RT-003 ruling, RT-003 domain review, TEMP-004 authorization | **NOT FOUND** in repo / `~/Downloads` |
| Governing specs (20-cycle spec, two-scene requirements) | **UNTRACKED** (`~/Downloads` only) |

Technical acceptance does not depend on these, but durable milestone closure does (commit + push + ARCH-003 import).

## 6. Deviations / limitations

1. **Harness frames are verified at render time, not on screen.** Every ExperienceRuntime harness (lifecycle, switch, this acceptance harness) drives `runtime.update()`+`runtime.draw()` from inside `ofApp::update()`. openFrameworks then auto-clears the back buffer to the background color before the draw phase, so **the window shows a flat dark color (#0C0C0C) during harness runs** (reported by the user mid-run).
   - Every assertion (screenshots via `grabScreen`/FBO readback, GL baseline, FBO hashes, HUD counts) reads the rendered frame before that clear, so the evidence is valid for what the runtime renders.
   - "Presented frame" in this report means "runtime frame rendered".
   - Normal (non-harness) runs draw inside `ofApp::draw()` and display correctly.
   - Optional tooling-only follow-up: `ofSetBackgroundAuto(false)` when a harness is active, so harness frames are also visible on screen.
2. **Three attempts.** Disclosed in §1. The harness was unchanged throughout, and the final attempt was declared as the last retry before it ran.
3. **§11 depends on a random production event.** A future rerun can fail §11 the same way. Architecture may want §11 evidence either to accept the harness's natural observation as is, or to come from a separately authorized deterministic setup (e.g. a dev-only transition-style override). No such override was used here.

## 7. Newly discovered risks

1. **Unscripted input reaches harness runs.** The harness does not lock out real input, and the window looks black, which invites interaction. Future acceptance runs should be left untouched, or the harness should ignore real input (tooling change, not authorized here).
2. **Carried forward, non-blocking:**
   - Temporal `shutdown()` releases nothing (TEMP-005 proposed);
   - `fragment-trail`'s buffer fork likely has the stale-playhead pattern (FRAG-001);
   - production Temporal changes pattern only after 90 s continuous activation;
   - HUD scene-switch controls hidden; raw media-title key; HUD media buttons ignore the transition phase.

## 8. Contract changes requested

```text
CONTRACT IMPACT: NONE
```

## 9. Recommended next step

Architecture first-two-scene acceptance review of RT-002 (clean B) with TEMP-004.

Then durability closure:
- commit TEMP-004 + the RT-002 harness, reports and logs;
- push `59c24f4` and the new commit;
- import the governing specs and rulings (ARCH-003).

### Work Registry updates

```markdown
## Work Registry Update
Work Item: RT-002 — Blob ↔ Temporal 20-cycle switching acceptance
Date: 2026-10-05
Previous Status: BLOCKED (TEMP-004)
Proposed Status: VERIFIED
Evidence: docs/reviews/rt-002-rerun-after-temp004-report.md; docs/reviews/logs/rt002-rerun-after-temp004-clean-b-PASS-2026-10-05.log
Gate Changes: Implementation PASS; Desktop Verification PASS; Integration Verification PASS; Architecture Acceptance PENDING; Pi NOT_STARTED; Documentation PASS; Source Control WORKTREE_ONLY
Source State: WORKTREE_ONLY (harness/reports); RT-003 COMMITTED (unpushed)
Next Action: Architecture first-two-scene acceptance review
Architecture Review Required: YES
```

```markdown
## Work Registry Update
Work Item: TEMP-004 — Temporal stale playhead invalidation
Previous Status: VERIFIED (focused)
Proposed Status: VERIFIED (focused + integration)
Gate Changes: Integration Verification PASS (RT-002 clean B: 0 stale / 0 warnings in 20 activations)
Source State: WORKTREE_ONLY
```

```markdown
## Work Registry Update
Work Item: FX-004 — Live authored-preset observation
Proposed Status: VERIFIED
Evidence: RT-002 clean B frame 2321 — preset.dither.20260809_172713_1 applied by TFEffectPicker; HudFrameData.effects present-active from the same frame
```

---

### Session-close report

```text
WORK ITEM: RT-002 (rerun); TEMP-004 (integration proof); FX-004 (observed)
PREVIOUS STATUS: RT-002 BLOCKED; TEMP-004 VERIFIED (focused); FX-004 READY
NEW STATUS (proposed): RT-002 VERIFIED; TEMP-004 VERIFIED; FX-004 VERIFIED

WHAT CHANGED: no source changes; evidence + this report.
WHAT WAS PROVEN: unchanged 20-cycle acceptance passes on the TEMP-004 code (115,726 checks, 0 failures);
  0 stale playhead presentations and 0 texture warnings across 20 Temporal activations; natural
  mid-TFFragmentTransition and authored-preset application observed.
WHAT WAS NOT PROVEN: on-screen presentation during harness runs (render-time only); Pi behavior.

NEW EVIDENCE: docs/reviews/logs/rt002-rerun-after-temp004-*.log; docs/reviews/rt002-captures/rerun-after-temp004/
NEW RISKS: §7
NEW BLOCKERS: none

NEXT ACTION: Architecture first-two-scene acceptance review; then commit/push/import
NEXT GATE: Architecture acceptance of the first two-scene milestone

REGISTRY UPDATE REQUIRED: YES
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: YES
```

**RT-002 PASS — READY FOR ARCHITECTURE FIRST-TWO-SCENE ACCEPTANCE**
