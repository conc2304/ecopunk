# OC-003B — Shared Video RAW Texture Contract Confirmation

Owner: Shared Video. Mode: source inspection only. No code, contract, scene, HUD, skin or decoder lifecycle was changed; RAW production sampling is not enabled; nothing staged, committed or pushed; no clones or worktrees created. No builds or tests were run (nothing was modified).
Written for: Architecture (OC-003B conditional accept follow-up) and the ExperienceRuntime owner implementing RAW sampling.
Builds on: [oc-003b-raw-video-reference-semantic-contract.md](oc-003b-raw-video-reference-semantic-contract.md).

Classification used throughout: **Confirmed** (read in source), **Platform-dependent** (differs by backend; source read, behaviour not run), **Requires runtime measurement**.

## 1. Source files and paths inspected

Repository: `shared/src/video-playback/VideoPlaybackService.{h,cpp}` (`currentTexture` :247, `attemptSelection` :158, `update` :204, `status` :273), `OfVideoDecoder.{h,cpp}`, `IVideoDecoder.h`, `VideoPlaybackStatus.h`; `sketches/experience_runtime/src/SceneColorSampler.{h,cpp}` (`validateSource` :63, `submitLinear` :168, shader selection :99/:170), `ExperienceRuntime.cpp` (`establishGlobalRenderingBaseline` :318, generation bump :823, FX sample :856).
openFrameworks 0.12.1 (`libs/openFrameworks/video`): `ofVideoPlayer.cpp` (`load` :152, `update` :280, `close` :318, `getTexture` :228, `isLoaded` :572), `ofAVFoundationPlayer.mm` (`getTexturePtr` :385, `initTextureCache` :402, `killTexture`), `ofGstVideoPlayer.cpp` (`getTexturePtr` :436), `ofGstUtils.cpp` (`ofGstVideoUtils::getTexture` :905), `gl/ofTexture.cpp` (arb-tex switch).

## 2. Ready / Degraded eligibility matrix

| Service state | Reachable in current code? | `currentTexture()` | RAW eligible |
|---|---|---|---|
| Ready | Yes (set after a successful `activateCandidate`, :188) | non-null if the decoder reports an allocated texture | **Yes, plus the conditions below** |
| Degraded | **No: the service never assigns it** (only Unavailable, Ready, Failed are assigned) | would be non-null (accessor allows it) | No (rule stays, for if it ever becomes reachable) |
| Loading | **No: never assigned.** Activation is synchronous inside `attemptSelection`; no draw runs mid-activation (single thread) | null once health is not Ready/Degraded | No |
| Failed / Unavailable | Yes | `nullptr` (`:248`) | No |

Confirmed. Behaviour notes (Confirmed unless stated):
- **Media change:** `load()` does close, then load, then `play()` synchronously. A failed candidate leaves the player closed and `isLoaded()` false, so `OfVideoDecoder::getTexture()` returns null; all-failed gives Failed.
- **Pause:** the service has no public pause; `OfVideoDecoder` only pauses before `close()`. No paused-playback state exists to classify.
- **Scene transitions** do not touch the decoder.
- **A texture can be allocated while holding a previous or placeholder frame** (Confirmed in both backends, differing causes):
  - AVFoundation: `videoTexture` is a member of the player, never freed on `close()` (`killTexture` only releases the CV reference), and `ofVideoPlayer::close()` clears `tex` but does **not** reset `playerTex`. After a switch, `getTexture()` can still return the previous media's texture object until the first new frame refreshes it.
  - GStreamer: `ofVideoPlayer::load()` allocates `tex[0]` from the player's pixel buffer straight away, before any frame has been decoded, so the texture exists but holds whatever the buffer contains (not a decoded frame of the new media).
- `ofVideoPlayer::isLoaded()` reads `tex[0]` when `bUseTexture`; on an empty `tex` vector this is an out-of-range access. `OfVideoDecoder::getTexture()` calls `isLoaded()` first, so this is only reachable between `close()` and a successful `load()`, which no caller observes today. Noted as a fragility; not changed.

