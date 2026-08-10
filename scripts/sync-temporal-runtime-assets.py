#!/usr/bin/env python3
"""Sync Temporal's own (non-shared) runtime data assets into every sketch
that needs them, from one canonical tracked source.

Scope — this script owns exactly two asset groups, both authored/owned by
the temporal-fields sketch and consumed at runtime via a plain relative
ofShader::load()/ofDirectory::listDir() path (resolved against whichever
binary's own bin/data/ is running, per-sketch, not shared at runtime):

1. The three Temporal-only shader pairs used by TFFragmentTransition
   (fragmentDissolve), TFPatternParticleField (particleExistenceFade), and
   TFAmbientTextureLayer (textureBlendFade). These are NOT part of the
   Shared Video Effects catalog (shared/assets/video-effects/ +
   scripts/sync-video-effect-assets.py) — they're Temporal-scene-local
   composition/transition shaders, never registered in
   shared/assets/video-effects/effect-assets.json, so that script's asset
   map doesn't (and shouldn't) cover them.

2. The Temporal ambient backgrounds set (TFAmbientTextureLayer's
   <name>_tint.png/<name>_mask.png pairs + textures_manifest.json, all in
   one flat "backgrounds" folder — see TFAmbientTextureLayer::setup()).

Canonical source (authored, tracked, NOT under any bin/ tree so the
repo-wide `**/bin/data/*` .gitignore rule never touches it):
    sketches/temporal-fields/data/shaders/
    sketches/temporal-fields/data/backgrounds/

Synced, as plain-file copies (matching this project's existing plain-file
bin/data/shaders/ convention — see docs/temporal-production-scene-2-migration-report.md
line 36 — in preference to a hand-created symlink), into:
    sketches/temporal-fields/bin/data/{shaders,backgrounds}/
    sketches/experience_runtime/bin/data/{shaders,backgrounds}/

Before this script existed, experience_runtime's copies were supplied by
hand during the Temporal migration session (plain-file copies for the
shaders, a symlink for backgrounds) directly into bin/data/, which is
.gitignore'd — i.e. real, but not reproducible from a fresh checkout. This
script is the deterministic replacement for that manual step; run it (or
let the `make Release`/`make Debug` hook in each sketch's config.make run
it for you) any time either sketch's bin/data/ is missing these assets.

Usage:
    scripts/sync-temporal-runtime-assets.py [--check] [sketch_dir ...]

With no sketch_dir arguments, both consuming sketches (temporal-fields,
experience_runtime) are synced. --check performs a dry run: nothing is
written, and the script exits non-zero if anything is missing or has
drifted from the canonical source.
"""
import argparse
import filecmp
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CANON_ROOT = REPO_ROOT / "sketches" / "temporal-fields" / "data"
CANON_SHADERS = CANON_ROOT / "shaders"
CANON_BACKGROUNDS = CANON_ROOT / "backgrounds"

# The three Temporal-only shader pairs this script owns. Deliberately a
# fixed filename list, not "every file in CANON_SHADERS" — bin/data/shaders/
# in both sketches also holds unrelated shaders (desaturate, erosion,
# fragmentEffects, motion_*, effects/) that this script must never touch.
SHADER_FILES = [
    "fragmentDissolve.vert", "fragmentDissolve.frag",
    "particleExistenceFade.vert", "particleExistenceFade.frag",
    "textureBlendFade.vert", "textureBlendFade.frag",
]

DEFAULT_SKETCHES = [
    REPO_ROOT / "sketches" / "temporal-fields",
    REPO_ROOT / "sketches" / "experience_runtime",
]


def sync_shaders(sketch_dir: Path, check: bool, report: list) -> bool:
    ok = True
    dst_dir = sketch_dir / "bin" / "data" / "shaders"
    for name in SHADER_FILES:
        src = CANON_SHADERS / name
        dst = dst_dir / name
        if not src.exists():
            report.append(f"[{sketch_dir.name}] ERROR: canonical shader missing: {src}")
            ok = False
            continue
        needs_write = not dst.exists() or dst.is_symlink() or not filecmp.cmp(src, dst, shallow=False)
        if needs_write:
            report.append(f"[{sketch_dir.name}] {'update' if dst.exists() else 'add'}: bin/data/shaders/{name}")
            if check:
                ok = False
            else:
                dst_dir.mkdir(parents=True, exist_ok=True)
                if dst.exists() or dst.is_symlink():
                    dst.unlink()
                import shutil
                shutil.copy2(src, dst)
    return ok


def sync_backgrounds(sketch_dir: Path, check: bool, report: list) -> bool:
    import shutil
    ok = True
    dst_dir = sketch_dir / "bin" / "data" / "backgrounds"
    canon_files = sorted(p.name for p in CANON_BACKGROUNDS.iterdir() if p.is_file())
    if not canon_files:
        report.append(f"[{sketch_dir.name}] ERROR: canonical backgrounds source is empty: {CANON_BACKGROUNDS}")
        return False

    # A prior manual repair (this project's actual history) used a symlink
    # for experience_runtime's backgrounds/ instead of a real directory of
    # copies. Replace any symlink (file or dir) with a real directory before
    # writing, so the delivery mechanism is uniform (plain copies) across
    # both sketches and doesn't silently keep pointing at another sketch's
    # bin/data/.
    if dst_dir.is_symlink():
        report.append(f"[{sketch_dir.name}] remove: bin/data/backgrounds (stale manual symlink -> {dst_dir.resolve()})")
        if check:
            return False
        dst_dir.unlink()

    for name in canon_files:
        src = CANON_BACKGROUNDS / name
        dst = dst_dir / name
        needs_write = not dst.exists() or not filecmp.cmp(src, dst, shallow=False)
        if needs_write:
            report.append(f"[{sketch_dir.name}] {'update' if dst.exists() else 'add'}: bin/data/backgrounds/{name}")
            if check:
                ok = False
            else:
                dst_dir.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dst)

    # Remove stale managed files: anything in dst_dir that used to come from
    # the canonical set but no longer does. Scoped to filenames that exist
    # in the canonical naming convention (*_tint.png/_mask.png/manifest) so
    # this never deletes a file some other tool dropped into the same
    # backgrounds/ folder.
    if dst_dir.exists():
        canon_set = set(canon_files)
        for path in dst_dir.iterdir():
            if not path.is_file():
                continue
            is_managed_name = (path.name.endswith("_tint.png") or path.name.endswith("_mask.png")
                                or path.name == "textures_manifest.json")
            if is_managed_name and path.name not in canon_set:
                report.append(f"[{sketch_dir.name}] remove: bin/data/backgrounds/{path.name} (no longer in canonical source)")
                if check:
                    ok = False
                else:
                    path.unlink()

    return ok


def sync_sketch(sketch_dir: Path, check: bool, report: list) -> bool:
    before = len(report)
    ok = sync_shaders(sketch_dir, check, report)
    ok = sync_backgrounds(sketch_dir, check, report) and ok
    if len(report) == before:
        report.append(f"[{sketch_dir.name}] up to date (Temporal runtime assets)")
    return ok


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sketch_dirs", nargs="*", help="Sketch directories to sync (default: temporal-fields, experience_runtime)")
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
