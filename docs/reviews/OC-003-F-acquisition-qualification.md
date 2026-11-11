# OC-003-F — Diagnostic CPU RAW acquisition: P0–P5 isolation, palette analysis, qualification package

Written for: ExperienceRuntime / Scene Observability Engineering (manager) and the supervised ROCK Pi 4B operator.
Status of this report: **desktop-verified diagnostic package, hardware-ready; no ROCK Pi hardware result exists.** Nothing here was staged, committed, pushed or deployed.

Companion: [decoder feasibility and Shared Video ownership](OC-003-F-decoder-feasibility-and-shared-video-ownership.md) (Runtime agent). Operator procedure: section L of [the operator package](OC-003-rockpi-attribution-validation-operator-package.md). Evidence: [oc-003f-evidence/](oc-003f-evidence/).

## 1. Summary

CPU RAW acquisition through the existing `VideoPlaybackService::currentPixels()` works on the macOS desktop, is diagnostic-only and off by default, and reproduces GPU RAW to rounding level on the same decoded frame. No decoder, Shared Video or contract change was needed. The ROCK Pi 4B (GStreamer backend, authoritative target) has **not** been measured.

| Question | Desktop answer (macOS, AMD Radeon Pro 560X, 1280×720, 960×540 media) | Hardware status |
|---|---|---|
| Does CPU RAW produce correct output? | CPU vs GPU RAW, 104 same-frame pairs: median mean abs error 0.00011 in sRGB-encoded space (max 0.00012), worst single cell 0.0003; AVG median diff 0.0002. In the Temporal scene (which does not otherwise convert pixels): CPU avg colour within 0.001 of GPU, 200 distinct live values | not run |
| Cost on the main thread? | per 2 Hz sample: access 0.003 ms, reduce 1.19 ms, analyze 0.39 ms, **total 1.59 ms p50 / 2.00 p95 / 2.16 max** (P4); 0 gaps in 174 samples | **unmeasured on the Pi** |
| Does it add GPU work? | No. P4 has no GPU colour-sampling stream at all; GPU RAW reads back 0.23 MB per sample (39.9 MB in 173 samples), CPU RAW 0 | n/a |
| macOS first-access pixel conversion? | **Not observed** in either scene (Blob access 0.0025 ms; Temporal 0.0033 ms) | Pi access cost **unmeasured** |
| Frame pacing impact? | Single quiet round: no long frames (>40 ms) in P0, P2–P5; P4/P5 p95 35.2/35.1 ms vs P0 34.3 ms. **One round has no spread, so this is not a result** | the Pi is the only valid answer |

## 2. Arms and the deviation from the prompt's table

| Planner id | FX GPU | GPU RAW | CPU RAW | Env (all with `PREDOMINANT_COLOR=1` when any sampling is on) |
|---|---|---|---|---|
| P0 | – | – | – | `SCENE_COLOR=0` |
| P1 | on | – | – | FX deferred/fence |
| P2 | on | on | – | FX + `VIDEO_COLOR=1` (deferred) — the pre-existing combination |
| P3 | – | on | – | `SCENE_COLOR=0`, `VIDEO_COLOR=1` |
| P4 | – | – | on | `SCENE_COLOR=0`, `CPU_RAW=1` |
| P5 | on | – | on | FX deferred/fence + `CPU_RAW=1` |
| PCMP | – | on | on | P3 + `CPU_RAW=1`; **correctness arm only, never a cost arm** |

**Deviation (stated explicitly):** the Runtime agent reports that the authorisation text's table defines P2 as RAW GPU only and P3 as FX + RAW GPU. This agent never saw P0–P3 defined and kept the repository's existing P0/P1/P2 (`plan_interleaved.py`, used by earlier OC-003 campaigns) so earlier data stay comparable. Crosswalk: prompt "P2 (RAW GPU only)" = planner **P3**; prompt "P3 (FX + RAW GPU)" = planner **P2**. P4 and P5 match the prompt. The runtime itself has no P-labels (only env variables and the `AcquisitionMode` line), so the reconciliation is tooling/documentation only. If the Manager wants the prompt's labels, only the planner table needs swapping.

## 3. Requirements → evidence