**Recommended eligibility predicate** (all must hold at the sample point; never a non-null pointer alone):

`status().health == Ready` AND `status().mediaId` present AND `currentTexture() != nullptr` AND `sourceSize()` > 0 and equals the texture's width/height AND **a new decoded frame has been observed since the current activation** (latched from `isFrameNew()`, polled every frame after the service update, because `isFrameNew()` is true for one update cycle only).

The latch closes the "previous or placeholder frame" hole above. Until it is set the RAW state is Unavailable (or a gap if a sample was due), never a measurement.

## 3. Decoder texture characteristics

| Property | Desktop macOS (AVFoundation) | Pi-class Linux (GStreamer) | Class |
|---|---|---|---|
| Who owns the `ofTexture` | `ofAVFoundationPlayer::videoTexture` (external GL name from the CVOpenGL texture cache) | `ofVideoPlayer::tex[0]` (allocated from pixels, `loadData` per new frame); `ofGstVideoUtils::getTexture()` returns null unless `OF_USE_GST_GL` | Confirmed (source); OF_USE_GST_GL build flag on the Pi not checked: Platform-dependent |
| Target the `ofTexture` reports | `GL_TEXTURE_2D` (the runtime calls `ofDisableArbTex()` globally, `ExperienceRuntime.cpp:331`) | `GL_TEXTURE_2D` (same) | Confirmed |
| Target of the underlying GL name | Cache textures on macOS are normally rectangle-target IOSurface textures; `ofTexture` is not told | n/a (plain 2D) | **Requires runtime measurement** |
| Internal format reported | `GL_RGBA` (`videoTexture.allocate(w, h, GL_RGBA)`) | from the pixel format: the repo never calls `setPixelFormat`, `ofVideoPlayer` defaults to `OF_PIXELS_RGB`, so `GL_RGB`/`GL_RGB8` | Confirmed (source); Pi format Platform-dependent |
| Channel order / alpha | cache texture is BGRA in memory; GL returns RGB(A) to the shader; alpha expected opaque | RGB, no alpha (sampled alpha = 1) | Platform-dependent |
| Transfer / colour | not stated by OF; decoder converts YUV to RGB; assumed display-encoded (sRGB-like) as the FX sampler assumes for the SceneFBO | same assumption; matrix chosen by GStreamer caps | **Assumption**; Requires runtime measurement (RAW vs a CPU reference on real clips) |
| Size / orientation | clip size (`getWidth/Height`); orientation irrelevant to an average | same | Confirmed |
| Update | refreshed when `getTexturePtr()` runs on a new frame (`bUpdateTexture`); GL name changes each refresh | `tex[0].loadData(pixels)` per new frame (CPU copy); reallocated if size or format changes | Confirmed |
| Identity across media changes | `videoTexture` object persists; its external GL name is replaced | `tex` is cleared on `close()` and reallocated on `load()`; GL name changes, the C++ address may or may not | Confirmed |

Consequence: the texture object, its GL name and its dimensions can all change at a media change, and on macOS the GL name changes every new frame. Nothing about the texture may be cached across frames.

> **Correction from desktop measurement (OC-003B implementation).** The macOS texture measured at runtime is `GL_TEXTURE_2D`, internal format `GL_RGB8` (0x8051), constant GL name, 960x540 matching `sourceSize()`. It is the `ofVideoPlayer`-owned texture, not the AVFoundation texture-cache `videoTexture` described in the macOS column above; the cache path is not the one in use. Consequently the desktop texture is RGB, not RGBA, and RAW validation must accept RGB8 on desktop too (implemented). The macOS "target of the underlying GL name" and "texture object persists" rows above do not apply to this build.

## 4. Borrowed texture ownership and lifetime

