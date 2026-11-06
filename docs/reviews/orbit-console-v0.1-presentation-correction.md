# Orbit Console v0.1 - presentation correction (investigation + prototypes; NO production change this phase)

Prototypes: `docs/reviews/orbit-placement/proposal/` (cmp_* = current | B | A on the right column; full_*_B/A = full HUD).
Bases are real Dark Moss + production-typography captures from an isolated app instance. Overlays are
painted SVG (Calculator font = production "info" face, 15 px; vitals rows in a native-width mono stand-in).
Blueprint "62%" is ILLUSTRATIVE (phase_progress is absent in every Blueprint capture). Generator: gen_prototypes.py.

## 1. Displaced-content matrix (old #15 widget -> current #5 line)
| Scene | Field | Old (#15) | Current | Class |
|---|---|---|---|---|
| Blob | primary state (title) | card title | #4 badge | duplicated (kept in #4) |
| Blob | region_count | card value | "REGION FIELD n" | preserved, reformatted |
| Fragment | state (preset-derived) | card title | #4 badge | duplicated |
| Fragment | fragment_count | card value | "TRAIL STATUS n" | preserved (no /max) |
| Fragment | movement_preset | card meta (HUNT) | none | OMITTED (same preset as #4 state, different vocabulary) |
| Quadrant | channel_0..3_state | 4 boxed cells, full text | one line, "QUIET ..." truncated | reformatted, partly truncated |
| Quadrant | strategy / state | - | #4 badge + detail | unchanged |
| Blueprint | phase_progress | ring + % | "CYCLE PROGRESS n%" | reformatted; value ABSENT in every capture (unavailable) |
| Temporal | field_activity history | 32-pt sparkline | nothing | UNAVAILABLE (mapping: always absent; old widget showed ambient fallback) |
| Temporal | effects.active + health | chips in #6 | chips in #5 slot 0 | preserved (only scene with an effects snapshot) |
| Contour | breakup_progress | ring | "BREAKUP n%" | reformatted (not a production scene) |
| Loading | all of the above | ambient fallback | hidden | omitted by design (Hide) |
Real losses: Fragment preset text; Quadrant full state text in one row; Blueprint ring graphic. Everything else was
duplicated or never had data.

## 2. Options (all inside existing HUD-006 v2 geometry)
- A - shared #15 (409.6 x 115.2): 35 px scene section + five 15.8 px vitals rows. Fits, but vitals fall below the
  23 px rows and need non-equal slot slicing in the renderer; the five rows get cramped.
- B (PREFERRED) - #5 (409.6 x 43.2) shows ONE of {effect chips, scene status}. They never coexist: only Temporal
  has chips, and Temporal has no status. With hidden bindings no longer reserving a slot, the scene status gets the
  full 43.2 px = two 15 px lines: Fragment "TRAIL STATUS n / PRESET HUNT", Quadrant 2x2 "Qn STATE" (full text),
  Blueprint "CYCLE PROGRESS n%" + thin bar. #15 stays five 23 px vitals rows; #6 gauge, #7/#8 Orbit unchanged.
  Needs ONE production change: HudWireframeRenderer slot counts must skip bindings whose primary role will not
  render (today every valid binding in a region reserves an equal slice - the reason the status is squeezed to 21.6 px),
  plus a two-line variant for the three status widgets. No geometry/ID/manifest/contract change.
- C - fallback: keep today's one-line status, accept losing preset text and Quadrant truncation.

## 3. Verification status of the proposal (honest)
Verified: legibility and non-overlap of A/B painted on real 1280x720 Dark Moss frames, real region bounds.
NOT verified: Loading/transition frames, fullscreen (RT-007 scales the canonical 1280x720 canvas, so layout is
resolution-independent by construction but was not captured), focus ring (controls untouched), the proposed render
path itself (painted, not rendered by the HUD).

## 4. Five-scene regression (isolated instance, wireframe and Dark Moss+typography)
102 passed / 1 failed in both runs (also 102/1 in the shared tree). Only failure: "no stale status ... violations",
5 distinct log lines, all "Fragment count above its maxFragments cap". Dark Moss run activated the skin
(night palette, 3/3 button faces) - no other failures, no GL errors.

## 5. Fragment maxFragments - root-cause evidence (no code changed)
- Assertion: ThreeSceneCircuitHarness.cpp:241-243, `pool().getFragmentCount() > parameters().getMaxFragments()` (default 18,
  FTParameterPanel.cpp:21).
- Cap enforcement: uncommitted sketches/fragment-trail/src/FTFragmentPool.cpp now evicts only when
  `fragments.size() > maxFragments * kTrailCount` (kTrailCount = 2; two parallel trails), i.e. 36, while the assertion and
  FragmentSemanticMapping (`fragmentCount / maxFragments`, clamped) still treat 18 as the cap.
- Observed counts above 18: 32 (capture at 21:18, before any of my HUD edits), 20 (21:57 run); capture at 22:40 showed 18 / 17.
  The assertion value itself is not logged (harness prints only the reason): counts cross 18, up to 36.
- Classification: scene change vs test/semantic-contract assumption mismatch (per-trail budget x2 not reflected in the harness
  or mapping). Not a stale snapshot, timing issue or HUD effect: reproduced with the old HUD layout and in an isolated instance.
- Owner of the fix: the Fragment-trail change author. Not weakened or changed here.

## 6. Isolation and concurrency
Isolated instance: copied .app + data (without captures) to scratchpad/iso2/sketches/experience_runtime/bin with a symlinked
assets dir (media root is data/../../../../assets); captures there only. No pkill/killall used this phase; the only processes
started were this session's own runs (all exited normally). Shared tree: other sessions are editing HudWireframeRenderer.cpp,
SparklineWidget.cpp, OrbitConsoleModel.*, ExperienceRuntime.*, HudCompositorBridge.h, OrbitInjectHarness.* and the fragment-trail
sources; I touched none of them this phase.

## 7. Files changed this phase
Added only: this file, docs/reviews/orbit-placement/proposal/* (images + gen_prototypes.py). No source file changed, nothing staged.
