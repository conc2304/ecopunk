# OC-003B — RAW Video Reference Semantic Contract (addendum)

Status: PROPOSED contract addendum for Architecture review. Operator-approved narrow semantic definition. Nothing is implemented; no code, public type, `HudFrameData` field, Shared Video contract, scene, HUD-004 manifest or HUD-006 geometry has been edited.
Written for: Architecture (acceptance), ExperienceRuntime and Shared Video owners (implementation handoff), HUD Runtime (consumer).
Amends: [orbit-console-raw-video-tier-c-proposal.md](orbit-console-raw-video-tier-c-proposal.md) section 8 (see 9.1). Does not reopen Tier C scene-colour authorization.

## 1. Inspection (Phase 1)

| Finding | Evidence |
|---|---|
| `VideoPlaybackService::currentTexture()` returns the decoder's current texture, `nullptr` unless health is `Ready` **or `Degraded`** | `VideoPlaybackService.cpp:247` |
| `isFrameNew()` is `false` outside Ready/Degraded; `sourceSize()` is exposed | `VideoPlaybackService.h:98-101` |
| Media identity is `VideoPlaybackStatus::mediaId` (catalog-authored, atomic with health). The status carries no media-generation counter. | `VideoPlaybackStatus.h:60-70`, DEC-013 |
| The service is the single owner of ordinary playback (DEC-013); Temporal-style history stays outside it (DEC-014). Temporal and Fragment read their own `TimeOffsetVideoBuffer`/`TimeOffsetPlaybackAdapter`. | Decision-Log-v3.md, `TemporalProductionScene.cpp`, `FragmentProductionScene.cpp`, `FTBackgroundLayer.h` |
| FX = `SceneColorSampler::onLiveFrame(sceneFbo_.getTexture())`, scene generation bumped on every activation in `ExperienceRuntime`; snapshot published via `SnapshotPublisher` into `HudFrameData::sceneColor` | `ExperienceRuntime.cpp:823-858, 919` |
| The HUD already has a RAW slot: `OrbitConsoleModel::update(fx, layout, raw*)`, unavailable = nothing drawn, never borrows FX. No runtime RAW source exists (gated synthetic fixture only). | `OrbitConsoleModel.h` |
| `runtime.scene_color.raw` and `runtime.video_color` are defined nowhere in code or ratified docs; `runtime.video_color` appears only in the unratified proposal. | repository search |

**Frozen-contract incompatibility: none found** for the definition below (all additive; see section 9).

Two things to settle before relying on this contract:
- `currentTexture()` is non-null in `Degraded` as well as `Ready`. This contract makes `Ready` the only eligible state (section 2).
- The sampler's source validation was written for the 8-bit, display-encoded SceneFBO. The decoder texture's format and target (rectangle vs 2D) are not verified here (acceptance item 8).

## 2. Canonical RAW source definition (Phase 2)

