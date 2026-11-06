# Tier C proposal — RAW VIDEO colour snapshot for Orbit Console (additive)

Status: PROPOSAL to Architecture (decision requested, see section 8 for the revised recommended semantics). No public type, `HudFrameData` field, semantic source or Shared Video contract has been edited.
Written for: Architecture (decision), ExperienceRuntime and Shared Video owners (implementation), HUD Runtime (consumer).

## 1. Finding (what exists today)
- `VideoPlaybackService` (shared/src/video-playback) is the single owner of the active media. It exposes `currentTexture()` (the decoded frame texture, nullptr unless Ready), `currentPixels()`, `sourceSize()`, `isFrameNew()` and `status()` (immutable `VideoPlaybackStatus`: `mediaId`, health, timing, `selectionOrigin`). All of it is upstream of scene shaders/effects.
- No decoded-video colour measurement, snapshot or semantic source exists. `SceneColorSnapshot` / `runtime.scene_color` measure the final SceneFBO only; it is NOT evidence of a RAW source and RAW must never be derived from it.
- Caveat on "the frame the scene shows": Temporal (and any `TimeOffsetPlaybackAdapter` consumer) explicit-loads the same file into its own buffered player and plays at a time offset, so the service's `currentTexture()` is the *live head* of the media, not necessarily the frame the scene is displaying. The proposal measures the live decoded frame and says so in the source identity.

## 2. Existing boundary
`HudFrameData` (runtime -> HUD) carries `sceneColor` (ratified). Sampling and readback live with the runtime/media owner; the HUD never reads back.

## 3. Proposed additive change (nothing else)
- New OF-free header `shared/src/hud-runtime/VideoColorSnapshot.h`: the same sample/ring layout as `SceneColorSnapshot` (linear-light average RGB, Rec.709 relative luminance, monotonic `t`, `valid`, `kCapacity=120`, 2 Hz, stale after 3 periods) plus identity: `std::string mediaId` (from `VideoPlaybackStatus::mediaId`), `uint64_t mediaGeneration` (bumped on every media change/reload), `uint64_t sceneGeneration`, `State {Unavailable, Valid, Stale}`.
- `HudFrameData::videoColor` : `std::shared_ptr<const videocolor::VideoColorSnapshot>` (null = none yet).
- Semantic source ID `runtime.video_color` (newest valid luminance, only while Valid) mirroring `runtime.scene_color`.

## 4. Source identity and multi-video policy
- One source: the media the Shared Video service currently has active. Scenes with several simultaneous streams (none today) would require a separate ruling; RAW is labelled "RAW VIDEO", never "scene".
- No video / procedural scene / health != Ready: `Unavailable` (HUD shows NO RAW VIDEO), no fabricated samples. A media change starts a new `mediaGeneration`; history is cleared, never smeared. A frame that is not new (`isFrameNew()==false` for several periods, paused/decoding stall) produces gaps, then Stale.
- Timestamp domain: the same steady runtime clock as `sceneColor.capturedAtSeconds`; FX and RAW are sampled independently (not forced to share an instant). The HUD aligns them by timestamp only and shows missing slots; it never invents synchronised samples.

## 5. Sampling seam and lifetime/performance
- Owner: ExperienceRuntime/Shared Video, not HUD and not scenes. Reuse the existing `SceneColorSampler` reduction (RGBA16F GPU linear-light reduction, two passes, tiny float readback; deferred readback option) by constructing a second instance bound to `VideoPlaybackService::currentTexture()` (texture, not CPU pixels, so no decode-thread or CPU full-frame average). `SceneColorSeries`/`SnapshotPublisher` are reused unchanged.
- Alpha: video textures are opaque RGB; sRGB-encoded 8-bit assumed (same validation as the SceneFBO; unsupported formats become gaps).
- Cost: same order as the FX sampler (desktop about 3.4 ms/sample sync at 2 Hz); must be measured separately on the Pi and enabled independently (`EXPERIENCE_RUNTIME_VIDEO_COLOR=0/1`). Memory: one 120-sample immutable snapshot rebuilt per sample, one refcount copy per frame.
- Shared Video decode and texture ownership are unchanged; the sampler only reads the texture.

## 6. Consumers
HUD Orbit Console RAW group (`OrbitSeriesView raw`, already implemented in the model with a gated SYNTHETIC fixture `EXPERIENCE_RUNTIME_ORBIT_RAW_FIXTURE`), `runtime.video_color` resolver.

## 7. Tests proposed
Same set as the FX series (colour space, Rec.709, valid black vs unavailable, gaps, media-generation reset, revision/publication, stale, bounded history), plus: no-video scene shows Unavailable; a scene shader changes the FX series but not the RAW series for the same input; media change while Loading; sampler off = no cost.

## 8. Decision requested (v0.2 — recommended semantics revised)
Approve the additive header, `HudFrameData::videoColor` and `runtime.video_color`, and rule on the seam.

**Recommended semantics (supersedes the "live decoded frame" wording in sections 1 and 3):** RAW measures the actual pre-effects video frame used by the active scene, not necessarily the decoder's live head. For a Temporal-style adapter that is the offset frame it binds, not `VideoPlaybackService::currentTexture()`.

Required properties of the ruling:
- Shared Video keeps ownership of decode and textures; no competing decoder.
- Measurement is scene-independent where possible: a Shared Video-side hook on the frame handed to the scene (including the adapter's offset frame), not a per-scene sampling branch.
- Monotonic timestamps; explicit scene and media identity (`mediaId`, `mediaGeneration`, `sceneGeneration`); Valid/Stale/Unavailable; bounded history (120 samples).
- No change to scene rendering policy.

Open for the Shared Video owner to confirm (not verified by this proposal): where the adapter's offset frame is exposed so a sampler can read it without a second decode. If it cannot be exposed scene-independently, Architecture should rule between the live-head fallback (labelled as such) and keeping RAW unavailable for those scenes.

Until ruled, the HUD keeps showing NO RAW VIDEO. The FX sampler and HUD presentation proceed independently.
