# BLUE-002 — Blueprint Emergence + Dark Moss Visual Integration: Completion Report

**Date:** 2026-10-11 · **Tree:** `44044c1` (`blueprint-emergence-updates`) · **Git:** no source changes; nothing staged or committed.

**Verdict:** **PASS** — Blueprint production HUD review, run with the experimental Dark Moss shell. No Blueprint defect found; no code changed.

**Dark Moss:** runnable, but **experimental**. `EXPERIENCE_RUNTIME_HUD_SKIN=dark-moss-experimental` loads and activates the shell-only package (`activated 'dark-moss-experimental'; peak GPU 3686400 B`). It is not a frozen production skin, so this review does not establish Dark Moss production readiness.

## 1. Summary

Blueprint Emergence was reviewed in a normal ExperienceRuntime session, not a harness. The session used the Dark Moss shell and real OS key presses, and lasted about 4.5 minutes, of which about 3.5 were in Blueprint. Captures cover:
- first activation;
- established fragments at 30, 60, 100 and 140 s;
- a RESEED;
- transitions into and out of Blueprint;
- a reactivation with 48 s of build-up.

Scene identity, HUD readouts, composition, the viewport crop, layering and transitions are all correct. There were no GL errors.

One defect was found **outside** Blueprint and reported rather than fixed: Temporal shows the previous visit's media for about a second when re-entered.

## 2. Method

- **Isolation.** A private copy of the built app ran from a scratchpad tree at the repo's folder depth. Skins were real copies, because the skin loader rejects symlinks; media and shaders were read-only links. This kept the run off the binary and captures folder that another session was using.
- **Normal run.** No harness. Real System Events key presses: `]` and `[` to switch scenes, `2` to reseed, `s` for scene-only captures. The log confirms every press was accepted.
- **Captures.** HUD composites are 1280×720 screen captures of the window, pinned on the main display; its geometry was checked as 1280×748 before any capture. Scene-only images come from the runtime's own `s` capture of the scene FBO.
- **GL errors.** Traced for the whole session with the RT-004 probe (`EXPERIENCE_RUNTIME_GL_ERROR_TRACE=1`).
- **Discarded attempt.** A first attempt opened the window on a secondary display, and its screen captures recorded other desktop content instead of the app. Those images were discarded and are not evidence.

## 3. Findings

