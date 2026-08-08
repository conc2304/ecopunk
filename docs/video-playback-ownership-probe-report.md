# Video Playback Current-to-Target Ownership Probe — EcopunkVideoCollage

**Scope**: the six approved runtime scenes only — `blob-region-prototype`, `contour-portrait`, `temporal-fields`, `fragment-trail`, `quadrant-crosshair`, `blueprint_emergence` — plus `RPVideoSampler` (radar-effects-gallery/radar-pulse) and `shader-effect-debugger`'s inline `ofVideoPlayer` as non-runtime precedents only. This is Phase 2 of `docs/Ecopunk-HUD-System-Master-Roadmap.md` ("Shared Video Playback Service Investigation and Design").

**Method**: static code inspection only (no runtime instrumentation, no Pi hardware). Every claim below is either a code fact (with file:line evidence), an explicit recommendation, or marked `Unverified`/`Requires runtime experiment`/`Requires Architecture review`. No production code, media, symlinks, or shared contracts were modified.

**Boundaries observed**: `shared/src/scene/SceneContract.h` was read but not edited. No scene source was edited. No media was moved. This document is the only file created.

---

## A. Executive Summary

1. **There is no shared video-playback service today.** Five different, structurally incompatible playback wrappers exist (`VideoSampler`, `TimeOffsetVideoBuffer` (shared) + its `fragment-trail` fork, `VideoSystem`, `ContourSource`), each with its own media-scanning, selection, and lifecycle logic, exactly as `docs/Ecopunk-HUD-System-Master-Roadmap.md:497-500` frames the problem. `shared/src/scene/SceneContract.h:120-125` (`SceneServices::sharedMediaRoot`) and `Scene-HUD-Contract-v1.md §3` already anticipate a single shared media root and defer the video-wrapper question explicitly ("not centralizing the video-player wrapper... without proven need" — `Scene-HUD-Contract-v1.md:37`; "Video-player wrapper unification — still Open" — `Scene-HUD-Contract-v1.md:350`).

2. **"Standardized video playback" (DEC-006) is not close to true today.** Of the six runtime scenes, only `quadrant-crosshair` has a manual **next** command; **none** has a manual **previous**, even though `SceneCommand::PreviousMedia` is already frozen in `shared/src/scene/SceneContract.h:85`. `fragment-trail` never calls `advanceToNextMedia()` at all — its buffer plays one clip for the life of the process. This is a direct, code-confirmed conflict between the frozen contract and current scene behavior (see §D and §I).

3. **The de facto canonical media root already exists by accident, not by design**: `sketches/blueprint_emergence/bin/data/media/` is the only directory in the repo holding real physical media files (27 `.mp4`s). Every other runtime scene's local `media`/`sharedMedia` folder is empty and reaches those files only through symlinks — some whole-directory (`temporal-fields`, `quadrant-crosshair`), some per-file (`blob-region-prototype`, `fragment-trail`), one path-fallback (`contour-portrait`). `ExperienceRuntime.cpp:44` already hardcodes a *different*, not-yet-real path (`"assets/shared/media/"`, matching `Scene-HUD-Contract-v1.md §12`'s canonical asset tree) that does not exist anywhere on disk today — see §F.

4. **The decoder/temporal-history boundary is already clean.** `TimeOffsetVideoBuffer` cleanly separates "the live decode" (`getRawVideoTexture()`/`getRawVideoPixels()`) from "the buffered history" (`getPlayheadTexture()`), and `blob-region-prototype` already proves the live-only path works standalone (it uses `TimeOffsetVideoBuffer` purely for `hasMedia()`/`getRawVideoPixels()` bookkeeping with a 2-second history it never queries via playheads). This means a shared decoder service is structurally compatible with `temporal-fields`' and `fragment-trail`'s history needs without touching `TimeOffsetVideoBuffer`'s internals — see §E.

5. **Two scenes are structurally different enough to need real design work, not just an adapter**: `contour-portrait`'s `ContourSource` is the only wrapper that must also support a static image and (in code, unused in the runtime target) a live camera — a decoder abstraction has to tolerate a non-video mode without leaking video-specific assumptions. `quadrant-crosshair`'s `VideoSystem` is entangled with a Pi-conditional color-grade shader pass (`#ifndef PLATFORM_PI`) that currently returns a *different texture object* (`fboAdjusted`) than the raw decode — any shared texture-access contract needs to account for a per-scene post-process step sitting between "decoded frame" and "what the scene actually draws."

6. **Recommended direction** (detail in §H/§I): introduce a `VideoPlaybackService` that owns playlist/selection/hold/no-repeat/status for the "continuous decode" scenes (`blueprint_emergence`, `blob-region-prototype`, `quadrant-crosshair`), keep `TimeOffsetVideoBuffer` exactly as-is as a **consumer** of that service's decoded frame rather than rewriting it, and treat `contour-portrait` and `fragment-trail` as second-wave migrations gated on prerequisites this report documents (mode-abstraction design review for Contour; local-fork graduation for Fragment, which `CLAUDE.md` already flags as deliberately un-migrated for shaders and is in the same boat for playback). This mirrors `Scene-HUD-Contract-v1.md §14`'s own two-stage, adapter-first migration philosophy.

---

## B. Wrapper Inventory

### B.1 `VideoSampler` — `shared/src/VideoSampler.h` / `.cpp`