**Source ID:** `runtime.scene_color.raw` (the proposal's `runtime.video_color` is superseded; no ratified equivalent exists). Resolver value type `Scalar`, like `runtime.scene_color`: the newest valid relative luminance, only while the snapshot is Valid.

**Meaning:** a colour measurement of the canonical live decoded-video texture, `VideoPlaybackService::currentTexture()`, independent of any scene-specific transformation. Same measurement definition as `SceneColorSnapshot.h` (composite over black is a no-op for opaque video; sRGB EOTF before averaging; linear-light average RGB; Rec.709 relative luminance), with the source texture substituted.

**Label:** "RAW VIDEO". It must never be described as the frame the scene shows.

**Availability:** only while the service health is `Ready` and the texture is non-null and passes the sampler's format validation. `Degraded` is not eligible (conservative; may be revisited by Shared Video). Otherwise Unavailable, with no samples fabricated and never derived from FX.

**Not guaranteed** (each must be stated wherever the source is documented):
1. Correspondence to the exact frame currently displayed.
2. Frame-for-frame synchronization with FX.
3. Coverage of scene-specific time-offset buffers (Temporal, Fragment): for these scenes RAW is the canonical live head, which may be a different frame than the scene shows.
4. Coverage of multiple simultaneously displayed source frames.
5. Representation of procedural scene output (a scene that draws no decoded video still gets a RAW series if media is Ready; the HUD must not read that as scene content).

## 3. Temporal contract (Phase 3)

| Aspect | Rule |
|---|---|
| Sample timestamp `t` | Monotonic steady-clock seconds, the same domain as `sceneColor.capturedAtSeconds`, taken when the sampler **issued** the reduction (deferred readback keeps its issue time, as FX does). It records when RAW was **observed**, not which media frame position it holds. |
| Alignment | Observation-time alignment only. Equal or nearby `t` between RAW and FX does **not** imply equal playback positions; RAW is the live head and FX is the composited scene output. The HUD aligns by `t` for display and never invents synchronised pairs. |
| Sampling cadence | Independent of FX (2 Hz target, 120-sample ring, stale after 3 missed periods, same constants). Either series may have samples the other lacks. |
| Media identity | Snapshot carries `mediaId` (from `VideoPlaybackStatus`) of the media measured. A change of `mediaId` starts a new `mediaGeneration` (bumped by the runtime; the status has no such counter and none is added). |
| Media replacement / reload | New `mediaGeneration`, history cleared, never smeared across media. An in-flight readback issued under the old generation is dropped. A reload of the same `mediaId` also bumps `mediaGeneration`. |
| Scene generation | Snapshot also carries `sceneGeneration` and `sceneId` (same values as FX). A scene change clears RAW history, as FX does, so the two series are always comparable per scene. |
| Ready to unavailable | Not Ready: no new samples, state Unavailable once history is cleared; a transient Loading during a media change is a new generation, not a gap. |
| Unavailable to Ready | New `mediaGeneration` begins; state stays Unavailable until the first valid sample. |
| Stale | A valid sample exists but none newer for 3 periods (decode stall, pause, `isFrameNew()` false throughout): Stale. Values shown dimmed; the resolver returns no scalar while Stale. |
| Gaps | A due sample with no usable texture or unsupported format is a gap (`valid=false`), never a zero. A valid black frame is `valid=true`, all zeros. |
| Independent availability | RAW may be Valid while FX is Unavailable and vice versa (for example a transition static frame). Neither series is a fallback for the other. |
| Playback discontinuity | A loop restart, seek or hold-advance within the same media is not a reset: the series continues, and samples across the jump are simply of different media positions. The contract defines no discontinuity marker; the HUD must not infer smoothness from adjacent samples. |

## 4. Integration boundary (Phase 4)

Smallest additive seam, ExperienceRuntime-owned, nothing in HUD Runtime reads GPU state:
- A second `scenecolorsampling::SceneColorSampler` instance in `ExperienceRuntime`, fed `videoPlayback_.currentTexture()` (texture, not CPU pixels) at the same point in the frame as the FX sampler, before HUD composition. `SceneColorSeries` and `SnapshotPublisher` are reused unchanged; the snapshot adds the identity fields via a thin, OF-free `shared/src/hud-runtime/` header (name left to the implementer; the earlier proposal used `VideoColorSnapshot.h`).
- Additive `HudFrameData` field carrying `std::shared_ptr<const ...>` (null = none yet); additive canonical source `runtime.scene_color.raw` in the resolver; Orbit's existing RAW slot consumes it (`OrbitSeriesView raw`). No change to production Orbit presentation is part of this contract.
- Switch `EXPERIENCE_RUNTIME_VIDEO_COLOR=0/1` (off by default until measured), independent of `EXPERIENCE_RUNTIME_SCENE_COLOR`.
- Not changed: Shared Video decode and texture ownership, selection policy, `VideoPlaybackStatus`, Temporal/Fragment/other scene playback, scene rendering.

Additional cost (to measure, not claimed): the existing FX sampler measured about 3.8 ms average and 11.7 ms max per sample on desktop (sync, about 0.57 ms first async sample), almost all CPU-side submission of the reduction chain, paid at 2 Hz; the earlier proposal budgeted 0.3 ms and the FX sampler exceeded it. A second instance should be expected to cost about the same again. The two samples should not be scheduled in the same frame (stagger by half a period). Pi 3B is unmeasured for both.

## 5. Deliverable summary (Phase 5)

1. **Canonical definition:** colour measurement of `VideoPlaybackService::currentTexture()` while Ready; section 2.
2. **Source ID and fields:** `runtime.scene_color.raw` (Scalar, newest valid luminance while Valid). Snapshot fields: everything in `SceneColorSnapshot` (`revision`, `capturedAtSeconds`, `state`, `colorSpace`, `nominalPeriodSeconds`, `count`, `history[]` of `{t, r, g, b, luma, valid}`) plus `mediaId`, `mediaGeneration`, `sceneGeneration`, `sceneId`.
3. **Availability and validity:** Ready only; Unavailable / Valid / Stale; gaps are `valid=false`; sections 2 and 3.
4. **Timestamp and generation rules:** section 3.
5. **Playback-alignment limitations:** section 2 (not guaranteed) and section 3 (observation time is not media-frame alignment).
6. **Owner and seam:** ExperienceRuntime owns the second sampler and snapshot publication; Shared Video owns the texture and `mediaId` and is unchanged; HUD Runtime is a consumer only. Section 4.
7. **Costs to measure:** per-sample submit/wait/convert/push time (existing `Stats` stages) for the second instance, desktop and Pi 3B, sync vs deferred; frame-time spike at 2 Hz with both samplers running; extra GPU memory for a second reduction chain; snapshot rebuild time.
8. **Acceptance criteria:**
   - No video, Loading, Failed or Unavailable media: RAW Unavailable, HUD shows NO RAW VIDEO, no samples.
   - `Degraded`: RAW Unavailable (or, if Shared Video rules otherwise, documented).
   - A scene shader changes the FX series but not the RAW series for the same media.
   - Media change starts a new `mediaGeneration`, clears history, and drops an in-flight readback.
   - Scene change clears RAW history with the same `sceneGeneration` as FX.
   - Stale after 3 periods; resolver returns no scalar while Stale or Unavailable; valid black is zeros and valid.
   - Decoder texture format and target (rectangle/2D, bit depth, encoding) validated or the frame is a gap, with the unsupported count logged once.
   - Bounded history (120), immutable snapshot, one refcount copy per frame; sampler off (`=0`) costs nothing.
   - Same accuracy method as the FX reference test against a CPU reference on real decoded frames.
   - Measurements from item 7 recorded, including Pi 3B, before enabling by default.
   - Temporal and Fragment playback, `VideoPlaybackStatus`, HUD-006 geometry, HUD-004 manifests and production Orbit presentation unchanged.
9. **Frozen-contract conflicts:** none for this definition (section 9.1 is a supersession of an unratified proposal, not of a frozen contract).

### 9.1 Relationship to the Tier C proposal v0.2

The proposal's recommended section 8 semantics ("the actual pre-effects frame used by the active scene, including the adapter's offset frame") required a scene-independent hook that was not confirmed to exist. This addendum instead adopts the narrower live-head definition and documents its limits, as the proposal itself listed as the fallback. If Architecture accepts this addendum, section 8 of the proposal should be marked superseded; `runtime.video_color` is replaced by `runtime.scene_color.raw`.

## 6. Decision log and handoff

- Decision log: a draft DEC entry (proposed, for Architecture to number) would read: "RAW VIDEO is the canonical live decoded-video reference from `VideoPlaybackService::currentTexture()`, source `runtime.scene_color.raw`, measured by a second ExperienceRuntime-owned sampler; it is not frame-aligned with FX and does not cover scene-specific time-offset buffers." Not written to `Decision-Log-v3.md`, which Architecture owns, until it accepts this addendum.
- Cross-domain handoff: target domains ExperienceRuntime and Shared Video; contract impact is additive; dependencies are the Shared Video owner confirming `Ready`-only eligibility and the decoder texture format; acceptance criteria are item 8 above. Filing this in the handoff record is left to the session-close report.

Stop point: semantic contract and implementation handoff only. No implementation, staging, commit or push performed.