All Confirmed unless noted.
- `currentTexture()` returns a non-owning pointer to a player-owned texture; the service owns the player (`decoder_`), the player owns the texture. The caller must not store it, free it or change its parameters.
- **Valid** from the frame it is read until the next `VideoPlaybackService::update()` (next decoder update or next `attemptSelection`, which closes the player and clears `tex`).
- **Invalidated by:** `load()`/`close()`/`shutdown()` of the decoder (any media change, automatic advance, `next`/`previous`/`advanceAtBoundary`), and, on macOS, by the next frame refresh (new GL name; the previous CV texture is released inside `initTextureCache`).
- Another operation can replace or release it: yes, but only on the main thread in `update()`, never concurrently with a draw. The runtime is single threaded for GL and for `ofVideoPlayer::update`; GStreamer's decode thread hands pixels over inside `update()`.
- **Reacquire every frame**, at the sample point, from `currentTexture()`.
- **Read-only use as a sampler input** needs no ownership change, no copy and no extra GPU synchronisation, because the reduction is issued on the same GL context and command stream. With the Deferred readback mode the source texture is touched only by the draw issued in that frame; only the small reduction target is read back later, so the borrowed texture is not used after the frame.

## 5. Safe sampling point and GL context

- **Sample point:** in `ExperienceRuntime::draw`, next to the FX `onLiveFrame` call (`ExperienceRuntime.cpp:856`), after `update()` has run `runtimeServices_.update` (which drives the service and the decoder). No decoder update runs between that update and the draw, so the texture handed out is the one the scene draws from. Sample **before** any later code can trigger a media change.
- **Context:** the same single main-thread GL context that performs the decoder's texture upload; no shared-context or cross-thread access is involved. Confirmed.
- **Frame-new latch:** poll `isFrameNew()` once per frame after the service update (in `update()` or at the top of the draw, before sampling) so the one-cycle flag is not missed. Requires a read-only accessor already present (`VideoPlaybackService::isFrameNew()`).

## 6. Media and scene generation, stale-result rejection

`status().mediaId` alone is **not** sufficient: it is identical across repeat visits to the same video, and across a reload of the same item (for example a single-item catalog, or automatic advance landing on the same index). `VideoPlaybackStatus` has no counter and its schema must stay frozen.

Smallest reliable strategy (recommended):
1. **Additive, read-only service accessor:** a monotonically increasing activation counter on `VideoPlaybackService` (incremented in the successful branch of `attemptSelection`, where health becomes Ready). It is not a status field, owns no state beyond one integer, and changes no selection or playback behaviour. This is the one public-API addition this report recommends (see section 10); it is additive and does not require an ownership change.
2. **Fallback if Shared Video declines the accessor:** the runtime derives its own `mediaGeneration` from changes in `status().mediaId`, plus `selectionOrigin` changes and a playback-position wrap or reset. This is **incomplete**: a same-media reselect with no position reset is not detected. It must be labelled as such.
3. The RAW sampler's generation is a runtime-side counter `rawGeneration` that bumps on **either** a media activation change or a scene activation (`sceneColorGeneration_`). `SceneColorSampler::beginGeneration` already clears history and drops an in-flight readback, so a queued GPU readback that completes after a media or scene change is discarded (this is the existing generation-drop path, `droppedGeneration`).
4. Leaving Ready (Failed/Unavailable) ends the RAW generation: no new samples; state becomes Unavailable after history is cleared.
5. A scene transition independent of media: `rawGeneration` bumps with the scene generation per the OC-003B addendum, so both series restart together; the media generation (`mediaGeneration`) is unchanged and reported separately.

## 7. Compatibility findings for the existing reduction path