- **Owning location**: `shared/src/` (shared), consumed by `blueprint_emergence` only among runtime scenes; also consumed by `radar-effects-gallery`'s `RPRevealMask.h` include (unused — see below) and `temporal-fields/src/TFImageCycler.h` (different, unrelated class — a false-positive grep hit, confirmed by inspection: `TFImageCycler` does not include or use `VideoSampler`).
- **Owns an `ofVideoPlayer`**: yes, one, privately (`VideoSampler.h:54`).
- **Setup/load API**: `setup(mediaPath)` scans a folder for `.mp4` (`VideoSampler.cpp:15-30`); `selectVideoForCycle()` advances round-robin and calls `player.load()`/`play()` (`VideoSampler.cpp:32-44`).
- **Update API**: `update()` — drives `player.update()` plus a seek-request state machine (`IDLE`/`SEEKING`) for `requestCapture()` (`VideoSampler.cpp:59-98`).
- **Draw/texture API**: `getTexture()` (`VideoSampler.h:38`).
- **Pixel API**: `getPixels()` (`VideoSampler.h:39`), plus an async `requestCapture(normalizedOffset, callback)` that seeks, waits up to `SETTLE_FRAMES_MAX` (60) frames, and calls back with a still-frame `ofPixels` (`VideoSampler.cpp:46-52`, `66-97`) — this is a **seek-and-freeze single-still-capture** pattern, structurally the opposite of `TimeOffsetVideoBuffer`.
- **Timing API**: none exposed (no position/duration/progress getter).
- **File-selection API**: `selectVideoForCycle()` only — sequential round-robin (`currentFileIndex = (currentFileIndex + 1) % mediaFiles.size()`), no shuffle, no manual previous, no no-immediate-repeat logic beyond "next in a fixed list."
- **Shutdown behavior**: none explicit; `cancelPending()` drops queued captures without destroying the player (`VideoSampler.cpp:54-57`) — must be called by the owner before destroying anything a pending callback would touch (documented hazard, `VideoSampler.h:29-32`).
- **Hidden assumptions**: none GL-specific beyond the repo-wide `ofDisableArbTex()` convention (not enforced by this class itself — the *caller* must have called it, see `blueprint_emergence/src/ofApp.cpp:10-21`'s comment). `player.play()` is called even when logically "paused," with a comment explaining paused mode never decodes frames on seek under macOS AVFoundation (`VideoSampler.cpp:41`) — a real, documented platform quirk future replacements must preserve or re-verify on Pi.

### B.2 `TimeOffsetVideoBuffer` (shared) — `shared/src/TimeOffsetVideoBuffer.h` / `.cpp`

- **Owning location**: `shared/src/`. Consumers among runtime scenes: `blob-region-prototype`, `temporal-fields` (heavily — 9 pattern classes + background layer + HUD layer all take a `TimeOffsetVideoBuffer*`).
- **Owns an `ofVideoPlayer`**: yes, one, privately (`TimeOffsetVideoBuffer.h:148`). It directly calls `player.update()`/`.load()`/`.close()`/`.play()`/`.setLoopState()` — full ownership, not a wrapper-of-a-wrapper.
- **Setup/load API**: `setup(mediaFolderPath, Settings)` — scans `.mp4`s, deterministically sorts for scan-stability then Fisher-Yates shuffles for playback order, loads file 0 (`TimeOffsetVideoBuffer.cpp:8-50`).
- **Update API**: `update(dt)` — advances the live decode, pushes a downscaled frame into the history deque on every new frame, trims history to `maxHistorySeconds * assumedSourceFps`, advances/settles ramping playheads (`TimeOffsetVideoBuffer.cpp:108-152`).
- **Draw/texture API**: `getPlayheadTexture(playheadIndex)` (buffered, quantized, uploaded once/frame on demand) and `getRawVideoTexture()` (the live decode, full-res, bypasses history) (`TimeOffsetVideoBuffer.h:89-96`, `.cpp:176-194`).
- **Pixel API**: `getRawVideoPixels()` (`TimeOffsetVideoBuffer.h:106`) — CPU pixels of the live decode only; **no pixel accessor exists for history frames** (they exist as `ofPixels` internally in `history`, `TimeOffsetVideoBuffer.h:163`, but nothing public exposes them — see §E).
- **Timing API**: no `getPosition()`/`getDuration()`/progress accessor. `isMediaAdvanceEligible()` (`TimeOffsetVideoBuffer.cpp:101-106`) is the closest thing — a boolean gate, not a numeric progress value.
- **File-selection API**: `advanceToNextMedia()` — wraps to next in the shuffled playlist, reshuffles at lap end with an anti-repeat swap at the seam, clears history first (`TimeOffsetVideoBuffer.cpp:59-92`). **No previous.** No manual/immediate selection API at all — advancement is caller-gated on `isMediaAdvanceEligible()` (min 30s **and** min 4 loops, `TimeOffsetVideoBuffer.h:42-43`), i.e. this class enforces a hold-like floor but has no concept of "the operator pressed next right now, override the floor."
- **Shutdown behavior**: none explicit — no `close()`/`shutdown()` method; relies on destructor/process exit. `advanceToNextMedia()` does call `player.close()` before loading the next file (`TimeOffsetVideoBuffer.cpp:80`).
- **Hidden assumptions**: `ofDisableArbTex()` must have been called by the owning `ofApp` before this class allocates any playhead texture, same convention as `VideoSampler` (not self-enforced). History is CPU-resident `ofPixels`, texture upload happens lazily per playhead per frame — a real, load-bearing GPU-memory design decision documented in the class's own header comment (`TimeOffsetVideoBuffer.h:23-29`).

### B.3 `TimeOffsetVideoBuffer` (fragment-trail local fork) — `sketches/fragment-trail/src/TimeOffsetVideoBuffer.h` / `.cpp`

- **Owning location**: sketch-local, `sketches/fragment-trail/src/`. Not shared.
- **Drift confirmed**: `.cpp` is byte-identical to `shared/src/TimeOffsetVideoBuffer.cpp` (`diff` exit 0). `.h` is **not** identical — the fork is missing `getRawVideoPixels()` (added to the shared header after the fork was taken; `diff` shows a 10-line addition present only in `shared/src/TimeOffsetVideoBuffer.h:98-107`). This is exactly the kind of silent fork-drift `CLAUDE.md` warns about for the shader/effect system, now independently confirmed for playback.
- **Consumers**: `FTBackgroundLayer`, `FTFragment`, `FTFragmentPool` (all take a pointer/reference to this local class, not the shared one — a real type-identity fork, not just a copy that happens to be unused).
- **Behavioral difference from shared class in practice**: `fragment-trail/src/ofApp.cpp` never calls `advanceToNextMedia()` anywhere in the sketch (`grep -rn "advanceToNextMedia" sketches/fragment-trail/src` finds only the declaration/comments, no call site). Combined with no manual next/previous command, **fragment-trail plays exactly one clip (the first of its shuffled order) for the entire process lifetime** — a materially different runtime behavior from `temporal-fields`, which shares the same class shape but *does* advance on pattern switches.
- **`numPlayheads` override**: 6, matching the shared default (`sketches/fragment-trail/src/ofApp.h:53`, `ofApp.cpp:42`).

### B.4 `VideoSystem` — `sketches/quadrant-crosshair/src/VideoSystem.h` / `.cpp`

- **Owning location**: sketch-local, `quadrant-crosshair` only.
- **Owns an `ofVideoPlayer`**: yes, one, privately (`VideoSystem.h:32`), **plus** an `ofFbo`/`ofShader` post-process pass (`fboAdjusted`/`adjustShader`) that is not part of any other wrapper in the repo.
- **Setup/load API**: `setup(mediaPath)` scans `.mp4`, builds a shuffled playlist, loads playlist[0] (`VideoSystem.cpp:6-18`).
- **Update API**: `update()` — `player.update()`, then (macOS-only, `#ifndef PLATFORM_PI`) re-renders the live frame through a saturation/contrast/brightness color-grade shader into `fboAdjusted` whenever a new frame arrives (`VideoSystem.cpp:93-114`); also runs its own loop/advance state machine (see below).
- **Draw/texture API**: `getTexture()` — returns `fboAdjusted.getTexture()` on non-Pi builds, the raw `player.getTexture()` on Pi builds (`VideoSystem.cpp:116-121`). **This is the only wrapper in the repo whose "current texture" is conditionally a different GPU object depending on build target** — a real complication for any shared texture-access contract (see §H).
- **Pixel API**: `getPixels()` (`VideoSystem.h:14`) — always the raw decode, never the color-graded FBO (the FBO is display-only).
- **Timing API**: none (`isFrameNew()` only).
- **File-selection API**: `nextFile()` (`VideoSystem.h:12`, `.cpp:126-133`) — advances a shuffled playlist position, rebuilds/reshuffles at the end, avoids an immediate repeat *only at the reshuffle seam* (`VideoSystem.cpp:28-29`, `130-131`). **No previous.** Automatic advancement also happens via its own loop-count state machine: `update()` uses `OF_LOOP_NONE` and manually detects end-of-clip (`getIsMovieDone()` or position ≥ 0.998), replays in place until a randomized `targetLoops` (3–5, `loopMin`/`loopMax`) is hit, then calls `nextFile()` (`VideoSystem.cpp:33-35`, `93-113`). This is a **loop-count hold**, not a wall-clock hold — structurally different from `TimeOffsetVideoBuffer`'s wall-clock-and-loop-count *combined* gate and from `RPVideoSampler`'s wall-clock-only gate.
- **Shutdown behavior**: none explicit.
- **Hidden assumptions / destructive getter**: `fileChanged()` **clears its own flag on read** (`VideoSystem.h:16-20`) — explicitly flagged as an unsafe-to-poll hazard in `scene-observability-profile-probe-report.md:177` and reconfirmed here by direct inspection; only one call site currently reads it (`quadrant-crosshair/src/ofApp.cpp:56`), so it is not yet a multi-consumer race, but any second consumer (e.g. a HUD status object) added naively would silently steal the flag from the first.

### B.5 `ContourSource` — `sketches/contour-portrait/src/ContourSource.h` / `.cpp`

- **Owning location**: sketch-local, `contour-portrait` only. Per `CLAUDE.md`, this sketch has **no `shared/src` dependency at all** — confirmed, `ContourSource` includes only `ofMain.h`.
- **Owns a decoder**: yes, but three mutually-exclusive ones — `ofImage`, `ofVideoPlayer`, `ofVideoGrabber` (`ContourSource.h:33-35`) — only one live at a time, enforced by `closeAll()` on every mode switch (`ContourSource.cpp:35-39`).
- **Setup/load API**: `setup()` (blank-texture init only); `applyMode(mode, imagePath, videoDir, cameraDeviceId)` does the actual load, is a no-op unless something about the requested mode actually changed (`ContourSource.cpp:41-140`). In `INPUT_VIDEO`, it **picks one random file from the folder and never revisits selection** — no playlist, no next/prev, no hold timer, no loop-based advance; the clip just loops forever via `OF_LOOP_NORMAL` (`ContourSource.cpp:86-92`) until the whole sketch's input mode is switched away and back.
- **Update API**: `update()` — routes to `video.update()` or `grabber.update()` by current mode (`ContourSource.cpp:142-148`).
- **Draw/texture API**: `getTexture()` — mode-routed, returns a blank fallback texture if nothing is available (`ContourSource.cpp:150-155`).
- **Pixel API**: none — this class exposes no pixel accessor at all; `contour-portrait`'s effect pipeline (`ContourDisplacementEffect`) works entirely from the texture.
- **Timing API**: none.
- **File-selection API**: single random pick at mode-apply time only (see above) — no equivalent of next/previous/advance anywhere in this class or its caller.
- **Shutdown behavior**: `~ContourSource()` calls `closeAll()`, wrapped in an Objective-C `@try/@catch` on Apple platforms specifically to survive a camera teardown throwing mid-`AVCaptureSession` reconfiguration (`ContourSource.cpp:15-33`) — a real, non-generic platform hazard tied to the camera path only, irrelevant to the video-only runtime target but must not be lost if this class is ever refactored (the try/catch would need to move with whatever still owns camera teardown, or be deleted deliberately alongside camera removal, not accidentally).
- **Hidden assumptions**: `isAvailable()` is mode-blind — true/false regardless of which of the three sources is active; a shared status contract needs a "which kind of source" signal, not just ready/not-ready, if `ContourSource`'s shape is preserved.

### B.6 Direct `ofVideoPlayer` ownership inside the six runtime scenes

Beyond the wrapper classes above, `grep` for `ofVideoPlayer` across all six scenes' own `src/` trees finds:
- `contour-portrait/src/ContourSource.h` — covered in B.5.
- `fragment-trail/src/TimeOffsetVideoBuffer.h` and `.../ofApp.cpp` (the latter only via the fork's member, not a second independent player) — covered in B.3.
- `quadrant-crosshair/src/VideoSystem.h` — covered in B.4.

**No runtime scene's `ofApp.cpp` owns a bare `ofVideoPlayer` directly** — every scene routes through one of the wrapper classes above. The only sketch in the whole repo with a bare, unwrapped `ofVideoPlayer` member is `shader-effect-debugger` (`sketches/shader-effect-debugger/src/ofApp.h:53`), which is out of runtime scope and covered in B.8.

### B.7 `RPVideoSampler` — non-runtime precedent (`radar-effects-gallery`, `radar-pulse`)

- Two copies, byte-identical (`diff` exit 0) — `sketches/radar-effects-gallery/src/RPVideoSampler.{h,cpp}` and `sketches/radar-pulse/src/RPVideoSampler.{h,cpp}`, both explicitly documented in their own header as modeled on `quadrant-crosshair`'s `VideoSystem` (`RPVideoSampler.h:11-16`).
- **Difference from `VideoSystem`** (self-documented, `RPVideoSampler.h:18-23`): advances after a minimum **wall-clock duration** (`minPlaySeconds`, default 30s) checked at the next natural loop boundary, not a random loop-count range. This is the same wall-clock-hold shape `TimeOffsetVideoBuffer` uses for its `minPlaytimeSeconds` half, minus the loop-count half.
- Not in runtime scope (both sketches are explicitly excluded scenes per `00-shared-project-context.md:34-35`) — recorded here only as a third, independently-invented "shuffle + hold + advance" precedent, useful evidence that this pattern has now been written three separate times in this repo (`VideoSystem`, `RPVideoSampler`, and `TimeOffsetVideoBuffer`'s `advanceToNextMedia` gate) with three slightly different hold semantics.

### B.8 `shader-effect-debugger` inline `ofVideoPlayer` — non-runtime precedent

- `sketches/shader-effect-debugger/src/ofApp.h:53` — a bare `ofVideoPlayer video` member, no wrapper class at all.
- **This is the only place in the entire repository with a real, working manual previous *and* next video command** — `nextVideo()`/`prevVideo()` (`ofApp.h:37-38`, wired to arrow keys at `ofApp.cpp:367,369`), simple index-based selection over a folder scan (`.mp4` and `.mov`, unlike every other wrapper's `.mp4`-only), no shuffle, no hold timer. Out of runtime scope per `00-shared-project-context.md:36`, but it is the closest existing precedent in the codebase for the "manual previous/next" behavior the target contract already assumes (`SceneCommand::PreviousMedia`/`NextMedia`) and that no runtime scene currently implements both halves of.

---

## C. Per-Scene Compatibility Matrix

| Scene | Current wrapper | Current media root (as resolved at runtime) | Texture req. | Pixel req. | Temporal req. | Current **next** | Current **previous** | Hold behavior | Error behavior | Likely adapter |
|---|---|---|---|---|---|---|---|---|---|---|
| `blob-region-prototype` | `TimeOffsetVideoBuffer` (live-path only) | local `media/` → 3 per-file symlinks → `blueprint_emergence/bin/data/media` | Yes (background + per-region crops) | Yes (CPU blob detection) | No (playheads never queried; 2s history kept only for bookkeeping) | None | None | None (continuous, never advances) | None found — no file-count-zero handling beyond `hasMedia()` guarding draw calls; `TimeOffsetVideoBuffer` logs a warning (`.cpp:31`) |  Consumer of `VideoPlaybackService`'s raw texture+pixels; no history/playlist logic needed from the service beyond "give me the live decode" |
| `contour-portrait` | `ContourSource` | local `media/` if non-empty, else `sharedMedia/` symlink → `blueprint_emergence/bin/data/media` | Yes | No | No | None | None | None (one random pick per mode-apply, loops forever) | `ofLogError` + `available=false` on missing dir / zero files / load failure (`ContourSource.cpp:74-95`); caller (`getTexture()`) degrades to a blank texture, no crash | Needs a **mode-aware** adapter — image/video/camera multiplex is real product surface even though camera is excluded from the runtime target; video-only slice can adapt to the service, image/camera cannot |
| `temporal-fields` | `TimeOffsetVideoBuffer` (shared) | own `media/`, **actually a whole-directory symlink** → `blueprint_emergence/bin/data/media` (see §F — this differs from `scene-observability-profile-probe-report.md:104`'s "own media/, 25 real clips present," which predates this symlink or was written against a different checkout state) | Yes, per-playhead + raw | Raw only (no history-pixel accessor exists) | **Yes — 6 playheads, quantized history, this is the scene's defining mechanic** | None (advance is automatic, gated on pattern switch × `isMediaAdvanceEligible()`) | None | Wall-clock (30s) **and** loop-count (4) combined gate, `TimeOffsetVideoBuffer::isMediaAdvanceEligible()` | `ofLogWarning` on zero files / load failure (`TimeOffsetVideoBuffer.cpp:31,48`); no visible on-screen error state | Needs a **temporal-history adapter**, not a swap-in — see §E; the live-decode half can come from a shared service, the playhead machinery should not be rewritten |
| `fragment-trail` | `TimeOffsetVideoBuffer` (**local fork**, drifted) | local `media/`, 26 per-file **absolute-path** symlinks → `blueprint_emergence/bin/data/media` | Yes, per-playhead + raw | Not exposed by the fork (missing `getRawVideoPixels()`) | Yes — 6 playheads (same shape as temporal-fields) | **None — `advanceToNextMedia()` is never called anywhere in this sketch** | None | N/A (never advances) | Same as B.3/B.2 (fork-inherited logging) | Needs fork graduation first (see §I) — cannot honestly be called "compatible" with a shared service until it stops diverging from `shared/src/TimeOffsetVideoBuffer` |
| `quadrant-crosshair` | `VideoSystem` | own `media/`, whole-directory symlink → `blueprint_emergence/bin/data/media` (Pi path `/home/pi/blueprint/media`, dead code) | Yes (color-graded FBO on non-Pi, raw on Pi — **two different textures depending on build target**) | Yes (motion extraction, brightness sampling) | No | **Yes** — `nextFile()`, bound to `n`/`N` (`ofApp.cpp:196`) | **None** | Randomized loop-count (3–5 loops), `loopMin`/`loopMax` | `ofLogError` on zero files (`VideoSystem.cpp:12-15`); no on-screen error state; `fileChanged()` is a **destructive getter** (§B.4) | Needs an adapter that reconciles the Pi-conditional color-grade FBO with a single "current texture" contract — see §H |
| `blueprint_emergence` | `VideoSampler` | own `media/` — **the only physical media root in the repo**, 27 real `.mp4` files (Pi path `/home/pi/blueprint/media/`, dead code) | Yes | Yes (motion extraction; also feeds `RidgelineRenderer` via `videoSampler->getPixels()`) | No (uses `VideoSampler`'s seek-and-freeze capture for per-fragment stills, not `TimeOffsetVideoBuffer`) | None as a discrete command — advances automatically, once per full composition cycle, sequential round-robin, on `composition.setOnCycleStart()` (`ofApp.cpp:83-93`) | None | None (advance is tied to the composition's own cycle-restart, not a wall-clock/loop-count hold on the video itself) | `ofLogWarning` on zero files (`VideoSampler.cpp:25-26`); `requestCapture()` degrades to a placeholder fill on seek failure (`VideoSampler.cpp:88-95`) | Is functionally the **canonical media source** today by accident; adapter work here is really "formalize what already exists," not "migrate to something new" |

---

## D. Current-to-Target Ownership Map

For each responsibility from the task's Primary Question 2, current owner → target owner, with evidence and review requirement.

| Responsibility | Current owner(s) | Problems / drift | Recommended target owner | Evidence | Review requirement |
|---|---|---|---|---|---|
| Media-root resolution | Each wrapper independently (`VideoSampler::setup`, `TimeOffsetVideoBuffer::setup`, `VideoSystem::setup`, `ContourSource::applyMode`) — each takes a raw path string from its `ofApp` | 5 independent resolution call sites; de facto root (`blueprint_emergence/bin/data/media`) is not the same as the path already hardcoded in the runtime skeleton (`ExperienceRuntime.cpp:44`, `"assets/shared/media/"`) — a real, already-latent conflict | `VideoPlaybackService`, fed `SceneServices::sharedMediaRoot` | `SceneContract.h:121`; `ExperienceRuntime.cpp:44` | **Requires Architecture review** to pick and formalize the real path (§F) |
| Directory scanning | Each wrapper (`ofDirectory` + `.allowExt("mp4")`, 5 near-identical implementations) | Genuinely duplicated, near-verbatim, 5 times | `VideoPlaybackService` / media catalog loader | `VideoSampler.cpp:15-18`, `TimeOffsetVideoBuffer.cpp:13-16`, `VideoSystem.cpp:6-9`, `ContourSource.cpp:73-80` | Supported by current code (mechanical consolidation) |
| Supported-extension filtering | Each wrapper — 4 of 5 filter `.mp4` only; `shader-effect-debugger` (non-runtime) also allows `.mov` | Inconsistent even among non-runtime precedents; not a runtime-scene problem today since all runtime scenes agree on `.mp4`-only | Media catalog/metadata loader | as above | Supported by current code |
| Metadata / filename handling | `VideoSystem::currentFilename_` (strips path+extension, `VideoSystem.cpp:47-51`) and `RPVideoSampler` (identical logic) are the only wrappers that derive a display-ish filename at all; `TimeOffsetVideoBuffer::getCurrentMediaFilename()` returns the **full path**, not a basename | No curated display-name concept anywhere (`scene-observability-profile-probe-report.md:456`, reconfirmed here — nothing in any wrapper maps a filename to a curated title) | Media catalog/metadata loader | `VideoSystem.cpp:47-51`; `TimeOffsetVideoBuffer.cpp:94-99` | Recommended refactor (basename normalization); curated-title authoring is **Requires Architecture review** (new content-authoring surface, per `scene-observability-profile-probe-report.md`'s own Open Question 2) |
| Playlist construction | `VideoSystem::buildPlaylist()`, `TimeOffsetVideoBuffer::shuffleMediaFiles()`, `RPVideoSampler::buildPlaylist()` — 3 independent Fisher-Yates/`std::shuffle` implementations; `VideoSampler` and `ContourSource` have **no playlist concept** (sequential index / one-shot random pick, respectively) | 3 duplicated shuffle implementations; 2 wrappers with no playlist at all | `VideoPlaybackService` | see B.1–B.5 | Supported by current code (consolidation), but see next row for the harder part |
| Playlist shuffle | as above | `TimeOffsetVideoBuffer` reshuffles at lap end with an anti-repeat seam swap; `VideoSystem`/`RPVideoSampler` do the same; algorithmically compatible across all three | `VideoPlaybackService` | `TimeOffsetVideoBuffer.cpp:66-77`; `VideoSystem.cpp:20-31`; `RPVideoSampler.cpp` (identical) | Supported by current code |
| Random selection | as above | same | `VideoPlaybackService` | as above | Supported by current code |
| Current-item index | Each wrapper's own `currentFileIndex`/`fileIndex`/`playlistPos` | Not exposed uniformly — `VideoSystem`/`RPVideoSampler` expose a filename, `TimeOffsetVideoBuffer` exposes a full path, `VideoSampler`/`ContourSource` expose neither | `VideoPlaybackService` (status struct) | see B.1–B.5 | Supported by current code |
| Recent-selection history (for no-repeat) | **Does not exist as a general concept anywhere.** Every wrapper's "no immediate repeat" is really just "don't let position 0 of a freshly-reshuffled playlist equal the just-finished file" — a single-item lookback, not a history window | This is a narrower guarantee than "no-immediate-repeat behavior" as the target roadmap phrases it (`Ecopunk-HUD-System-Master-Roadmap.md:522`) might imply to a HUD/product reader — worth naming explicitly so the target design doesn't silently promise more (e.g. "never repeat within N clips") than any current implementation provides | `VideoPlaybackService` | `TimeOffsetVideoBuffer.cpp:73-76`; `VideoSystem.cpp:27-29`; `RPVideoSampler.cpp` (identical pattern) | Recommended refactor if a deeper history window is wanted; **current 1-item guarantee is Supported by current code** as a baseline |
| "No-repeat" logic proper | as above | as above | `VideoPlaybackService` | as above | Recommended refactor |
| Previous behavior | **`shader-effect-debugger` only** (non-runtime) | **No runtime scene implements this at all**, despite `SceneCommand::PreviousMedia` already being frozen in the contract (`SceneContract.h:85`) | `VideoPlaybackService` (new logic — index-decrement or a small ring history) | `SceneContract.h:83-88`; absence confirmed by `grep` across all 6 scenes' `keyPressed`/command surfaces | **Requires runtime experiment** to define desired semantics (does "previous" replay the last-shown clip, or step backward through the shuffled order?) before implementation — this is a real design gap, not just a migration task |
| Next behavior | `quadrant-crosshair::VideoSystem::nextFile()` only, among runtime scenes; `blueprint_emergence`/`temporal-fields` advance automatically but have no manual trigger; `blob-region-prototype`/`contour-portrait`/`fragment-trail` have no next of any kind | Wildly inconsistent — 1 of 6 scenes has a working manual next | `VideoPlaybackService` | `VideoSystem.h:12`, `.cpp:126-133`; see §C | Recommended refactor to bring the other 5 scenes to parity; **quadrant-crosshair's existing behavior is Supported by current code** as the reference implementation |
| Automatic advancement | `blueprint_emergence` (per composition cycle), `temporal-fields` (per pattern switch × eligibility), `quadrant-crosshair` (per loop-count hold), `fragment-trail` (never) | 3 different triggers for "automatic," 1 scene with none at all | `VideoPlaybackService`, parameterized per scene's trigger need | see §C | Recommended refactor — the trigger *source* (cycle/pattern/loop-count) should stay scene-owned; the *mechanics* of "advance now" should centralize |
| Hold timer | `TimeOffsetVideoBuffer` (wall-clock **and** loop-count), `VideoSystem`/`RPVideoSampler` (loop-count **or** wall-clock respectively, not both) | 3 different hold shapes for conceptually the same "don't cut a clip short" guarantee | `VideoPlaybackService` | `TimeOffsetVideoBuffer.h:42-43`; `VideoSystem.h:28-29`,`.cpp:33-35`; `RPVideoSampler.h:35` | Recommended refactor; **which shape becomes canonical is a product call**, not purely technical — flag for Architecture/product review |
| EOF / loop behavior | `VideoSampler`/`TimeOffsetVideoBuffer`/`ContourSource` all use `OF_LOOP_NORMAL` (oF's own internal loop, no app-level EOF detection); `VideoSystem`/`RPVideoSampler` use `OF_LOOP_NONE` with manual `getIsMovieDone()`/position-based EOF detection | Two structurally different loop strategies in active use | `VideoPlaybackService` (pick one, or keep both as it maps to whether history-tracking is needed) | `TimeOffsetVideoBuffer.cpp:41`; `VideoSystem.cpp:41,103-113` | Recommended refactor — `OF_LOOP_NORMAL` is simpler and is what the temporal-history-owning scenes already rely on for uninterrupted history capture; switching `quadrant-crosshair`/`blueprint_emergence`-style scenes to it changes observable EOF-driven behavior and needs a runtime check |
| Playback progress | **No wrapper exposes this today** | Full gap — `Ecopunk-HUD-System-Master-Roadmap.md:516`'s "playback progress" status field has no source anywhere in the codebase | `VideoPlaybackService` (new) | absence confirmed across B.1–B.5 | Recommended refactor (new getter over `ofVideoPlayer::getPosition()`, straightforward) |
| Loading state | **No wrapper exposes this today** as a discrete state; only implicit via `hasMedia()`/`isAvailable()`/`isReady()` boolean-style gates | No `SceneHealth::Loading`-shaped signal exists in any wrapper | `VideoPlaybackService` / scene status adapter | `VideoSampler.h:20`; `ContourSource.h:24`; `RPVideoSampler.h:30` | Recommended refactor |
| Ready state | `hasMedia()` (`VideoSampler`, `TimeOffsetVideoBuffer`), `isAvailable()` (`ContourSource`), `isReady()` (`RPVideoSampler`) — 3 different names, same concept | Naming drift only, not a functional gap | `VideoPlaybackService` | as above | Supported by current code (rename/consolidate) |
| Error state | Log-only in every wrapper (`ofLogWarning`/`ofLogError`), never surfaced as queryable state | Full gap for `SceneHealth::Failed`/`message` | `VideoPlaybackService` / scene status adapter | `VideoSampler.cpp:25-26`; `TimeOffsetVideoBuffer.cpp:31,48`; `VideoSystem.cpp:13`; `ContourSource.cpp:75,82,94` | Recommended refactor |
| Retry / skip behavior | **Does not exist anywhere.** A failed `player.load()` just leaves the wrapper in its not-ready state; nothing retries or auto-skips to the next file | Real gap — a corrupt/unsupported file in the media root would currently just silently produce a blank/placeholder output forever, not skip forward | `VideoPlaybackService` (new) | as above (absence) | Recommended refactor; needs a defined policy (skip-on-load-failure vs. retry-once vs. surface-as-Failed) — **Requires runtime experiment** (§ "Required runtime experiments," "startup with one invalid file") before committing to a policy |
| Source texture | Each wrapper's own `getTexture()`/`getPlayheadTexture()`/`getRawVideoTexture()` | Shape-compatible (all return `const ofTexture&`/`ofTexture&`) except `quadrant-crosshair`'s conditional FBO-vs-raw swap | `VideoPlaybackService` for the current/live texture; `TimeOffsetVideoBuffer` unchanged as history-texture owner | see B.1–B.5 | Recommended refactor for the 5 "current texture" cases; `quadrant-crosshair`'s color-grade FBO is **Requires Architecture review** — is post-process grading a scene concern layered on top of the service's texture, or does the service need to support a per-scene post-process hook? |
| Source pixels | `getPixels()`/`getRawVideoPixels()` where they exist; absent in `ContourSource` and the `fragment-trail` fork | Real gap in 2 of 6 scenes' wrappers (though neither scene currently *needs* pixels from this specific source — `fragment-trail` doesn't call for them; `contour-portrait`'s effect is texture-only) | `VideoPlaybackService` | see B.1–B.5 | Supported by current code where needed; no action required where not needed |
| Source dimensions | `getVideoWidth()/getVideoHeight()` (`VideoSampler`), `getVideoSize()` (`VideoSystem`/`RPVideoSampler`), `getBufferWidth()/getBufferHeight()` (`TimeOffsetVideoBuffer` — **buffer** size, not source size), none (`ContourSource`) | Naming and semantic drift (`TimeOffsetVideoBuffer`'s dimension getters describe the downscaled history buffer, not the source clip — a real distinct concept, not just a naming difference) | `VideoPlaybackService` (source dims) + `TimeOffsetVideoBuffer` unchanged (buffer dims stay buffer-scoped) | `VideoSampler.h:40-41`; `VideoSystem.h:11`; `TimeOffsetVideoBuffer.h:70-71` | Supported by current code (naming clarification only) |
| Temporal-history capture | `TimeOffsetVideoBuffer` (shared) and its `fragment-trail` fork, exclusively | Only the fork is a problem (drift, §B.3) | **Temporal-history adapter**, wrapping `TimeOffsetVideoBuffer` unchanged | see B.2/B.3/E | `TimeOffsetVideoBuffer` internals: Requires Architecture review only if changed; **wrapping it is Supported by current code** |
| Temporal playheads / offsets | `TimeOffsetVideoBuffer::jumpPlayhead`/`rampPlayheadTo`/`holdPlayhead`/`getPlayheadOffset` | None — this is the one fully mature, single-owner subsystem in the whole inventory | Temporal-history adapter (unchanged internals) | `TimeOffsetVideoBuffer.h:79-84` | Supported by current code |

---

## E. Decoder/History Boundary Report

Direct answers to Primary Question 5, from `TimeOffsetVideoBuffer.cpp`/`.h` inspection:

- **Does it directly own and update `ofVideoPlayer`?** Yes — `player` is a private member (`TimeOffsetVideoBuffer.h:148`), and `update(dt)` calls `player.update()` itself every frame (`TimeOffsetVideoBuffer.cpp:113`). There is no seam today between "decode" and "buffer" as separate objects — they are one class.
- **How do frames enter the history buffer?** Every frame where `player.isFrameNew()` is true, the current `ofPixels` is copied, resized down to `bufferWidth`×`bufferHeight`, and pushed to the front of a `std::deque<ofPixels>` (`TimeOffsetVideoBuffer.cpp:115-118`). This is a **pull**, not a push — nothing external can inject a frame into history today.
- **CPU- or GPU-backed?** CPU (`std::deque<ofPixels>`, system RAM) — deliberately, per the class's own header comment, to avoid a ~220MB VRAM cost of a naive texture-per-frame approach on a 1GB Pi 3B (`TimeOffsetVideoBuffer.h:23-29`). Only the small number of **playheads actually queried** get uploaded to a GPU texture, lazily, once per frame (`TimeOffsetVideoBuffer.cpp:176-194`).
- **How many historical frames / playheads exist?** History capacity = `maxHistorySeconds * assumedSourceFps` frames (`getHistoryCapacityFrames()`, `TimeOffsetVideoBuffer.cpp:202-204`) — e.g. default 10s × 24fps = 240 frames capped in the deque, though `blob-region-prototype` configures a 2s cap and `temporal-fields` configures its own via `paramPanel.getMaxHistorySeconds()`. Playheads: a fixed count set once in `setup()` (`playheads.assign(numPlayheads, Playhead{})`, `TimeOffsetVideoBuffer.cpp:10`) — 6 for both `temporal-fields` and `fragment-trail`.
- **Do playheads perform actual video seeks?** **No.** This is the class's entire reason for existing over `VideoSampler` (`TimeOffsetVideoBuffer.h:15-21`) — a playhead only ever indexes into the already-buffered `history` deque (`offsetToHistoryIndex()`, `TimeOffsetVideoBuffer.cpp:206-213`); it never calls `player.setPosition()`.
- **How are offsets calculated?** A playhead's `currentOffset` is a normalized `[0,1]` value (0 = most recent, 1 = oldest buffered), quantized to `numQuantizeBands` discrete steps (`quantize()`, `TimeOffsetVideoBuffer.cpp:196-200`) before being mapped to a history-deque index proportionally (`offsetToHistoryIndex()`). Motion between offsets is either an instant `jumpPlayhead()` or a rate-limited `rampPlayheadTo()` interpolation, advanced per-`update()` (`TimeOffsetVideoBuffer.cpp:137-151`).
- **Can current-frame access be separated from history ownership?** **Yes, and it already effectively is, in practice**: `blob-region-prototype` uses this exact class purely for its live-decode accessors (`getRawVideoTexture()`/`getRawVideoPixels()`/`hasMedia()`) with a deliberately tiny 2-second history it never queries via any playhead (`blob-region-prototype/src/ofApp.cpp:45-47`, comment: *"this prototype doesn't use temporal offsets — kept short, just enough for hasMedia()/isFrameNew() bookkeeping"*). The class's public API already draws this line cleanly at the method level (`getRawVideoTexture()`/`getRawVideoPixels()` vs. `getPlayheadTexture()`), even though internally one object still owns both halves.
- **Could an external decoded texture/pixel source feed it?** Not without a real API change today — `update(dt)` unconditionally calls its own `player.update()` and reads `player.isFrameNew()`/`player.getPixels()` internally (`TimeOffsetVideoBuffer.cpp:113-118`); there is no injection point. Feeding it from an external decoder would require either (a) a new `pushFrame(const ofPixels&)`-shaped method that lets a caller drive history capture from a source `TimeOffsetVideoBuffer` doesn't itself decode, or (b) restructuring the class to hold a reference/pointer to an external decoder interface instead of owning `player` directly. **Both are non-trivial, `TimeOffsetVideoBuffer`-touching changes** — explicitly out of this probe's boundaries ("Do not rewrite `TimeOffsetVideoBuffer`") and are correctly scoped as **Requires Architecture review** if pursued.
- **Likely copy/allocation/synchronization costs of such a separation**: every history-push already does one full-frame `ofPixels` copy-and-resize (`frame.resize(...)`, `TimeOffsetVideoBuffer.cpp:117`) regardless of source; adding an external-feed seam would not add a *new* per-frame cost, but would move the "who owns the decode's lifetime/thread-safety" question outside this class, which today is safe only because `update()` runs on the same thread as everything else in an oF `ofApp::update()` call.
- **Pi 3B+ risks**: the class's own header already documents the primary one (VRAM budget if history were texture-backed instead of CPU-backed, `TimeOffsetVideoBuffer.h:26-29`, explicitly flagged as **not yet measured on real hardware**). A second, not-yet-documented risk this probe surfaces: `frame.resize()` on every new decoded frame (`TimeOffsetVideoBuffer.cpp:117`) is a CPU resample cost paid once per source frame regardless of how many playheads actually read history that frame — on Pi 3B's weaker CPU this is a real, currently-unmeasured per-frame cost independent of playhead count, and is unaffected by any of the ownership changes this report considers (it exists today, in the current code, for both `temporal-fields` and `fragment-trail`).

**Safest target boundary** (recommendation, not a code change): keep `TimeOffsetVideoBuffer` exactly as it is — do not attempt the external-feed injection point in this migration. Instead, treat it as its own fully-formed "temporal-history adapter" in the target ownership model (§H), consuming the shared `VideoPlaybackService`'s *file selection* (which clip to load, when to advance) while continuing to own its *own* `ofVideoPlayer` instance for the actual decode+history capture, exactly as it does today. This avoids the higher-risk decoder/history separation entirely for scenes that need temporal history, while still letting those scenes' *media-root/playlist/selection* logic be shared. `blob-region-prototype` is the concrete existence-proof this works: it already runs the same class in "basically no history" mode successfully.

---

## F. Media-Root Inventory and Migration Risks

### F.1 Physical vs. symlinked, by sketch

| Sketch | `bin/data/media` | `bin/data/sharedMedia` | Real files present |
|---|---|---|---|
| `blueprint_emergence` | **real directory, 27 physical `.mp4` files** | — | **This is the only physical media root among all 9 sketches inspected.** |
| `blob-region-prototype` | real directory containing 3 **per-file symlinks** (relative paths) → `../../../../blueprint_emergence/bin/data/media/<file>` | — | 0 |
| `temporal-fields` | **whole-directory symlink** → `../../../blueprint_emergence/bin/data/media` | — | 0 (symlink target has 27) |
| `quadrant-crosshair` | **whole-directory symlink** → `../../../blueprint_emergence/bin/data/media` | — | 0 |
| `fragment-trail` | real directory containing 26 **per-file symlinks** (**absolute** paths, `/Users/joseconchello/...`) → `blueprint_emergence/bin/data/media/<file>` | — | 0 |
| `contour-portrait` | real, empty directory (deliberate: local override wins if populated, per `applyInputMode()` comment) | **symlink** → `../../../blueprint_emergence/bin/data/media` | 0 in `media/`; `sharedMedia` resolves to 27 |
| `shader-effect-debugger` (non-runtime) | real, empty directory | — | 0 — this sketch has **no media at all** today; its own `ofApp.cpp:31` logs a warning to this effect |
| `radar-pulse` (non-runtime) | real directory, only a `.gitkeep` (0 real clips) | symlink → `quadrant-crosshair/bin/data/media` (itself a symlink to `blueprint_emergence`) | 0 direct; resolves to 27 via two symlink hops |
| `radar-effects-gallery` (non-runtime) | not inspected in detail (out of scope) | symlink → `quadrant-crosshair/bin/data/media` | resolves to 27 via two symlink hops |

**No duplicate physical copies were found.** The one directory that initially looked like a duplicate (`radar-pulse/bin/data/media`) contains only a `0`-byte `.gitkeep`, confirmed via direct listing — not a second copy of any clip.

### F.2 Hardcoded paths

- **`MEDIA_PATH` constant, `blueprint_emergence/src/BESettings.h:391-395` and `BESettings_presets.h:343-347`** (two copies of the same conditional, one per settings-preset file):
  ```cpp
  #ifdef PLATFORM_PI
  constexpr char MEDIA_PATH[] = "/home/pi/blueprint/media/";
  #else
  constexpr char MEDIA_PATH[] = "media/";
  #endif
  ```
- **`quadrant-crosshair/src/ofApp.cpp:11-14`**: identical `PLATFORM_PI` conditional, `/home/pi/blueprint/media` vs. `ofToDataPath("media", true)`.
- **`radar-effects-gallery/src/ofApp.cpp:23`** and **`radar-pulse/src/ofApp.cpp:24`** (non-runtime): unconditional `"/home/pi/blueprint/media"` literal with **no** `#ifdef` guard at all — these two would attempt to read that absolute path even on a Mac dev build, unlike the other two `PLATFORM_PI`-guarded sites. Not runtime-scope, but worth noting as the least-defensive of the four `/home/pi` sites.
- **`PLATFORM_PI` is never defined anywhere in the build**: `grep` across every `config.make`/`Makefile` in the repo found zero definitions of this macro. All five `#ifdef PLATFORM_PI` / `#ifndef PLATFORM_PI` blocks (spanning `blueprint_emergence`, `quadrant-crosshair`'s `VideoSystem`/`MotionExtraction`/`ofApp`, and `shared/src/MotionExtraction`) are **currently dead code on every build this repo actually produces**, exactly as `CLAUDE.md`'s framing and `scene-observability-profile-probe-report.md:34` already note for `blueprint_emergence` specifically — this report confirms the same is true for every other `PLATFORM_PI` site in the codebase, not just that one.
- **`ExperienceRuntime.cpp:44`**: `services.sharedMediaRoot = "assets/shared/media/";` — a **third, different** path convention (relative to a not-yet-existing `assets/` tree, matching `Scene-HUD-Contract-v1.md §12`'s aspirational canonical layout) that exists only in the not-yet-real runtime skeleton, not in any shipped sketch. **`find` confirms no `assets/shared/media/` directory exists anywhere in this repo.**

So there are, today, **three mutually-incompatible ideas of "the media root"** live in the codebase simultaneously:
1. `media/` (relative to each sketch's own `bin/data/`) — what every runtime scene's dev build actually uses.
2. `/home/pi/blueprint/media` — dead code, never compiled in, and not even consistent with this repo's own directory name (`EcopunkVideoCollage`, not `blueprint`).
3. `assets/shared/media/` — what the not-yet-real `ExperienceRuntime` skeleton already hardcodes, matching the *aspirational* contract layout but backed by nothing on disk.

### F.3 Migration risks

- **Symlink fragility**: `fragment-trail`'s per-file symlinks use **absolute** paths tied to this specific checkout's location on disk (`/Users/joseconchello/openFrameworks/...`) — these break the instant the repo is cloned/moved to any other machine or path, unlike every other sketch's relative symlinks. This is a real, immediate portability risk independent of any playback-service migration, and should be flagged to whoever owns repo/media tooling regardless of this report's outcome.
- **`temporal-fields`' media root drifted from its own prior documentation**: `scene-observability-profile-probe-report.md:104` describes `temporal-fields` as having "own `media/`, 25 real clips present" — current inspection finds `temporal-fields/bin/data/media` is now a **whole-directory symlink** to `blueprint_emergence`'s media, with 0 physical files of its own. This is either a repo change made after that report was written, or a discrepancy in how that report characterized a symlinked directory. Either way, **any migration plan should re-verify media-root state at execution time, not trust either document blindly** — media roots in this repo have moved at least once already outside of any playback-service work.
- **Canonicalizing on `blueprint_emergence/bin/data/media` formally**: since it is already the de facto root for 5 of 6 runtime scenes (directly or via symlink chain), the lowest-risk canonicalization is to make this explicit and intentional rather than switching to the `assets/shared/media/` path the runtime skeleton currently hardcodes — but that decision has real implications for the `ExperienceRuntime`/`SceneServices` skeleton already written, and for the canonical `assets/` tree `Scene-HUD-Contract-v1.md §12` describes. **This is a product/architecture decision, not something this probe can resolve** — flagged as **Requires Architecture review** (see §J).
- **`contour-portrait`'s local-override-wins convention** (`localMedia` check before falling back to `sharedMedia`, `ContourSource.cpp` call site at `ofApp.cpp:84-87`) is a real, intentional per-scene override mechanism that a canonical shared-root design needs to either preserve explicitly or deliberately decide to remove — silently dropping it would change this scene's actual behavior if anyone had ever populated its local `media/` folder (it is empty today, so currently inert, but the mechanism is live code, not dead code).

---

## G. Status Reachability Table

Against the status fields `Ecopunk-HUD-System-Master-Roadmap.md:513-520` and `Scene-HUD-Contract-v1.md §7`'s `SceneHudStatus` call for:

| Proposed status field | Already exists? | Where | Notes |
|---|---|---|---|
| Media filename | Partial | `VideoSystem::getCurrentFilename()` (basename, extension-stripped); `TimeOffsetVideoBuffer::getCurrentMediaFilename()` (**full path**, not basename) | Inconsistent shape between the two existing accessors — a status adapter must normalize, not just forward |
| Curated/display title | **No** | — | Confirmed absent everywhere, matches `scene-observability-profile-probe-report.md`'s Open Question 2 exactly; this is new authoring work, not a reachability gap |
| Media ID | **No** | — | No wrapper has any concept of a stable ID distinct from a filename/path |
| Playback position | **No** (underlying `ofVideoPlayer::getPosition()` exists at the oF level but no wrapper exposes it) | — | Straightforward new getter if needed |
| Duration | **No** | — | Same — `ofVideoPlayer::getDuration()` exists at the oF level, unexposed by any wrapper |
| Playback progress | **No** | — | Would be position/duration, itself ungated on both being exposed first |
| Hold progress | **Partial, boolean only** | `TimeOffsetVideoBuffer::isMediaAdvanceEligible()` | A yes/no gate, not a `[0,1]` progress value — would need new internal elapsed-time tracking exposed as a ratio, not just a threshold check |
| Time remaining | **No** | — | Same gap as duration |
| Ready/loading/failed state | **Partial, ready/not-ready only** | `hasMedia()`/`isAvailable()`/`isReady()` (naming varies, see §D) | No `Loading` or `Failed` discrete state anywhere — only a boolean that conflates "never loaded," "failed to load," and "no media found" |
| Error message | **No** (log-only) | `ofLogWarning`/`ofLogError` call sites throughout | Never captured as a queryable string — would need each wrapper's failure paths to also set a member, not just log |
| Manual vs. automatic selection | **No** | — | No wrapper distinguishes why the current file was selected; a `VideoPlaybackService` would need to track this itself as new state, since none of today's advance-triggering call sites (composition cycle, pattern switch, loop-count hold, keypress) pass that information down into the wrapper today |

**Destructive getters found**: `VideoSystem::fileChanged()` (`VideoSystem.h:16-20`) clears its own boolean on read — already flagged in `scene-observability-profile-probe-report.md:177` and independently reconfirmed here by direct inspection. No other destructive getter was found in any of the wrappers inspected for this report (`VideoSampler`, `TimeOffsetVideoBuffer` × 2, `ContourSource`, `RPVideoSampler` all use plain non-mutating accessors).

---

## H. Conceptual API Alternatives

Two options, both illustrative only, neither implemented or approved.

### H.1 Option A — the task's own sketch, evaluated

```cpp
struct VideoPlaybackStatus;
struct MediaMetadata;

class IVideoPlaybackService {
public:
    virtual ~IVideoPlaybackService() = default;
    virtual void update(float dt) = 0;
    virtual bool next() = 0;
    virtual bool previous() = 0;
    virtual const ofTexture* currentTexture() const = 0;
    virtual const ofPixels* currentPixels() const = 0;
    virtual VideoPlaybackStatus status() const = 0;
};
```

**Evaluation**:
- **Texture/pixel access as raw pointers returned from `const` accessors** is consistent with how every existing wrapper already behaves (`getTexture()`/`getPixels()` returning references/pointers into internally-owned oF objects) — this shape is not a stretch for this codebase.
- **One-active-decoder-instance assumption**: this interface implies a single current texture/pixels pair, i.e. one decoder live at a time. That is **true for 4 of 6 runtime scenes** (`blueprint_emergence`, `blob-region-prototype`, `contour-portrait`'s video mode, `quadrant-crosshair`) but **false for `temporal-fields` and `fragment-trail`**, which need one *decode* plus **6 simultaneously-addressable historical playheads**, each independently queryable. A bare `currentTexture()` does not model that — those two scenes would need the temporal-history adapter (§E) sitting *in front of* this interface, consuming its `next()`/`previous()`/`status()` for file selection while bypassing `currentTexture()`/`currentPixels()` entirely in favor of `TimeOffsetVideoBuffer`'s own playhead accessors.
- **Service ownership**: reasonable as the target owner for playlist/selection/hold/status (§D), **not** reasonable as the sole owner of every scene's "what texture do I actually draw" question — `quadrant-crosshair`'s color-grade FBO (§B.4) sits downstream of the raw decode and upstream of what the scene draws; this interface's `currentTexture()` would need to be understood as "the service's best current frame," with the scene's own adapter free to post-process it further, not as "the literal thing every scene draws unmodified."
- **Missing from this sketch, evident from the inventory**: no `mediaRoot`/`load(path)` setup call, no way to distinguish "no media found" from "loaded and playing," and no separate signal for "the file changed" (`SceneHudStatus::mediaName` reporting is currently the closest, but nothing in this sketch drives it). `VideoPlaybackStatus`/`MediaMetadata` are left undefined in the task's own illustration; §G's table is a reasonable starting field list for `VideoPlaybackStatus` if this shape is pursued.

### H.2 Option B — split decode-selection from frame-access (recommendation-leaning, still illustrative)

```cpp
// Owns: media root, directory scan, playlist, shuffle, no-repeat,
// hold timer, next()/previous(), status. Does NOT own a decoder itself.
class IVideoSelectionService {
public:
    virtual ~IVideoSelectionService() = default;
    virtual void update(float dt) = 0;
    virtual bool next() = 0;
    virtual bool previous() = 0;
    virtual std::string currentFilePath() const = 0;   // consumer loads it
    virtual VideoPlaybackStatus status() const = 0;
};

// Owns: one ofVideoPlayer, decode+update, current texture/pixels.
// Constructed per-scene (or per-active-scene), fed a path by the
// selection service above rather than scanning/selecting itself.
class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;
    virtual bool load(const std::string& path) = 0;
    virtual void update(float dt) = 0;
    virtual const ofTexture* currentTexture() const = 0;
    virtual const ofPixels* currentPixels() const = 0;
};
```

**Evaluation**: this shape maps much more directly onto what already exists — `IVideoSelectionService` is close to what `VideoSystem`/`TimeOffsetVideoBuffer`/`RPVideoSampler` already do for playlist/advance/hold, and `IVideoDecoder` is close to what `VideoSampler`'s live-video half already does. Critically, it **does not force a one-decoder-per-service assumption**: `temporal-fields`/`fragment-trail` can use `IVideoSelectionService` for "which file, when to advance" while keeping their own `TimeOffsetVideoBuffer`-owned decoder entirely separate and unchanged, exactly matching §E's recommended boundary. The cost is a slightly larger API surface (two interfaces instead of one) and a real question this report does not resolve: **does `IVideoSelectionService` hand off a path string (loose coupling, what's shown above) or a live `IVideoDecoder*` it also constructs (tighter coupling, closer to what `VideoPlaybackService` in §D's table implies)?** That decision has real consequences for `quadrant-crosshair`'s color-grade FBO and for whether "one decoder per active scene" needs to be a hard rule — **flagged as Requires Architecture review**, not decided here.

**Neither option is implemented.** Both are offered strictly to make the ownership-split tradeoff (§I's migration order depends on it) inspectable, per the task's explicit "do not implement this interface" instruction.

---

## I. Behavior-Preserving Migration Sequence

Per `Scene-HUD-Contract-v1.md §14`'s own adapter-first, two-stage philosophy, and consistent with the migration-risk table it already carries forward (`blob-region-prototype`/`temporal-fields`/`blueprint_emergence` Phase 1; `fragment-trail`/`quadrant-crosshair` Phase 2, blocked on `CrosshairSystem` graduation).

1. **Lowest-risk first consumer: `blob-region-prototype`.** It already uses `TimeOffsetVideoBuffer` in "live-decode only" mode with no playlist/next/prev/hold behavior to preserve — the smallest possible surface to validate a `VideoPlaybackService`-shaped adapter against, and its own code comments already describe it as "deliberately thin." Adapter needed: essentially a pass-through from the service's current texture/pixels to what `TimeOffsetVideoBuffer`'s live-decode accessors provide today — no behavior change expected, since this scene has almost no current behavior to preserve beyond "always show the live decode."

2. **`blueprint_emergence` next.** It is the de facto canonical media root (§F) and its advance behavior (sequential, once per composition cycle) is simple and already well-isolated behind a single callback (`setOnCycleStart`). Adapter needed: a thin shim translating the service's `next()`/status into the existing `onCycleStart` call site, preserving "advance exactly once per cycle, sequential order" as observable behavior. This is also the point at which the **real canonical-media-root decision** (§F.3) has to be made, since this sketch currently *is* that root physically — migrating it without first resolving the `media/` vs. `assets/shared/media/` question would be premature.

3. **`quadrant-crosshair`'s `nextFile()` becomes the reference for manual-next across the other scenes, but full scene migration should wait.** Per `Scene-HUD-Contract-v1.md §14`'s own table, `quadrant-crosshair` is explicitly Phase 2, blocked on `CrosshairSystem` graduation — this report finds nothing in the video-playback code itself that would override that sequencing, but does find an independent, video-specific reason to be cautious: the Pi-conditional color-grade FBO (§B.4/§H.1) needs an Architecture decision on where post-processing sits relative to a shared service's texture *before* this scene's adapter can be written correctly, not just before it can graduate structurally.

4. **`temporal-fields` migrates once the decoder/selection split (§E, §H.2) is validated on scene 1 or 2** — not before. Its behavior (6 playheads, quantized offsets, wall-clock+loop-count hold gating pattern-switch-triggered advance) must be **fully preserved**, per the task's explicit boundary — this report's recommendation is that the *selection* half (which file, when eligible to advance) can safely move to a shared service while the *decode+history* half stays exactly as `TimeOffsetVideoBuffer` already implements it (§E's "safest target boundary"). This is a **compatibility adapter**, not a rewrite: it would translate the service's `advanceToNextMedia()`-equivalent call into `TimeOffsetVideoBuffer`'s existing method of the same name, and nothing about the playhead API changes.

5. **`fragment-trail` must wait on its local-fork cleanup before any service migration is meaningful**, and this is now confirmed by two independent findings, not one: (a) `CLAUDE.md`'s existing, already-documented reason (shader/`ShaderLibrary` name collisions), and (b) this report's own finding that its `TimeOffsetVideoBuffer` fork has already drifted from the shared header (§B.3, missing `getRawVideoPixels()`) and that the sketch **never calls `advanceToNextMedia()` at all** — meaning "migrate fragment-trail's playback to the shared service" is not well-defined today, because there is no current automatic-advance behavior to preserve; a product decision is needed first on whether fragment-trail *should* advance media at all (it currently doesn't, possibly intentionally, possibly an oversight — this report cannot tell which from static inspection alone, hence **Requires runtime experiment**: watch a long-running session and confirm with whoever owns this scene's design intent whether single-clip-for-life is deliberate).

6. **`contour-portrait` is a separate track, not a "wait until last" scene** — its blocking issue is not structural complexity like `quadrant-crosshair`, it's the mode-multiplexing (image/video/camera) design question from §B.5/§C. Recommend a dedicated design pass (mirroring what `docs/video-effect-second-wave-evaluation.md` already did for this scene's *shader* path, per `CLAUDE.md`) before attempting a playback-service adapter, rather than folding it into the same sequencing logic as the other five.

**Compatibility adapter needed per scene** (summary):
- `blob-region-prototype`: pass-through adapter, live-decode only.
- `blueprint_emergence`: cycle-triggered-advance adapter + canonical-root resolution.
- `quadrant-crosshair`: adapter deferred to Phase 2 per existing contract sequencing; needs an Architecture answer on post-process/FBO placement first.
- `temporal-fields`: selection-only adapter in front of unchanged `TimeOffsetVideoBuffer`.
- `fragment-trail`: blocked on fork graduation + a product decision on intended advance behavior.
- `contour-portrait`: blocked on a dedicated mode-abstraction design pass.

**Rollback boundaries**: each scene's adapter should be introduced as an additive wrapper around the scene's existing wrapper class, not an in-place rewrite of that class — every wrapper inspected in this report (§B) is self-contained enough (no cross-wrapper shared mutable state found anywhere) that a scene can be reverted to its pre-adapter `ofApp.cpp` wiring independently of any other scene's migration state. This is a direct consequence of how isolated these five implementations already are from each other, not something the migration needs to newly engineer.

**Tests required before moving to the next scene**: for each migrated scene — a same-day A/B comparison of the scene running against its original wrapper vs. its new adapter, confirming at minimum: startup selection matches pre-migration behavior class (same trigger, same ordering guarantee), no-repeat-at-seam still holds, and (for `temporal-fields`) playhead offsets/history continuity are visually unchanged across a media-change boundary. None of this is implemented or run by this report — see "Required runtime experiments" below.

---

## J. Risks and Unresolved Questions

**Code facts** (established by this report, not requiring further review):
- No shared video-playback service exists today; 5 structurally distinct wrapper implementations do (§B).
- `SceneCommand::PreviousMedia` is frozen in the contract but implemented in zero of six runtime scenes; `NextMedia`-equivalent behavior exists in exactly one (§D).
- `fragment-trail`'s `TimeOffsetVideoBuffer` fork has already drifted from the shared header, and the sketch never calls its own fork's `advanceToNextMedia()` (§B.3).
- Three incompatible "canonical media root" conventions currently coexist in the codebase (relative `media/`, dead-code `/home/pi/blueprint/media`, not-yet-real `assets/shared/media/`) (§F.2).
- `blueprint_emergence/bin/data/media` is the only physical media root among all sketches inspected; every other runtime scene reaches it only through symlinks (§F.1).
- `temporal-fields`' media root has changed since `scene-observability-profile-probe-report.md` was written (own directory → whole-directory symlink) (§F.3).
- `VideoSystem::fileChanged()` is a destructive getter (§B.4/§G).
- `TimeOffsetVideoBuffer` cleanly separates live-decode accessors from history/playhead accessors at the public-API level already, even though it owns both internally (§E).

**Recommendations** (this report's judgment, not yet decided by any domain):
- Canonicalize on `blueprint_emergence/bin/data/media` as the literal on-disk root, or formally adopt `assets/shared/media/` and migrate the one real copy there — do not leave both conventions live simultaneously (§F.3).
- Split "selection" (playlist/shuffle/next/previous/hold/status) from "decode" (own an `ofVideoPlayer`, expose texture/pixels) as two responsibilities, not one service, so `temporal-fields`/`fragment-trail` can adopt the selection half without touching `TimeOffsetVideoBuffer`'s decode/history half (§E/§H.2).
- Migrate `blob-region-prototype` first, `blueprint_emergence` second, defer `quadrant-crosshair`/`fragment-trail` to Phase 2, treat `contour-portrait` as its own design track (§I).

**Runtime experiments needed** (cannot be resolved by static inspection — see also the "Required runtime experiments" list below, none of which were run in this session beyond the buildability check noted there):
- Whether `fragment-trail`'s single-clip-for-life behavior is intentional or an oversight (§I item 5).
- What "previous" should mean once implemented — replay-last-shown vs. step-backward-through-shuffle-order — since no current scene defines this even implicitly (§D, "Previous behavior" row).
- Retry/skip policy on a load failure mid-playlist (§D, "Retry / skip behavior" row).
- Actual Pi 3B+ memory/CPU cost of `TimeOffsetVideoBuffer`'s per-frame `resize()` call, independent of any service migration (§E, "Pi 3B+ risks").
- Temporal-history continuity across a media change, under whatever selection-service adapter is eventually built for `temporal-fields` (§E's recommended boundary, unvalidated at runtime).

**Architecture decisions needed** (per `01-architecture-governance.md`'s review triggers — these change canonical asset roots and/or introduce a new runtime service, both explicit triggers):
- The real canonical media-root path and its relationship to the already-hardcoded (but not-yet-real) `assets/shared/media/` in `ExperienceRuntime.cpp:44` (§F.3).
- Whether `IVideoPlaybackService`/`IVideoSelectionService`-shaped types become a new `RuntimeServices`-owned service at all — `Scene-HUD-Contract-v1.md §15` item 1 already lists "video-player wrapper unification" as open, and `01-architecture-governance.md`'s trigger list explicitly includes "introduces a new runtime service" and "creates a second source of truth for video" (§H).
- Where `quadrant-crosshair`'s post-process color-grade sits relative to a shared texture contract (§B.4/§H.1).
- Whether `contour-portrait`'s image/video/camera mode-multiplexing is preserved as-is (with only the video leg touching the new service) or redesigned — camera is out of the runtime target per `00-shared-project-context.md:43`, but the mode-switch *mechanism* itself is live code that a service design needs to either accommodate or explicitly declare out of scope (§B.5).
- Canonical hold-timer shape (wall-clock, loop-count, or both) — currently three different answers exist in the codebase for what reads as one product concept (§D, "Hold timer" row).

---

## Required Runtime Experiments — Status

Per the task's own instruction not to run expensive/behavior-changing experiments unless necessary, and to report clearly if the environment cannot support them:

- **Buildability**: existing `bin/*.app` bundles for all six runtime scenes carry recent timestamps, indicating each sketch has built and been run successfully on this machine before. A fresh incremental `make Release` was attempted for `blob-region-prototype` in this session; it did not complete within the session's available time budget and was not force-completed, consistent with the instruction to avoid expensive builds — **this is reported as attempted-but-inconclusive, not as a pass or fail**. No sketch was rebuilt from clean, and no sketch was run interactively.
- **Startup with an empty media directory / one valid file / one invalid file**: **not run** — would require populating/emptying real scene media directories, which risks disturbing the shared `blueprint_emergence` media root every other scene depends on (§F.1). **Unverified — proposed test**: point a scratch copy of one wrapper's `setup()` at an empty temp directory and confirm the already-visible warning-log-only behavior (§D) is the full extent of the failure mode, with no crash.
- **Next/previous with one file, next/previous with multiple files, no-immediate-repeat, hold-timer reset after manual navigation, media failure recovery, temporal-history continuity after media change, resource release/reload**: **not run** — all require either interactive/instrumented execution or code changes (adding logging/assertions) that exceed this probe's "discovery only, no code changes" boundary. Each is marked `Unverified` in the relevant section above (§C/§D/§I) with the specific runtime check that would resolve it.

No results were fabricated for any of the above; every claim in §C/§D about *current* behavior is instead grounded directly in the control-flow read from source (cited by file:line throughout), which is a materially different (and weaker) form of evidence than an observed run — flagged here explicitly per the task's own instruction to distinguish the two.

---

## Final Report — Required Format

**1. Summary**: No shared video-playback service exists across the six approved runtime scenes; five structurally distinct wrapper implementations (`VideoSampler`, `TimeOffsetVideoBuffer` + a drifted `fragment-trail` fork, `VideoSystem`, `ContourSource`) currently own decode, selection, and status independently. DEC-006 ("standardized video playback required") is not yet close to true in code: only one scene (`quadrant-crosshair`) has a manual next command and none has manual previous, despite `PreviousMedia` already being frozen in `SceneContract.h`. The de facto canonical media root (`blueprint_emergence/bin/data/media`) exists by accident, and a third, different, not-yet-real path (`assets/shared/media/`) is already hardcoded in the unfinished `ExperienceRuntime` skeleton — this conflict needs an Architecture decision before any media-root migration. `TimeOffsetVideoBuffer`'s decoder/history boundary is already clean at the API level and does not need to be rewritten to support a shared selection service sitting in front of it. Full findings, evidence, and a proposed (not approved) target ownership split are above.

**2. Files inspected**: All required authoritative docs (`Ecopunk-HUD-System-Master-Roadmap.md`, `Scene-HUD-Contract-v1.md`, `scene-observability-profile-probe-report.md`, `00-shared-project-context.md`, `01-architecture-governance.md`, `02-cross-domain-handoff-protocol.md`, `03-decision-log.md`, plus `CLAUDE.md`); `shared/src/VideoSampler.{h,cpp}`, `shared/src/TimeOffsetVideoBuffer.{h,cpp}`, `shared/src/scene/SceneContract.h`; per-scene sources for all six runtime scenes (`ofApp.{h,cpp}` plus each scene's video wrapper — `ContourSource`, `VideoSystem`, the `fragment-trail` `TimeOffsetVideoBuffer` fork, `BEComposition`/`BESettings`); non-runtime precedents `RPVideoSampler.{h,cpp}` (both copies) and `shader-effect-debugger/src/ofApp.{h,cpp}`; the `sketches/experience_runtime/` skeleton (`ExperienceRuntime.{h,cpp}`, `SceneManager.{h,cpp}`, `FakeScene.{h,cpp}`); `tools/convert_media.sh`; on-disk media directories and symlinks for all nine sketches under `sketches/`; `config.make`/`Makefile` files (grepped for `PLATFORM_PI`).

**3. Files changed**: None, except this report (`docs/video-playback-ownership-probe-report.md`, newly created).

**4. Tests/builds run**: One incremental `make Release` attempted for `blob-region-prototype`; did not complete within the session's time budget, no result obtained (see "Required Runtime Experiments — Status" above). No other builds or runtime tests were executed.

**5. Results**: See §A–§J above for the full findings. Headline results: 5 incompatible wrappers, 1-of-6 scenes with manual next, 0-of-6 with manual previous, 3 incompatible media-root conventions, 1 confirmed fork-drift (`fragment-trail`), 1 confirmed destructive getter (`VideoSystem::fileChanged()`), 1 confirmed documentation/code drift (`temporal-fields`' media root).

**6. Deviations from prompt**: None substantive. The prompt's own "Conceptual API options" section was answered with two options as requested (§H). The prompt's "Required runtime experiments" section was addressed by attempting the one experiment that could be run without risking shared media/source state (a build), and explicitly marking the rest `Unverified`/`not run` with proposed checks, per the prompt's own instruction to avoid expensive or behavior-changing experiments and to report plainly if the environment can't support them rather than fabricate results.

**7. Newly discovered risks**: (a) `fragment-trail`'s per-file media symlinks use absolute, machine-specific paths — a portability hazard independent of this migration; (b) `temporal-fields`' media root has silently changed shape since the last probe report describing it; (c) `ExperienceRuntime.cpp` already hardcodes a canonical media path that has no corresponding directory anywhere in the repo, a latent conflict waiting to surface the moment real scene adapters are wired to it; (d) `radar-effects-gallery`/`radar-pulse` hardcode `/home/pi/blueprint/media` with no platform guard at all (non-runtime, but worth flagging to whoever maintains those sketches).

**8. Contract changes requested**: None implemented or proposed as a fait accompli. Per §J, the following are flagged as needing Architecture review before any contract or shared-header change: the real canonical media-root path; whether a video-playback service becomes a new `RuntimeServices` member; where `quadrant-crosshair`'s post-process color-grade sits relative to a shared texture contract; the canonical hold-timer shape. No change to `SceneContract.h` was made or is proposed by this report.

**9. Recommended next step**: Bring §F.3's media-root conflict (`media/` vs. `assets/shared/media/`) to Architecture review before any other video-playback work proceeds — every other recommendation in this report (the selection/decode split in §H.2, the migration order in §I) is compatible with either outcome, but building against the wrong root first is the one mistake in this inventory that would force rework across all six scenes' adapters rather than just one.
