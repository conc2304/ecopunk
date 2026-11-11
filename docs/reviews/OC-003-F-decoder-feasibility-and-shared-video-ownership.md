# OC-003-F — Decoder feasibility gate and Shared Video ownership report

Mandatory first gate of OC-003-F (written **before** CPU RAW was implemented; the implementation followed from it).
Repository `blueprint-emergence-updates` @ `f6fe76135313c2ec469604fbeba98c451da83d75`, working tree uncommitted. Facts **[F]** are code reads or measurements; inferences **[I]**; unverified **[H]**.

## 1. Decision

**CPU RAW is SUPPORTED as a diagnostic-only prototype. No Shared Video file, contract, lifecycle or media authority is changed.**
It consumes an accessor that already exists, on the thread and at the point where the existing RAW sampler already runs, and it makes no copy, conversion, download, extra decode or wait on the Linux/GStreamer path (the ROCK Pi path). Two conditions are part of the decision and are enforced in code, not by convention: (a) main thread, same frame as the decoder update, pointer never retained; (b) only packed 8-bit RGB/RGBA pixels are accepted, anything else is refused and counted (`badFormat`), never reinterpreted.

## 2. What the decoder actually is [F]

| Question | Finding | Evidence |
|---|---|---|
| Backend | One `ofVideoPlayer` inside `OfVideoDecoder` (the repo-wide "one ofVideoPlayer" rule). On the Pi, openFrameworks 0.12.1 uses `ofGstVideoPlayer` (GStreamer appsink); on macOS `ofAVFoundationPlayer`. | `shared/src/video-playback/OfVideoDecoder.h`, `OfVideoDecoder.cpp:19-23, 29-31`; `libs/openFrameworks/video/ofGstVideoPlayer.cpp:131-132` |
| Do canonical decoded pixels exist in CPU memory? | **Yes.** `IVideoDecoder::getPixels()` → `OfVideoDecoder::getPixels()` → `ofVideoPlayer::getPixels()`, surfaced as `VideoPlaybackService::currentPixels()` (`const ofPixels*`, null when not Ready/Degraded). It already exists and is already used once by the runtime as the CPU reference of the RAW diagnostic (`ExperienceRuntime.cpp:1304`). | `IVideoDecoder.h:64`; `OfVideoDecoder.cpp:66-70`; `VideoPlaybackService.h:103`, `.cpp:275` |
| Same frame as the GPU texture? | **Yes, by construction.** On a new frame `ofVideoPlayer::update()` uploads `player->getPixels()` plane by plane into its texture (`tex[i].loadData(plane)`) unless the player offers a native texture; for GStreamer `getTexturePtr()` returns `NULL` unless built with `OF_USE_GST_GL`. So the texture the GPU RAW sampler reads is a copy of exactly these pixels. | `ofVideoPlayer.cpp:283-310`; `ofGstUtils.cpp:906-915` |
| Pixel format, dimensions, stride | Default `OF_PIXELS_RGB` (`ofVideoPlayer.cpp:93`); the Pi's texture logged `internalFormat=0x8051` (GL_RGB8), dimensions 960×540 (`02_P2_r1.stdout.log`). `ofPixels` is packed (stride = width × channels). **[H]** the GL internal format alone cannot prove channel order (BGR maps to GL_RGB8 too), so the runtime checks `ofPixels::getPixelFormat()` and refuses non-RGB/RGBA. | stdout log of the 2026-11-08 Pi campaign; `ExperienceRuntime.cpp` `sampleCpuRaw()` |
| Ownership of the buffer | Owned by the player. GStreamer: `copyPixels == false` (`ofGstUtils.cpp:865`), so the front `ofPixels` are an **external view into the `GstBuffer`**, kept alive by `frontBuffer = backBuffer` (`:933-935`). | `ofGstUtils.cpp:917-935, 1378-1396` |
| Thread safety and lifetime | The decoder thread writes only `backPixels` under `mutex` (`:1378-1399`). The main thread swaps front/back **only inside `ofGstVideoUtils::update()`** (`:921-935`), i.e. in `ofVideoPlayer::update()`. So the front pixels are stable between one update and the next **when read on the main thread**. After the next update the previous `GstBuffer` is dropped and the pointer dangles. **Consequence:** CPU RAW must run synchronously on the main thread in the same frame, and never retain the pointer; `cpuraw::PixelView` is consumed inside one call. Reading from another thread would be unsafe and is not done. | as above; `CpuRawAcquisition.h` header comment |
| Frame identity / media identity / timestamp / generation | `isFrameNew()` (`VideoPlaybackService.cpp`), `status().mediaId`, `activationCount()` (`VideoPlaybackService.h:138`), the runtime's scene generation, and a monotonic `steadySeconds()` timestamp. CPU RAW uses its **own** `rawvideo::Gate` instance with the *same rules* as the GPU RAW gate (activation, scene change, eligibility loss, first-new-frame latch, 1.5 s stall) and its own `Series`, so generations never depend on whether GPU RAW is enabled. | `RawVideoGate.h`; `ExperienceRuntime.cpp` `sampleCpuRaw()` |
| Copy / conversion / GPU download / extra decode / blocking wait when accessing pixels | **GStreamer (Pi): none** — `getPixels()` returns a reference to an already-resident `ofPixels`; the CPU cost is only our own reduction. **macOS AVFoundation:** `getPixels()` converts lazily from the `CVPixelBuffer` with vImage once per new frame (`ofAVFoundationPlayer.mm:301-340`). That is a real cost on the desktop only; the runtime asks for pixels **only when a sample is due** (2 Hz), and the call is timed separately (`accessWallMs/accessCpuMs`). No GPU readback exists in this path. | `ofAVFoundationPlayer.mm:226-340`; `sampleCpuRaw()` |
| Can existing decoder timing expose acquisition overhead? | Partly. The frame stream's `update` segment already contains `ofVideoPlayer::update()` (and, on the Pi, the texture upload), and `RT004_SCOPE("OfVideoDecoder.ofVideoPlayer.update")` exists under the stall probe. CPU RAW adds its own `access / reduce / analyze / total` timings (wall **and** thread CPU) and a per-frame `vid_new` flag (decoder delivered a frame) so playback stability can be compared across modes. | `OfVideoDecoder.cpp:29-31`; `Rt004FramePhaseProbe.cpp` CSV columns |

