# Phosphor Contours v2 — structural banding and raster relief: evidence

All images are real output of the openFrameworks renderer (standalone `phosphor-contours` and the runtime harness), desktop GL 2.1,
AMD Radeon Pro 560X, macOS, 1280x720. Pi 3B+ is UNMEASURED. Nothing here is Pi evidence.

## Contact sheets (same frozen frame per clip, `PHC_FREEZE=0.4`, fixed dt)
Layout: row 1 source | structural luma | preferred (ReferenceDense) — row 2 bare Isophote | bare RasterRelief | bare Hybrid
(contour signal only: no dot mask, dropout, slip, grain, persistence or glow) — row 3 finished Isophote | RasterRelief | Hybrid.

- a_cloud_Lightning_Storm_contact_sheet.png (smooth gradient, very dark clip: exercises auto tonal range)
- b_beach_White_Sand_Beach_contact_sheet.png (fine texture; the original failure example)
- c_foliage_Forest_Canopy_contact_sheet.png (foliage)
- d_glass_Glass_Flower_contact_sheet.png (bright high-contrast on black; substituted for "lightning or glass": Lightning is used for (a))
Individual PNGs (source, structural_luma, bare_*, finished_*, preferred_*) sit beside each sheet.

## Runtime with HUD (opt-in scene, ReferenceDense)
runtime_06_media_White_Sand.png (beach through the clipped/scaled HUD viewport), runtime_01/02 other clips, runtime_03 SparseSignal (Isophote),
runtime_04 MeltedRaster (RasterRelief), runtime_05 after scene re-entry, runtime_07 simulated shader failure (untreated video, HUD shows DEGRADED).

## Motion
beach_motion_120f_30fps.mp4 + beach_motion_filmstrip.png (frames 0/40/80/119). Frame-accurate series (video stepped 1/30 s per frame; NOT a
real-time capture of the app). `PHC_SEQ=120`: mean |frame delta| effect 2.12/255 (max 13.05), source 0.55/255 (ratio 3.84), 0.31% of pixels change by >64
between consecutive frames. The effect changes ~4x more than the (slow aerial) source per frame because lines follow the displaced structure, plus
the 18 Hz grain clock and per-cell dropout events. Visually continuous, no crawl seen in the filmstrip; the single 13/255 peak was not
investigated. A real-time recording was not made.

## Desktop cost (`PHC_BENCH=1`, glFinish-bracketed pass cost, EMA, 1280x720, two runs; includes the range readback)
| quality | algorithm | ms/frame (run 1 / run 2) | GPU targets |
|---|---|---|---|
| Desktop | Isophote | 3.8 / 4.9 | 21.97 MiB |
| Desktop | RasterRelief | 5.8 / 5.7 | 21.97 MiB |
| Desktop | Hybrid | 6.5 / 6.4 | 21.97 MiB |
| PiSafe profile | Isophote / RasterRelief / Hybrid | 4.2 / 4.3 / 4.4 (run 1), 4.3 / 4.3 / 4.4 (run 2) | 18.68 MiB |
Process RSS ~62 MiB in the standalone (flat across all 12 configurations: no per-frame allocation growth). autoRange on/off made no measurable
difference (differences were run noise). Matrix runs on the same machine at other times read 1.8-2.3 ms for Hybrid, and the earlier pre-change
pipeline measured 1.3-1.5 ms: this dual-GPU laptop shows large state-dependent variance, so treat these as a 2-7 ms envelope, not a precise figure.
PiSafe is NOT cheaper than Desktop in this measurement (the dGPU is not bound by pixel count here); it is only smaller in memory. Runtime harness
pass cost at 1280x720 inside the full runtime: 6.4-7.3 ms (contended with video decode, HUD, other work). Pi 3B+ will be much slower; unmeasured.

## Commands
- `cd sketches/phosphor-contours && make Release -j4`; `cd sketches/experience_runtime && make Release -j4`
- `PHC_VIDEO=<clip> PHC_FREEZE=0.4 PHC_MATRIX=<tag> PHC_AUTOSHOT_DIR=<dir> PHC_FIXED_DT=0.016667 bin/phosphor-contours.app/Contents/MacOS/phosphor-contours`
- `PHC_BENCH=1`, `PHC_SEQ=120 PHC_FREEZE=0.15 PHC_FIXED_DT=0.033333` (same executable)
- Harness: `EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_SCENE=1 EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_HARNESS=1 "EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_HARNESS_MEDIA=White Sand" bin/experience_runtime.app/Contents/MacOS/experience_runtime`
