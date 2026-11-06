#!/usr/bin/env python3
"""Sync Blueprint Emergence's own (non-shared) runtime data assets into every
sketch that runs the Blueprint artwork, from one canonical tracked source.

Same model as scripts/sync-temporal-runtime-assets.py. Scope — exactly the
scene-local files BlueprintSceneCore loads by relative bin/data path:

    codefragments.txt                 (AnnotationRenderer code-text lines)
    fonts/MadawaskaRiver.otf    (BESettings.h CODE_FONT_PATH)

Shared shaders (fragmentEffects, erosion, motion_*, effects/) are NOT owned
here — they belong to shared/src/shaders, shared/assets/video-effects and
scripts/sync-video-effect-assets.py.

Canonical source (authored, NOT under any bin/ tree, so the repo-wide
`**/bin/data/*` ignore rule never touches it):
    sketches/blueprint_emergence/data/

Synced, as plain-file copies, into:
    sketches/blueprint_emergence/bin/data/
    sketches/experience_runtime/bin/data/

Usage:
    scripts/sync-blueprint-runtime-assets.py [--check] [sketch_dir ...]
"""
import argparse
import filecmp
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CANON_ROOT = REPO_ROOT / "sketches" / "blueprint_emergence" / "data"

MANAGED_FILES = [
    "codefragments.txt",
    "fonts/MadawaskaRiver.otf",
]

DEFAULT_SKETCHES = [
    REPO_ROOT / "sketches" / "blueprint_emergence",
    REPO_ROOT / "sketches" / "experience_runtime",
]


def sync_sketch(sketch_dir: Path, check: bool, report: list) -> bool:
    ok = True
    before = len(report)
    for rel in MANAGED_FILES:
        src = CANON_ROOT / rel
        dst = sketch_dir / "bin" / "data" / rel
        if not src.exists():
            report.append(f"[{sketch_dir.name}] ERROR: canonical asset missing: {src}")
            ok = False
            continue
        needs_write = not dst.exists() or dst.is_symlink() or not filecmp.cmp(src, dst, shallow=False)
        if needs_write:
            report.append(f"[{sketch_dir.name}] {'update' if dst.exists() else 'add'}: bin/data/{rel}")
            if check:
                ok = False
            else:
                dst.parent.mkdir(parents=True, exist_ok=True)
                if dst.exists() or dst.is_symlink():
                    dst.unlink()
                shutil.copy2(src, dst)
    if len(report) == before:
        report.append(f"[{sketch_dir.name}] up to date (Blueprint runtime assets)")
    return ok


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sketch_dirs", nargs="*", help="Sketch directories to sync (default: blueprint_emergence, experience_runtime)")
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
