#!/usr/bin/env python3
"""OC-003: list the canonical media ids of a media folder WITHOUT starting the runtime (read-only; no display, no service interaction).

  list_media_ids.py --media-root <project root>/assets/shared/media

The runtime synthesizes `media.auto.<fnv1a32(relativePath)>` (shared/src/video-playback/MediaCatalog.cpp, synthesizeMediaId), so the id of a file depends
only on its path relative to the media root and is identical on every machine that has that file name. This list is ADVISORY: the authoritative check is
the runtime's own `VideoVerification: startup media ACTIVE id=... file=...` line, which verify_run.py requires for every run.
Pick the LONGEST clip (fewest loops over a 300 s run) when durations are available (needs ffprobe on PATH; otherwise the file size is shown).
"""
import argparse
import os
import shutil
import subprocess

EXT = {".mp4", ".mov", ".m4v", ".webm", ".mkv", ".avi"}


def fnv1a32_hex(text):
    h = 2166136261
    for b in text.encode():
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return "%08x" % h


def duration(path):
    if not shutil.which("ffprobe"):
        return None
    try:
        r = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "default=nw=1:nk=1", path],
                           capture_output=True, text=True, timeout=30)
        return float(r.stdout.strip())
    except (ValueError, subprocess.SubprocessError):
        return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--media-root", required=True)
    a = ap.parse_args()
    root = os.path.abspath(a.media_root)
    if not os.path.isdir(root):
        raise SystemExit(f"{root} is not a directory")
    rows = []
    for dirpath, _d, files in os.walk(root):
        for f in files:
            if os.path.splitext(f)[1].lower() in EXT:
                full = os.path.join(dirpath, f)
                rel = os.path.relpath(full, root).replace(os.sep, "/")
                rows.append(("media.auto." + fnv1a32_hex(rel), rel, os.path.getsize(full), duration(full)))
    rows.sort(key=lambda r: (-(r[3] or 0), -r[2]))
    print(f"{'media id':24s} {'duration s':>10s} {'size MiB':>9s}  file")
    for mid, rel, size, dur in rows:
        print(f"{mid:24s} {'' if dur is None else format(dur, '10.1f'):>10s} {size / 2**20:9.1f}  {rel}")
    print(f"{len(rows)} files; sorted longest first (or largest first when ffprobe is unavailable)")


if __name__ == "__main__":
    main()