| # | Requirement | How met | Evidence |
|---|---|---|---|
| 1 | Reuse Shared Video source and `rawvideo::Gate` | `sampleCpuRaw()` reads `runtimeServices_.video()` (`currentPixels()`, `currentTexture()` for eligibility only) and its own `rawvideo::Gate` instance of the unchanged class | code read; `raw_video_gate_tests` 12/12 |
| 2 | Main thread, after service update, before navigation | called in `ExperienceRuntime::update` after `RuntimeServices.update` (step 7) and before the frame's scene construction; pixels are requested only when a sample is due | code read |
| 3 | Activation-frame rejection and generation invalidation | the Gate is unchanged; a real run with fast media advance began generation 1→2→3 for **both** GPU and CPU RAW at the same activations, 0 gaps, 0 dim-mismatch across different clip sizes | `oc-003f-evidence/media-transition/` |
| 4 | 2 Hz | `kPeriodSeconds = 0.5` | 173–174 samples per ~87 s steady state |
| 5 | Monotonic timestamps, no PTS accessor | `steadySeconds()`; no decoder interface touched | code read |
| 6 | Never retain borrowed pointers | `PixelView` is consumed inside one call; nothing stored | code read; **no sanitizer run** |
| 7 | Bounded extraction, no full-frame copy | `reduceToGrid` reads the borrowed buffer in place into a fixed 160×90×4 float grid, ≤24 texels/axis | `cpu_raw_acquisition_tests` 46/46 incl. an independent naive reference |
| 8 | Independent costs | access, reduce, analyze, total — each wall and thread-CPU, p50/p95/max | `timing-p0-p5/sampler.csv` |
| 9 | GPU RAW path unchanged | additive, inert palette hook only (`paletteSink_` null unless `EXPERIENCE_RUNTIME_PALETTE_DIAG`); P2/P3 GPU RAW stats are of the same shape and size as before | `make test` pass; campaigns |
| 10 | Diagnostic, off by default | `EXPERIENCE_RUNTIME_CPU_RAW`/`_PALETTE_DIAG` unset ⇒ no new work; `AcquisitionMode` line only printed when the probe/CPU RAW/palette is on | code read; P0/P1/P2 campaigns unchanged |

## 4. Verification matrix

| Item | Result | Basis |
|---|---|---|
| Media activation, reactivation | activation: PASS (real run + unit). Same-media reactivation: by design via `activationCount()` (OC-003B), **not separately exercised** in a real run | media-transition evidence; gate tests |
| Loop and transitions | 300 s PCMP over a 100.4 s clip: 558 samples, 0 gaps across loops. Scene transitions: 5 scenes, 12 changes, generation lines match (independently re-verified by this agent) | `pcmp-cpu-vs-gpu/`, `multi-scene-correctness/` |
| Stale-frame rejection | PASS in unit tests (`AwaitingFirstFrame`, activation frame never latches); no real-run injection | unit |
| Pixel-buffer lifetime safety | design + no crash in any of the runs above; **not proven** by ASan/TSan | – |
| Null / unready pixels | unit tests (`noPixels`, refused views); no real-run injection | unit |
| Formats and stride | packed 8-bit RGB/RGBA with `rowBytes ≥ w·ch` accepted; BGR, planar, 16-bit refused and counted (`badFormat`), never reinterpreted — unit-tested. Real run: `badFormat=0`. **Pi pixel format unverified** | unit; sampler.csv |
| CPU vs GPU RAW | PASS on 104 same-frame pairs (0.00011). The all-pairs figure (0.0182 in the first 150 s run) is dominated by a ~1-frame offset between the two samplers' due times: pairs with Δt<17 ms differ by 0.0001. Verdict uses same-frame pairs only and is INCONCLUSIVE below 5 | `pcmp-cpu-vs-gpu/palette_report.json`, `temporal-pcmp/` |
| Main-thread cost | desktop numbers in §1; Pi pending | sampler.csv |
| Palette determinism | replay of 1136 stored grids reproduces every CSV row exactly (worst abs diff 0.0); two replays byte-identical | `palette_report.json` |
| No duplicate decode / new GPU readback | P4 has no GPU sampler stream; no extra decoder objects constructed (code read) | logs |
| No default regression | all OF-free repo tests pass (`make -f Makefile.tests test`); default code path unchanged by inspection. **No full production-default soak was run** | tests |
| macOS first-access conversion | not observed (Blob 0.0025 ms, Temporal 0.0033 ms p50). Temporal pixels verified live and correct (not a stale buffer) | `temporal-access-cost/`, `temporal-pcmp/` |

