# EcopunkVideoCollage

openFrameworks app. macOS dev, **Raspberry Pi 3B** deployment target.

Scaffolded from the stock openFrameworks `emptyExample` template (not the
`projectGenerator` GUI/CLI) on 2026-06-22.

## Build

```bash
# macOS
make Release -j4
make RunRelease
```

Or open `EcopunkVideoCollage.xcodeproj` in Xcode and build the
**EcopunkVideoCollage** target.

**Addons:** none yet — add via `addons.make` (one addon name per line).

## Deploy target

Raspberry Pi 3B (quad-core Cortex-A53, 1GB RAM, VideoCore IV GPU) — notably
weaker than the Rock Pi 4B/RK3399 target used by the sibling `FireplaceWaterfall`
app. Confirmed scope: 1-2 simultaneous video layers, so plain
`ofVideoPlayer` (GStreamer-backed on Linux) should be sufficient without
needing OMX/MMAL hardware-decode plumbing. Revisit this if the layer count
grows — the Pi 3B's GPU does not have headroom for naive multi-stream
software decode.
