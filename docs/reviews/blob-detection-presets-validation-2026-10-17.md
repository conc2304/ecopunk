# Blob Per-Video Detection Presets — Validation Record (Mac)

**Date:** 2026-10-17
**Disposition:** MANUALLY AUTHORED · PERSISTENCE VERIFIED · PRODUCTION LOAD VERIFIED · tuning follow-ups listed in §4

**Inputs.** The authored file `assets/shared/blob/blob-detection-profiles.json` holds 23 presets, plus 1 added during this run (see §5). The catalog has 26 clips.

**How it was run.**
- **Standalone:** one fresh launch reading from disk. A full catalog sweep (`BLOB_PROFILE_DEMO=verify-all`, a dev-only driver) visited every clip 4–6 times. At each visit it checked the live detector config against the file and recorded about 4 s of tracking stats.
- **Production:** ExperienceRuntime lifecycle harness (media switches plus deactivate/resume) and the Blob→Temporal→Blob switching harness, both reading the real file.

## 1. Results by stage

| Stage | Result |
|---|---|
| A — persistence per clip | PASS. 26/26 clips; every visit matched the file (117 checks, 0 FAIL) |
| B — restart | PASS. The sweep was a fresh process; every value came from disk (`source=per_media`) |
| C — isolation | PASS. Clips were revisited in varying order 4–6 times each, always with their own values |
| D — no-profile fallback | PASS. Crystal Bloom and Fern Foliage got exact defaults right after profiled clips |
| E — JSON on disk | PASS. One entry per ID, no duplicates, no orphan IDs, all five fields valid |
| F — production load | PASS. Loaded 24 profiles. Calm Ocean, Raindrops, Forest Path, Snail and Autumn Leaves applied exactly (1555/1555) |
| G — production switching | PASS. Media switches and deactivate→resume reapply the right preset; Blob→Temporal→Blob 5351/0 |
| H — difficult clips | Mixed. Tenerife Shore fixed; under- and over-detection on several clips (§4) |

## 2. Per-clip record

Columns: preset values; tracked regions averaged over each visit; peak fragments (capped at 10).

