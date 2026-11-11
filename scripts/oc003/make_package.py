#!/usr/bin/env python3
"""OC-003 validation package builder (DEVELOPMENT machine only; reads the repository, writes only under --out).

  make_package.py --out DIR [--doc docs/reviews/OC-003-rockpi-attribution-validation-operator-package.md] [--refresh-manifest]

--refresh-manifest  regenerates scripts/oc003/attribution-source.sha256 from the fixed MANIFEST_FILES list below (the only place that list lives),
                    then builds the package. Without it the existing manifest is used and is CHECKED against the working tree first (stops on a difference).
Package layout (a plain directory + tar.gz + .sha256; nothing is installed anywhere):
  oc003-attribution-package/
    OPERATOR.md                 the authoritative procedure (a copy: docs/ is NOT deployed to the Pi)
    PACKAGE_INFO.json           when/where built, repository branch+HEAD+dirty count (informational), manifest hash
    PACKAGE_SHA256SUMS          sha256 of every file; verify with `sha256sum -c PACKAGE_SHA256SUMS` after transfer
    scripts/oc003/*.py + attribution-source.sha256     every tool; they use only paths relative to themselves
    source/<manifest sketch files>   the desktop-accepted sources, same relative paths as in the project (Route B overlay input; NOT applied automatically)
The package contains no binary, no media, no settings and no secrets.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import verify_provenance as vp  # noqa: E402

MANIFEST_FILES = [
    "sketches/experience_runtime/src/SceneColorSampler.h",
    "sketches/experience_runtime/src/SceneColorSampler.cpp",
    "sketches/experience_runtime/src/SceneColorSamplerDiag.cpp",
    "sketches/experience_runtime/src/Rt004FramePhaseProbe.h",
    "sketches/experience_runtime/src/Rt004FramePhaseProbe.cpp",
    "sketches/experience_runtime/src/ExperienceRuntime.cpp",
    "sketches/experience_runtime/src/ExperienceRuntime.h",
    # OC-003-F diagnostic CPU RAW acquisition + palette analysis (all off by default)
    "sketches/experience_runtime/src/CpuRawAcquisition.h",
    "sketches/experience_runtime/src/PaletteAnalyzer.h",
    "sketches/experience_runtime/src/PaletteSink.h",
    "sketches/experience_runtime/src/RawVideoGate.h",
    "sketches/experience_runtime/src/PredominantColor.h",
    "sketches/experience_runtime/src/SceneColorSeries.h",
    "sketches/experience_runtime/test/palette_replay.cpp",
    "sketches/experience_runtime/test/Makefile.tests",
    "scripts/oc003/aggregate_frames.py",
    "scripts/oc003/build_diagnostic.py",
    "scripts/oc003/list_media_ids.py",
    "scripts/oc003/make_package.py",
    "scripts/oc003/palette_report.py",
    "scripts/oc003/run_qualification.py",
    "scripts/oc003/sysmon.py",
    "scripts/oc003/verify_generations.py",
    "scripts/oc003/test_verify_generations.py",
    "scripts/oc003/test_correctness_kind.py",
    "scripts/oc003/cpu_access_cost.py",
    "scripts/oc003/plan_interleaved.py",
    "scripts/oc003/reconcile_events.py",
    "scripts/oc003/run_campaign.py",
    "scripts/oc003/sampler_stats.py",
    "scripts/oc003/stall_analysis.py",
    "scripts/oc003/summarize_campaign.py",
    "scripts/oc003/test_diag_tooling.py",
    "scripts/oc003/test_event_analysis.py",
    "scripts/oc003/test_run_campaign.py",
    "scripts/oc003/verify_provenance.py",
    "scripts/oc003/verify_run.py",
]
# make_package.py hashes itself into the manifest, so the manifest is regenerated AFTER any edit of this file.
TOOLS = [f for f in MANIFEST_FILES if f.startswith("scripts/oc003/")]


def sh(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    return r.stdout.strip() if r.returncode == 0 else f"unavailable ({cmd[0]})"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=".")
    ap.add_argument("--doc", default=os.path.join(ROOT, "docs", "reviews", "OC-003-rockpi-attribution-validation-operator-package.md"))
    ap.add_argument("--refresh-manifest", action="store_true")
    ap.add_argument("--manifest-only", action="store_true", help="regenerate attribution-source.sha256 and stop (no package)")
    ap.add_argument("--name", default="oc003-attribution-package")
    a = ap.parse_args()

    for rel in MANIFEST_FILES:
        if not os.path.isfile(os.path.join(ROOT, rel)):
            sys.exit(f"manifest file missing in the working tree: {rel}")
    manifest = vp.MANIFEST
    if a.refresh_manifest or a.manifest_only:
        with open(manifest, "w") as fh:
            for rel in MANIFEST_FILES:
                fh.write(f"{vp.sha256(os.path.join(ROOT, rel))}  {rel}\n")
        print("manifest regenerated:", manifest)
    rows = vp.check_manifest(ROOT, manifest, ROOT)
    bad = [r["path"] for r in rows if r["status"] != "OK"]
    listed = {r["path"] for r in rows}
    if bad or listed != set(MANIFEST_FILES):
        sys.exit(f"manifest does not match the working tree / file list: {bad or sorted(listed ^ set(MANIFEST_FILES))}; use --refresh-manifest")

    if a.manifest_only:
        print("manifest verified against the working tree:", len(rows), "files")
        return
    if not os.path.isfile(a.doc):
        sys.exit(f"operator document not found: {a.doc}")
    pkg = os.path.join(os.path.abspath(a.out), a.name)
    if os.path.exists(pkg):
        sys.exit(f"{pkg} already exists; this tool deletes nothing")
    for rel in TOOLS + ["scripts/oc003/attribution-source.sha256"]:
        dst = os.path.join(pkg, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(os.path.join(ROOT, rel), dst)
    for rel in MANIFEST_FILES:
        if rel.startswith("sketches/"):
            dst = os.path.join(pkg, "source", rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(os.path.join(ROOT, rel), dst)
    shutil.copy2(a.doc, os.path.join(pkg, "OPERATOR.md"))
    info = {"built": time.ctime(), "host": sh(["uname", "-a"]), "branch": sh(["git", "rev-parse", "--abbrev-ref", "HEAD"]),
            "head": sh(["git", "rev-parse", "HEAD"]), "dirty_paths": len(sh(["git", "status", "--short"]).splitlines()),
            "note": "the implementation is UNCOMMITTED: HEAD is the base revision, the manifest is the identity", "manifest_sha256": vp.sha256(manifest),
            "manifest_files": len(MANIFEST_FILES)}
    with open(os.path.join(pkg, "PACKAGE_INFO.json"), "w") as fh:
        json.dump(info, fh, indent=2)
    sums = []
    for dirpath, _d, files in sorted(os.walk(pkg)):
        for f in sorted(files):
            p = os.path.join(dirpath, f)
            if f != "PACKAGE_SHA256SUMS":
                sums.append(f"{vp.sha256(p)}  {os.path.relpath(p, pkg)}")
    with open(os.path.join(pkg, "PACKAGE_SHA256SUMS"), "w") as fh:
        fh.write("\n".join(sorted(sums, key=lambda x: x.split("  ", 1)[1])) + "\n")
    tgz = pkg + ".tar.gz"
    with tarfile.open(tgz, "w:gz") as tf:
        tf.add(pkg, arcname=a.name)
    with open(tgz + ".sha256", "w") as fh:
        fh.write(f"{vp.sha256(tgz)}  {os.path.basename(tgz)}\n")
    print(f"package : {pkg}\ntarball : {tgz}\nsha256  : {vp.sha256(tgz)}\nfiles   : {len(sums)}")


if __name__ == "__main__":
    main()
