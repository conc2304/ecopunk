# OC-003-E Phase 0 — GPU Work Isolation Investigation

Work item OC-003 (Sampling-Frame Attribution), phase OC-003-E Phase 0 · domain ExperienceRuntime / Scene Observability · consult: ROCK Pi Runtime & Performance · review: Architecture Manager
Date 2026-11-08 · authorization: architecture-approved, **strictly read-only** · author: ExperienceRuntime GPU Pipeline Investigation Agent

Nothing was implemented, built, deployed or changed. The only file written is this report. Nothing was staged or committed. No Pi was contacted.

Conventions: **[F]** observed fact (a file/line read, or a number recomputed from archived data in this session) · **[I]** technical inference (follows from facts, not directly observed) · **[H]** unverified hypothesis · **[S]** supplied by Architecture and carried unverified.
Line numbers refer to the working tree described in §1.

---

## 1. Executive summary and evidence-access limitations

### 1.1 Identity [F]

| Item | Value |
|---|---|
| Repository root | `/Users/joseconchello/openFrameworks/apps/myApps/EcopunkVideoCollage` (the parent `openFrameworks/` directory is not a git repository) |
| Branch / HEAD | `blueprint-emergence-updates` / `f6fe76135313c2ec469604fbeba98c451da83d75` |
| Working tree | 65 dirty paths (`git status --short`), **0 staged**; the OC-003 implementation is uncommitted, so HEAD is the base revision, not the identity of the code read here |
| Runtime sources read | `sketches/experience_runtime/src/ExperienceRuntime.cpp/.h`, `SceneColorSampler.cpp/.h`, `SceneColorSamplerDiag.cpp` (grep only: inert unless `..._PREDOM`), `Rt004FramePhaseProbe.cpp/.h`, `SceneColorDeferredGate.h`, `SceneRenderGuard.h`, `BlobProductionScene.cpp`, `BlobSceneCore.cpp`, `VideoRegionController.cpp`, `VideoRegionEffectRenderer.cpp`, `SceneActivityProbe.cpp`; `shared/src/video-playback/OfVideoDecoder.cpp`; `shared/src/hud-compositor/HudWireframeRenderer.cpp` (grep) |
| Framework sources read | `libs/openFrameworks/app/ofAppGLFWWindow.cpp`, `app/ofMainLoop.cpp`, `events/ofEvents.cpp`, `gl/ofFbo.cpp/.h`, `gl/ofTexture.cpp`, `gl/ofGLProgrammableRenderer.cpp` (openFrameworks 0.12.1 as vendored here) |
| Platform-specific GL code | None in the repository. The only platform code is openFrameworks' GLFW window and renderer. Mesa/Panfrost driver source is **not** available on this machine, so every driver-level statement below is [H]. |

### 1.2 Evidence access

* **Available:** the OC-003 attribution archive `~/Downloads/oc003-evidence/oc003_attr_20261108_1330` (P0, P2, P2F × 4 rounds × 300 s; per-frame CSVs, phase logs, stdout logs, `preflight.json`, `plan.json`). Hardware per the archive [F]: `rockpi-4b`, Linux 6.18.26-current-rockchip64 aarch64, `DISPLAY=:0` (an X11 session), `glRenderer="Mali-T860 (Panfrost)"`, `glVersion="3.1 Mesa 25.0.7-2"`, 1280×720 window, `targetFps=30`, `limiter=on`, `vsync=on` (the CONFIG line records the *request*, `Rt004FramePhaseProbe.cpp:116`).
* **NOT available:** the **OC-003-D campaign raw data** (the P2G and P2N runs). A filesystem search found no `*P2G*` or `*P2N*` data anywhere outside this session's desktop rehearsal scratch (which is macOS and not Pi evidence). **The ten OC-003-D findings preserved in §4 are therefore carried as supplied [S]; they are not independently verified here.**
* **What I could verify independently:** the *earlier* P0/P2/P2F campaign. Recomputed this session with the repository's `scripts/oc003/stall_analysis.py` over all four rounds:

| Condition | frames | frame p50 / p95 (ms) | long (>40 ms) | exclusive classes (frames / long) |
|---|---|---|---|---|
| P0 | 32,261 | 33.33 / 33.80 | 14 | quiet 32,261 / 14 |
| P2 | 32,303 | 33.27 / 37.85 | 958 | event 7,001 / 323 · after-issue 639 / **604** · after-collect 2,651 / **0** · lag-2 2,481 / 1 · quiet 19,531 / 30 |
| P2F | 32,289 | 33.27 / 38.02 | 981 | event 7,170 / 274 · after-issue 732 / 688 · after-collect 2,738 / 0 · lag-2 2,545 / 2 · quiet 19,104 / 17 |

  This is a **different campaign** from OC-003-D, so it does not verify the preserved numbers. It shows the same phenomenon at the same order of magnitude (P2 frame p95 37.85 vs the supplied 37.37 ms; `postDraw` blocked p95 7.22 vs 8.32 ms; exclusive after-issue long frames 604 vs 518). Treat it as a plausibility check on the supplied figures, not as confirmation of them.

### 1.3 Headline conclusions