## 5. Tests and builds run

- Release build of the sketch: exit 0 (22:19 build; later rebuilt by another session, see §7).
- C++ (OF-free): `cpu_raw_acquisition_tests` 46/46, `palette_analyzer_tests` 45/45, `palette_sink_tests` 11/11, `raw_video_gate_tests`, `scene_color_series_tests`; full `make -f Makefile.tests test` exit 0.
- Python: `test_diag_tooling` 42, `test_correctness_kind` 10, `test_verify_generations` 13, `test_event_analysis` 8, `test_run_campaign` 10 — all with `-I -W error`, all OK. The same three tooling suites also pass from the extracted package.
- Desktop campaigns on the frozen binary: timing P0–P5 (6 × 100 s, 1 round) verified/reconciled/restored; PCMP 300 s with palette capture verified/reconciled/restored; Temporal-scene PCMP 110 s; fast-media-advance 60 s direct run; the Runtime agent's 5-scene correctness run (re-verified here) and `cpu_access_cost.py` run (numbers cited from their output file).
- `run_qualification.py` stages unit, tooling, plan, audit executed for real (all PASS); timing/palette stages are exactly the `run_campaign.py` invocations above and were run through that tool, not through the wrapper.

## 6. Files

**Changed/added by this agent:** `scripts/oc003/{plan_interleaved,verify_run,summarize_campaign,reconcile_events,sampler_stats,run_campaign,make_package,test_diag_tooling}.py` (P3–P5/PCMP, `--palette-diag`, `--scene-env`, `AcquisitionMode` and CPU-RAW checks, CPU RAW stat parsing, sysmon wiring, generations hook); new `palette_report.py`, `run_qualification.py`; `sketches/experience_runtime/test/{palette_sink_tests,palette_replay,palette_fixture_gen}.cpp` and `Makefile.tests` targets; operator doc section L; this report; `oc-003f-evidence/`. The planner/verifier `--kind correctness` support was written by the Runtime agent and re-reviewed by running its tests and its output.

**Present in the tree from the Runtime agent (`openframeworks-23`), not authored by this agent:** `CpuRawAcquisition.h`, `PaletteAnalyzer.h`, `PaletteSink.h`, the OC-003-F blocks of `ExperienceRuntime.{h,cpp}`, the palette hook in `SceneColorSampler.{h,cpp}`, the `cpu_raw*`/`pal_us`/`vid_new` CSV columns in `Rt004FramePhaseProbe.cpp`, `sysmon.py`, `verify_generations.py`, `cpu_access_cost.py`, their tests, and the feasibility report. This agent read and exercised them; it did not review every line.

**Not touched:** `IVideoDecoder`, `OfVideoDecoder`, `VideoPlaybackService`, any shared contract. Shared Video has pre-existing uncommitted edits unrelated to OC-003-F (activation count, startup media id, resume windows).

## 7. Identity, deviations and risks

- **Binary identity.** The binary used by the first verified campaigns (`03876496…`, 22:19) was overwritten by another session's rebuild at 22:57 and can no longer be reproduced; those runs are kept only as supporting data. **All evidence cited above is from the frozen copy `89ec0f6e022ddfa741c79a6409602c54ba486219738efefd01b5d955cf11702d`.** Source manifest sha256 `8e39f9199959e010c82fe270ac5c635f3b9be1d26a582c1ff1980d31327bf089` (37 files, taken 11-10 10:01). `ExperienceRuntime.{h,cpp}` were modified again at 11-10 09:24, after that binary was built, so the manifest may be newer than the binary for those two files. The tree also contains an unrelated opt-in CRT-phosphor scene (`EXPERIENCE_RUNTIME_CRT_SCENE`), compiled into that binary; it is not used by any run here.
- **Shared tree.** Three sessions edit it; one foreign `experience_runtime` process (pid 1251) aborted one campaign by design (fail-closed). An input event reached the focused app window once (log shows `immersive: on (any key/press exits)` then a `manual_previous`; the source is presumed to be a keypress but was not identified) and changed the media, which aborted another campaign. Both campaigns were discarded and rerun in full, never pooled.
- **Single round, desktop.** Frame-time comparisons have no round-to-round spread and an earlier, noisier desktop run showed long frames in the *no-sampling* baseline. Do not draw frame-pacing conclusions from the desktop.
- **Palette summary.** A run ended by `timeout -s INT` does not write `palette_summary.json` (no signal handler); up to 7 trailing CSV rows per stream are lost; `palette_report.py` records this.
- **Not proven:** anything on the ROCK Pi 4B (GStreamer access cost, pixel format, thermals, RSS growth); ASan/TSan lifetime proof; same-media reactivation in a real run; a long production-default soak.
- **Contract changes requested:** none. No PTS accessor was introduced.

