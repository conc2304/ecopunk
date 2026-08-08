# shared/src/video-playback/

The shared video playback subsystem — canonical media catalog,
media-root resolution, playlist/selection history, automatic selection
policy, hold timer, previous/next semantics, active decoder, media health,
and the immutable `VideoPlaybackStatus` snapshot — per
`Implement-Shared-Video-Playback-System-Agent-Prompt.md` and
`docs/video-playback-ownership-probe-report.md`.

```text
ExperienceRuntime
└── RuntimeServices
    └── VideoPlaybackService
```

## Files

| File | OF-dependent? | Purpose |
|---|---|---|
| `VideoPlaybackStatus.h` | No | Immutable per-frame status snapshot type (`VideoPlaybackHealth`, `MediaSelectionOrigin`, `VideoPlaybackStatus`). |
| `MediaMetadata.h` | No | One catalog entry (`mediaId`, `relativePath`, `titleId`, `fallbackDisplayTitle`, `enabled`, `piSafe`). |
| `MediaCatalog.h` / `.cpp` | No | Pure catalog validation/synthesis: turns a list of discovered files plus optional curated overrides into a validated `vector<MediaMetadata>`. |
| `MediaCatalogJsonLoader.cpp` | Yes (nlohmann::json only, no GL) | Parses an optional catalog JSON file into override rows for `MediaCatalog::build()`. |
| `VideoSelectionPolicy.h` / `.cpp` | No | Pure playlist/session-history/hold-timer/no-repeat decision logic — the "which candidate next, what happened after confirmation" state machine. |
| `IVideoDecoder.h` | Yes | The decoder seam (`ofTexture`/`ofPixels`/`glm`) between the pure policy above and a real decoded video source. |
| `OfVideoDecoder.h` / `.cpp` | Yes | Concrete `IVideoDecoder` backed by one `ofVideoPlayer`. |
| `VideoPlaybackService.h` / `.cpp` | Yes | The public service: owns catalog + policy + decoder, orchestrates setup/retry-on-failure/status assembly. This is the only class a scene adapter or `RuntimeServices` should ever touch directly. |

## Why the pure/OF-dependent split

`MediaCatalog` and `VideoSelectionPolicy` have **zero** openFrameworks
dependency on purpose, matching the standalone-test convention already
established elsewhere in this repo (`sketches/blob-region-prototype/test/`,
`sketches/experience_runtime/test/`, `sketches/temporal-fields/test/`) —
even `ofRectangle.h` alone pulls in `GL/glew.h` transitively, so anything
meant to be unit-tested with a bare compiler has to avoid every
openFrameworks include, not just the obviously heavy ones.

See `test/Makefile.tests` for how to build and run those tests, and that
file's own header comment for an important caveat about
`PROJECT_EXCLUSIONS` in any sketch that pulls in all of `../../shared/src`
rather than a scoped subset.

## Using the service

```cpp
#include "VideoPlaybackService.h"

VideoPlaybackService playback;  // production default: owns a real OfVideoDecoder

VideoPlaybackService::Config config;
config.mediaRoot = ofToDataPath("media", true);
config.holdDurationSeconds = 30.0f;
config.automaticAdvance = true;
playback.setup(config);

// per frame:
playback.update(dt);
const ofTexture* tex = playback.currentTexture();
VideoPlaybackStatus status = playback.status();

// manual navigation:
playback.next();
playback.previous();

// teardown:
playback.shutdown();
```

## What this subsystem deliberately does not do (yet)

- It does not replace or reach into `TimeOffsetVideoBuffer` — temporal-
  history scenes (`temporal-fields`, `fragment-trail`) are a later
  migration stage; see the implementation report for this increment for
  exactly what was and wasn't done.
- It does not migrate the canonical media root to
  `assets/shared/media/` — every existing scene's physical media location
  is left untouched; see the implementation report's "Deviations from this
  prompt" / "Remaining migration work" for why, and
  `docs/video-playback-ownership-probe-report.md` §F for the full
  media-root inventory this decision is based on.
- It does not add a catalog *file* to any scene's media directory — every
  scene currently runs on auto-synthesized catalog entries only. A
  catalog file is fully supported (`Config::catalogPath`) but purely
  additive/optional.
