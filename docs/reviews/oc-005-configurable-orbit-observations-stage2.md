# OC-005 — Configurable Orbit Observations, Stage 2 (production integration)

**Owner:** HUD Runtime & Validation Studio · **Coordination:** ExperienceRuntime (installation-settings ownership)
**Scope delivered:** persistence, input routing verification, real-runtime binding verification. No UI redesign, no new sampling, no telemetry fields.

## 1. TL;DR and recommended disposition

The committed A/B Orbit selection now persists in the existing installation-settings file, restores on restart, falls back safely on bad data, and writes only on Done (only the edited observation, only when it changed). Preview and Escape never touch disk. The nine-stop ring, modal input ownership, Settings coexistence, and the real FX/RAW AVG bindings were verified on the desktop (real telemetry where available).

**Recommended disposition: Desktop accepted — ROCK Pi qualification pending.**

## 2. Repository provenance

- Branch `blueprint-emergence-updates`, HEAD `03b3bb2`. Working tree **156 modified/untracked paths at start, 164 at the end**. Index untouched; nothing staged, committed, stashed or reset.
- Concurrent modifications observed *during* this task by another session (not mine; not touched):
  - `sketches/experience_runtime/test/Makefile.tests` was edited mid-task (17:33) and its `test` target is now **broken** (`$(SCENE_COLOR_BIN) X_BIN)` stray token, "No rule to make target `X_BIN)'"). I did not repair it; I ran each of the 24 runtime unit binaries individually instead (§8).
  - `InstallationControl.h` carried ~44 uncommitted lines (System Mode / button labels, from earlier work) before Stage 2; `ExperienceRuntime.cpp` carries a ~440-line uncommitted diff from other work. Stage 2 edits are additive hunks inside those files.
  - HUD-compositor suites changed their own counts between runs (presentation 1120 → 1127, real-frame 302 → 323, vitals 250 → 258): other work is adding tests.
  - Machine load average was **81–107** during the verification runs (many concurrent processes).
- Environmental incident: two in-tree runs of the *real* phase aborted (`filesystem_error: getcwd failed` at a capture) while the in-tree `bin/` was being used; the same phase run twice from an isolated copy of the bundle completed cleanly (11/11 and 12/12). I attribute it to concurrent activity on `bin/` but did not prove it. All final harness runs used an isolated copy (`scratchpad/app7`, data symlinked).
- Stage 1 re-inspection: the Stage 1 report matches the working tree (`OrbitObservationConfig.h`, `OrbitDraw.h`, widget, bridge, router, runtime hooks all present as described). One addition vs. Stage 1: `orbitBoundSource()` (§3).

## 3. Files changed and why

| File | Change |
|---|---|
| `src/InstallationControl.h` | `Settings::orbitA/orbitB` (`OrbitSelection{source, metric}` tokens), validators, `parseOrbitSelection`, `Request::Kind::SetOrbitObservation`, per-field validation in `load()`, no-op when unchanged. **The existing controller; no new service.** |
| `src/ExperienceRuntime.cpp/.h` | Parse `orbitObservations` on load, write it on save, `applyOrbitSettingsToHud()` after load, `commitOrbitObservation()` on Done. |
| `hud-compositor/OrbitObservationConfig.h` | `orbitSourceToken/orbitMetricToken/orbitFromTokens`, `OrbitConfigEditor::setCommitted()` (ignored while a preview is open). |
| `hud-compositor/OrbitDraw.h`, `widgets/SparklineWidget.cpp` | `orbitSourceIdOf` / `orbitBoundSource` — one resolution rule shared by the draw path and the harness (refactor of Stage 1 inline logic; no behaviour change). |
| `src/OrbitInjectHarness.h/.cpp` | Dev-only `EXPERIENCE_RUNTIME_OC005_PHASE=write|verify|invalid|real` (real key path; scratch settings file). |
| `test/installation_control_tests.cpp`, `hud-compositor-test/hud_orbit_config_tests.cpp` | New tests (§8). |

Not touched: HUD-006 v2 geometry, HUD-004 manifests, `SceneColorSnapshot`, scene/media authority, Temporal/Fragment, `scene.activity.overall`, sampling, telemetry defaults, Settings page.

