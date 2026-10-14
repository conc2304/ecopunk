#!/usr/bin/env python3
"""Sync the shared shaders ExperienceRuntime loads by their legacy bin/data
paths, from their canonical tracked sources.

Same model as scripts/sync-temporal-runtime-assets.py and
scripts/sync-blueprint-runtime-assets.py. Scope — exactly the files the
runtime's shared C++ loads by relative bin/data path:

    shaders/effects/*.glsl, of_nature_shader_pack_glsl/*.glsl
        ShaderLibrary::setup() (shared/src/ShaderLibrary.cpp)
    shaders/effects/vert.glsl, shaders/motion_{extract,accum}.glsl
        MotionExtraction (shared/src/MotionExtraction.cpp)
    shaders/fragmentEffects.{vert,frag}
        Fragment / BEFragment / BlueprintSceneCore
    shaders/erosion.{vert,frag}
        ErosionFBO (shared/src/ErosionFBO.cpp)

These were previously supplied by hand into bin/data/ (which the repo-wide
`**/bin/data/*` ignore rule excludes, and deploy.sh's rsync skips), so a
fresh tree — e.g. the Rock Pi — had none of them.

Canonical sources (no file is authored here; this only copies):
    shared/assets/video-effects/{common,single-pass,motion-extraction}/
    shared/src/shaders/

The bin/data/shared-video-effects/ tree (manifest-driven shader assets +
knowledge pack) is NOT owned here — that is scripts/sync-video-effect-assets.py.
Temporal-local shaders in bin/data/shaders/ are owned by
scripts/sync-temporal-runtime-assets.py. Files not listed below are left
untouched.

Usage:
    scripts/sync-shared-shader-runtime-assets.py [--check] [sketch_dir ...]
"""
import argparse
import filecmp
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
VIDEO_EFFECTS = "shared/assets/video-effects"

SHADER_LIBRARY_EFFECTS = [
    "desaturate", "invert", "recolor", "threshold", "dither", "solarize",
    "scanlines", "channelshift", "hue_rotate", "ascii_threshold_solarpunk",
    "heatmap_recolor",
]
NATURE_PACK_EFFECTS = [
    "bioluminescence", "chromatic_aberration", "edge_glow", "ink_outlines",
    "pixel_drift", "pixel_sorting", "water_refraction",
]

# bin/data-relative destination -> repo-relative canonical source.
MANAGED_FILES = {
    "shaders/effects/vert.glsl": f"{VIDEO_EFFECTS}/common/vert.glsl",
    **{f"shaders/effects/{n}.glsl": f"{VIDEO_EFFECTS}/single-pass/{n}.glsl" for n in SHADER_LIBRARY_EFFECTS},
    **{f"of_nature_shader_pack_glsl/{n}.glsl": f"{VIDEO_EFFECTS}/single-pass/{n}.glsl" for n in NATURE_PACK_EFFECTS},
    "shaders/motion_extract.glsl": f"{VIDEO_EFFECTS}/motion-extraction/motion_extract.glsl",
    "shaders/motion_accum.glsl": f"{VIDEO_EFFECTS}/motion-extraction/motion_accum.glsl",
    "shaders/fragmentEffects.vert": "shared/src/shaders/fragmentEffects.vert",
    "shaders/fragmentEffects.frag": "shared/src/shaders/fragmentEffects.frag",
    "shaders/erosion.vert": "shared/src/shaders/erosion.vert",
    "shaders/erosion.frag": "shared/src/shaders/erosion.frag",
}

DEFAULT_SKETCHES = [
    REPO_ROOT / "sketches" / "experience_runtime",
]


def sync_sketch(sketch_dir: Path, check: bool, report: list) -> bool:
    ok = True
    before = len(report)
    for rel, src_rel in sorted(MANAGED_FILES.items()):
        src = REPO_ROOT / src_rel
        dst = sketch_dir / "bin" / "data" / rel
        if not src.is_file():
            report.append(f"[{sketch_dir.name}] ERROR: canonical asset missing: {src}")
            ok = False
            continue
        needs_write = not dst.exists() or dst.is_symlink() or not filecmp.cmp(src, dst, shallow=False)
        if needs_write:
            report.append(f"[{sketch_dir.name}] {'update' if dst.exists() or dst.is_symlink() else 'add'}: bin/data/{rel}")
            if check:
                ok = False
            else:
                dst.parent.mkdir(parents=True, exist_ok=True)
                if dst.exists() or dst.is_symlink():
                    dst.unlink()
                shutil.copy2(src, dst)
    if len(report) == before:
        report.append(f"[{sketch_dir.name}] up to date (shared shader runtime assets)")
    return ok


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sketch_dirs", nargs="*", help="Sketch directories to sync (default: experience_runtime)")
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