1. **[F] The sampler's GPU work is issued and its completion is only ever polled; no sampler call blocks on the GPU in the issue frame.** The issue path creates a fence (`SceneColorSampler.cpp:397`), optionally flushes (`:398`, off in P2), and every later check is `glClientWaitSync(…, timeout 0)` (`:548`, `:609`). The one blocking-capable call, `glGetTexImage` via `ofFbo::readToPixels` (`:334`), runs only after the fence has reported signaled (`:571-575`).
2. **[F] The stall does not follow the readback.** In P2, frames after a collect-only frame had 0 long frames out of 2,651; frames after an issue frame, with no event of their own, were long 604 of 639 times (94.5%). The readback frame is therefore not the trigger; the issue frame is.
3. **[I] The data show that the stall depends on GPU completion timing, not that the sampler's GPU *execution* costs 11–15 ms.** P2G moves the wait from the next frame's `postDraw` into `glFinish` (supplied: `postDraw` blocked p95 8.32 → 0.03 ms; `glFinish` wall p95 27.42 ms at 0.41 ms CPU). `glFinish` drains **everything** queued (scene + sampler + HUD + any earlier backlog), so it cannot say which part costs what.
4. **[F, new] FX-issue and RAW-issue frames produce the same stall magnitude.** From the P2 archive: after an FX-only issue, `postDraw` wall p50 12.94 ms (161 of 164 long); after a RAW-only issue, 12.99 ms (436 of 468 long). The FX pass does ~1.8× the texel fetches per output pixel (8×8 = 64 vs 6×6 = 36, `reduceFragment` loop, `SceneColorSampler.cpp:173-178`, block size from `:245`; sizes 1280×720 and 960×540 from the stdout logs) yet produces the same stall. **[I]** This argues against stall duration scaling with sampler-shader execution time, and is compatible with a fixed-quantum present/swap wait. It does not rule out sampler cost as the *trigger*.
5. **[I] The existing evidence cannot separate "sampler GPU cost" from "existing rendering backlog".** P0-vs-P2 is a matched on/off comparison, but sampler-on also changes the HUD (§4, B6) and adds CPU submission, readbacks and fences together.
6. **Recommendation (§7):** one experiment that needs **no production C++ change**: hold the sampler configuration fixed and remove the scene's GPU load with the already-existing `EXPERIENCE_RUNTIME_DIAG_SCENE_CONTROL=clear` switch (`ExperienceRuntime.cpp:911-913`, `:947`, `:986`). If the post-issue stall disappears, the sampler alone does not cause it and the existing GPU backlog is a necessary condition. If it persists at the same magnitude, the sampler (or its interaction with presentation) is sufficient.

---

## 2. Exact GPU command ownership map

Chronological order within one frame, as executed. "Order" is derived from the call sequence read in the files, not from class ownership. Frame segments are those of the frame-phase probe (`Rt004FramePhaseProbe.cpp:228`): update, preDraw, draw, glFinish (diagnostic only), postDraw.

