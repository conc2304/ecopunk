# Phosphor Contours — request to add an eighth scene to the Production roster (Tier C, roster scope)

Status: APPLIED on the user's explicit instruction (2026-11-12): `phosphor-contours` is now registered unconditionally as the
EIGHTH scene of `ProductionRoster::Production`, with no flag. The user's approval is not an Architecture/domain-manager
acceptance: the official roster record still needs updating by Architecture, and Pi 3B+ feasibility remains unmeasured.
`EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_SCENE` now only selects it as the startup scene. The opt-in flag
(`phosphorContoursOptIn_`) was removed. Harness rosters are unchanged; the CRT selector and FX gallery harnesses now
expect eight production scenes.

## What exists

- Shared render core `shared/src/phosphor-contours/` (header-only `phc::Pipeline`), consumed by the standalone sketch
  `sketches/phosphor-contours` and by `PhosphorContoursScene` in `sketches/experience_runtime`.
- `PhosphorContoursScene` implements the existing `IEcopunkScene` (setup/activate/deactivate/update/draw/reset/shutdown),
  reads `VideoPlaybackService::currentTexture()` only, advertises existing `SceneCommand`s only (NextMedia, PreviousMedia,
  NextMode = one of four modes TOPOLOGY / ISOLINES / SCANLINES / STRATA, Regenerate = reseed; two Tier C option axes DENSITY and DECAY via `capabilities().optionAxes` + `stepOption`), publishes health + mode through the existing `SceneHudStatus`, and gets the
  data-only `phosphor-contours` HUD presentation profile for the two selector rows (same shape as the FX gallery profile; no scene-specific HUD rendering branch, no new semantic slot).
- Registration (now): `ProductionRoster::Production` registers it as the EIGHTH ring entry. `EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_SCENE=1`
  (`ExperienceRuntime::installPhosphorContoursProductionScene`) only starts the run in it.

## Decision requested from Architecture

Approve the scene id and ring position below and update the official roster record, or reject / defer.

- Scene id: `phosphor-contours` (display name `CONTOURS`)
- Position: eighth, appended after `shared-effects-gallery` (NextScene ring order:
  blob, temporal, blueprint, fragment, quadrant, crt-phosphor, shared-effects-gallery, **phosphor-contours**)
- Roster change on approval: `ProductionRoster::Production` registers it unconditionally; the opt-in flag
  (`phosphorContoursOptIn_`) and env variable are then removed. Harnesses keep the rosters they were accepted against.

## Not requested / unchanged

- No change to `SceneContract.h`, `SceneCommand`, `SceneManager`, the HUD layout, the 19-region contract, skins, or
  semantic slot ids. No new shared effect-catalog entry, so `EffectActivityStatus` ownership is untouched (the scene has no
  canonical effect producer: `effects = nullopt`).
- Pi 3B+ feasibility is UNMEASURED. A `PiSafe` quality profile exists (half-res contours, quarter-res luma, 1/8 glow,
  no persistence) but is a hypothesis until measured on hardware. Do not enable in the Pi rotation on this request alone.
