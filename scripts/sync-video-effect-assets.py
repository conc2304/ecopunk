#!/usr/bin/env python3
"""Sync canonical video-effect shader assets into each sketch's own data dir.

Copies the subset of shared/assets/video-effects/ that a sketch's
effect-manifest.json enables into <sketch>/bin/data/shared-video-effects/,
per docs/shared-video-effect-architecture.md §2.2/2.3. Runs per-sketch so
each sketch stays independently buildable/deployable (no runtime
cross-sketch asset sharing) while shared/assets/video-effects/ stays the
single edited-by-hand source of truth.

Usage:
    scripts/sync-video-effect-assets.py [--check] [sketch_dir ...]

With no sketch_dir arguments, every sketches/*/effect-manifest.json found is
synced. Pass one or more sketch directories to limit the run.

--check performs a dry run: nothing is written, and the script exits
non-zero if any managed file is missing, stale, or has drifted from its
canonical source (including hand-edited generated files).
"""
import argparse
import filecmp
import json
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CANON_DIR = REPO_ROOT / "shared" / "assets" / "video-effects"
ASSET_MAP_PATH = CANON_DIR / "effect-assets.json"
MANAGED_SUBDIR = Path("bin/data/shared-video-effects")
MARKER_NAME = ".generated-from-shared-video-effects"


def load_asset_map():
    with open(ASSET_MAP_PATH) as f:
        return json.load(f)["effects"]


def load_manifest(sketch_dir: Path):
    with open(sketch_dir / "effect-manifest.json") as f:
        return json.load(f)


def required_assets_for_manifest(manifest, asset_map):
    """Sorted canonical-relative asset paths this sketch needs, plus any
    effect ids in the manifest that don't exist in the asset map."""
    required = set()
    unknown = []
    for effect_id, cfg in manifest.get("effects", {}).items():
        if not cfg.get("enabled", False):
            continue
        if effect_id not in asset_map:
            unknown.append(effect_id)
            continue
        required.update(asset_map[effect_id]["assets"])
    return sorted(required), unknown


def sync_manifest_copy(sketch_dir: Path, check: bool, report: list):
    """VideoEffectService::setup() reads effect-manifest.json via
    ofToDataPath(), which only resolves inside bin/data/ — but the manifest's
    source of truth lives at the sketch root (like config.make/addons.make),
    where this script and a human both expect to find it. Keep a synced copy
    in bin/data/ so the C++ runtime can see it too.

    Returns (changed, ok): `changed` is True if a write was needed (whether
    or not it was actually performed, e.g. under --check); `ok` is False only
    under --check when a write was needed but skipped."""
    sketch_name = sketch_dir.name
    src = sketch_dir / "effect-manifest.json"
    dst = sketch_dir / "bin" / "data" / "effect-manifest.json"
    if not src.exists():
        return False, True
    needs_write = not dst.exists() or not filecmp.cmp(src, dst, shallow=False)
    if not needs_write:
        return False, True
    report.append(f"[{sketch_name}] {'update' if dst.exists() else 'add'}: bin/data/effect-manifest.json (from sketch-root manifest)")
    if check:
        return True, False
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    return True, True


def sync_sketch(sketch_dir: Path, asset_map, check: bool, report: list):
    sketch_name = sketch_dir.name
    manifest = load_manifest(sketch_dir)
    required, unknown = required_assets_for_manifest(manifest, asset_map)

    if unknown:
        report.append(
            f"[{sketch_name}] ERROR: manifest references unknown effect id(s): {', '.join(unknown)}"
        )
        return False

    manifest_changed, manifest_copy_ok = sync_manifest_copy(sketch_dir, check, report)

    managed_dir = sketch_dir / MANAGED_SUBDIR
    marker_path = managed_dir / MARKER_NAME

    previously_managed = []
    if marker_path.exists():
        try:
            previously_managed = json.loads(marker_path.read_text()).get("managedFiles", [])
        except (ValueError, OSError):
            previously_managed = []

    ok = True
    changed = []

    # 1. copy/update every required file.
    for rel in required:
        src = CANON_DIR / rel
        dst = managed_dir / rel
        if not src.exists():
            report.append(f"[{sketch_name}] ERROR: canonical asset missing: {rel}")
            ok = False
            continue
        needs_write = not dst.exists() or not filecmp.cmp(src, dst, shallow=False)
        if needs_write:
            changed.append(("update" if dst.exists() else "add", rel))
            if not check:
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dst)

    # 2. remove managed files that are no longer required — from the marker's
    # own record first (authoritative), then defensively from whatever is
    # actually on disk under the managed dir (covers an interrupted prior
    # run). Never touches anything outside MANAGED_SUBDIR.
    already_flagged = {rel for _, rel in changed}
    stale = [rel for rel in previously_managed if rel not in required]
    for rel in stale:
        if rel in already_flagged:
            continue
        changed.append(("remove", rel))
        already_flagged.add(rel)
        if not check:
            dst = managed_dir / rel
            if dst.exists():
                dst.unlink()

    if managed_dir.exists():
        for path in managed_dir.rglob("*"):
            if not path.is_file() or path.name == MARKER_NAME:
                continue
            rel = str(path.relative_to(managed_dir))
            if rel not in required and rel not in already_flagged:
                changed.append(("remove", rel))
                already_flagged.add(rel)
                if not check:
                    path.unlink()

    # 3. (re)write the marker whenever anything changed, or it doesn't exist yet.
    if not check and (changed or not marker_path.exists()):
        managed_dir.mkdir(parents=True, exist_ok=True)
        marker_path.write_text(
            json.dumps(
                {
                    "generatedBy": "scripts/sync-video-effect-assets.py",
                    "doNotEdit": True,
                    "managedFiles": required,
                },
                indent=2,
            )
            + "\n"
        )

    if changed:
        for action, rel in changed:
            report.append(f"[{sketch_name}] {action}: {rel}")
        if check:
            ok = False
    elif not manifest_changed:
        report.append(f"[{sketch_name}] up to date ({len(required)} assets)")

    return ok and manifest_copy_ok


def discover_sketches():
    return sorted(p.parent for p in (REPO_ROOT / "sketches").glob("*/effect-manifest.json"))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "sketch_dirs", nargs="*", help="Sketch directories to sync (default: all with an effect-manifest.json)"
    )
    parser.add_argument("--check", action="store_true", help="Dry run; fail if anything would change")
    args = parser.parse_args()

    asset_map = load_asset_map()

    sketch_dirs = [Path(s).resolve() for s in args.sketch_dirs] if args.sketch_dirs else discover_sketches()

    if not sketch_dirs:
        print("No sketches with effect-manifest.json found.")
        return 0

    report = []
    all_ok = True
    for sketch_dir in sketch_dirs:
        all_ok = sync_sketch(sketch_dir, asset_map, args.check, report) and all_ok

    print("\n".join(report))
    if args.check and not all_ok:
        print("\n--check found drift (see above). Run without --check to sync.")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