| Question | Finding | Class |
|---|---|---|
| Can the reduction shader sample the decoder texture target? | Yes for `GL_TEXTURE_2D` (`reduce2D_`); the sampler picks rect vs 2D from `texData.textureTarget` (`SceneColorSampler.cpp:170`), which will report 2D. If the macOS cache name is really a rectangle texture, the 2D path samples the wrong target. The runtime's existing scenes draw this texture with `sampler2D` shaders today, which suggests it works, but this is not proven for the sampler. | **Requires runtime measurement** (log the bound target) |
| Pixel format adaptation | `validateSource` accepts only `GL_RGBA`/`GL_RGBA8` and framebuffer-sRGB off. Desktop `videoTexture` is `GL_RGBA`: accepted. Pi-class `ofVideoPlayer` RGB textures are `GL_RGB`/`GL_RGB8`: **rejected, every sample would be a gap**. RAW needs `validateSource` extended (or a parameter) to accept RGB (opaque) for a video source. | Confirmed (source) |
| Alpha handling | The shader composites over black (`rgb * a`), a no-op for opaque video. RGB textures read alpha = 1. | Confirmed |
| Independent schedules | Yes. A second `SceneColorSampler` instance has its own series, due-time (`markIssued`), readback mode and statistics; FX and RAW share nothing. | Confirmed |
| Desktop GL vs GLES | The linear-light GPU path is desktop only; under `TARGET_OPENGLES` `setupLinearPath()` returns false and the 8-bit encoded halving chain with CPU linearisation is used (documented lower accuracy). Async/PBO is unavailable on GLES2; Sync or Deferred apply. Which GL profile the Pi build actually runs was not checked. | Platform-dependent |
| Incremental cost | One more reduction chain per sample (FX measured about 3.8 ms average and 11.7 ms max per sample, desktop, mostly CPU submission, at 2 Hz; GPU memory for a second 160x90 RGBA16F + two 16x9 targets). Stagger RAW and FX by half a period so they never land in the same frame. Pi unmeasured. | **Requires runtime measurement** |
| Unavailable/stale RAW without contaminating FX | Separate sampler, series, publisher and snapshot; a RAW gap, Unavailable or Stale never writes into the FX series, and the FX snapshot is untouched. The HUD already treats RAW absence independently (`OrbitConsoleModel` raw slot). | Confirmed |

## 8. Remaining measurements (none performed here)

1. Log, on desktop, `currentTexture()` target, internal format, size, and the GL name over several frames and a media change.
2. Same on the Pi: whether `OF_USE_GST_GL` is defined, the texture format, and whether `validateSource` rejects it today.
3. RAW luminance vs a CPU reference on real decoded frames, both platforms (accuracy and the transfer-encoding assumption).
4. Per-sample cost (submit, wait, convert, push) for a second sampler on desktop and the Pi, sync vs deferred; frame-time spike at 2 Hz with both samplers; GPU memory.
5. A forced media change, a forced failed load, and a same-media reselect, confirming generation bumps and that no in-flight readback is accepted.
6. First-frame behaviour after activation: how many frames pass before `isFrameNew()` is true on each backend, and what the texture holds meanwhile.

## 9. Proposed additive OC-003B contract language

> **RAW texture eligibility.** The RAW source `runtime.scene_color.raw` may sample `VideoPlaybackService::currentTexture()` only when (a) `status().health` is Ready, (b) `status().mediaId` is present, (c) the returned pointer is non-null and its dimensions match `sourceSize()`, and (d) at least one new decoded frame has been observed since the current media activation. A non-null pointer alone never establishes eligibility. Degraded and Loading are not eligible.
>
> **Borrowed texture.** The texture is a read-only borrow, valid only for the frame in which it was acquired. It must be re-acquired each frame at the sample point and never cached, stored, retargeted or modified. Sampling occurs on the main GL context after the service update and before any further playback operation. The decoder's ownership, lifecycle and selection authority are unchanged.
>
> **Format.** The sampler accepts an opaque 8-bit RGB or RGBA 2D texture. Unsupported formats record a gap. Transfer encoding is assumed display-encoded (sRGB-like); this is an assumption, not a decoder guarantee.
>
> **Generation.** RAW samples belong to a runtime-side generation that advances on every media activation and every scene activation. The media identity of a generation is `(mediaId, activation counter)`; `mediaId` alone does not distinguish repeat visits. In-flight readbacks from a prior generation are discarded.
>
> **Unavailable/stale** RAW is represented only in the RAW snapshot; the FX snapshot is never altered.

