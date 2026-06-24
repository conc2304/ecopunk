# EcopunkVideoCollage

Multi-sketch openFrameworks monorepo. macOS dev, **Raspberry Pi 3B**
deployment target.

```text
shared/src/             # code shared by every sketch (GridSystem, AnnotationRenderer,
                         # Settings.h color tokens), pulled in via PROJECT_EXTERNAL_SOURCE_PATHS
sketches/
  blueprint_emergence/   # first sketch — generative archival/specimen animation system
                         # see docs/blueprint_emergence_engineering_plan.md
```

Each sketch under `sketches/<name>/` is generated with OF's `projectGenerator`
CLI (`projectGenerator -o <of_root> -s ../../shared/src sketches/<name>`) so
relative paths to the OF root and to `shared/src` are computed correctly.

## Build

```bash
cd sketches/blueprint_emergence
make Release -j4
make RunRelease
```

Or open `sketches/blueprint_emergence/blueprint_emergence.xcodeproj` in Xcode.

## Media

Drop source footage here, per sketch:

```text
sketches/blueprint_emergence/bin/data/media/
```

Format (see `docs/blueprint_emergence_engineering_plan.md` §05):

| Property | Requirement |
|---|---|
| Container | MP4 (`.mp4`) |
| Codec | H.264, Baseline or Main profile |
| Resolution | 720p max; 540p preferred for multi-video |
| Frame rate | 24 or 30fps source |
| Duration | Minimum 30s |
| Audio | Stripped or muted |
| Color space | sRGB, no HDR/Log |

Files dropped in `media/` are gitignored (the folder itself is tracked via
`.gitkeep`, the contents are not) — no size limit worries from committing
footage by accident. Not consumed by the app yet: `VideoSampler` (engineering
Phase 3) is what actually reads this folder; until then it's just a chosen,
documented drop point.

## Deploy target

Raspberry Pi 3B (quad-core Cortex-A53, 1GB RAM, VideoCore IV GPU) — notably
weaker than the Rock Pi 4B/RK3399 target used by the sibling `FireplaceWaterfall`
app. Confirmed scope: 1-2 simultaneous video layers, so plain
`ofVideoPlayer` (GStreamer-backed on Linux) should be sufficient without
needing OMX/MMAL hardware-decode plumbing. Revisit this if the layer count
grows — the Pi 3B's GPU does not have headroom for naive multi-stream
software decode.
