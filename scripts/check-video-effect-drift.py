#!/usr/bin/env python3
"""Drift checks for the consolidated video-effect system.

Per docs/shared-video-effect-architecture.md §9 Phase 10 ("Add checks for
asset synchronization, duplicate IDs, manifest resolution, local
re-registration, local managed shader copies, and duplicated
implementations"). Read-only — never modifies anything. Intended to be run
manually (or wired into CI later); mirrors scripts/check-hud-library-uniqueness.sh's
existing "fail loudly if something in this repo has drifted" role.

Usage:
    scripts/check-video-effect-drift.py

Exits non-zero if any check fails.
"""
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
VIDEO_EFFECTS_SRC = REPO_ROOT / "shared" / "src" / "video-effects"
SKETCHES_DIR = REPO_ROOT / "sketches"

# Effect names the canonical catalog owns (shared/src/video-effects/catalog/
# DefaultVideoEffectCatalog.cpp + effects/*.cpp) — kept in sync manually with
# that file's registerEffect() calls. Used by check 3 below to flag sketch
# source outside shared/src/video-effects/ that still hand-writes a
# per-name uniform cascade for one of these, which is the "local
# re-registration" / "duplicated implementation" pattern this check exists
# to catch going forward.
CANONICAL_EFFECT_IDS = [
    "desaturate", "invert", "solarize", "scanlines", "recolor", "threshold", "dither",
    "channelshift", "heatmap_recolor", "hue_rotate", "ascii_solarpunk", "bioluminescence",
    "chromatic_aberration", "edge_glow", "ink_outlines", "pixel_drift", "pixel_sorting",
    "water_refraction", "caustics", "motion_extraction", "motion_composite",
    "erosion_accumulation", "erosion_history_blend", "ridgeline", "temporal_trails",
    "reaction_diffusion",
]


def check_asset_sync() -> bool:
    print("== 1/3: asset synchronization (delegates to sync-video-effect-assets.py --check) ==")
    result = subprocess.run(
        [sys.executable, str(REPO_ROOT / "scripts" / "sync-video-effect-assets.py"), "--check"],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
    )
    print(result.stdout.strip())
    if result.stderr.strip():
        print(result.stderr.strip())
    return result.returncode == 0


def check_duplicate_ids() -> bool:
    print("\n== 2/3: duplicate canonical effect IDs in shared/src/video-effects ==")
    id_pattern = re.compile(r'def\.id\s*=\s*"([^"]+)"')
    seen = {}
    ok = True
    for cpp_file in sorted(VIDEO_EFFECTS_SRC.rglob("*.cpp")):
        text = cpp_file.read_text(errors="replace")
        for match in id_pattern.finditer(text):
            effect_id = match.group(1)
            if effect_id in seen and seen[effect_id] != cpp_file:
                print(f"  ERROR: duplicate id \"{effect_id}\" registered in both {seen[effect_id]} and {cpp_file}")
                ok = False
            else:
                seen[effect_id] = cpp_file
    if ok:
        print(f"  {len(seen)} canonical ids, no duplicates found")
    return ok


def check_local_reregistration() -> bool:
    print("\n== 3/3: possible un-migrated per-effect uniform cascades outside shared/src/video-effects ==")
    # Best-effort: an `if (... == "<effect id>")` or `name == "<effect id>"`
    # style comparison against one of the canonical ids, found in a sketch's
    # own src/ (not shared/src/video-effects/, not shared/assets/, not this
    # repo's docs/). A hit here doesn't necessarily mean a bug — some of
    # these are intentionally-preserved per-instance-randomized branches
    # (documented inline where that's the case, e.g. BEFragment.cpp's
    # dither/threshold/recolor/channelshift/hue_rotate/heatmap_recolor/
    # pixel_sorting branches) — but every hit is worth a human glance to
    # confirm it's an intentional exception, not silent duplication drift.
    warnings = []
    id_alternation = "|".join(re.escape(i) for i in CANONICAL_EFFECT_IDS)
    pattern = re.compile(r'==\s*"(' + id_alternation + r')"')

    for sketch_dir in sorted(SKETCHES_DIR.glob("*/src")):
        for cpp_file in sorted(sketch_dir.rglob("*.cpp")):
            text = cpp_file.read_text(errors="replace")
            matches = sorted(set(m.group(1) for m in pattern.finditer(text)))
            if matches:
                warnings.append((cpp_file.relative_to(REPO_ROOT), matches))

    if not warnings:
        print("  none found")
    else:
        for path, ids in warnings:
            print(f"  NOTE: {path} references canonical effect id(s) by name: {', '.join(ids)}")
        print(f"  {len(warnings)} file(s) flagged for human review (not a failure by itself)")
    return True  # advisory only — never fails the run


def main() -> int:
    results = [check_asset_sync(), check_duplicate_ids(), check_local_reregistration()]
    print()
    if all(results):
        print("All video-effect drift checks passed.")
        return 0
    print("One or more video-effect drift checks FAILED — see above.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
