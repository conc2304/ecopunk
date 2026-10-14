# RT-004 — S1 GPU/CMA Residency Attribution (offline, Phase 0)

**To:** Architecture · **From:** Raspberry Pi Runtime & Performance · **Date:** 2026-10-12
**Scope:** offline reconciliation of existing S1 evidence only — no rebuild, no Pi run, no code change to production paths.
**Evidence:** campaign `campaign-20261012T023119Z-747b`, run `S1-20261012T023401Z-4ac6` (copy on the development host at `~/Desktop/campaign-20261012T023119Z-747b/`). Source `44044c13e371f246ca8a7fe662919d0dc06addca+dirty` (diff sha256 `1fa02620…5837`), binary sha256 `62322abd378027dd…`, Pi 3B+ kernel `6.12.75+rpt-rpi-v7`, Mesa 24.2.8 VC4 (desktop GL 2.1), display 1024×600 (app window 1280×720), `gpu_mem=76M`, CmaTotal 256 MiB.
**Stop recorded:** watchdog `cma_free<=40MiB_floor (33.6)` at +31.42 s; SIGTERM, no exit within 5 s, SIGKILL at +36.43 s.

## Executive finding

The S1 floor stop happened **inside Blueprint's first (lazy) `setupEntry`**: `scene.setup: blueprint_emergence` at +30.63 s, CmaFree 48.5 MiB at +31.04 s, 33.5 MiB on the watchdog's own read at +31.42 s, with no `scene.activate` logged — so ≥15.0 MiB was allocated by Blueprint setup in ≤0.4 s, and that setup had not finished. It crossed the floor because ~77 % of CMA was already resident, almost all of it **measured** through VC4 `bo_stats` / dma-buf counters (readable as the normal user — DRM is *not* a blind spot on this Pi):

| Phase | CmaFree | What the counters show |
|---|---|---|
| Pre-launch | 231.9 MiB | 24.1 MiB already used by the system (dumb/scanout 3.6 MiB + firmware/other) |
| Startup → first Blob frame (+15.3 s) | → 72.7 MiB (−159.2) | binner 16.0 MiB; window dma-buf 3.5 MiB; canonical decoder dma-buf +21.5 MiB (22 buffers); V3D +127.8 MiB / +25 BOs during the ambient preload |
| Temporal 1st activation / off | → 54.5 MiB (−17.2) | Temporal's dedicated decoder dma-buf +21.5 MiB (22 buffers); V3D +12.7 MiB (playheads, Temporal FBOs/shaders); Mesa cache reuse absorbs part; **unchanged on Temporal→Blob** |
| Temporal 2nd activation | → 50.8 MiB (−3.7) | decoder close/reopen (pool size unchanged), playheads cleared+reallocated, churn retained in Mesa's purgeable cache |
| Temporal → Blueprint setup | 49.6 → 48.5 → **33.5 MiB** | ≥15.0 MiB in Blueprint setup before the stop (lower bound) |

The 5.3 MiB samples at +32…36 s are **teardown after SIGTERM**, not Blueprint: Mesa's purgeable "userspace BO cache" jumps to 171.9 MB (108 BOs) as the app frees GL objects, one decoder's dma-buf pool is released (46→24 buffers), and CmaFree recovers to 221.5 MiB after exit.

**Not a leak, on this evidence:** every large block has an identified owner and a bounded lifetime; CMA recovers on exit; repeated Temporal activation adds ~3.7 MiB of mostly purgeable churn, not a growing pool. The problem is **concurrent residency** in a 256 MiB pool: permanent ambient textures + two live decoders + Temporal resources retained while inactive + Blueprint's first-setup allocations.

## Largest contributors