## 4. Persistence ownership and data format

- **Owner:** the existing `installation::Controller` (settings in `installation_`, file written by `ExperienceRuntime::saveInstallationSettings()` via the existing temp-file-then-rename path). Global to the installation; not per scene, not per skin.
- **Format** (new optional key; absent in old files):
```json
"orbitObservations": {
    "a": { "source": "fx",  "metric": "avg" },
    "b": { "source": "raw", "metric": "avg" }
}
```
  Tokens: source `fx|raw`, metric `avg|dom|vib` (exact, lower-case).
- **Defaults:** A = fx/avg, B = raw/avg.
- **Validation / fallback (per field):** missing key, wrong JSON type, unknown token, or an unparsable file ⇒ that field keeps its documented default; invalid *tokens* are logged (`invalid orbitObservations.a.source in settings; default kept`). Verified with four bad files (§9). A selection such as `raw/vib` is valid and persists (DOM/VIB are selectable; they render as unavailable).
- **Write policy:** one write per Done that *changes* the edited observation; re-committing the same value writes nothing; the other observation is carried through untouched; preview, Escape, HUD-hide, Immersive, Settings, scene changes: zero writes (verified by comparing file text).

## 5. Preview / commit / cancel

Unchanged from Stage 1 and now backed by persistence: preview is session-local; **Done** → `commitOrbitObservation(target)` → `SetOrbitObservation` → save; **Escape** → restore, no write; HUD hide, Immersive entry and Settings opening cancel (restore, no write). Restart restores the committed pair.

## 6. Input ownership and nine-stop focus

- Ring verified in the real runtime: `scene_previous > scene_next > mode > media_previous > media_next > fullscreen > menu > orbit.a (stop 8) > orbit.b (stop 9)`; stop 10 wraps to stop 1; reverse wrap from stop 1 lands on Orbit B; existing seven keep their order.
- While the editor is open: arrows, Tab, Enter and Esc are consumed by it; after a run of Right/Down/Tab/Up/Left/Enter inside the editor, **media id unchanged and no focus activation other than `HudOrbit::OpenEditor`** (no scene/media command leaked). Focus stays on the edited observation and is correct after closing.
- Settings: F1 with the editor open cancels the editor and the page takes input; Esc closes the page without exiting the runtime; the existing Settings harness (menu mode) is unchanged (16/16, MENU still the 7th stop, focus restored to MENU).
- Hotkeys that are not editor navigation (e.g. `]` next scene) still act — Stage 1 behaviour, kept: the editor survives a scene change with its configuration unchanged.
- Routed only through existing infrastructure (`HudFocusInput`, `DpadInput::Back`, `InputRouter`); no new input system.

## 7. Real FX/RAW AVG binding evidence (real telemetry, no fixtures)

`EXPERIENCE_RUNTIME_OC005_PHASE=real`, isolated bundle, scratch settings, Dark Moss, real Blob scene.
- **Default, RAW sampling off:** real FX series valid (`r,g,b = 0.648, 0.646, 0.632`); RAW `Unavailable`, B shows **NO RAW VIDEO**, never filled from FX. RAW sampling was *not* enabled by the HUD.
- **RAW sampling on (`EXPERIENCE_RUNTIME_VIDEO_COLOR=1`, opt-in env only):** FX `0.292, 0.301, 0.278` vs RAW `0.282, 0.327, 0.233` — two independent real measurements.
- **Same source and metric:** A=RAW/AVG with B=RAW/AVG bind to the *same* RAW source model (identical values); A=B=FX/AVG bind to the FX model.
- **Different sources:** A=FX/AVG, B=RAW/AVG bind to different source models (`orbitBoundSource`).
- **Scene change with committed selection:** selection kept; FX series **generation advanced** (new scene ⇒ new series) and FX valid again after fresh samples.
- **DOM/VIB:** `orbitBoundSource` is `nullptr` → explicit "NOT AVAILABLE"; never AVG, never the other source.
- Valid black vs missing, stale and generation protections are unchanged Stage 1/OC-003 behaviour; covered by unit tests and injected fixtures (real black/stale frames cannot be forced on demand). **Injected fixtures** (deterministic FX series, channel-rotated RAW series) are used only in the `write`, `verify`, `invalid` phases and the Stage 1 regression run, and only for visuals; their screenshots are labelled below.