## 3. Cost expectation and what is *not* proven

* **[F] Desktop (macOS, Release):** reduce 0.95 ms p50 / 1.39 ms p95, analyze (AVG + DOM/VIB) 0.36 ms p50, total 1.31 ms p50 / 2.07 ms max wall per 2 Hz sample; CPU time equals wall time (no blocking). 19 samples per 10 s, 0 gaps, 0 noPixels/badFormat/dimMismatch.
* **[F] CPU RAW vs GPU RAW, same frames, desktop:** 79 paired samples. AVG max |diff| 0.00136 (median 0.00033, linear light); DOM max |diff| 0.0003. The two paths share the definition (`SceneColorSampler.cpp reduceFragment` arithmetic reproduced on the CPU; AVG/DOM/VIB by the unchanged production code), so agreement is expected; the residue is half-float RGBA16F in the GPU target.
* **[H] ROCK Pi:** the reduction touches 960×540×3 bytes with table lookups every 500 ms; the Pi's cost is **unmeasured** and is the point of the qualification (it must be compared as CPU time, frame pacing and total CPU overhead, not assumed from the desktop). Whether the Pi's decoder delivers RGB (not BGR) is unverified; the runtime reports `badFormat` instead of guessing.

## 4. Shared Video ownership and contract impact

| Item | Status |
|---|---|
| Shared Video source files | **No OC-003-F edit.** `git status` lists uncommitted modifications under `shared/src/video-playback/` (`VideoPlaybackService.*`, `VideoSelectionPolicy.*`, tests); they pre-date OC-003-F (earlier OC-003 work: activation count, startup media id, `currentPixels()` accessor) and were not touched or relied on beyond reading the accessors listed above. This agent's files are only under `sketches/experience_runtime/` and `scripts/oc003/` |
| Public contracts (`IVideoDecoder`, `VideoPlaybackService`, `VideoPlaybackStatus`, `HudFrameData`, Scene Color snapshots) | **Unchanged.** `currentPixels()` is an existing accessor; CPU RAW is never published to a snapshot, an Orbit widget or a semantic binding |
| Media authority / selection / seek | **Unchanged.** No media-selection or seek API was added; the campaign pins media with the existing `EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID` |
| Second decoder or independent media source | **None** |
| New GPU readback path or pixel format | **None** (CPU RAW adds no GPU work; the palette analyzer reuses grids already read back) |
| Production defaults | **Unchanged.** `EXPERIENCE_RUNTIME_CPU_RAW`, `..._PALETTE_DIAG` are off unless set (exact `"1"` / a directory) |

**Escalation triggers checked and not hit:** invasive decoder change (no), shared contract/ownership change (no), unsafe lifetime (provable on the main thread, enforced by single-call consumption), new readback/format (no), default change (no).

## 5. Residual risks

1. A decoder pixel format other than packed 8-bit RGB/RGBA on the Pi makes CPU RAW refuse every sample (`badFormat > 0`, reported in the stat line and the campaign verification). That would end P4/P5 for that build, not break anything else.
2. If the Pi build ever enables `OF_USE_GST_GL` the decoder texture would no longer be a copy of the CPU pixels; CPU RAW would still measure the CPU frame, but equivalence with GPU RAW would have to be re-checked.
3. A future decoder refactor that moves `ofVideoPlayer::update()` to another thread or retains `getPixels()` references across frames invalidates the lifetime argument above; any such change should re-open this gate with Shared Video Engineering.
