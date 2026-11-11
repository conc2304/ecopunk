#!/usr/bin/env python3
"""OC-003 Route B: build an ISOLATED diagnostic binary from a SIBLING COPY of the project. Operator-run, on the machine that will run
the campaign (native build). It never modifies the production checkout, never touches the installed binary, never stops or starts a service.

  build_diagnostic.py --checkout <project root>  [--overlay <dir>] [--jobs N] [--dry-run]
      <project root> = the directory that contains sketches/ and shared/ (on the Pi: .../apps/myApps/EcopunkVideoCollage)

What it does (and in this order; it stops at the first failure):
  1. Chooses the STAGE directory: `<checkout>_oc003diag`, a SIBLING of the checkout (--stage may rename it but must keep the same parent). It must be
     at the same depth because the sketch Makefile reaches OF_ROOT by the RELATIVE path `../../../../..`, and the shared openFrameworks library
     objects are named after that exact string: building the stage with the default relative OF_ROOT reuses the already-built shared library
     (up to date, not touched). Do NOT override OF_ROOT with an absolute path: that makes make treat every library object as new and rewrites
     the shared OF object tree (observed during development; this tool no longer does it). The stage's own objects live in the stage.
     It REFUSES if the stage exists (it deletes nothing; remove an old stage yourself).
  2. Pre-checks: Makefile present, `make`/`c++`/python3 present, free disk space >= 3 GB, the stage's filesystem is the checkout's.
  3. Copies the project into the stage, EXCLUDING .git, docs, every obj/ and bin/ (so no production binary or object file is copied or shared),
     then copies only sketches/experience_runtime/bin/data (runtime data; the Makefile hooks refresh generated files inside the STAGE) and
     SYMLINKS assets/ (the media root, read-only use; 400+ MB).
  4. --overlay DIR (optional, off by default): DIR is laid out like the project root (e.g. the package's source/ directory); each file is shown
     with whether it differs from the checkout's, then copied INTO THE STAGE ONLY. Use it only for files the manifest reported as different,
     and only after the Manager approved overlaying those files.
  5. Verifies the source manifest against the stage. If it does not MATCH, it stops BEFORE building.
  6. Runs `make Release -j<jobs>` in <stage>/sketches/experience_runtime, log -> <stage>/oc003-build.log (+ timestamps).
  7. Hashes the binary, checks the probe capability and freshness, writes <stage>/oc003-provenance.json (manifest result, compiler, build
     log hash, binary hash/mtime). Prints the exact --bin and --workdir for plan_interleaved.py / run_campaign.py.
The installed production binary, its objects and its service stay untouched. The result is an operator-recorded build, not a reproducible-build proof.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import verify_provenance as vp  # noqa: E402

SKIP_DIRS = {".git", "docs", "obj", "bin", ".claude", "__pycache__"}


def die(msg):
    print("STOP:", msg)
    sys.exit(1)


def copy_tree(src, dst, dry):
    n = 0
    for dirpath, dirnames, filenames in os.walk(src):
        rel = os.path.relpath(dirpath, src)
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS and not (rel == "." and d == "assets")]
        for f in filenames:
            n += 1
            if dry:
                continue
            target = os.path.join(dst, rel, f) if rel != "." else os.path.join(dst, f)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            if os.path.islink(os.path.join(dirpath, f)):
                os.symlink(os.readlink(os.path.join(dirpath, f)), target)
            else:
                shutil.copy2(os.path.join(dirpath, f), target)
    return n


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--checkout", required=True)
    ap.add_argument("--stage", help="stage directory (default: <checkout>_oc003diag); must not exist and must be a sibling of the checkout (same parent directory)")
    ap.add_argument("--allow-of-lib-rebuild", action="store_true", help="continue even if the dry run shows the SHARED openFrameworks library would be recompiled (writes into the shared OF tree; Manager decision)")
    ap.add_argument("--overlay")
    ap.add_argument("--jobs", type=int, default=2, help="make -j (default 2: the build must not starve a running installation)")
    ap.add_argument("--dry-run", action="store_true", help="run the pre-checks and print the plan; copy and build nothing")
    a = ap.parse_args()

    checkout = os.path.abspath(a.checkout.rstrip("/"))
    stage = os.path.abspath(a.stage) if a.stage else checkout + "_oc003diag"
    of_root = os.path.realpath(os.path.join(checkout, "sketches", "experience_runtime", "..", "..", "..", "..", ".."))
    sketch = os.path.join(checkout, "sketches", "experience_runtime")
    if not os.path.isfile(os.path.join(sketch, "Makefile")):
        die(f"{sketch}/Makefile not found: --checkout must be the project root")
    if os.path.dirname(stage) != os.path.dirname(checkout):
        die(f"--stage must be a sibling of the checkout ({os.path.dirname(checkout)}) so the relative OF_ROOT in config.make resolves identically")
    if not os.path.isfile(os.path.join(of_root, "libs", "openFrameworksCompiled", "project", "makefileCommon", "compile.project.mk")):
        die(f"OF_ROOT {of_root} (derived from the checkout location) does not look like an openFrameworks tree")
    if os.path.exists(stage):
        die(f"{stage} already exists; this tool deletes nothing. Inspect it, remove it yourself if it is stale, and run again")
    for tool in ("make", "c++", "python3"):
        if shutil.which(tool) is None:
            die(f"`{tool}` not found")
    free = shutil.disk_usage(os.path.dirname(checkout)).free / 2**30
    if free < 3:
        die(f"only {free:.1f} GiB free next to the checkout; need >= 3")
    print(f"checkout : {checkout}\nOF root  : {of_root} (derived; config.make uses it relatively)\nstage    : {stage}\nfree disk: {free:.1f} GiB\njobs     : {a.jobs}\noverlay  : {a.overlay or 'none'}")
    print("plan     : copy (no .git/docs/obj/bin), copy sketches/experience_runtime/bin/data, symlink assets, [overlay], verify manifest, make Release")
    if a.dry_run:
        print("dry run: nothing copied or built")
        return

    t0 = time.time()
    n = copy_tree(checkout, stage, False)
    data_src = os.path.join(sketch, "bin", "data")
    if os.path.isdir(data_src):
        shutil.copytree(data_src, os.path.join(stage, "sketches", "experience_runtime", "bin", "data"), symlinks=True)
    else:
        print("note: checkout has no sketches/experience_runtime/bin/data; the build hooks will create what they can")
    assets = os.path.join(checkout, "assets")
    if os.path.exists(assets):
        os.symlink(assets, os.path.join(stage, "assets"))
    print(f"copied {n} files in {time.time() - t0:.0f}s; assets symlinked to {assets}")

    if a.overlay:
        ov = os.path.abspath(a.overlay)
        for dirpath, _d, files in os.walk(ov):
            for f in files:
                src = os.path.join(dirpath, f)
                rel = os.path.relpath(src, ov)
                tgt = os.path.join(stage, rel)
                same = os.path.isfile(os.path.join(checkout, rel)) and vp.sha256(os.path.join(checkout, rel)) == vp.sha256(src)
                print(f"overlay {'(identical to checkout)' if same else 'DIFFERS from checkout    '} {rel}")
                os.makedirs(os.path.dirname(tgt), exist_ok=True)
                shutil.copy2(src, tgt)

    rows = vp.check_manifest(stage)
    bad = [r for r in rows if r["status"] != "OK"]
    for r in rows:
        print(f"{r['status']:9s} {r['path']}")
    if bad:
        die("the staged sources do not match the manifest; NOT building. Reconcile with the Manager (overlay the differing files if approved)")
    print("SOURCE_IDENTITY: MATCH for the stage")

    sk = os.path.join(stage, "sketches", "experience_runtime")
    log = os.path.join(stage, "oc003-build.log")
    # Dry run first: if make would recompile the SHARED openFrameworks library, stop (that would rewrite shared state outside the stage).
    dry = subprocess.run(["make", "-n", "Release"], cwd=sk, capture_output=True, text=True)
    # Only genuine OF-core compile announcements count. (Every PROJECT compile line also contains `-I.../libs/openFrameworks/...` include paths, and a
    # fresh stage has no objects, so matching on that substring would always trip.)
    import re
    of_compiles = [l for l in dry.stdout.splitlines() if re.match(r"^(echo )?Compiling\s+\S*libs/openFrameworks/", l)]
    if of_compiles and not a.allow_of_lib_rebuild:
        die(f"make -n shows {len(of_compiles)} openFrameworks CORE sources would be recompiled into the shared OF tree (e.g. {of_compiles[0][:120]}). "
            "Nothing was built. The shared library is stale or was built with different flags; rebuilding it is the same as a normal deployment build "
            "but writes outside the stage: ask the Manager, then re-run with --allow-of-lib-rebuild")
    cmd = ["make", "Release", f"-j{a.jobs}"]
    print("building:", " ".join(cmd), "in", sk, "-> log", log)
    t1 = time.time()
    with open(log, "w") as fh:
        fh.write(f"# oc003 diagnostic build {time.ctime()}\n# cwd {sk}\n# {' '.join(cmd)}\n")
        fh.flush()
        rc = subprocess.call(cmd, cwd=sk, stdout=fh, stderr=subprocess.STDOUT)
        fh.write(f"\n# exit {rc} after {time.time() - t1:.0f}s\n")
    if rc != 0:
        die(f"make failed (exit {rc}); see {log}. Nothing outside the stage was changed")
    cands = [os.path.join(sk, "bin", "experience_runtime"), os.path.join(sk, "bin", "experience_runtime.app", "Contents", "MacOS", "experience_runtime")]
    binp = next((c for c in cands if os.path.isfile(c)), None)
    if not binp:
        die("build reported success but no binary was found under " + sk + "/bin")
    rec = os.path.join(stage, "oc003-provenance.json")
    r = subprocess.call([sys.executable, os.path.join(HERE, "verify_provenance.py"), "--root", stage, "--bin", binp, "--record", rec, "--build-log", log])
    print(f"\nbuild finished in {time.time() - t1:.0f}s")
    print(f"diagnostic binary : {binp}\nplan/runner workdir: {sk}\nprovenance record  : {rec}\nbuild log          : {log}")
    print("provenance check exit", r, "(0 = source MATCH + probe PRESENT + fresh)")
    sys.exit(r)


if __name__ == "__main__":
    main()