## 8. Hardware handoff (supervised, operator-run)

Package: `/Users/joseconchello/oc003f-handoff/oc003f-diagnostic-package.tar.gz`, sha256 `7e2bdd4873b9d21fd6b197682ac94c6b6ccbe1a0507989bab16cc25e5c46c03c` (40 files, verified intact and its tests pass from an independent extraction). It contains `OPERATOR.md` (section L), all `scripts/oc003` tools, and the accepted sources under `source/` for the Route B overlay; no binary, media or settings. Transfer, install and operation follow sections B–C of the operator document; the coding agent does not operate the Pi.

Recommended order on the Pi: capability check → `run_qualification.py --dry-run` → smoke (L.3, 90 s, 1 round) → Manager go-ahead → timing campaign (4 rounds × 300 s, P0–P5) → PCMP palette campaign (≥300 s; the Pi's GStreamer frame offsets may give different same-frame yield) → multi-scene correctness (L.6, ≥150 s) → `cpu_access_cost.py --scenes blob,temporal`. Expected to learn: Pi access cost (a reference vs a conversion), reduce/analyze cost on the Cortex-A72, pixel format, RSS/thermal behaviour, and whether P4/P5 pacing differs from P0/P1 beyond round-to-round spread.

## 9. Recommended next step

Manager decision to authorise the supervised ROCK Pi 4B smoke run, then the full campaign. Until then CPU RAW stays diagnostic and off.

---

```text
WORK ITEM: OC-003-F (diagnostic CPU RAW acquisition feasibility + P0–P5 isolation + palette analysis + qualification package) — not listed in docs/shared-project-docs/03-work-registry.md at the time of writing
PREVIOUS STATUS: not registered; prior state: feasibility authorised, P0–P3 / palette tooling partly present in the tree
NEW STATUS: implementation complete and desktop-verified; hardware-ready package built; awaiting supervised ROCK Pi 4B qualification (coding agent does not set ACCEPTED/CLOSED)

WHAT CHANGED: see §6
WHAT WAS PROVEN: CPU RAW is correct vs GPU RAW on the same frame (desktop); generations/transitions behave for media and scene changes; palette analysis is deterministic and reproducible from stored grids; tooling fails closed on identity, environment and foreign-process violations; package integrity
WHAT WAS NOT PROVEN: anything on the ROCK Pi 4B; main-thread cost and pixel format on GStreamer; frame-pacing effect (desktop single round only); lifetime safety by sanitizer; same-media reactivation in a real run; the original binary 03876496 (overwritten)

NEW EVIDENCE: docs/reviews/oc-003f-evidence/ (timing P0–P5, PCMP CPU-vs-GPU, Temporal PCMP, Temporal access cost, multi-scene correctness, media transition); package sha256 7e2bdd48…c03c
NEW RISKS: shared working tree with concurrent unrelated edits/rebuilds (binary-identity risk); focused-window input can disturb desktop runs; palette_summary.json absent on SIGINT; ExperienceRuntime.{h,cpp} newer than the frozen binary
NEW BLOCKERS: none for the desktop; hardware qualification requires supervised Pi access

NEXT ACTION: Manager authorises Pi smoke run (operator section L)
NEXT GATE: ROCK Pi 4B qualification results reviewed by ExperienceRuntime Engineering

REGISTRY UPDATE REQUIRED: YES (item not registered; status and evidence list new)
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: NO (no contract, decoder or Shared Video change; escalation conditions not hit)
```
