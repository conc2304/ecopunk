# RT-006 — Fragment Trail Production Migration (Scene #4): Completion Report

**Date:** 2026-10-11 · **Base:** `44044c1` (`blueprint-emergence-updates`) · **Worktree:** `apps/myApps/EcopunkVideoCollage-rt006` (detached, isolated build/capture paths) · **Git:** nothing staged or committed.

**Status:** final. All builds, harnesses and unit suites have finished; every result below is read from its `RESULT:` record.

## 1. Summary

Fragment Trail now runs in ExperienceRuntime as the fourth production scene. The ring is now [Blob, Temporal, Blueprint, Fragment]. No shared contract changed. The artwork is the same code the standalone sketch runs: both now drive a new `fragmenttrail::FragmentSceneCore`. In production, Fragment follows the canonical `VideoPlaybackService` through the existing `TimeOffsetPlaybackAdapter` (Temporal's pattern), and its forked buffer is not compiled into the runtime. The desktop four-scene proof passes 82/82 over 10 alternating circuits (40 real switches).

One real visual defect was found and fixed in a scene-local way. Fragment's `glScissor` clipping was vertically mirrored inside the runtime's scene FBO.

The standalone `fragment-trail` sketch builds again; it had been silently broken since `557edc3` (2026-08-08).

**Pi:** not tested, so not passed.

## 2. Verified dependency classification

| Area | Finding at `44044c1` | Resolution |
|---|---|---|
| Lifecycle | `ofApp` owned setup/update/draw directly; no adapter | Moved into `FragmentSceneCore`, now used by both the standalone and the adapter |
| Video | Forked `TimeOffsetVideoBuffer`: scans `bin/data/media`, no `configure()`/`loadExplicit()`, no TEMP-004 reset. The standalone never advances media. | Production binds to the **shared** buffer (a strict API superset; identical `Settings`) through `TimeOffsetPlaybackAdapter`. The standalone keeps its fork. |
| `ShaderLibrary` | Fork with the same API; its nine effect shaders are **byte-identical** to the runtime's canonical `shaders/effects/*` | Production uses the shared `ShaderLibrary`. Fragment's per-fragment uniform code is unchanged and stays artwork-internal. |
| `LFOBank`, `TriggerBus` | Different APIs that share a global name with the shared classes (duplicate-symbol risk, e.g. `update(float)`) | All Fragment-local code is in `namespace fragmenttrail` |
| `CrosshairSystem` | Not in `shared/src`; differs from Quadrant's; one consumer | Kept local (namespaced); not graduated |
| Host-global sizing | `ofGetWidth()`/`ofGetHeight()` in `FTFragment`, `CrosshairSystem`, `TriggerBus`, `FTOverlayDirector` | `FTCanvas.h`: fixed at 1280×720 in production; follows the live window in the standalone, exactly as before |
| GUI | `FTParameterPanel` (ofxGui); `HudOverlayDialPanel` (ofxGui) | Panel compiled headless under `FT_PRODUCTION_RUNTIME` with the same `ofParameter` defaults; the dial panel is not compiled into the runtime |
| GL | Per-fragment `glScissor` using the window-framebuffer y-flip | Flip only when the window framebuffer is bound (see §5) |
| Assets | `bg_dim.glsl` was untracked, hand-supplied | Tracked at `sketches/fragment-trail/data/shaders/`; synced by a new script |
| HUD | Frozen profile `fragment-trail`, §18.4 state/metric IDs and vocabulary already exist | Mapped honestly; no HUD change |

## 3. Changes and why

**Fragment Trail (`sketches/fragment-trail/`)**
- `FragmentSceneCore.{h,cpp}` (new). The artwork: crosshair → trigger bus → mode controller → fragment pool, over the dimmed split-video backdrop, with the `hud_overlay` "room". It borrows the buffer and shader library rather than owning them. `activate()` rebuilds crosshair, trigger, LFO, mode, pool and overlay state, so every activation starts a fresh trail.
- `FTCanvas.h` (new) holds the canvas size. `FTVideoTypes.h` (new) selects the shared or forked buffer and shader library at compile time.
- Every Fragment-local `.h`/`.cpp` is wrapped in `namespace fragmenttrail`, and their includes go through `FTVideoTypes.h`. These edits are mechanical.
- `FTParameterPanel`: headless under `FT_PRODUCTION_RUNTIME`.
- `FTBackgroundLayer`: shader paths can be configured (defaults unchanged).
- `FTFragmentPool`: new `getSpawnCount()` accessor.
- `FTFragment.cpp`: the scissor fix.
- `ofApp.{h,cpp}`: now a thin host over the core. It keeps the fork buffer, fork shader library, GUI panels and keys.
- `config.make`: excludes `hud-compositor%`, `hud-runtime%`, `scene%` and `video-playback%` from the recursive `shared/src` sweep. This is a **pre-existing** build fix: `hud-compositor-test`'s `main()` test units broke the standalone.
- `data/shaders/bg_dim.glsl` (new, tracked; byte-identical to the previously hand-supplied file).

**ExperienceRuntime (`sketches/experience_runtime/`)**
- `FragmentProductionScene.{h,cpp}` (new), `kSceneId = "fragment-trail"`:
  - **Media:** a canonical follower (`syncVideoAdapter()` uses only the service's status and path); the adapter is invalidated on reactivation, and frame history is released on `deactivate()` (`configure()`; up to ~166 MB at the defaults).
  - **Effects:** registered without an effect source (`effects = nullopt`).
  - **Commands:** NextMedia/PreviousMedia only. There's no Regenerate, because Fragment has no curated reseed. Reset is honoured but not advertised.
- `FragmentSemanticMapping.{h,cpp}` (new), against the frozen §18.4 slots:
  - **State:** comes from the preset. `decaying` is never produced, because no state backs it.
  - **Activity:** density/overall = count ÷ max; motion = crosshair energy.
  - **Metrics:** fragment_count, movement_preset, trail_persistence (sustain + decay), measured spawn_cadence, content_mode.
- `FragmentTrail_*.cpp` and `HudOverlay_*.cpp` (new, 17 files). These are one-line wrapper translation units, not symlinks, so each included file resolves its own quoted includes from its real directory.
- `config.make`: `-DFT_PRODUCTION_RUNTIME`, plus the `hud`/`hud_overlay` include paths. `fragment-trail/src` goes **last**, so bare shared-header names still resolve to `shared/src`. It also hooks `sync-fragment-runtime-assets`.
- `ExperienceRuntime.{h,cpp}`: a `ProductionRoster {BlobTemporal, ThreeScene, Full}` enum replaces the `includeBlueprint` flag. Fragment is registered 4th under `Full`, with timer-driven Shared Video cadence like Blob/Temporal.
- `ofApp.cpp`: every existing harness keeps the ring it was accepted against. RT-002/RT-003 get two scenes. The other harnesses and the RT-004 stall driver get three, which preserves their exact roster assertions and key scripts. The normal run and the four-scene proof get `Full`. New option: `EXPERIENCE_RUNTIME_FRAGMENT_SCENE`.
- `ThreeSceneCircuitHarness`: generalised to the registered ring. `EXPERIENCE_RUNTIME_FOUR_SCENE_HARNESS` adds four Fragment checks:
  - the follower is on the canonical selection every Idle frame;
  - frame history is released while Fragment is inactive;
  - the trail is fresh on reactivation;
  - total scene updates equal live frames.
- `test/fragment_semantic_mapping_tests.cpp` plus its `Makefile.tests` target.

**Repo:** `scripts/sync-fragment-runtime-assets.py` (new; same model as the Blueprint script).

## 4. Builds and tests (results read from `RESULT:` records)

| Item | Result |
|---|---|
| ExperienceRuntime Release (worktree) | Clean. The only warning is a pre-existing one in `BEFragment.cpp`. |
| Standalone `fragment-trail` Release | Clean; renders through the core (`rt006-captures/standalone_fragment_trail.png`) |
| Four-scene harness, run 1 (before the scissor fix) | 82/82 (counters can't see a mirrored clip; found visually) |
| **Four-scene harness, run 2 (final)** | **82/82**. 10 circuits / 40 switches, both directions; 80 mid-transition switch requests and 40 scene commands rejected; per scene: 1 setup, activations = visits, 1 capability query per activation, status pulls = updates = draws; 4739 scene updates = 4739 live frames (no inactive updates); 5259 HudFrameData = 5259 HUD draws; 4259 Idle frames with 0 stale-state violations; 968 Fragment Idle frames with the follower on canonical media; 20 media changes (some while other scenes were active); 0 GL errors; scene FBO allocated once; RSS 300.8 → 170.7 MiB from circuit 2 to the end (peak 330.4) |
| RT-005 three-scene harness (three-scene roster, new binary) | 64/64 |
| RT-003 SceneSwitchHarness | PASS |
| RT-002 TwoSceneAcceptance | PASS |
| Blob lifecycle harness | 895/895 |
| Temporal lifecycle harness | PASS |
| Blueprint HUD harness | 26/26 |
| Parity soak | 37/37 |
| Blueprint roster harness | 16/18. Both failures are the "max frame < 1000 ms" check on the Blob (1204 ms) and Temporal (1410 ms) legs. This is the known measurement artifact: the probe attributed it during RT-005/BLUE-002 to the harness's own PNG captures, and it already failed 17/18 at `44044c1`. The harness runs on the three-scene roster, where Fragment isn't registered. This run was not re-attributed with the probe. |
| Unit suites (9) | All pass: 545 checks, including the new Fragment mapping test at 68/68 |
| `check-video-effect-drift.py` | No duplicate IDs; Fragment appears only as advisory NOTEs (unchanged per-fragment uniforms). Section 1 fails only because other sketches' gitignored `bin/data` folders are unsynced in the fresh worktree (an environment artifact). |

Logs: `docs/reviews/logs/rt006-*-2026-10-11.log` (regression `RESULT:` records: `rt006-regressions-2026-10-11.log`). Every harness ran from the worktree's own binary, one at a time, with a guard that flags any overlapping ExperienceRuntime instance. None overlapped.

## 5. Visual evidence and the fixed defect

- **Defect (fixed):** each fragment's video content was clipped into the vertical mirror image of its frame. `FTFragment::drawContent()` flipped the scissor y for a bottom-up window framebuffer. Under `ofFbo::begin()`'s default matrix flip, canvas row y is stored at FBO row y. The fix flips only when framebuffer 0 is bound, which leaves the standalone unchanged.
- Captures in `docs/reviews/rt006-captures/`:
  - `s_fragment_scene.png`, `s_fragment_window.png`: after the fix, content sits inside the chrome, with the HUD showing Fragment Trail / HUNTING / 11 / HUNT.
  - `t_c2_blob_to_fragment_fadein_mid.png`: reactivation is fresh (0 fragments, 0 s, new canonical media, no Blob carry-over).
  - `t_c1_blueprint_to_fragment_fadeout_mid.png`
  - `standalone_fragment_trail.png`
- The full set is in the worktree's `bin/data/captures/rt006-four-scene/`.

## 6. Known limitations and risks

- **Pi:** untested. A second history buffer (Temporal + Fragment) could reach ~166 MB each. Fragment frees its own on deactivate, but Temporal does not (TEMP-005). Fragment's per-fragment shaders and scissor draws are unmeasured on VideoCore IV.
- **Media cadence differs from the standalone:** production follows Shared Video's timer, while the standalone plays one clip forever.
- **Backgrounds:** production points at an absent `fragment-trail/backgrounds` folder to match the standalone's empty one, so the alternate still-image section never appears. If images are wanted, that needs an asset decision.
- **Late-run switch frames:** run 2's last three circuits had several 175–192 ms switch frames (run 1: ≤119 ms). That's under the stall limit but unattributed. No other ExperienceRuntime instance was running, but CPU load from builds in another session's checkout wasn't ruled out.
- **Vocabulary:** covers only drift/scan/hunt. Production stays on HUNT (auto-cycle off), but nervous/orbit would show raw IDs if enabled.
- **One instance per process:** `FTCanvas` is process-wide, which is fine for a single Fragment instance.
- **Standalone ordering:** the dial panel is now read one frame earlier, and the video updates before the crosshair. This is not visibly different.
- **Data reproducibility (pre-existing):** several ExperienceRuntime `bin/data` groups (`knowledge`, `of_nature_shader_pack_glsl`, shader groups) aren't produced by any sync script. The worktree used read-only symlinks to the main checkout.
- **Unrelated, observed during BLUE-002:** Temporal shows the previous visit's media for about a second on re-entry. It's still open and outside this item.

## 7. Deviations from the prompt

- **Standalone sketch edited and its build repaired.** The brief centred on the runtime adapter. Sharing one core required moving the standalone onto `FragmentSceneCore`, and its `config.make` needed the pre-existing exclusion fix before it could build at all.
- **Wrapper translation units instead of the repo's usual `src/` symlinks** for Fragment and `hud_overlay` sources. Symlinked `LFOBank.cpp`/`TriggerBus.cpp` would resolve the shared headers of the same name; relative includes in `hud_overlay/organisms` also rule symlinks out.
- **Roster API changed** from the `includeBlueprint` flag to a `ProductionRoster` enum, so existing harnesses keep their accepted rings while the normal run gets four scenes.
- **The RT-005 harness was generalised rather than a new harness added.** The three-scene mode gained one check ("no inactive scene updates"): 64 checks, previously 63.
- **No Regenerate capability** for Fragment, since no curated reseed exists.
- **Standalone visual parity** was checked on one captured frame, not a side-by-side sequence.

## 8. Contract impact

**NONE.** No change to `IEcopunkScene`, `SceneHudStatus`/`HudFrameData` schemas, HUD profiles/vocabulary/geometry, SceneManager/HUD command ownership, media authority, or canonical asset roots. The roster enum is runtime-internal and test-facing; all previously accepted rosters are preserved.

## 9. Recommended next step

1. Fix BlueprintRosterHarness so frames after a capture are excluded (as in ThreeSceneCircuitHarness); its stall check currently measures PNG encoding.
2. Bring the worktree changes into `blueprint-emergence-updates`. The main checkout has another session's uncommitted RT-004 edits to `ofApp.cpp`/`ofApp.h`, so those two files need a manual merge.
3. Do a targeted Pi run of the four-scene ring (memory, frame timing, thermals).

```text
WORK ITEM: RT-006 (Fragment Trail production scene #4)
PREVIOUS STATUS: not started (no adapter; RT-005 inventory)
NEW STATUS: WORKTREE_ONLY — desktop verified (four-scene 82/82); Pi NOT_TESTED
WHAT CHANGED: FragmentSceneCore shared by standalone + FragmentProductionScene; canonical follower; namespaced local classes; headless panel; scissor fix; roster enum; four-scene harness mode; bg_dim asset sync; standalone build repaired
WHAT WAS PROVEN: 4 scenes in one binary; 40 bidirectional switches; lifecycle/caps/HUD/no-inactive-update invariants; canonical media follower; history release; fresh reactivation; GL/FBO isolation; bounded RSS; three-scene RT-005, RT-003, RT-002, Blob/Temporal lifecycle, Blueprint HUD and parity intact (Blueprint roster 16/18: known capture-time artifact)
WHAT WAS NOT PROVEN: Pi; probe attribution of this run's Blueprint-roster capture-frame stalls; standalone visual parity beyond one frame
NEW EVIDENCE: docs/reviews/logs/rt006-*; docs/reviews/rt006-captures/
NEW RISKS: dual history buffers on Pi; unattributed ~190 ms late-run switch frames; nervous/orbit vocabulary gap
NEW BLOCKERS: none
NEXT ACTION: merge worktree into branch (manual ofApp merge); fix BlueprintRosterHarness capture-frame timing; Pi run
NEXT GATE: Fragment scene-domain acceptance
REGISTRY UPDATE REQUIRED: YES (new item RT-006 / Fragment migration evidence)
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: NO
```