| Video | Media ID | Source | Thr | Min | Max | Er | Di | Tracks avg (per visit) | Frag peak |
|---|---|---|---|---|---|---|---|---|---|
| Autumn Leaves | f26fab29 | per_media | 1 | 0.001118 | 0.106378 | 1 | 1 | 11–12 | 10 |
| Bamboo Grove | 52f569d8 | per_media | 17 | 0.004937 | 0.05 | 0 | 1 | 10 | 10 |
| Black Centipede | 4a619257 | per_media | 25 | 0.0008 | 0.35 | 0 | 0 | 7.5 | 10 |
| Bonfire | b6b38c6d | per_media | 38 | 0.010284 | 0.496684 | 1 | 0 | 4.7–5.2 | 9 |
| Calm Ocean | b5b696ee | per_media | 1 | 0.003155 | 0.435969 | 1 | 0 | 10 | 10 |
| Cliffside Surf | 40d022de | per_media | 21 | 0.005956 | 0.691837 | 0 | 0 | 2.2 | 5 |
| Crystal Bloom | 0b153878 | defaults | 40 | 0.0008 | 0.35 | 1 | 2 | 17–18 | 10 |
| Fern Foliage | 3838009d | defaults | 40 | 0.0008 | 0.35 | 1 | 2 | **0** | 7 |
| Forest Canopy | 497121bc | per_media | 2 | 0.0001 | 0.574745 | 1 | 2 | 22–24 | 10 |
| Forest Path | 96b64003 | per_media | 1 | 0.002391 | 0.05 | 1 | 1 | 20–24 | 10 |
| Glass Flower | 379e4f0a | per_media | 16 | 0.0008 | 0.35 | 0 | 0 | 7 | 10 |
| Hurricane Milton | 2c15ecd7 | per_media | 2 | 0.014357 | 0.05 | 1 | 2 | 2.3–2.6 | 7 |
| Ladybug | d9621168 | per_media | 7 | 0.0008 | 0.35 | 1 | 1 | 26–28 | 10 |
| Lightning Storm | 46d6aa46 | per_media (= defaults) | 40 | 0.0008 | 0.35 | 1 | 2 | **0.3** | 7 |
| Lily Bloom | 5c8a1794 | per_media | 21 | 0.00341 | 0.35 | 0 | 1 | **0.1–0.3** | 8 |
| Melting Flower | 768dc974 | per_media (= defaults, added during run) | 40 | 0.0008 | 0.35 | 1 | 2 | **0.1–0.8** (one visit 17) | 7 |
| Mountain Flight | 644595e0 | per_media | 37 | 0.002646 | 0.201786 | 0 | 1 | 7.5 | 9 |
| Neon Drip | d0a9a208 | per_media | 94 | 0.002901 | 0.245153 | 0 | 2 | 9.4 | 10 |
| Raindrops On Leaves | ffe87a97 | per_media | 1 | 0.0001 | 0.158418 | 1 | 1 | 27–28 | 10 |
| Rainfall | 7021ad87 | per_media | 19 | 0.0001 | 0.370918 | 0 | 1 | 32–36 | 10 |
| Snail | 29d9e437 | per_media | 21 | 0.004683 | 0.466327 | 0 | 1 | 2–3 | 6 |
| Stormy Waves | 1ab5450d | per_media | 30 | 0.003155 | 0.648469 | 0 | 1 | 14 | 10 |
| Tenerife Shore | c7ba0540 | per_media | 50 | 0.002137 | 0.509694 | 0 | 2 | 23–26 | 10 |
| Turquoise Waves | c15c43ce | per_media | 21 | 0.008502 | 0.05 | 0 | 1 | 4.4–4.8 | 9 |
| Vecteezy … fern (1) | 61933905 | per_media | 24 | 0.001373 | 0.05 | 0 | 1 | 8 | 10 |
| White Sand Beach | 823000cc | per_media | 2 | 0.002137 | 0.774235 | 1 | 2 | 22–24 | 10 |

Captures (`blob-detection-presets-evidence/`):
- `validation_4a619257.png`: Black Centipede, an easy clip;
- `validation_c7ba0540.png`: Tenerife Shore, the difficult clip, now tracking well;
- `validation_823000cc.png`: White Sand Beach, noisy.

## 3. Failure interpretation

No load, persistence, ID-mismatch or leakage failures. Every finding below is a **retune** item, not a system bug.

## 4. Tuning follow-ups

- **Under-detection.**
  - Fern Foliage: no preset, 0 tracks. Its duplicate file (`vecteezy…`, `61933905`) has a preset, so the same footage behaves differently under its two IDs. Copy the preset across, or drop the duplicate clip.
  - Lily Bloom: about 0.1–0.3 tracks.
  - Lightning Storm and Melting Flower: their entries equal the defaults and give under 1 track.
- **Probable over-detection (noise).** Seven clips use threshold 1–2; 20+ tracked regions against a 10-fragment cap suggests noise. Examples: Forest Canopy, Forest Path, Raindrops, White Sand Beach (visibly noisy: large overlapping boxes, speckled diff), and Rainfall (32–36 tracks). Fragments are still capped at 10, so this costs churn rather than safety.
- **Slider floors.** Several presets sit on GUI slider limits (maxArea 0.05, minArea 0.0001). The intended value may lie beyond the slider's range.

## 5. Incident during the run

While the standalone sweep ran, keystrokes reached the app window; it most likely took keyboard focus on launch. The app's own `s`, `r` and `n`/`p` handlers fired: 2 saves, 3 reloads, and extra navigation.

**Net change to the authored file:** a new `media.auto.768dc974` (Melting Flower) entry equal to the detector defaults. Forest Path was re-saved with identical values. Behavior is unchanged, but the entry is unintended unless you made it; remove it if so. The checks were unaffected: each one compared against the file as loaded at that moment.