| # | Operation (actual GL call or wrapper) | Owner | Caller (function, file:line) | Order in frame | GPU dependency | CPU synchronization | Evidence status |
|---|---|---|---|---|---|---|---|
| 1 | Video decode and texture upload: `ofVideoPlayer::update()` | RuntimeServices / video | `OfVideoDecoder::update`, `shared/src/video-playback/OfVideoDecoder.cpp:29-31` | **update** segment, before any draw | Writes the video texture later sampled by scene and RAW sampler | Not established. The upload inside `ofVideoPlayer` (GStreamer path) was **not traced**. | [F] call site only; the upload mechanism is **not established by the code read** |
| 2 | Frame limiter (CPU wait; no GL) | framework | `ofCoreEvents::notifyDraw`, `events/ofEvents.cpp:324-326` (`timerFps.waitNext()` before the draw event) | **preDraw** (D0 − U1 includes it, probe header) | none | Intentional sleep, 30 fps | [F] |
| 3 | `startRender`, `setupScreen` | framework | `ofAppGLFWWindow::draw`, `app/ofAppGLFWWindow.cpp:491-492`; `ofGLProgrammableRenderer::startRender`, `gl/ofGLProgrammableRenderer.cpp:86-` | start of draw | default framebuffer | state only | [F] |
| 4 | SceneFBO bind: `glBindFramebuffer` via `ofFbo::begin` → `ofGLProgrammableRenderer::begin/bind`; then `ofClear(0,0,0,0)` | ExperienceRuntime | `ExperienceRuntime::draw`, `ExperienceRuntime.cpp:977-978`; `ofGLProgrammableRenderer.cpp:1601-1616, 1641` | draw #1 | SceneFBO color texture; allocated `GL_RGBA`, `numSamples=0` (`ExperienceRuntime.cpp:476`; default arg `gl/ofFbo.h:59`) so **no MSAA resolve** exists in this path | none | [F] |
| 5 | Scene render: background draw (`drawSubsection` of the video texture, or one effect shader) and per-fragment region effects | Scene (blob-region-prototype) | `sceneManager_.drawActiveScene()` `ExperienceRuntime.cpp:989` → `BlobProductionScene::drawToCurrentTarget` `BlobProductionScene.cpp:163` → `BlobSceneCore::draw` `BlobSceneCore.cpp:103-119` → `drawBackground` `:121-154`, `VideoRegionController::draw` `VideoRegionController.cpp:162` → `VideoRegionEffectRenderer::render` `VideoRegionEffectRenderer.cpp:127` | draw #2 | Reads the video texture; per effect fragment either draws directly (`:158`) or renders into `scratchSourceFbo` (`:173-178`) then `scratchResultFbo` (`:237-247`) and draws the result (`:256`) | none visible | [F] structure. **How many regions/effect paths were active during the Pi campaign is not established by the archive.** |
| 6 | SceneFBO unbind: `glBindFramebuffer(default)`; `flagDirty` | ExperienceRuntime | `ExperienceRuntime.cpp:993`; `ofGLProgrammableRenderer::unbind`, `:1667-1677` | draw #3 | SceneFBO texture becomes sampleable | none | [F] |
| 7 | OC-004 activity probe | Scene Observability | `ExperienceRuntime.cpp:995, 1016` | draw #4 | n/a | inert: the planner unsets `EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY` / `..._ACTIVITY_PROBE` (`scripts/oc003/plan_interleaved.py`, `MANAGED`) | [F] by tooling design |
| 8 | **FX sample issue.** Pass: `linA_.begin()`; `ofClear`; reduction shader sampling the **SceneFBO texture**; `ofDrawRectangle` 160×90; `end()`. Coarse 16×9 pass is **skipped** when predominant sampling is active. Then `glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0)`; `glFlush` only if `DEFER_FLUSH` | FX SceneColorSampler (`sceneColor_`) | `onLiveFrame` `SceneColorSampler.cpp:427` → `sampleLinearGpu` `:390` → `submitLinear` `:232-254` (pass lambda `:238-250`, pass `:251`, coarse pass `:252` gated by `:394`); fence `:397`; flush `:398`; called at `ExperienceRuntime.cpp:999` | draw #5, **after** the scene pass, **before** HUD | **Reads the just-rendered SceneFBO texture** (render-to-texture dependency on #5). Writes `linA_`, RGBA16F 160×90 (`:212-224`). Fence guards everything submitted so far in the context. | Issue path has no blocking call. CPU cost measured: `submit` p50 0.42 ms (FX stats line, `02_P2_r1.stdout.log`). First issue also runs one-time `validateSource`/`setupLinearPath` (`:434, :448`). | [F] |
| 9 | FX poll / collect. `glClientWaitSync(fence, 0, 0)` each frame while pending; on signaled → `target.readToPixels(pxBuf_)` | FX SceneColorSampler | `poll` `SceneColorSampler.cpp:540`, fence query `:548`, collect `:571-575`, `collectLinear` `:327`, readback `:334`; wrapper `ofFbo::readToPixels(ofFloatPixels)` `gl/ofFbo.cpp:1085-1089` → `ofTexture::readToPixels` → **`glGetTexImage(…, GL_FLOAT, …)`** `gl/ofTexture.cpp:1269` | draw #6 | `linA_` RGBA16F texture, 160×90 → 230,400 B float (`:336`) | `glGetTexImage` is a synchronous call; by design it is reached only after the fence has signaled (comment `:334`). Measured CPU time of the call (`stats_.wait`, `:354-355`): p50 ≈ 1.5 ms. This is **CPU time of transfer + conversion after completion**, not GPU waiting. | [F]. Desktop-GL branch assumed because the Pi context reports GL 3.1 Mesa desktop (`glVersion`), not ES **[I]**; the `TARGET_OPENGLES` branch would use `glReadPixels` (`ofFbo.cpp:1093`). |
| 10 | **RAW sample issue.** Same class (`videoColor_`, `SourceKind::DecodedVideo`, `ExperienceRuntime.cpp:505`), same two-pass-capable pipeline, source = `video.currentTexture()` (the decoder's texture, 960×540, internal format `GL_RGB8` per stdout) | RAW SceneColorSampler | `sampleRawVideo` `ExperienceRuntime.cpp:1224` (tex `:1230`, `onLiveFrame` `:1256`, `poll` `:1260`), called at `:1049`; shader picks `reduceRect_` vs `reduce2D_` by texture target (`SceneColorSampler.cpp:234`) | draw #7, after FX poll, before HUD data assembly | Reads the decoder texture (written in update, #1); writes its own `linA_` | same as #8/#9. RAW phase offset half a period (`ExperienceRuntime.cpp:1249`). | [F]. Whether the decoder texture is `GL_TEXTURE_2D` or rectangle is **not logged** (unknown). |
| 11 | HUD data assembly (CPU) and HUD update | ExperienceRuntime / HudCompositor | `ExperienceRuntime.cpp:1053-1115, 1152` | draw #8 | none | none | [F] |
| 12 | HUD draw. At a 1280×720 framebuffer this is the direct path (`presentation_.isIdentity()`); the HUD's media viewport draws the **SceneFBO texture** (`HudWireframeRenderer.cpp:373`); other framebuffer sizes go through `presentationFbo_` + `presentComposedFrame` | HudCompositor | `ExperienceRuntime.cpp:1182-1188` (identity) / `:1197-1210`, `:1311-1320` (composed) | draw #9 | Samples SceneFBO texture; renders to the default framebuffer | none | [F]. The Pi window is 1280×720 (CONFIG), so the direct path applies **[I]**. The HUD also **consumes the sampler's published snapshots** (`HudWireframeRenderer.cpp:606, 630`). |
| 13 | `glFinish()` (diagnostic only, presence of `EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH`) | Frame-phase probe | `Rt004FramePhaseProbe.cpp:74, 194-199` | after the app's draw, before swap | everything submitted to the context | Blocks until the GPU is idle; that wait is the measured `glFinish` segment | [F] |
| 14 | **Swap:** `glfwSwapBuffers` | framework | `ofAppGLFWWindow::draw`, `app/ofAppGLFWWindow.cpp:523-525` (`settings.doubleBuffering`) | after draw | the default framebuffer, everything above | May block (see §3). Swap interval: default `setVerticalSync(true)` `:387`; the probe's `VSYNC=off` calls `setVerticalSync(false)` → `glfwSwapInterval(0)` (`Rt004FramePhaseProbe.cpp:107-109`; `ofAppGLFWWindow.cpp:1662-1667`). **Request only; the driver's real interval is not read back.** | [F] code; blocking location **[H]** |
| 15 | `finishRender`, loop event, `pollEvents` | framework | `ofAppGLFWWindow.cpp:530`; `ofMainLoop.cpp:152` | end of frame (with #14 forms **postDraw**) | none | `postDraw` lumps swap + finishRender + loopEvent + pollEvents (`Rt004FramePhaseProbe.h:25-40`); the probe cannot say which of the four blocks | [F] |

**What the code does not establish** (stated, not inferred from generic OpenGL): whether Mesa/Panfrost flushes on `glFenceSync`; whether switching to `linA_` and sampling the freshly rendered SceneFBO texture splits or flushes the GPU job; how many GPU jobs a frame becomes; what `glfwSwapBuffers` waits for under X11 on this stack; whether `glClientWaitSync(…, 0, 0)` flushes anything. None of that is visible above the driver.

---

## 3. Synchronization dependency diagram

Facts and hypotheses are marked. `──►` = data/ordering dependency in the code [F]; `┈┈►` = hypothesised driver behaviour [H].

```
UPDATE segment (CPU)
  video decode → video texture upload (#1) ............................ mechanism not traced

PREDRAW  (CPU sleeps in limiter, 30 fps)  [F ofEvents.cpp:324-326]

DRAW segment (CPU submits; GPU executes later, asynchronously)  [F]
  SceneFBO bind+clear (#4)
     │
     ├─► scene draw (#5) ──► [video tex] ─► SceneFBO colour
     │        (region effects may bounce through scratchSource/Result FBOs)
     ▼
  SceneFBO unbind (#6)
     │  SceneFBO texture now read by: ◄─────────────────────────────┐
     ├─► FX reduction pass → linA_ (#8)  [render-to-texture dep]     │
     │        └─► glFenceSync (#8)  ┈┈► may flush the batch? [H]      │
     ├─► (frame+1.. polls) glClientWaitSync(0) (#9) — never blocks   │
     │        └─► on signaled: glGetTexImage(linA_) (#9) — sync, CPU 1.5 ms
     ├─► RAW reduction pass (video tex → linA_) + fence (#10)         │
     └─► HUD draw → default framebuffer (#12) samples SceneFBO ◄──────┘
  [diagnostic] glFinish (#13): blocks CPU until ALL of the above is done

POSTDRAW segment
  glfwSwapBuffers (#14) ┈┈► [H] waits for the previous present / buffer release
  finishRender, loopEvent, pollEvents (#15)

Hypothesised GPU queue (never observed directly in any evidence):
  ... frame N-1 work ... | frame N: scene → FX pass → fence → RAW pass → HUD | frame N+1: ...
                                                            ▲
                      post-issue stall: observed in frame N+1's postDraw (11-15 ms, ~0.76 ms CPU,
                      one voluntary context switch) [F, from the earlier attribution analysis]
```

Separation of the three costs the Architecture asked to keep apart:

| Concept | What measures it today | Status |
|---|---|---|
| **CPU submission time** | `draw` segment: P0 p50 7.48 / p95 8.26 ms → P2 p50 8.23 / p95 11.74 ms (pooled, all rounds); sampler `submit` stat 0.4–0.8 ms | [F]. Draw wall ≈ draw CPU (rounds 1-3 of P2: 8.25 vs 8.18 ms p50): submission is CPU-bound, not blocked. The P0→P2 difference mixes sampler submission, readback CPU, snapshot publishing and HUD content (§4 B6). |
| **GPU execution time** | **Nothing on the Pi.** Only proxies: P2G `glFinish` wall p95 27.42 ms [S] (residual of *all* queued work), fence latency (below) | [F] absent. The sampler's GPU execution time is **unmeasured**. A static count only: per issue, FX ≈ 14,400 outputs × 64 fetches ≈ 0.92 M fetches (each with an sRGB EOTF), RAW ≈ 14,400 × 36 ≈ 0.52 M **[I, from code, not a time]**. |
| **CPU blocking time** | `postDraw` blocked = wall − CPU: P2 p95 7.22 ms (pooled, recomputed); stall frames 11–15 ms at ~0.76 ms CPU with one voluntary switch (earlier analysis of this archive, not recomputed this session beyond wall p50 12.9 ms after issue) | [F]. Which call inside `postDraw` blocks is unresolved. |

`readbackBytes` is not a synchronization cost. The readback is 160×90×4×4 = 230,400 B per sample (`SceneColorSampler.cpp:336`), and the frames that contain it were never followed by a stall (0 of 2,651). Byte count and stall are independent in this data.

The existing "fence latency" statistic (`stats_.latency`, `:572`) is issue-to-observed-signal **at the poll cadence of one check per frame**: FX p50 33.7 / p95 34.1 / max 84.9 ms, RAW p50 32.7 / p95 66.9 / max 77.4 ms (`02_P2_r1.stdout.log`, last stats lines). It is quantized to the 33.3 ms frame and so cannot resolve sub-frame GPU completion. It also measures in-order queue drain (a fence signals only after everything submitted before it), so it is not sampler execution time.

---

## 4. OC-003-D evidence-versus-hypothesis matrix

### 4.1 The ten preserved findings (all [S]; raw OC-003-D data not available locally)

| # | Preserved finding | Status here | What it establishes | What it does **not** establish |
|---|---|---|---|---|
| A1 | P2 frame p95 **37.37 ms** | [S] (local P2 of an earlier campaign: 37.85) | Baseline tail of the sampler-on condition | Cause of the tail |
| A2 | P2G frame p95 **46.25 ms** | [S] | Forcing a drain makes the frame **slower** | Whether the extra time is sampler GPU work or any other queued work |
| A3 | P2N frame p95 **37.89 ms** | [S] | Requesting `VSYNC=off` did not change the tail | **Not proof that vsync was disabled at the driver level.** The CONFIG line records the request (`Rt004FramePhaseProbe.cpp:116`); the swap interval is never read back. If the request had no effect on Panfrost/X11, P2N is uninformative about swap pacing. |
| A4 | P2 `postDraw` blocked p95 **8.32 ms** | [S] (local earlier campaign: 7.22) | The CPU waits in `postDraw` at the tail | Which of swap/finishRender/loopEvent/pollEvents blocks |
| A5 | P2G `postDraw` blocked p95 **0.03 ms** | [S] | The `postDraw` wait is **GPU-completion-dependent**: when the GPU is drained first, nothing is left to wait for | That the sampler is the owner of the outstanding work |
| A6 | P2G `glFinish` wall p95 **27.42 ms** | [S] | Substantial GPU work (up to ~27 ms at p95) is still queued when the CPU finishes submitting the frame | Its composition (scene vs sampler vs HUD vs backlog); the pooled p95 includes all frames |
| A7 | P2G `glFinish` CPU p95 **0.41 ms** | [S] | The `glFinish` time is **waiting, not CPU work** | n/a |
| A8 | P2 exclusive after-issue long frames **518** | [S] (local earlier campaign: 604) | Delayed stalls follow issue frames | Mechanism |
| A9 | P2G exclusive after-issue long frames **0** | [S] | Draining at the end of every frame removes the *delayed* class | That the sampler is cheap or expensive |
| A10 | P2G sampling-event long frames **3,017** | [S] | The same GPU wait reappears **inside** the event frames' own `glFinish` (relocation, plus serialization costs) | n/a |

**Exclusive vs overlapping populations.** Counts 518 and 0 (A8, A9) are *exclusive* classes of the stall analysis (a frame is in exactly one class: `event`, `no_event_after_issue`, `no_event_after_collect`, `no_event_lag2`, `quiet`; class long-counts sum to the total, asserted in `scripts/oc003/stall_analysis.py`). Count 3,017 (A10) is an *overlapping* event population ("frames with any sampling event", which includes frames that are also the frame after another event). It must not be added to, subtracted from or compared directly against 518 or 0. In the local earlier P2 data (rounds 1-3 only, where I ran this view), the overlapping view of "frames after an issue" is 2,971 frames / 792 long (26.7%), while the exclusive no-event subset is 558 / 524 (94%); the two views answer different questions.

### 4.2 Why P2G shows synchronization sensitivity without isolating sampler GPU cost

1. **[I] What P2G shows.** The post-issue stall is not intrinsic CPU work (CPU is 0.76 ms in the stall frame) and it is removed when the GPU is forced idle before the swap. So the blocked time in `postDraw` is time spent waiting for outstanding GPU work or for a present that depends on it.
2. **[I] Why that does not isolate the sampler.** `glFinish` waits for *all* commands in the context. The 27.42 ms p95 is the residual of the whole frame's GPU work (scene regions, FX pass, RAW pass, HUD, and anything still queued from earlier frames). With a 33.3 ms frame budget and ~27 ms of residual at p95, the GPU is **plausibly close to its frame budget independent of the sampler** **[H]**. In that situation any additional job, however small, can tip a frame past the present deadline. That is the "existing backlog" explanation and it is indistinguishable, in the supplied data, from "the sampler's work is itself expensive".
3. **[I] P2G also changes the system it measures.** `glFinish` removes the CPU/GPU overlap that the limiter sleep normally provides, so the frame gets slower (A2) for reasons unrelated to the sampler. The 3,017 long event frames (A10) include that effect.
4. **Missing in the supplied data [blind spot].** The `glFinish` wall distribution **conditioned on frame class** (issue frame vs quiet frame). If quiet frames show a similar ~25 ms `glFinish`, the backlog is baseline load; if issue frames are materially larger, the excess estimates the sampler's residual GPU work. `scripts/oc003/stall_analysis.py` already computes `glFinish` distributions for the `issue_frame` and `quiet` populations, so this can be extracted from the OC-003-D P2G raw data **without any new run** once that data is made available. It is the cheapest next step and should precede or accompany the experiment in §7.

### 4.3 Findings recomputed from the local archive (new in this phase)

| # | Finding | Value | Status |
|---|---|---|---|
| B1 | Sampler off (P0) has essentially no stalls | 14 long of 32,261 | [F] |
| B2 | Stall follows **issue**, not collect | after-issue (no own event): 604/639 long; after-collect: 0/2,651; lag-2: 1/2,481 | [F] |
| B3 | Stall duration is the same after an FX issue and after a RAW issue | `postDraw` wall p50 12.94 ms (n=164, 161 long) vs 12.99 ms (n=468, 436 long); both-issue frames (n=7) 23.3 ms | [F] (script `scratchpad/oc003e/stream_split.py`, not in the repo; selection: next frame must carry no event) |
| B4 | The issue frame itself is usually *not* the long one | event class: 323/7,001 long (4.6%) | [F] |
| B5 | `DEFER_FLUSH` (`glFlush` after the fence) does not remove the stall | P2F after-issue 688/732 long | [F] |
| B6 | **Confounder:** sampler-on also changes what the HUD renders | `currentHudFrameData_.sceneColor`/`sceneColorRaw` are null when disabled (`ExperienceRuntime.cpp:1077-1079, 1098-1100`); the HUD consumes them (`HudWireframeRenderer.cpp:606, 630`) | [F] code; GPU effect of the difference **unmeasured** |

B3 caveat: a fixed-size wait of ~13 ms is also what a present quantized to a 60 Hz refresh (16.7 ms) would produce **[H]**; the display mode on the Pi was not recorded in the evidence read here.

### 4.4 Competing explanations the data cannot yet separate

| Explanation | Supporting | Against / open |
|---|---|---|
| **E-cost:** sampler pass execution is long enough to delay completion | B2 (issue-linked) | B3 (FX 64 fetches vs RAW 36 fetches, same stall); P2G alone cannot isolate it |
| **E-backlog:** GPU is near budget from scene/HUD load; the sampler tips it | A6 (27 ms residual) | needs class-conditioned `glFinish` (§4.2.4) and a load-removal run (§7) |
| **E-structure:** render-target switch + sampling a just-rendered texture + fence creates a flush/serialization point (tile-renderer behaviour) | B2, B5 (flush after fence no help) | **[H]**: no driver-specific evidence exists; nothing in the repo shows how Panfrost batches this |
| **E-present:** swap/vsync scheduling on X11 quantizes a late frame to the next flip | B3 equal magnitudes; A5 | A3 (P2N no change) is only informative if the request took effect; not verified |

---

## 5. Attribution blind spots

1. **No GPU-side timing exists on the Pi** for any pass, scene or HUD. Every GPU statement is inferred from CPU-side waits.
2. **The blocking call inside `postDraw` is unlocalised.** swap, `finishRender`, loop event and `pollEvents` are one segment (`Rt004FramePhaseProbe.h:25-40`). A5 argues the wait is GPU-dependent, which points at the swap, but that is [I].
3. **`glFinish` is a coarse instrument.** It drains the whole context (§4.2) and perturbs the overlap it is measuring.
4. **P0-vs-P2 bundles several differences:** FX and RAW passes, fences, polls, readbacks, CPU averaging/predominant tracking, snapshot publishing and HUD content (B6). A single on/off contrast cannot assign cost to any one of them.
5. **FX/RAW isolation has never been run on the Pi.** The planner defines `P1` (FX only) but the archived campaign contains only P0/P2/P2F. B3 is a within-run substitute (event-conditioned), not a controlled comparison.
6. **The OC-003-D raw data (P2G, P2N) is not on this machine**, so A1–A10 are unverified and class-conditioned `glFinish` is unavailable.
7. **Swap interval is a request, not a measurement.** `Rt004FramePhaseProbe.cpp:106-116` logs the request; nothing queries the effective interval, the display refresh rate, or the X11 present path.
8. **Scene GPU load is uncharacterised.** Which blob effects and how many region FBO bounces were active (§2 row 5) is not recorded in the archive.
9. **Timer-query support on Mali-T860/Panfrost is unknown.** No extension list is logged anywhere in the evidence read; the only `GL_TIME_ELAPSED` use in the repository is the OC-004 probe, tested on desktop only (`SceneActivityProbe.cpp:183-185, 333-369`; follow-up note `docs/reviews/oc-004-scene-visual-activity-probe.md:258` says to repeat on the Pi "if the driver exposes `GL_EXT_disjoint_timer_query`").
10. **Content dependence.** The reduction shader loop has no data-dependent branch beyond the loop bound, but texture-cache and compression behaviour on a tile GPU can depend on content **[H]**; no data exists either way.
11. **Pooled statistics.** The supplied figures are pooled; per-round variation (the tail tracks per-round `notReadyPolls`, found earlier) is not in the preserved set.

---

## 6. Comparison of the six diagnostic alternatives

| # | Technique | Existing instrumentation sufficient? | Source change needed | GL/API requirement | Panfrost / Mali-T860 portability | Overhead | Risk of adding synchronization | Separates sampler execution from unrelated backlog? |
|---|---|---|---|---|---|---|---|---|
| 1 | **GPU timer queries** (`GL_TIME_ELAPSED` around each pass) | **No.** None in the sampler. Precedent: `SceneActivityProbe.cpp:183-185, 263, 333-369` (queries created at `:263`, results read at `:367-369` after the readback would have waited anyway) | **Yes**: begin/end around the passes in `submitLinear` (`SceneColorSampler.cpp:251-252`) plus result handling | `ARB_timer_query` (core 3.3) on desktop GL, or `EXT_disjoint_timer_query` on ES. The Pi context is GL **3.1** Mesa, so availability is by **extension string only** | **Unknown.** No evidence in the repo or archive. Must be enumerated first (zero-risk: one `glGetString(GL_EXTENSIONS)` token on the existing line `SceneColorSampler.cpp:71`) | Low if results are read only after the fence signals | Low if `GL_QUERY_RESULT_AVAILABLE` is polled and `GL_QUERY_RESULT` read after fence; reading too early would stall | **Best in principle** (execution time of just the sampler passes, excluding scene), **if supported**. On a tile GPU the measured interval may include overlap with other jobs **[H]**. Does not by itself explain the stall. |
| 2 | **Nonblocking sampler-pass timing** (CPU timestamps at fence signal) | **Partly.** `latencyMs` exists (`:572`) but is polled once per frame, so resolution = 33 ms | Yes to increase poll rate or add timestamps | none beyond sync objects (already present) | Same as current | Negligible | A second blocking waiter would add synchronization; polling with timeout 0 does not | **No.** In-order queue drain time, includes everything queued before the fence |
| 3 | **Matched sampler-enabled/disabled** | **Yes (coarse).** P0 vs P2 exists (B1) | none | none | n/a | none | none | **No**: bundles FX+RAW+HUD+CPU changes (§5.4). A *matched load-removal* variant is possible with existing switches (§7) |
| 4 | **FX/RAW diagnostic isolation** | **Partly.** Frame CSV has per-stream event columns (B3); planner has `P1` (FX only); RAW-only has no planner condition (`sceneColorEnabled_` and `videoColorEnabled_` are independent env switches, `ExperienceRuntime.cpp:480, 503`) | Planner/scripts only for FX-only and RAW-only conditions | none | n/a | none | none | **Partly.** Separates the two streams' contributions but not execution from backlog. B3 already suggests equal behaviour |
| 5 | **Fence submission-to-signal latency** | **Partly** (same as 2) | Yes for sub-frame resolution | sync objects (present: `syncObjects=yes`) | Fence polling works on the Pi (stdout) | Negligible | Low | **No** (in-order queue) |
| 6 | **Sampler-event correlation with later frame-phase waits** | **Yes.** `stall_analysis.py` (B2–B5) | none | none | n/a | none | none | **No**: establishes association and timing (issue → next frame), cannot attribute cost |

Assessment: alternatives 3, 4 and 6 are already exploited to their limit; alternatives 2 and 5 are structurally weak (quantization, in-order drain); alternative 1 is the only direct measurement of execution time but its availability on this GPU is unverified. The recommended experiment in §7 is therefore a *matched load-removal comparison* (a variant of 3 using existing switches) because it answers the Architecture's question, "sampler cost or existing backlog", without depending on timer-query support and without any production change. Timer queries become the right follow-up if §7 shows the sampler is sufficient on its own.

---

## 7. Exactly one recommended bounded experiment

### 7.1 Name and hypothesis

**OC-003-E1 — Scene-load removal under matched sampling.**

* **H-backlog (tested):** the post-issue `postDraw` stall requires pre-existing GPU load from scene rendering; the sampler's issue-frame work alone does not produce it.
* **H-sampler (competitor):** the stall persists with the scene's GPU work removed, i.e. the sampler's own work, its dependency on the SceneFBO/video textures, or its interaction with present is sufficient.

One controlled variable: **whether the active scene is drawn into the SceneFBO**. Sampler configuration, fences, cadence, HUD, limiter, vsync request, media and window are identical across arms.

### 7.2 Diagnostic control

No new instrumentation, no production C++ change. The control already exists: `EXPERIENCE_RUNTIME_DIAG_SCENE_CONTROL=clear` (`ExperienceRuntime.cpp:907-915` documents it; read every frame at `:947`; when on, the SceneFBO is still bound and cleared at `:977-978` but `drawActiveScene()` is skipped at `:986-990`; scene and video `update()` still run; a log line `RT-008 diagnostic … ON` is emitted at `:920`). It is not gated by the frame-phase probe. FX still samples the (black) SceneFBO; **RAW still samples the real decoded video**, so RAW's source is unchanged between arms.

### 7.3 What would change (all deferred to an authorized implementation phase; none done now)

| File | Change | Class |
|---|---|---|
| `scripts/oc003/plan_interleaved.py` | add conditions `P2C = P2 + DIAG_SCENE_CONTROL=clear` and `P0C = P0 + DIAG_SCENE_CONTROL=clear`; add the variable to `MANAGED` | tooling (script) |
| `scripts/oc003/verify_run.py` | fail-closed audit: exact expected value, single-variable pair diffs (P2→P2C, P0→P0C), require the `RT-008 diagnostic … ON` stdout line in `*C` runs and its absence elsewhere | tooling |
| `scripts/oc003/summarize_campaign.py`, `scripts/oc003/test_diag_tooling.py`, `make_package.py` manifest, `OPERATOR.md` | pairs, tests, regenerated package | tooling/docs |
| `ExperienceRuntime.cpp`, `SceneColorSampler.cpp/.h`, build system | **none** | — |

Precondition to check on the Pi at preflight: the installed binary contains the RT-008 switch. The in-run proof is the log line above; if it is absent the arm is invalid and the run stops (rebuilding would need separate authorization).

### 7.4 Expected observations

| Outcome | P2C vs P2 observation | Interpretation | Next step |
|---|---|---|---|
| **A: backlog-dependent** | after-issue exclusive long rate falls to ≤ 20% of P2's, and `postDraw` blocked p95 within 1.5× of P0C, in every round | scene GPU load is a necessary condition; sampler alone does not stall | quantify headroom: HUD-omit switch (`DIAG_HUD_OMIT=1`, `ExperienceRuntime.cpp:914, 948`), per-effect load reduction, then timer queries if supported |
| **B: sampler-sufficient** | rate ≥ 80% of P2's in every round, stall-frame `postDraw` wall p50 within ±25% of ~13 ms | stall does not depend on scene load; equal magnitude additionally suggests a fixed present/swap quantum rather than execution time | shader-workload dose-response (reduce per-fragment work, keep pass/fence structure), timer-query capability probe, swap-interval verification |
| **C: mixed** | between A and B, or direction differs between rounds | scene load modulates but is not necessary | add the HUD-omit arm; do not conclude |

Per-stream cross-check using the frame CSV's `fx_issue` / `raw_issue` columns (as in B3): RAW's source is unchanged in P2C. If RAW stalls vanish together with FX stalls, the effect is scene-load-driven regardless of sampled content. If only FX stalls vanish, the FX result is confounded by the black SceneFBO content (cache/compression, §5.10) and the outcome must be reported as inconclusive for FX.

### 7.5 Desktop verification (before any Pi time)

1. Existing tests (30 + 8 + 10) plus new negative tests: `P2C`/`P0C` plan has exactly one differing variable from `P2`/`P0`; an inherited `DIAG_SCENE_CONTROL` is scrubbed from non-`C` runs; value other than the exact string `clear` is rejected by the audit (the runtime acts only on exact `clear`, `ExperienceRuntime.cpp:918`).
2. A 6-run desktop rehearsal (P0C, P2, P2C × 2 rounds, short duration) with the real runner: `RT-008 diagnostic … ON` appears only in `*C` runs; issue/collect event counts equal across P2 and P2C (cadence unchanged); 0 GL errors; service/settings restored. Desktop timing is **not** representative of the Pi and is used only for correctness.

### 7.6 ROCK Pi measurement matrix and duration

| Item | Value |
|---|---|
| Conditions | **P0C, P2, P2C** (rotated order, `--order rotate`, supported by the planner) |
| Rounds × duration | 4 × 300 s per run → 12 runs, ≈ 60 min of runs plus runner settle/warm-up |
| Constant | Rainfall.mp4 / `media.auto.7021ad87`, 1280×720, `blob-region-prototype`, limiter on, vsync **request** default (on), `DEFER_FLUSH` **unset**, probe + frame CSV on, same installed binary and settings as the 2026-11-08 campaign |
| Why P0C | guards the harness: a cleared scene with the sampler off must show no stalls, otherwise the clear switch itself perturbs pacing |
| Sample size | P2 shows ≈ 150 post-issue stall frames per 300 s run (604 over 4 runs), enough to detect a 5× reduction per round |

### 7.7 Acceptance criteria

*Validity gates (all must pass before the outcome is interpreted):*
V1 plan and audit pass fail-closed; V2 `RT-008 diagnostic … ON` present in every `*C` run and absent in all others; V3 **P2 reproduces**: per run, after-issue long rate ≥ 0.85 and ≥ 100 long after-issue frames; V4 P0C ≤ 10 long frames per run (P0 baseline: 14 over 4 runs); V5 issue counts in P2C within 2% of P2; V6 0 GL errors, 0 fence discards/timeouts, telemetry never disabled; V7 CONFIG lines match the plan (`limiter=on`); V8 restoration verified.
*Decision:* per §7.4, evaluated per round and pooled, with the thresholds fixed before the run.

### 7.8 Failure and stop conditions

Stop and report (do not interpret) on: any V-gate failing; any GL error or fence discard; a runner abort or an unrestored service; P2 failing to reproduce (drift); thermal or power events in `preflight`/`dmesg`; the diagnostic line missing in a `*C` run; operator-observed display fault. A failed run is reported as failed, not retried silently.

### 7.9 Restoration and rollback

Env-only arms: the runner already scrubs inherited `EXPERIENCE_RUNTIME_*`, uses a private installation-settings copy with both auto-cycles off, and restores the service and watched files (`restoration.json`). No binary is replaced and nothing is deployed outside the package procedure. Rollback is "stop the runner; restore the service; delete the run directory". Production settings are never touched.

### 7.10 Ownership and authorization dependencies

| Step | Owner | Authorization needed |
|---|---|---|
| Approve E1 and its pre-registered thresholds | Architecture Manager | explicit |
| Tooling implementation in `scripts/oc003` + docs | ExperienceRuntime / Scene Observability | Architecture (this report is not an authorization to implement) |
| Package transfer, Pi preflight, 12-run campaign | ROCK Pi Runtime & Performance (hardware owner) | separate hardware-owner authorization |
| Any production C++ change (not required by E1) | ExperienceRuntime | separate written proposal |

---

## 8. Ownership, verification, stop conditions and rollback (summary)

* **Ownership:** sampler and runtime code: ExperienceRuntime / Scene Observability. Pi execution and the installed service: ROCK Pi Runtime & Performance. Decision on direction and thresholds: Architecture Manager. No scene, HUD or framework ownership boundary is touched by E1.
* **Verification:** desktop correctness only (§7.5). Hardware conclusions only from the Pi runs, under the validity gates (§7.7).
* **Stop conditions:** §7.8. **Rollback:** §7.9.
* **Out of scope and unchanged:** sampling cadence, scheduling, formats, readback path, GPU synchronization, SceneManager, HUD, framework. P2G's `glFinish` is a measurement tool only and is **not** recommended as a production fix: it serializes CPU and GPU, raised the frame p95 from 37.37 to 46.25 ms [S], and moves the wait rather than removing it.

---

## 9. Architecture decision request

1. **Approve OC-003-E1** (§7) as the single next experiment, with the pre-registered outcome table (§7.4) and validity gates (§7.7).
2. **Authorize the tooling implementation** (scripts, tests, docs, regenerated package; no production C++) as a separate step, and separately the hardware owner's Pi run.
3. **Make the OC-003-D raw P2G/P2N archive available** on the development machine so the class-conditioned `glFinish` distribution (§4.2.4) can be extracted with the existing tool. This is analysis only and costs no hardware time; its result may change the reading of E1.
4. **Decide whether to request a one-token extension-string log** (timer-query availability) on the existing deferred-readback log line (`SceneColorSampler.cpp:71`) at the next authorized sampler edit. Not part of E1; it makes alternative 1 plannable.
5. **Confirm** that P2N must not be cited as evidence that vsync was disabled, and that production `glFinish` is not a candidate fix.

**Not claimed:** the stall mechanism is not identified; OC-003 is not closed; this report asserts no status change.

---

### Appendix — how the recomputed figures were produced

* `python3 -I scripts/oc003/stall_analysis.py ~/Downloads/oc003-evidence/oc003_attr_20261108_1330/*_<COND>_r*.frames.csv --json` for COND in P0, P2, P2F (4 files each).
* FX-vs-RAW split: `scratchpad/oc003e/stream_split.py` (read-only, not part of the repository): for each pair of consecutive non-excluded frames with an issue in the first and `ev == 0` in the second, tally `total_ms > 40` by `fx_issue` / `raw_issue`.
* Sampler stat lines (`submit`, `wait`, `latencyMs`, `notReady`) from `02_P2_r1.stdout.log` (last `stream=FX` / `stream=RAW` lines).
* Hardware/context strings from `02_P2_r1.phase.log` (CONFIG) and `campaign/preflight.json`.