| # | Owner / allocation path | Lifetime | Bytes | CmaFree correlation | Confidence | Evidence |
|---|---|---|---|---|---|---|
| 1 | Temporal ambient: 19 tint textures, `TFAmbientTextureLayer::setup` via `ExperienceRuntime` preload | process | **measured** V3D +127.8 MiB / +25 BOs over the preload window (+6…+15 s); **estimate** 114.0 MiB (1536×1024 / 1672×941 × 4 B — VC4 stores RGB as 4 B/px); ~13.8 MiB / 6 BOs of the delta is other startup GPU (scene FBO, Blob, HUD, shaders — not individually separated) | −124.6 MiB over the same window | High (BO count/size delta ≈ 19 × 6.3 MB; residency note at +14.73 s) | samples.tsv +6…+15 s; stall-probe NOTE `temporal.ambientResidency` |
| 2 | Two v4l2 decoders (dma-buf): canonical (`OfVideoDecoder`) since startup, Temporal dedicated (`TimeOffsetVideoBuffer`) since first Temporal activation | canonical: process; Temporal: from first activation, **kept while Temporal is inactive** | **measured** dma-buf 46 objects / 47.5 MiB = window buffer 3.5 MiB + 2 × 22 buffers × ~1.0 MiB (21.5 MiB each) | +21.5 MiB at +5…6 s and at +19 s | High (count step coincides with `canonical.load` / `temporal.loadExplicit` + GStreamer decoder-start assertion; pool persists through Temporal→Blob; drops 46→24 at teardown) | samples.tsv dmabuf columns; stall NOTEs |
| 3 | V3D binner (driver-owned tile binning) | process | **measured** 16.0 MiB (1 BO) | −21.3 MiB at +4…5 s with window buffer + first V3D BOs | High | samples.tsv +5.04 s |
| 4 | Blueprint first setup: `BlueprintSceneCore::setup` → `ErosionFBO` 3 × 1280×720 RGBA; `MotionExtraction` 2 × window-sized RGB extract FBOs + 60 × 160×90 history FBOs + 2 accum FBOs; `ShaderLibrary`, fragment shader, code font | from first Blueprint setup, never released (lazy, cached across reactivation) | **observed ≥15.0 MiB** (setup incomplete at stop); **estimate** 18.4–21.9 MiB for complete setup (10.55 + 5.27–7.03 + 2.47–3.30 + ~0.1 + <1 MiB; VC4 tiling/padding unknown) | −15.0 MiB at +31.04…+31.42 s | High for attribution (time-contained in setupEntry, no other app activity; decoder counts unchanged); medium for size | timeline TSV +30.63…+31.42; source `BlueprintSceneCore.cpp:11-71`, `ErosionFBO.cpp:11-18`, `MotionExtraction.cpp:4-72` |
| 5 | Temporal GPU besides decoder: 6 playhead textures 640×360 (`getPlayheadTexture`), Temporal setup FBOs/shaders | from first activation; **retained while inactive**; playheads cleared and reallocated on reactivation | **measured** V3D +12.7 MiB / +14 BOs across first activation (mixed); playheads **estimate** 3.96 MiB RGB / 5.27 MiB RGBA | part of −17.2 MiB | Medium (mixed BO delta, cache reuse) | samples.tsv +17…+23 s; frame-phase EVENTs |
| 6 | Mesa userspace (purgeable) BO cache | transient | **measured** 3–13 MB during run | absorbs/retains churn | High (semantics) | `userspace BO cache` column |

**Pi-specific configuration finding (Blueprint owner):** `PLATFORM_PI` is never defined (stated in `BESettings.h:395`), so on the Pi `MotionExtraction` allocates window-sized extract FBOs and 60 history frames instead of its own Pi branch (≤640×360, 15 frames) — an extra ~4–6 MiB of CMA (estimate) plus fill cost. `BEFragment` also allocates an `effectSourceFbo`/`effectResultFbo` pair per effect fragment lazily during the run (not reached in S1; count-dependent).

## Confirmed vs suspected vs unresolved

**Confirmed (measured and time-correlated):** binner 16.0 MiB; window buffer 3.5 MiB; two 22-buffer decoder pools of 21.5 MiB each, the Temporal one held while Temporal is inactive; ambient preload ≈ 128 MiB of V3D BOs (incl. ≤13.8 MiB of other startup GPU); the floor crossing occurring inside Blueprint's first setup; post-SIGTERM samples are teardown; no growing pool across Temporal reactivation.

**Suspected (estimate-backed, not individually measured):** Blueprint setup total 18.4–21.9 MiB — i.e. the floor would have been crossed even if setup had completed (48.5 − ≈20 ≈ 28 MiB); playhead texture size 5.27 MiB; MotionExtraction Pi misconfiguration ~4–6 MiB.

