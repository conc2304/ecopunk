# OC-005 — Configurable Orbit Observations, Stage 1

**Owner:** HUD Runtime & Validation Studio · **Scope:** UI prototype only. No production data expansion, no persistence.
**Status of this report:** Stage 1 implemented and validated on the desktop. Stopped as instructed. Not ACCEPTED/CLOSED.

## 1. Provenance and concurrent modifications

- Repository HEAD `03b3bb2` ("Typography presets, Experience Settings page, and MENU nav button"). The working tree was already heavily modified when this task started (≈142 modified paths: HUD compositor, widgets, hud-compositor-test, ExperienceRuntime). Several files this task touches (`SparklineWidget.cpp`, `HudWireframeRenderer.cpp/.h`, `ExperienceRuntime.cpp/.h`, `hud-compositor-test/Makefile.tests`) already carried uncommitted changes from other work. All edits here were additive, in place, and nothing was reverted, staged, stashed or committed. `OrbitDraw.h` was reported as "changed on disk" mid-task; that was this task's own edit.
- `git status` count at the end: 153 paths (the +11 are this task's new/edited files plus unrelated concurrent work and earlier OC-003/OC-004/Workstream A files; they are not separable in an index-free tree).

## 2. Inspection findings (Phase 1)

| Area | Finding |
|---|---|
| `OrbitConsoleModel` | Two fixed sources (`src_[0]` FX, `src_[1]` RAW), AVG only (r,g,b,luma per bin). Geometry is keyed on series revision/generation/layout, **not** on any configuration — so a config change needs no geometry rebuild. |
| `SparklineWidget::drawOrbit` | Variant `orbit.upper` = FX, `orbit.lower` = RAW, hard-wired. Upper hosts the one shared radar (FX+RAW), lower hosts its legend. Draws only pre-built `OrbitDrawData`. |
| Focus/navigation | `HudFocusNavigator` (bridge-owned) over 7 control regions; device-neutral `HudFocusInput` (Step ±n / Activate). Availability comes from `renderer.controlEnabled()` (nullopt = hidden). Arrows → Step, Enter → Activate (`InputRouter`). A physical jog wheel is not wired but is defined to produce the same `HudFocusInput`; Settings uses a separate `DpadInput` incl. `Back` (Esc). |
| Escape | Esc is bound to `RuntimeCommand::Exit` **and** OF's own escape-quit. The Settings page already suppresses OF's quit while open (`ofSetEscapeQuitsApp(false)`, restored one frame after close). The editor reuses that mechanism. |
| State ownership | HUD presentation state lives in the renderer (`HudWireframeRenderer`), HUD navigation in the bridge. No HUD configuration store exists; Installation settings are persisted by the runtime (not touched). |
| Data availability | FX: `HudFrameData::sceneColor`; RAW: `sceneColorRaw` (opt-in, null ⇒ Unavailable). **No DOM/VIB data exists anywhere.** |
| Radar semantics | Today: FX and RAW average RGB polygons on one 0–1 R/G/B scale, FX green / RAW blue, no frame synchronisation claim. |

## 3. Configuration model and ownership

New OF-free header `shared/src/hud-compositor/OrbitObservationConfig.h`:

- `OrbitSource {Fx, Raw}`, `OrbitMetric {Avg, Dom, Vib}`, `OrbitObs {A, B}`.
- `OrbitObservationConfig {source, metric}`, `OrbitConsoleConfig {a, b}`. **Defaults A = FX/AVG, B = RAW/AVG** (identical to the previous fixed console).
- `orbitMetricAvailable(metric)` — only AVG is a published measurement.
- `OrbitConfigEditor` — modal state machine (open / step / activate / cancel, preview, snapshot, revision) and `orbitEditorLayout()` geometry.

**Ownership:** HUD-owned presentation state, held in `OrbitDrawData::ui` inside `HudWireframeRenderer` (`renderer.orbitUi()`), separate from `OrbitConsoleModel` and from every telemetry snapshot (`SceneColorSnapshot` untouched). The bridge only forwards input. **Session-local only — nothing is persisted** (as instructed).

## 4. Presentation

- Compact labels in the swatch: `A FX/AVG`, `B RAW/VIB` (the HUD bitmap font is ASCII-only 8 px/char, so the `·` and spaced slashes of the brief were not usable; the label is the 84 px swatch's width).
- Instrument structure, 409.6 × 72 px regions and Dark Moss skin unchanged. A keeps the green accent, B the blue accent (so the defaults look exactly as before); the label, not the colour, carries the source.
- Radar legend now reads `A <src/metric>` / `B <src/metric>` instead of fixed FX/RAW.
- Focus: 1.6 px accent frame + `ENTER=EDIT` tag around the whole observation region. Editing: warning-coloured frame + `EDITING` tag; the observation stays fully visible.

## 5. Unavailable-data behaviour (never substitutes)

| Situation | Shown |
|---|---|
| FX unavailable | `NO DATA`, bars `--`, `Y --` |
| RAW unavailable | `NO RAW VIDEO`, bars `--`; **never filled from FX** |
| DOM or VIB (either source) | `DOM NOT AVAILABLE` / `VIB NOT AVAILABLE`, bars `--`, no ribbon, no polygon; **never AVG** |
| Valid black | Measurement `0.00 / 0.00 / 0.00`, black swatch (a valid sample) |
| All-invalid / missing | `NO DATA` — distinct from black |
| Stale | existing dimmed + `STALE`/`RAW STALE` treatment |

Unit-tested (`hud_orbit_config_tests`): valid black reads 0,0,0 and is a measurement; an all-invalid series is missing; FX never becomes RAW; DOM/VIB unavailable for both sources.

## 6. Focus and editor behaviour

- **Focus:** two new stops, `orbit.a` (region #7, `universal.activity_trace`) and `orbit.b` (region #8, `universal.secondary_trace`), appended after `controls.menu` in the existing `HudFocusNavigator`. Availability = HUD visible and orbit layout built. **Behaviour change:** the focus ring is now nine stops; wrap from MENU goes to Orbit A, not back to PREV SCENE (see §9, harness expectations updated).
- **Open:** Enter (Activate) on a focused observation opens its editor. It is HUD presentation state only; it dispatches no scene/media/runtime command.
- **Layout:** the *other* observation's whole region becomes the editor — it cannot overlap the clock, nav controls or any other region (unit-checked: all cells inside 409.6 × 72, no overlaps, clear of headers). Left: `FX | RAW`; right: `AVG | DOM | VIB` (DOM/VIB captioned `NO DATA`); `DONE` top-right.
- **Input (one path, no new global input system):** Left/Up/Shift+Tab = step −1; Right/Down/Tab = step +1 (cursor wraps over FX RAW AVG DOM VIB DONE; jog = the same `HudFocusInput` Step, any detent count); Enter = select an option (**previews immediately**) or, on DONE, commit and close; Esc = Back → cancel and restore. Tab/Shift+Tab were added to `InputRouter::translateToFocusInput` (the existing router) — a global mapping that also steps HUD focus.
- **Focus restoration:** HUD focus never leaves the edited observation, so closing returns to it (verified). A 0.6 s guard stops the closing Enter's key-repeat from re-opening the editor; activate is debounced 250 ms like Settings.
- **Cancel/hide rules:** Esc cancels (OF's escape-quit suppressed while open); entering Immersive, opening Settings, or hiding the HUD ('h') cancels and restores. A scene change while editing leaves the editor open and the configuration unchanged.
- **Not provided:** pointer hit-testing. The HUD has no pointer-driven control (`pointerPressed` only exits Immersive); "click" is the encoder/Enter activate.

## 7. Radar disposition

Narrowest safe adaptation implemented: **the radar compares A and B**, each polygon using *its own selected source's* RGB, labelled A/B in the legend (A green, B blue). A polygon is hidden when that observation's metric is unavailable (DOM/VIB) or its source has no data. Because the radar axes are R/G/B channel values, only AVG (the one published RGB measurement) can draw. **Reported limitations, not silently changed:**

1. The two polygons are independent measurements — not frame-synchronised and not necessarily the same algorithm (previously FX vs RAW; now A vs B). The legend does not claim otherwise.
2. If DOM/VIB later ship as colour-valued metrics, their axes semantics must be decided by Architecture before they plot on a shared radar (a dominant colour is not an average).
3. The radar lives in region #7. While **B** is edited, region #7 is the editor overlay, so the radar is hidden for that interval (A stays visible and the radar returns on close). This follows directly from "the other region becomes the overlay"; flagged for review.
4. With identical A/B selections the two polygons coincide (B drawn first, A on top).

## 8. Files changed

New:
- `shared/src/hud-compositor/OrbitObservationConfig.h`
- `shared/src/hud-compositor-test/hud_orbit_config_tests.cpp`
- `docs/reviews/oc-005-configurable-orbit-observations-stage1.md`, `docs/reviews/oc-005-evidence/` (12 full captures, 8 crops, `oc005-results.txt`)

Edited (additive):
- `shared/src/hud-compositor/OrbitDraw.h` (config/editor + focus flags in the draw data)
- `shared/src/hud-compositor/HudWireframeRenderer.h/.cpp` (focus ids, `orbitUi()`, `orbitObservationsPresent()`, RAW series override for verification)
- `shared/src/hud-compositor/widgets/SparklineWidget.h/.cpp` (per-observation drawing, editor overlay, focus frame)
- `shared/src/hud-compositor-test/Makefile.tests` (`test-orbit-config`)
- `sketches/experience_runtime/src/HudCompositorBridge.h/.cpp` (two focus stops; editor forwarding)
- `sketches/experience_runtime/src/InputRouter.h/.cpp` (Tab / Shift+Tab)
- `sketches/experience_runtime/src/ExperienceRuntime.h/.cpp` (editor input routing, Esc, guards)
- `sketches/experience_runtime/src/OrbitInjectHarness.h/.cpp` (dev-only `oc005` mode: real key path, deterministic series, captures)
- `sketches/experience_runtime/src/Hud006v2Harness.cpp`, `FocusMenuHarness.cpp` (focus-ring expectations 7 → 9 stops)

Not touched: HUD-006 geometry, HUD-004 manifest, `SceneColorSnapshot`, GPU sampling, Shared Video, scenes, scene-color contracts, persistence.

## 9. Tests and results

| Check | Result |
|---|---|
| `hud_orbit_config_tests` (new): defaults, labels, all 2×3×2×3 = 36 combinations selected through the editor, identical A/B, preview/commit/cancel, cancel-after-commit, non-re-entrant open, wrap both ways, editor geometry, valid-black vs missing, DOM/VIB, FX≠RAW | **214 / 214** |
| Existing HUD suites (`test-all`: orbit 57, presentation 1120, real-frame 302, viewport 357, typography 14, vitals 250) | **all pass** |
| Experience runtime unit suites (`make -C test test`) | **all pass** (no failure lines) |
| `make Release` of `experience_runtime` | **passes** |
| Real-runtime OC-005 harness (Dark Moss, 1280×720, real key path) | **24 / 24 pass**, 0 fail |
| `HUD006V2` harness | 23 / 0 (after expectation update) |
| `FOCUS` harness | 21 / 0 (after expectation update) |
| Settings page `menu` harness (fresh settings file) | 16 / 0 — MENU still the seventh stop; focus returns to MENU |
| Five-scene `MODE` harness | 52 / 0 |
| Full-ring five-scene circuit harness | 102 pass, **1 fail**: "no stale status/effects/capabilities/semantics on any frame" (224 violations). **Pre-existing**: the pre-OC-005 binary shows the same single failure (75 violations); the count is timing-dependent and unrelated to HUD orbit code. Not investigated further. |

Real-runtime harness coverage: default configuration; RAW unavailable (and recovery); focus reaches A as the 8th stop; Enter opens A; overlay region is B; focus stays on A; select previews immediately; Tab moves the cursor; Esc restores the prior configuration, closes the editor, keeps the runtime alive, returns focus to A; DONE commits (A = RAW/VIB, B = RAW/AVG); editing B; B previews FX/AVG while A keeps RAW/VIB; scene change while editing keeps editor + configuration; Esc restores B; Immersive and HUD-hide cancel the editor; Tab/Shift+Tab = ±1; Esc maps to the device-neutral Back.

**Not tested / limits:** the physical jog wheel (no hardware; it is the same `HudFocusInput`, unit-tested as multi-detent steps); Shift+Tab was tested at the router level (a synthetic key cannot carry the shift modifier); native OS fullscreen (`f`) with the editor open (the canvas is a fixed 1280×720 presented through the presentation transform, so editor geometry is resolution-independent, but it was not exercised); HUD "loading" visuals (observations are focus-hidden until their layout exists, and the HUD-hide path is tested); no Pi run. Screenshots use the dev injection harness (deterministic FX series + a channel-rotated RAW series) because production RAW is opt-in and DOM/VIB do not exist; every unavailable state shown is the real draw path.

## 10. Screenshots (real ExperienceRuntime, Dark Moss experimental, 1280×720)

`docs/reviews/oc-005-evidence/` (full frame `oc005_*.png`; crops `crop_*.png` of the two regions):

1. Default A = FX/AVG, B = RAW/AVG — `oc005_01_default.png`
2. RAW unavailable (`NO RAW VIDEO`, A only on the radar) — `oc005_02_raw_unavailable.png`
3. Focused A (frame + `ENTER=EDIT`), B unfocused — `oc005_03_focus_a.png`
4. Editing A (overlay in region #8, A visible with `EDITING`) — `oc005_04_edit_a.png`
5. Editing A, preview RAW/AVG (identical to B) — `oc005_05_*`
6. Editing A, DOM selected (`DOM NOT AVAILABLE`) — `oc005_06_*`
7. After Esc (restored) — `oc005_07_cancel_restored.png`
8. Mixed committed A = RAW/VIB (unavailable), B = RAW/AVG — `oc005_08_mixed_committed.png`
9. Editing B (overlay in region #7, radar hidden) — `oc005_09_edit_b.png`; preview FX — `oc005_10_*`; scene change while editing — `oc005_11_*`

## 11. Remaining risks

1. **Focus ring grew to nine stops** — a navigation behaviour change that required updating two existing harness expectations; the wrap-around now passes through the orbit regions. Architecture may prefer them as a separate focus layer.
2. **Tab / Shift+Tab are now global focus steps** (router mapping), not editor-only.
3. **Radar disappears while B is edited** (§7.3); radar semantics for future DOM/VIB are undecided.
4. **Edited state is session-local**; nothing persists across restarts (intended, pending lifecycle approval). The editor is cancelled by Immersive/Settings/HUD-hide.
5. Compact labels are `A FX/AVG` (ASCII, 8 px glyphs), not the brief's `A · FX / AVG`.
6. No pointer/touch hit-testing; no real jog-wheel test; no Pi (Pi 3B / Rock Pi) performance measurement of the added draw branches (they add no allocation and no GPU work; the draw path only branches on config).
7. The pre-existing full-ring "stale status" failure remains (not caused here).
8. Several touched files carry other people's uncommitted changes; a reviewer should diff by hunk, not by file.

## 12. Recommendation for production integration

Proceed to a production design review only for the **FX/RAW × AVG** surface, which is truthful today; keep DOM/VIB as explicit "not available" until Architecture approves published metrics and their radar semantics. Before integration Architecture should decide: (a) the configuration lifecycle (persistence scope, reset-on-restart, per-skin or global), (b) whether the observations should be a separate focus layer rather than ring stops, (c) the radar's meaning for non-AVG metrics and where it should live when region #7 hosts an editor, (d) pointer/touch interaction, (e) a Rock Pi/Pi 3B HUD-draw cost check with the editor open. No contract changes were made or are requested for Stage 1.

---

## Completion report

1. **Summary** — Two configurable observations (source FX/RAW × metric AVG/DOM/VIB, 36 combinations) implemented as HUD-owned presentation state with a keyboard/jog editor overlay, focus integration, A/B radar, explicit unavailable states; validated by unit tests and a real-runtime key-path harness.
2. **Files inspected** — `OrbitConsoleModel.h/.cpp`, `OrbitDraw.h`, `SparklineWidget.cpp/.h`, `HudWireframeRenderer.h/.cpp`, `HudWidgetInput.h`, `HudCompositorBridge.h/.cpp`, `HudFocusNavigator.h/.cpp`, `InputRouter.h/.cpp`, `ExperienceRuntime.cpp/.h`, `OrbitInjectHarness.*`, `Hud006v2Harness.cpp`, `FocusMenuHarness.cpp`, `SettingsPageHarness.cpp`, test Makefiles.
3. **Files changed** — §8.
4. **Tests/builds run** — §9.
5. **Results** — §9; one pre-existing harness failure unchanged.
6. **Deviations from prompt** — ASCII compact labels; focus ring extended (two harness expectations updated); Tab/Shift+Tab added to the shared router; radar hidden while B is edited; no pointer click; Immersive/Settings/HUD-hide cancel the editor (not specified).
7. **Newly discovered risks** — §11.
8. **Contract changes requested** — None (no scene-color/activity/HUD-006/HUD-004 change; `OrbitObservationConfig.h` is a new HUD-internal header).
9. **Recommended next step** — Architecture review of the Stage 1 prototype and the §12 decisions; no persistence until the lifecycle is approved.

```text
WORK ITEM: OC-005 Configurable Orbit Observations, Stage 1
PREVIOUS STATUS: Authorized (UI prototype)
NEW STATUS: Stage 1 implemented and desktop-validated. Proposed: IN REVIEW (not ACCEPTED/CLOSED)

WHAT CHANGED: HUD-owned observation config + editor overlay, two focus stops, A/B radar, explicit unavailable states, Tab mapping, dev harness mode, tests and evidence.
WHAT WAS PROVEN: all 36 selections work through the editor; preview/commit/cancel/focus-restoration on the real key path; DOM/VIB and missing RAW never substitute AVG or FX; valid black distinct from missing; existing HUD/runtime suites and focus/mode/settings harnesses pass.
WHAT WAS NOT PROVEN: physical jog wheel; Pi/Rock Pi HUD cost; native fullscreen with editor open; production RAW/DOM/VIB data (none exist); radar semantics beyond AVG.

NEW EVIDENCE: docs/reviews/oc-005-evidence/, hud_orbit_config_tests, oc005-results.txt
NEW RISKS: see 11.
NEW BLOCKERS: none for Stage 1; production integration blocked on Architecture decisions in 12.

NEXT ACTION: Architecture review.
NEXT GATE: configuration lifecycle approval before any persistence.

REGISTRY UPDATE REQUIRED: YES
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: YES (focus-ring extension; Tab mapping; radar A/B semantics)
ARCHITECTURE HANDOFF REQUIRED: YES
```
