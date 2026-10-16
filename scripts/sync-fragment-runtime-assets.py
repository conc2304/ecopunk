#!/usr/bin/env python3
"""Sync Fragment Trail's own (non-shared) runtime data assets into every
sketch that runs the Fragment Trail artwork, from one canonical tracked source.

Same model as scripts/sync-blueprint-runtime-assets.py. Scope — exactly the
scene-local file FragmentSceneCore loads by relative bin/data path:

    shaders/bg_dim.glsl   (FTBackgroundLayer's desaturate + dim backdrop pass)

The nine per-fragment effects (desaturate, invert, recolor, threshold, dither,
solarize, scanlines, channelshift, heatmap_recolor) and vert.glsl are NOT owned
here: in ExperienceRuntime they come from the canonical shaders/effects/ set
(scripts/sync-video-effect-assets.py), byte-identical to the standalone's
copies. The standalone sketch's own bin/data/shaders/ copies of those remain
hand-supplied, as before.

Canonical source (authored, NOT under any bin/ tree, so the repo-wide
`**/bin/data/*` ignore rule never touches it):
    sketches/fragment-trail/data/

Synced, as plain-file copies, into (destination differs per sketch — the
runtime keeps scene-local shaders under a scene-scoped directory):
    sketches/fragment-trail/bin/data/shaders/bg_dim.glsl
    sketches/experience_runtime/bin/data/shaders/fragment-trail/bg_dim.glsl

Usage:
    scripts/sync-fragment-runtime-assets.py [--check] [sketch_dir ...]
"""
import argparse
import filecmp
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CANON_ROOT = REPO_ROOT / "sketches" / "fragment-trail" / "data"

# canonical relative path -> destination relative path under bin/data/, per sketch
MANAGED_FILES = {
    "fragment-trail": {"shaders/bg_dim.glsl": "shaders/bg_dim.glsl"},
    "experience_runtime": {"shaders/bg_dim.glsl": "shaders/fragment-trail/bg_dim.glsl"},
}

DEFAULT_SKETCHES = [
    REPO_ROOT / "sketches" / "fragment-trail",
    REPO_ROOT / "sketches" / "experience_runtime",
]


def sync_sketch(sketch_dir: Path, check: bool, report: list) -> bool:
    ok = True
    before = len(report)
    mapping = MANAGED_FILES.get(sketch_dir.name)
    if mapping is None:
        report.append(f"[{sketch_dir.name}] ERROR: not a Fragment Trail runtime sketch")
        return False
    for rel, dst_rel in mapping.items():
        src = CANON_ROOT / rel
        dst = sketch_dir / "bin" / "data" / dst_rel
        if not src.exists():
            report.append(f"[{sketch_dir.name}] ERROR: canonical asset missing: {src}")
            ok = False
            continue
        needs_write = not dst.exists() or dst.is_symlink() or not filecmp.cmp(src, dst, shallow=False)
        if needs_write:
            report.append(f"[{sketch_dir.name}] {'update' if dst.exists() else 'add'}: bin/data/{dst_rel}")
            if check:
                ok = False
            else:
                dst.parent.mkdir(parents=True, exist_ok=True)
                if dst.exists() or dst.is_symlink():
                    dst.unlink()
                shutil.copy2(src, dst)
    if len(report) == before:
        report.append(f"[{sketch_dir.name}] up to date (Fragment Trail runtime assets)")
    return ok


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sketch_dirs", nargs="*", help="Sketch directories to sync (default: fragment-trail, experience_runtime)")
    parser.add_argument("--check", action="store_true", help="Dry run; fail if anything would change")
    args = parser.parse_args()

    sketch_dirs = [Path(s).resolve() for s in args.sketch_dirs] if args.sketch_dirs else DEFAULT_SKETCHES

    report = []
    all_ok = True
    for sketch_dir in sketch_dirs:
        all_ok = sync_sketch(sketch_dir, args.check, report) and all_ok

    print("\n".join(report))
    if args.check and not all_ok:
        print("\n--check found drift (see above). Run without --check to sync.")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
