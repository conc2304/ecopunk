#!/usr/bin/env python3
"""Sync canonical video-effect assets into each sketch's own data dir.

Two independent things are synced, from two independent canonical roots:

1. Shader assets: the subset of shared/assets/video-effects/ that a
   sketch's effect-manifest.json enables, into
   <sketch>/bin/data/shared-video-effects/, per
   docs/shared-video-effect-architecture.md §2.2/2.3.
2. Shared Effect Knowledge pack (DEC-016): the single file at
   assets/shared/video-effects/knowledge/effect-knowledge-pack.json,
   authored by shader-effect-debugger's export action, into every
   consuming sketch's
   <sketch>/bin/data/shared-video-effects/knowledge/effect-knowledge-pack.json
   verbatim — NOT filtered by effect-manifest.json, since the pack is one
   bundle covering every effect, not a per-effect asset. Synced for every
   sketch with an effect-manifest.json, PLUS every sketch that imports the
   pack but has no manifest of its own (e.g. temporal-fields) — see
   discover_knowledge_only_sketches().

Runs per-sketch so each sketch stays independently buildable/deployable (no
runtime cross-sketch asset sharing) while each canonical root stays the
single edited-by-hand/authored source of truth.

Usage:
    scripts/sync-video-effect-assets.py [--check] [sketch_dir ...]

With no sketch_dir arguments, every discovered sketch (per both rules
above) is synced. Pass one or more sketch directories to limit the run —
a named directory with no effect-manifest.json still gets the knowledge
pack synced (never shader assets).

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

# Shared Effect Knowledge (DEC-016, Shared Effects Architecture-Closure
# Session) — a SEPARATE canonical root from CANON_DIR above, one level up
# (assets/shared/video-effects/, not shared/assets/video-effects/). The
# knowledge pack is authored by shader-effect-debugger's export action
# (see ofApp::exportKnowledgePack()), not hand-edited here, and unlike the
# shader assets above it is NOT filtered by any sketch's
# effect-manifest.json — every effect's whitelist/blacklist/effect-level
# defaults are bundled into one file, and it is every synced sketch's job
# (via importEffectKnowledgePack's own knownEffectIds check) to ignore
# entries for effects it doesn't have, not this script's job to pre-filter.
KNOWLEDGE_CANON_DIR = REPO_ROOT / "assets" / "shared" / "video-effects" / "knowledge"
KNOWLEDGE_PACK_FILENAME = "effect-knowledge-pack.json"
# Relative to MANAGED_SUBDIR (not to bin/data directly) so it shares the same
# physical bin/data/shared-video-effects/ tree the shader-asset sync uses,
# and so sync_sketch()'s stale-file cleanup (which walks that whole tree
# looking for anything not in its shader-asset `required` list) can be told
# to leave this one path alone — see the `rel == KNOWLEDGE_REL_IN_MANAGED_DIR`
# checks below. It is its own file, synced by its own function, never listed
# in a sketch's `required` shader-asset list.
KNOWLEDGE_REL_IN_MANAGED_DIR = Path("knowledge") / KNOWLEDGE_PACK_FILENAME


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


def sync_knowledge_pack(sketch_dir: Path, check: bool, report: list):
    """Copy the one canonical Shared Effect Knowledge pack into this
    sketch's bin/data/, unconditionally (not gated on effect-manifest.json
    contents — see KNOWLEDGE_CANON_DIR's comment above).

    Returns (changed, ok), same contract as sync_manifest_copy: `changed`
    is True if a write was needed; `ok` is False only under --check when a
    write was needed but skipped.

    A missing source pack (nothing exported yet) is NOT an error — matches
    EffectKnowledgePackImportReport::ok's own "an absent pack file is not a
    failure" policy on the consuming side. No destination file is written
    or removed in that case; an already-synced copy from a previous export
    is left in place rather than deleted out from under a running sketch."""
    sketch_name = sketch_dir.name
    src = KNOWLEDGE_CANON_DIR / KNOWLEDGE_PACK_FILENAME
    if not src.exists():
        return False, True
    dst = sketch_dir / MANAGED_SUBDIR / KNOWLEDGE_REL_IN_MANAGED_DIR
    dst_rel_display = MANAGED_SUBDIR / KNOWLEDGE_REL_IN_MANAGED_DIR
    needs_write = not dst.exists() or not filecmp.cmp(src, dst, shallow=False)
    if not needs_write:
        return False, True
    report.append(f"[{sketch_name}] {'update' if dst.exists() else 'add'}: {dst_rel_display} (from canonical knowledge pack)")
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
    knowledge_changed, knowledge_copy_ok = sync_knowledge_pack(sketch_dir, check, report)

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
    stale = [rel for rel in previously_managed if rel not in required and rel != str(KNOWLEDGE_REL_IN_MANAGED_DIR)]
    for rel in stale:
        if rel in already_flagged:
            continue
        changed.append(("remove", rel))
        already_flagged.add(rel)
        if not check:
            dst = managed_dir / rel
            if dst.exists():
                dst.unlink()

    knowledge_rel_str = str(KNOWLEDGE_REL_IN_MANAGED_DIR)
    if managed_dir.exists():
        for path in managed_dir.rglob("*"):
            if not path.is_file() or path.name == MARKER_NAME:
                continue
            rel = str(path.relative_to(managed_dir))
            if rel == knowledge_rel_str:
                continue  # managed by sync_knowledge_pack(), not the shader-asset `required` list above
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
    elif not manifest_changed and not knowledge_changed:
        report.append(f"[{sketch_name}] up to date ({len(required)} assets)")

    return ok and manifest_copy_ok and knowledge_copy_ok


def discover_manifest_sketches():
    return sorted(p.parent for p in (REPO_ROOT / "sketches").glob("*/effect-manifest.json"))


def discover_knowledge_only_sketches():
    """Sketches that import the Shared Effect Knowledge pack
    (importEffectKnowledgePack(...)) but have no effect-manifest.json of
    their own — e.g. temporal-fields, whose TFEffectPicker imports the
    pack directly, bypassing VideoEffectService's per-sketch manifest
    gating entirely. Found by grepping each candidate sketch's src/ tree
    rather than a hardcoded list, so a newly-migrated consumer is picked
    up the next time this script runs without an edit here. Sketches
    already covered by discover_manifest_sketches() are excluded — those
    get the knowledge pack synced as part of sync_sketch()'s normal run,
    not double-synced here."""
    found = []
    for src_dir in sorted((REPO_ROOT / "sketches").glob("*/src")):
        sketch_dir = src_dir.parent
        if (sketch_dir / "effect-manifest.json").exists():
            continue
        for path in src_dir.rglob("*.cpp"):
            try:
                if "importEffectKnowledgePack(" in path.read_text(errors="ignore"):
                    found.append(sketch_dir)
                    break
            except OSError:
                continue
    return found


def sync_knowledge_only_sketch(sketch_dir: Path, check: bool, report: list):
    """Entry point for a sketch found by discover_knowledge_only_sketches()
    — no shader-asset manifest to validate/sync, just the knowledge pack."""
    changed, ok = sync_knowledge_pack(sketch_dir, check, report)
    if not changed:
        report.append(f"[{sketch_dir.name}] up to date (knowledge pack only)")
    return ok


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "sketch_dirs",
        nargs="*",
        help="Sketch directories to sync (default: every sketch with an effect-manifest.json, "
        "plus every sketch that imports the Shared Effect Knowledge pack)",
    )
    parser.add_argument("--check", action="store_true", help="Dry run; fail if anything would change")
    args = parser.parse_args()

    asset_map = load_asset_map()

    if args.sketch_dirs:
        manifest_dirs = [Path(s).resolve() for s in args.sketch_dirs]
        knowledge_only_dirs = []
    else:
        manifest_dirs = discover_manifest_sketches()
        knowledge_only_dirs = discover_knowledge_only_sketches()

    if not manifest_dirs and not knowledge_only_dirs:
        print("No sketches with effect-manifest.json or knowledge-pack imports found.")
        return 0

    report = []
    all_ok = True
    for sketch_dir in manifest_dirs:
        # Explicit args may name a dir with no effect-manifest.json (a
        # caller asking to sync just the knowledge pack for one sketch) —
        # fall back the same way discover_knowledge_only_sketches() would.
        if (sketch_dir / "effect-manifest.json").exists():
            all_ok = sync_sketch(sketch_dir, asset_map, args.check, report) and all_ok
        else:
            all_ok = sync_knowledge_only_sketch(sketch_dir, args.check, report) and all_ok
    for sketch_dir in knowledge_only_dirs:
        all_ok = sync_knowledge_only_sketch(sketch_dir, args.check, report) and all_ok

    print("\n".join(report))
    if args.check and not all_ok:
        print("\n--check found drift (see above). Run without --check to sync.")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