## 10. Narrow ExperienceRuntime implementation recommendations

1. Add a second `SceneColorSampler`, fed only from the eligibility predicate in section 2, at the FX sample point, behind `EXPERIENCE_RUNTIME_VIDEO_COLOR` (default off until section 8 is done).
2. Latch `isFrameNew()` per frame and reset the latch on every media activation.
3. Maintain `rawGeneration` (media activation counter + scene generation) and call `beginGeneration` on change.
4. Let `validateSource` accept `GL_RGB`/`GL_RGB8` for the RAW instance (a parameter or constructor option); keep FX validation strict.
5. Stagger RAW and FX sampling by half a period; use Deferred readback.
6. Reacquire the texture every frame; do not keep a pointer or GL name between frames.
7. **API change recommended (additive, minor):** a read-only activation counter on `VideoPlaybackService` for section 6. This is the only public-API addition suggested. It changes no ownership or selection behaviour and is therefore not an escalation trigger by itself; Shared Video should confirm it. If declined, use fallback 2 in section 6 and accept its documented gap.

## Completion report

1. **Summary:** Ready is the only practically reachable eligible state (Degraded/Loading are never assigned); the decoder texture is `GL_TEXTURE_2D`-reported, `GL_RGBA` on macOS and RGB on the Pi path; the sampler would reject the Pi texture as written; media identity needs an additive activation counter; borrowed use is safe on the main GL context when re-acquired each frame and gated on a decoded-frame latch.
2. **Files inspected:** section 1.
3. **Files changed:** this report only.
4. **Tests/builds run:** none (read-only inspection).
5. **Results:** findings classified Confirmed / Platform-dependent / Requires runtime measurement above.
6. **Deviations from prompt:** none. The Pi GStreamer behaviour was read from source, not run on hardware.
7. **Newly discovered risks:** `ofVideoPlayer::close()` leaves `playerTex` and the AVFoundation `videoTexture` pointing at the previous media until the first new frame; GL target of the macOS cache name vs `ofTexture`'s reported 2D is unverified; RGB textures fail the current `validateSource`; `ofVideoPlayer::isLoaded()` touches `tex[0]` on a possibly empty vector (not reachable today).
8. **Contract changes requested:** one additive read-only accessor on `VideoPlaybackService` (activation counter). No change to `VideoPlaybackStatus`, ownership, decoder lifecycle or specialized playback.
9. **Recommended next step:** Shared Video confirms the accessor, then ExperienceRuntime implements section 10 behind the off-by-default switch and runs the section 8 measurements on desktop and the Pi.

```text
WORK ITEM: OC-003B (Shared Video RAW texture contract confirmation)
PREVIOUS STATUS: Conditional Accept (per the brief)
NEW STATUS: inspection complete, awaiting Architecture review (not ACCEPTED/CLOSED)

WHAT CHANGED: one documentation file added; no code
WHAT WAS PROVEN: eligibility states, texture ownership/lifetime and format behaviour as read in the sources listed in section 1
WHAT WAS NOT PROVEN: runtime texture target on macOS, Pi texture format and GL profile, transfer-encoding accuracy, sampler cost, first-frame behaviour

NEW EVIDENCE: this report
NEW RISKS: see completion report item 7
NEW BLOCKERS: none for the contract; implementation is blocked on Shared Video confirming the accessor (or accepting the fallback)

NEXT ACTION: Shared Video decision on the accessor; ExperienceRuntime implementation behind the off-by-default switch
NEXT GATE: Architecture acceptance of the contract language in section 9

REGISTRY UPDATE REQUIRED: NO
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: YES
```
