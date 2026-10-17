# Blob Per-Video Detection Presets — Mac Implementation Report

**Date:** 2026-10-17
**Status:** IMPLEMENTED — PER-VIDEO PRESETS WORK ON MAC (manual presets only; not Pi-validated)

## 1. Source inspection

| Question | Answer |
|---|---|
| Detector config owner | `BlobDetector::Config` (`shared/src/BlobDetector.h`), constructed in `BlobSceneCore::setup()`. The standalone app rewrites it from GUI params every frame (`ofApp::syncParamsToSystems()`). Production never changed it until now |
| Standalone controls | enabled, analysisWidth/Height, processEveryNFrames, threshold, min/maxBlobArea, maxBlobs, erode, dilate (ofxGui) |
| Production media-change point | `BlobProductionScene::update()`, from `VideoPlaybackService::status().mediaId` |
| Media identity | `media.auto.` + FNV-1a-32 of the clip's path relative to the media root (`MediaCatalog::synthesizeMediaId`), or a curator-authored override ID. Deterministic across runs and platforms, and reproducible offline (`Fern_Foliage.mp4` → `media.auto.3838009d`, matching the runtime). Renaming or moving a clip changes its ID |
| JSON | nlohmann/json, already used by `MediaCatalogJsonLoader` and bundled with openFrameworks |
| Asset root | `assets/shared/…`, resolved as `ofToDataPath("../../../../assets/shared/…")` like the shared media root |
| Shared by both apps | Yes, with no Shared Video change. Both apps load the same catalog root, so they see the same media IDs |

No identity gap; no stop condition hit.

## 2. Files

**New**
- `shared/src/BlobDetectionProfileStore.{h,cpp}`: profile type, store, tracker. OF-free.
- `shared/src/BlobDetectionProfileApply.h`: converts to and from `BlobDetector::Config`; defaults; path.
- `assets/shared/blob/blob-detection-profiles.json`: the authored file, initially empty.
- `sketches/experience_runtime/test/blob_detection_profile_tests.cpp`.
- Symlink `experience_runtime/src/BlobDetectionProfileStore.cpp` (the sketch's existing convention for shared sources).

**Changed**
- `BlobProductionScene`: read-only load at setup; apply once per media change; test accessors.
- `blob-region-prototype/src/ofApp.{h,cpp}`: apply into the GUI; `s` / `r` / `D` keys; overlay lines; scripted `BLOB_PROFILE_DEMO`.
- `BlobLifecycleHarness`: a profile check at every media step.
- `Makefile.tests`.

## 3. Schema (real saved example)

```json
{
  "profiles": {
    "media.auto.497121bc": {
      "dilateIterations": 2,
      "erodeIterations": 0,
      "maxBlobAreaNormalized": 0.35,
      "minBlobAreaNormalized": 0.0005,
      "threshold": 31
    }
  },
  "schemaVersion": 1
}
```

- **Partial entries.** Every field is optional; absent fields mean `BlobDetector::Config{}` defaults, so default values are never duplicated.
- **Validation.** threshold 0–255; areas in [0,1] with min < max; erode/dilate 0–8.
- **Deterministic output.** Keys are sorted, with 2-space indent and tidy numbers (0.0005, not 0.000500000024).

## 4. Runtime behavior

- **Load.** At setup, production loads the file **read-only**. `EXPERIENCE_RUNTIME_BLOB_DETECTION_PROFILES` overrides the path for fixtures and scratch copies.
- **When it applies.** Once per media change, including the first media seen, and never per frame.
- **Hit.** If the media ID has a profile, it is applied (`source=per_media`).
- **Miss.** Otherwise the canonical defaults are applied explicitly (`source=defaults`), so a previous media's values never leak.
- **Log.** Each application logs the `[BLOB DETECTION PROFILE]` block (media, source, threshold, minArea, maxArea, erode, dilate).
- **Untouched.** Only the five profile fields change; other detector fields and the tracker are left alone.

## 5. Developer workflow (standalone `blob-region-prototype`)

1. Play a video (`n`/`p` to navigate) and tune Detection in the GUI.
2. Press `s` to save the current settings for the current media.
   - It merges into the file: other entries, invalid entries and unknown fields are preserved, and the write goes through a temp file plus rename.
   - It prints `Saved Blob detection profile` with the values.
   - With no media ID it logs `NOT SAVED: no canonical media id`.
3. Other keys:
   - `r`: reload the file and reapply.
   - `D` (shift+d): restore detector defaults, without saving.
4. The overlay shows the media ID, the profile source, the file status and the five live values.

Production never writes the file. There is no HUD exposure.

## 6. Mac validation

Run against a scratch file. Logs: `logs/blob-detection-presets-*-2026-10-17.log`. Evidence: `blob-detection-presets-evidence/`.

| Test | Result |
|---|---|
| A — distinct profiles | A saved as 28 / erode 0 / min 0.0005. B started on **defaults (A did not leak)**, then saved as 46 / dilate 1 / max 0.5. 3/3 |
| B — restart persistence | Fresh launch: A → 28, B → 46, unprofiled clip → defaults, B → 46, A → 28. 5/5 |
| C — no-profile fallback | Unprofiled clips get exact defaults, in both apps |
| D — update existing | A changed to 31. The JSON diff is one line (`"threshold": 28` → `31`), with B untouched. After another restart: A 31, B 46. 5/5 |
| E — malformed | Wrong schemaVersion: file rejected, defaults everywhere, saving disabled. Bad field: only that entry skipped. Missing file: defaults. Production kept running in all three |
| Production integration | Lifecycle harness with a 13-of-26-profiled fixture: each media step's detector config matched its profile or defaults (`per_media` thresholds 24 / 40 / 36 / 34, then `defaults`). 2299/2299 |
| Visual | `profile_demo_A.png`, `profile_demo_B.png`: GUI and overlay show each clip's applied values |

The first demo design started each run on whatever clip the app opened with, and startup media differs between launches. The verify and update drivers now seek the authored media IDs, as a user would by navigating back.

## 7. Builds and tests

| Check | Result |
|---|---|
| `make -C test -f Makefile.tests test` | PASS (`blob_detection_profile_tests` 175/175) |
| blob-region-prototype and experience_runtime Release builds | PASS |
| Blob lifecycle, with presets | 2299/2299 |
| RT-003 Blob→Temporal→Blob | 5351 / 0 failures |

## 8. Contract changes

None. No change to Shared Video, the HUD, SceneManager or IEcopunkScene.

## 9. Limitations

- Manual presets only: no calibration search, no runtime auto-trim. The seam for auto-trim is a saved baseline plus a temporary offset applied after `applyTo()`.
- Not Pi-validated.
- Renaming or moving a clip changes its synthesized media ID and orphans its profile. Use a catalog `mediaId` override for curated clips.
- The authored file ships empty. Example candidate: *Tenerife Shore* produced 0 detections at the defaults, likely because the merged surf blob exceeds `maxBlobAreaNormalized` 0.35.

## 10. Next step

**IMPLEMENT OFFLINE PROFILE CALIBRATION HARNESS**