**Unresolved:** per-object BO identity (`bo_stats` gives per-label totals; `gem_names` not collected); exact Blob/HUD/scene-FBO split of the 13.8 MiB startup remainder; 10.4 MiB not yet recovered 3 s after exit (221.5 vs 231.9 MiB; possibly X server buffers — not measured); why the app did not exit within 5 s of SIGTERM (consistent with the known destructor-time GStreamer EOS wait, not shown); GL_INVALID_ENUM 0x0500 after ambient load — separate, unrelated to CMA.

**Why CmaFree alone cannot prove a V3D allocation or a leak:** CMA is one pool shared by V3D BOs, the binner, scanout/dumb buffers, v4l2 decoder dma-bufs and firmware; Mesa returns freed GL objects to a *purgeable* userspace cache that stays CMA-resident (and is reclaimable by the kernel under pressure), so a falling CmaFree can mean freed-but-cached memory; cache reuse lets new allocations consume no new CMA (the first Temporal activation added ≈34 MiB of counted allocations but CmaFree fell only 17.2 MiB); and the 1 s collector cadence cannot separate events closer than a second (the decisive 15 MiB drop was caught only by the watchdog's own 0.25 s read). Attribution here relies on BO/dma-buf counter deltas plus event containment, not on CmaFree.

## Smallest justified correction (proposal only — not implemented)

**Release Temporal's dedicated decoder (and its playhead textures/history) when Temporal is deactivated.** Owner: Temporal Fields; Architecture review required because it changes what deactivation releases.

- Evidence: the 21.5 MiB Temporal decoder pool (plus ≈5 MiB playheads and 15.8 MiB CPU history) stays resident through Temporal→Blob and Temporal→Blueprint (timeline +23…+31 s), yet every reactivation already performs a full reload (`invalidateForReactivation` → `loadExplicit`: decoder close/reopen, history reset, playhead reallocation — observed at +26.4…+27.1 s). The retained resources are never reused.
- Expected effect (inference): CmaFree at the Blueprint transition ≈ 49.6 + ~21.5 (+ up to ~5) ≈ 71–76 MiB → after a complete Blueprint setup (−18.4…−21.9) ≈ 49–58 MiB, above the 40 MiB floor; ambient, B and C behavior unchanged; TEMP-004 invalid-history guarantee preserved (reactivation still starts from a reset).
- Not proposed: any ambient format/resolution change (largest contributor, but forbidden and artwork-affecting); enabling `PLATFORM_PI` (it also switches the PLATFORM_PI-aware shader contract B — a Blueprint/Effects owner decision, listed above as a finding only).

**Verification gate (Phase 2 proposal, requires separate approval; only after the correction is reviewed and built):** one supervised run of the existing `smoke-circuit` driver under the unchanged watchdog (40 MiB floor, all guards), expected ≤45 s; success = dma-buf count returns 46→24 and V3D drops by ≈ playhead size at each Temporal deactivation, and CmaFree stays above the floor through Blueprint setup. Early stop: any watchdog condition; Blueprint remains NOT RUN if the floor is approached. Fallback if debugfs is unreadable on that boot: CmaFree + dma-buf from `/sys/kernel/debug/dma_buf/bufinfo` only, attribution reported as reduced-confidence.

## Machine-readable timeline and evidence index

- Timeline: `docs/reviews/logs/rt004-s1-gpu-cma-timeline-2026-10-12.tsv` (114 rows; collector, watchdog, frame-phase, stall NOTE/LONG FRAME on one CLOCK_MONOTONIC basis; header records run id, source revision, build id, stop). Regenerate: `python3 scripts/rt004/s1_cma_timeline.py <S1 run dir>` (read-only).
- Raw files (sha256 prefix): `samples.tsv` b7db906a9cdeb4d0 · `watchdog.tsv` 44e7f137daaf8691 · `frame-phase.log` c7cba26d899ee2ea · `stall-probe.log` f2828483c319d914 · `driver.log` b1ebf528457c1cbe · `manifest.json` d9be313044998703 · `app-stdout.log` 709ff192e8272e87 · `app-stderr.log` b07851083521a824. Campaign `preflight.json` records the Linux offline gate (21/21, 0 skipped) and debugfs readability.

No Phase 1 instrumentation was added: the offline evidence was sufficient to identify the allocation path. Nothing was rebuilt, run, staged, committed or pushed.

**RT-004 PI FOLLOW-UP REQUIRED**