| Check | Result | Evidence (`docs/reviews/blue002-captures/`) |
|---|---|---|
| Scene identity / profile / semantics | Correct: "Blueprint Emergence"; PLACING→BUILDING; activity 25→100%; activation-relative active time; cycle progress; RESEED shown only in Blueprint | `04_blueprint_first`, `05_blueprint_140s`, `10_blueprint_reactivated_45s` |
| Recognizable composition | Grid, orange divider, light zone A, crop-sampled media fragments, code labels, measurement lines, circle scanner, artwork-internal fragment effects | `05_blueprint_140s_scene`, `06_blueprint_reseed_25s`, `10_blueprint_reactivated_45s_scene` |
| RESEED | New cycle (progress back to 1%; mode changed); advances canonical media, as in the standalone. Active time keeps counting from activation by design. | `06_blueprint_reseed_1s`, `06_blueprint_reseed_25s` |
| `MediaViewportMesh` crop | Canonical cover-crop trims about 91 scene px from each side. Edge fragments are partly cut but never fully hidden. This is the shared contract, not a Blueprint bug. | compare `10_…45s` with `10_…45s_scene` |
| Stale Blob/Temporal overlays | None in Blueprint. Loading shows a neutral status; Blueprint enters fresh (1 s, one new fragment); Blob returns with only its own HUD content. | `03_t_temporal_to_blueprint_a/b`, `09_blueprint_reactivated`, `11_t_blueprint_to_blob_a`, `12_blob_after_blueprint` |
| Layering / transitions | Correct HUD ownership through FadingOut → Loading → FadingIn | `03_*`, `07_*`, `11_*` |
| HUD overlap / readability | No Blueprint-specific overlap. Shared shell text crowding noted (§5). | all composites |
| GL / FBO | 0 real GL errors over the session (the trace's only entry is its expected 0x500 self-test) | `logs/blue002-gl-trace-and-long-frames-2026-10-11.log` |

## 4. Defect outside BLUE-002 scope (not fixed)

**Temporal shows the previous visit's media when re-entered.** In `07_t_blueprint_to_temporal_a.png` the HUD title already reads the new canonical media (black centipede), but Temporal still draws the flower clip from its earlier visit (`02_temporal.png`). It corrects itself within about a second (`07_t_blueprint_to_temporal_b.png`). It appears when the canonical media changes while another scene is active, here through Blueprint's RESEED. The cause is in Temporal's history/playhead path (TEMP-004 territory) and belongs to the Temporal owner. At the time of the review another session had uncommitted edits to `TimeOffsetVideoBuffer` in the main checkout.

## 5. Remaining limitations

- **Shared HUD text crowding.** "READY"/"TRANSITION" and "PLACING"/"ACTIVITY" sit on or touch the Dark Moss shell's box edges. It's the same in every scene and in the wireframe, and HUD geometry is frozen, so it belongs to HUD/skin art, not Blueprint.
- **Empty cycle-progress ring.** When the phase isn't timer-bounded, the ring shows no value. That's the honest metric-absent behaviour, but a viewer may read it as broken.
- **Slow placement.** Blueprint holds at most 4 fragment slots for tens of seconds each, so early frames are sparse by design (matches the standalone).
- **Capture-caused long frames.** Two ~560 ms frames happened during screen captures and were spent outside runtime code (about 4.5 ms inside it), so the screenshot tooling caused them, not the runtime.
- **Dark Moss is experimental.** It's a shell-only package (no atlas or emissive assets) with no frozen Skin Package or Pi texture budget, so production skin readiness is not evaluated here.
- **Pi:** not tested, so not passed.

## 6. Files changed

**No source changes.** Added evidence only:
- `docs/reviews/blue-002-blueprint-dark-moss-visual-review-report.md` (this report)
- `docs/reviews/blue002-captures/` (15 captures; the full set is in the gitignored `sketches/experience_runtime/bin/data/captures/blue002-dark-moss/`)
- `docs/reviews/logs/blue002-normal-session-run-2026-10-11.log`, `docs/reviews/logs/blue002-gl-trace-and-long-frames-2026-10-11.log`

## 7. Tests

No code changed, so no suites were re-run for BLUE-002. RT-005's three-scene circuits (63/63) and the Blueprint HUD harness (26/26) on the same `44044c1` tree still apply.

## 8. Deviations from the prompt

- No isolated **git** worktree: BLUE-002 changed no source, so isolation was done with a private copy of the build output and data instead.
- Normal-run captures were driven by OS-level key presses and screen capture, not a harness. A first attempt was discarded (§2).

## 9. Contract impact

**NONE.**

## 10. Recommended next step

1. Hand the Temporal re-entry defect (§4) to the Temporal owner.
2. HUD/skin art: decide whether the shell's text crowding and the empty-ring presentation need changes in a future skin or Blueprint revision.
3. Dark Moss production readiness depends on the Skin Package freeze and measured Pi texture budgets (ART-001).

```text
WORK ITEM: BLUE-002 (Blueprint Emergence + Dark Moss visual integration)
PREVIOUS STATUS: not started
NEW STATUS: WORKTREE_ONLY evidence — desktop visual review PASS; Dark Moss experimental; Pi NOT_TESTED
WHAT CHANGED: no source; evidence added under docs/reviews/
WHAT WAS PROVEN: Blueprint renders recognizably through the production HUD path with the Dark Moss shell; correct identity/semantics; RESEED; canonical crop behaviour; no stale overlays; correct transitions; 0 GL errors
WHAT WAS NOT PROVEN: Dark Moss production readiness; Pi
NEW EVIDENCE: docs/reviews/blue002-captures/; docs/reviews/logs/blue002-*-2026-10-11.log
NEW RISKS: Temporal re-entry shows previous-visit media (~1 s); shared shell text crowding
NEW BLOCKERS: none
NEXT ACTION: hand Temporal re-entry defect to Temporal owner
NEXT GATE: Blueprint scene-domain visual acceptance
REGISTRY UPDATE REQUIRED: YES (BLUE-002 evidence; Temporal re-entry defect)
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: NO
```