## 8. Test counts and failures

| Suite | Result |
|---|---|
| `hud_orbit_config_tests` (Stage 1 + Stage 2: tokens, `setCommitted`, occlusion) | **229 / 229** |
| `hud_orbit_tests` (existing) | 57 / 57 |
| Existing HUD suites `test-all` (presentation 1127, real-frame 323, viewport 357, typography 14, vitals 258, + one further 144-check suite in the same target whose source I did not inspect) | all pass |
| `installation_control_tests` (+ orbit: defaults, load validation/fallback by field, commit independence, no-op write, rejects, unrelated settings untouched) | **58 / 58** |
| 24 runtime unit binaries (run individually because `make test` is broken by another session's Makefile edit) | all pass |
| Stage 1 real-runtime harness (regression) | **24 / 24** |
| Stage 2 `write` (no-persist-on-preview, Esc rollback, Done persists A, then B independently, scene change) | **10 / 10** |
| Stage 2 `verify` (restart restoration, nine-stop ring + wrap, isolation, Esc, Settings coexistence, bound sources) | **15 / 15** |
| Stage 2 `invalid` ×4 (bad token + wrong type; non-JSON file; key absent; wrong JSON types) | 2 / 2 each |
| Stage 2 `real`, RAW off / RAW on | 11 / 11 and 12 / 12 |
| HUD006V2 / FOCUS / Settings `menu` harnesses | 23/0, 21/0, 16/0 |
| Five-scene `MODE` harness | 52/0 on the final run; **flaky under load**: two earlier runs timed out waiting on a scene step ("fragment backdrop effect settled", "temporal live") with 39 and 6 passes at load average ~100; third run 52/0 |

Failures: none attributable to Stage 2. The full-ring stale-status failure was **not re-run or touched** (tracked separately; no new causal evidence either way).

## 9. Release build

`make Release` of `experience_runtime`: **passes** (final build after the last source edit; no errors).

## 10. Real desktop screenshots (Dark Moss experimental, 1280×720)

`docs/reviews/oc-005-evidence/stage2/` (full frames `oc005s2_*.png`, region crops `crop_*.png`, Stage 1 frames for editing B/cancel):

| Required | File | Source |
|---|---|---|
| Default startup | `oc005s2_real_01_real_default.png` (RAW off), `…_rawon.png` | **real telemetry** |
| Editing A | `oc005s2_verify_03_editing_a.png`, `oc005_04_edit_a.png` | fixtures |
| Editing B | `oc005_09_edit_b.png` | fixtures |
| Preview / cancellation | `oc005s2_write_01_preview.png`, `oc005_07_cancel_restored.png` | fixtures |
| Done, then restart restoration | `oc005s2_write_02_mixed_committed.png` → `oc005s2_verify_01_restored_after_restart.png` | fixtures |
| Mixed FX/RAW | `oc005s2_real_03_real_fx_raw_rawon.png` (real), `…write_02…` | real / fixtures |
| RAW unavailable | `oc005s2_real_01_real_default.png` (`NO RAW VIDEO`) | **real** |
| DOM/VIB unavailable | `oc005s2_verify_01_…` (A = RAW/VIB, "VIB NOT AVAILABLE") | fixtures |
| Scene change with committed config | `oc005s2_write_03_after_scene_change.png`, `oc005s2_real_05_real_after_scene_change(_rawon).png` | fixtures / **real** |
| Nine-stop focus | `oc005s2_verify_02_focus_b_stop9.png` (focus on Orbit B, stop 9) | fixtures |
| Invalid-file fallback | `oc005s2_invalid_01_invalid_file.png` | fixtures |

## 11. Remaining hardware risks (no hardware evidence exists for any of these)

Coordinate with the existing **A7 FX qualification schedule**; do not enable new telemetry:
1. **HUD draw CPU cost, editor closed** (new per-frame branches + config reads; no allocation or GPU work was added — unmeasured on Rock Pi / Pi 3B).
2. **HUD draw CPU cost, editor open** (overlay replaces one region's instrument; fewer draws expected, unmeasured).
3. **Focus/navigation responsiveness** on the real input device (jog wheel never exercised; Tab/Shift+Tab verified at router level only).
4. **Installation-settings read/write** on the target storage (a single temp-file+rename write per Done; unmeasured on SD/eMMC).
5. **Startup restoration** with a real file on the device, including a corrupt/partial file.
6. **Scene changes** with a non-default selection and RAW off/on.
7. **Missing RAW** behaviour on the device (RAW sampling remains opt-in).
8. **Frame pacing and GL errors** with the editor open/closed (desktop showed none; desktop load was very high and not a pacing test).
Also open and *not* Stage 2: the full-ring stale-status nondeterminism; the broken runtime `Makefile.tests` `test` target; the unexplained in-tree harness abort.

## 12. Exact production-readiness recommendation

**Desktop accepted — ROCK Pi qualification pending.** Stage 2 meets its functional requirements on the desktop with real FX/RAW telemetry where available. No hardware qualification is claimed or recommended on desktop evidence alone. Architecture retains production-acceptance authority.

---

## Completion report

1. **Summary** — A/B Orbit selection persisted in installation settings; restore, fallback and write policy verified; navigation/modal ownership and real bindings verified.
2. **Files inspected** — Stage 1 files; `InstallationControl.h`, `ExperienceRuntime.cpp/.h` load/save/focus paths; `HudCompositorBridge`, `InputRouter`, `OrbitDraw.h`, `SparklineWidget.cpp`.
3. **Files changed** — §3.
4. **Tests/builds run** — §8, §9.
5. **Results** — §8; no Stage 2 failures.
6. **Deviations from prompt** — Runtime unit suites run per binary (the shared `test` target is broken by other work); harness runs from an isolated bundle copy; real-telemetry "valid-black/stale" not forceable (fixtures + unit tests used).
7. **Newly discovered risks** — Concurrent edits to shared Makefiles/tests; harness timing sensitivity under heavy load; unexplained in-tree abort (not reproduced from an isolated copy).
8. **Contract changes requested** — None to scene-color/activity/HUD-006/HUD-004. New *optional* `orbitObservations` key in the installation-settings file (backward compatible; old files load with defaults) — noted for the settings-schema record.
9. **Recommended next step** — Add Stage 2 to the A7 schedule for the eight hardware items in §11; repair the shared runtime test Makefile (owner of that edit).

```text
WORK ITEM: OC-005 Configurable Orbit Observations, Stage 2
PREVIOUS STATUS: Stage 1 accepted; Stage 2 authorized
NEW STATUS: Stage 2 implemented and desktop-validated. Proposed: IN REVIEW (not ACCEPTED/CLOSED)

WHAT CHANGED: persistence in installation settings; nine-stop/modal/Settings/isolation verification; real FX/RAW binding verification; tests and evidence.
WHAT WAS PROVEN: Done persists only the edited observation and only on change; preview/Esc/scene change write nothing; restart restores; invalid values fall back per field; ring of nine with deterministic wrap; no input leak to scene/media; real FX and optional RAW resolve independently, RAW never inferred from FX; DOM/VIB never substituted.
WHAT WAS NOT PROVEN: any Rock Pi / Pi 3B behaviour (CPU cost, I/O, pacing, GL errors); physical jog wheel; real valid-black/stale on demand.

NEW EVIDENCE: docs/reviews/oc-005-evidence/stage2/, harness phases write/verify/invalid/real, extended unit tests.
NEW RISKS: see 11 and 2.
NEW BLOCKERS: none for desktop; hardware qualification pending.

NEXT ACTION: schedule §11 items with A7.
NEXT GATE: Architecture production-acceptance decision after hardware evidence.

REGISTRY UPDATE REQUIRED: YES
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: YES (orbitObservations settings key)
ARCHITECTURE HANDOFF REQUIRED: YES
```
