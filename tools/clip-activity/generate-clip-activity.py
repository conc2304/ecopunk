#!/usr/bin/env python3
"""Measure how much each shared-media clip changes frame over frame, once, offline.

For every .mp4 under assets/shared/media/ this decodes the clip with ffmpeg at a fixed step (default
10 Hz), reduces each frame to a 64x36 luma grid (area/box mean), and records the mean absolute difference
between successive grids as `raw` (0..1). Same quantity as the runtime's live probe
(sketches/experience_runtime/src/SceneActivityCore.h), so the runtime can map it with the same fixed log
curve. Output: assets/shared/clip-activity/clip-activity.json, keyed by file name.

Requires ffmpeg/ffprobe on PATH. Only the standard library is used.

    python3 tools/clip-activity/generate-clip-activity.py            # (re)measure new or changed clips
    python3 tools/clip-activity/generate-clip-activity.py --force    # re-measure everything
    python3 tools/clip-activity/generate-clip-activity.py --check    # exit 1 if the file is stale/missing

TODO: let users upload single videos to add to the experience, instead of dropping files into
assets/shared/media/ by hand. An upload should run through one pipeline, in order:
  1. Format: convert to the container, codec and resolution the Pi needs (what tools/convert_media.sh
     does today: MP4, H.264 Baseline/Main, 540p preferred / 720p max, 24 or 30 fps, no audio, bt709
     yuv420p).
  2. Clip activity: measure the converted clip and add it to clip-activity.json. measure() and the
     per-clip entry here are the building blocks; this script currently re-measures the whole folder.
  3. Blob coverage: give the new clip a Blob detection preset (tools/blob-preset-coverage and
     scripts/check-blob-detection-presets.sh report which production videos have none).
Open questions: where uploads land and who may upload, how a failed stage is reported and rolled back,
and how the running installation picks up the new clip without a restart.
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

GRID_W, GRID_H = 64, 36
CELLS = GRID_W * GRID_H
SCHEMA = 1

ROOT = Path(__file__).resolve().parents[2]
MEDIA = ROOT / "assets" / "shared" / "media"
OUT = ROOT / "assets" / "shared" / "clip-activity" / "clip-activity.json"


def probe_duration(path):
    r = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(path)],
                       capture_output=True, text=True, check=True)
    return float(r.stdout.strip())


def measure(path, step):
    """raw[k] = mean |grid(k+1) - grid(k)| / 255 over a 64x36 luma grid sampled every `step` seconds."""
    cmd = ["ffmpeg", "-v", "error", "-i", str(path), "-an", "-sn",
           "-vf", f"fps={1.0 / step:.6f},scale={GRID_W}:{GRID_H}:flags=area,format=gray",
           "-f", "rawvideo", "-pix_fmt", "gray", "-"]
    data = subprocess.run(cmd, capture_output=True, check=True).stdout
    frames = len(data) // CELLS
    raw = []
    prev = None
    for i in range(frames):
        cur = data[i * CELLS:(i + 1) * CELLS]
        if prev is not None:
            raw.append(sum(abs(a - b) for a, b in zip(cur, prev)) / (255.0 * CELLS))
        prev = cur
    return raw


def signature(path):
    return {"bytes": path.stat().st_size}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--step", type=float, default=0.1, help="seconds between samples (default 0.1)")
    ap.add_argument("--force", action="store_true", help="re-measure every clip")
    ap.add_argument("--check", action="store_true", help="verify the file is current; change nothing")
    args = ap.parse_args()

    clips = sorted(MEDIA.glob("*.mp4"))
    existing = {}
    if OUT.exists():
        try:
            doc = json.loads(OUT.read_text())
            if doc.get("schemaVersion") == SCHEMA and abs(doc.get("stepSeconds", 0) - args.step) < 1e-9:
                existing = doc.get("clips", {})
        except (ValueError, OSError):
            existing = {}

    stale = [c.name for c in clips if existing.get(c.name, {}).get("bytes") != c.stat().st_size]
    gone = [n for n in existing if not (MEDIA / n).exists()]
    if args.check:
        if stale or gone or not OUT.exists():
            print("clip-activity.json is out of date:", {"missing_or_changed": stale, "removed": gone})
            return 1
        print(f"clip-activity.json is current ({len(clips)} clips)")
        return 0

    out = {}
    for c in clips:
        if not args.force and c.name not in stale and c.name in existing:
            out[c.name] = existing[c.name]
            continue
        raw = measure(c, args.step)
        if not raw:
            print(f"  {c.name}: no frames decoded; skipped", file=sys.stderr)
            continue
        out[c.name] = {**signature(c), "durationSeconds": round(probe_duration(c), 3),
                       "raw": [float(f"{v:.5g}") for v in raw]}
        mean = sum(raw) / len(raw)
        print(f"  {c.name}: {len(raw)} samples, mean raw {mean:.4f}, max {max(raw):.3f}")

    OUT.parent.mkdir(parents=True, exist_ok=True)
    doc = {
        "comment": "Generated by tools/clip-activity/generate-clip-activity.py - do not edit by hand. raw = mean absolute "
                   "difference between successive 64x36 luma grids (0..1) at stepSeconds; raw[k] spans (k*step, (k+1)*step). "
                   "Keyed by media file name; re-run the tool when media changes.",
        "schemaVersion": SCHEMA,
        "stepSeconds": args.step,
        "grid": [GRID_W, GRID_H],
        "clips": out,
    }
    OUT.write_text(json.dumps(doc, separators=(",", ":")) + "\n")
    print(f"wrote {OUT.relative_to(ROOT)} ({len(out)} clips, {OUT.stat().st_size // 1024} KiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
