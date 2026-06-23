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

## Deploy target

Raspberry Pi 3B (quad-core Cortex-A53, 1GB RAM, VideoCore IV GPU) — notably
weaker than the Rock Pi 4B/RK3399 target used by the sibling `FireplaceWaterfall`
app. Confirmed scope: 1-2 simultaneous video layers, so plain
`ofVideoPlayer` (GStreamer-backed on Linux) should be sufficient without
needing OMX/MMAL hardware-decode plumbing. Revisit this if the layer count
grows — the Pi 3B's GPU does not have headroom for naive multi-stream
software decode.
