#!/usr/bin/env python3
"""OC-003 provenance check that does NOT need .git. Read-only. Separates three different questions and never merges them:

  1. SOURCE IDENTITY      do the files on disk hash exactly like the desktop-accepted sources (attribution-source.sha256)?
  2. BINARY CAPABILITY    does the binary CONTAIN the attribution probe (its CSV header / variable names are compiled in)?
  3. BINARY FRESHNESS     is the binary newer than every source file of the manifest (a binary older than its source is stale)?

A matching manifest proves the SOURCE files are identical. It does NOT prove a given binary was built from them: only a build
that this tooling observed end to end (build_diagnostic.py: manifest verified immediately before `make`, binary hashed immediately after,
build log hashed) comes close, and even that record is "recorded by the operator's build", not a reproducible-build proof.
Platform-dependent: binary hashes are only comparable for the same file on the same device (before/after; run to run).

  verify_provenance.py --root <project root containing sketches/>  [--bin PATH] [--record OUT.json] [--build-log FILE] [--manifest FILE]

  --root      checkout (or staged copy) whose sketches/... files are checked. scripts/oc003/... entries of the manifest are checked
              against THIS PACKAGE (the directory two levels above this script), so the check works from an unpacked package that
              has no docs/ and no repository around it.
Exit 0 only if source identity MATCHES and (when --bin is given) the capability is PRESENT and the binary is not older than the sources.
"""
import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PKG_ROOT = os.path.dirname(os.path.dirname(HERE))
MANIFEST = os.path.join(HERE, "attribution-source.sha256")
# Strings that exist in the binary only when the attribution probe is compiled in (CSV header, event columns) or that the campaign relies on.
CAPABILITY = [b"raw_cfail", b"fx_cfail", b"ev_fail", b"EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES", b"EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH"]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def read_manifest(path):
    entries = []
    with open(path) as fh:
        for line in fh:
            line = line.strip()
            if line and not line.startswith("#"):
                digest, rel = line.split(None, 1)
                entries.append((digest, rel.strip().lstrip("*")))
    return entries


def check_manifest(root, manifest=MANIFEST, pkg_root=PKG_ROOT):
    rows = []
    for digest, rel in read_manifest(manifest):
        base = pkg_root if rel.startswith("scripts/oc003/") else root
        p = os.path.join(base, rel)
        if not os.path.isfile(p):
            rows.append({"path": rel, "where": base, "status": "MISSING", "expected": digest, "actual": None})
        else:
            actual = sha256(p)
            rows.append({"path": rel, "where": base, "status": "OK" if actual == digest else "DIFFERENT", "expected": digest, "actual": actual})
    return rows


def scan_binary(path, needles=CAPABILITY):
    found = {n.decode(): False for n in needles}
    keep = max(len(n) for n in needles)
    tail = b""
    with open(path, "rb") as fh:
        while True:
            chunk = fh.read(8 << 20)
            if not chunk:
                break
            buf = tail + chunk
            for n in needles:
                if n in buf:
                    found[n.decode()] = True
            tail = buf[-keep:]
    return found


def tool_version(cmd):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=20)
        return (r.stdout or r.stderr).strip().splitlines()[0]
    except Exception as e:  # noqa: BLE001
        return f"unavailable ({type(e).__name__})"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", required=True)
    ap.add_argument("--bin")
    ap.add_argument("--record")
    ap.add_argument("--build-log")
    ap.add_argument("--manifest", default=MANIFEST)
    a = ap.parse_args()
    ok = True
    if not os.path.isfile(a.manifest):
        sys.exit(f"manifest {a.manifest} not found")

    rows = check_manifest(a.root, a.manifest)
    for r in rows:
        print(f"{r['status']:9s} {r['path']}" + ("" if r["status"] == "OK" else f"   (expected {r['expected'][:12]}…, got {str(r['actual'])[:12]}…)"))
    src_ok = all(r["status"] == "OK" for r in rows)
    print("SOURCE_IDENTITY: " + ("MATCH (all %d files identical to the desktop-accepted sources)" % len(rows) if src_ok else
          "MISMATCH (%d of %d files differ or are missing; not automatically a defect, but the Manager must see it before a campaign)" %
          (sum(r["status"] != "OK" for r in rows), len(rows))))
    ok &= src_ok

    rec = {"time": time.ctime(), "root": os.path.abspath(a.root), "package_root": PKG_ROOT, "manifest": a.manifest, "manifest_sha256": sha256(a.manifest),
           "source_rows": rows, "source_identity": "MATCH" if src_ok else "MISMATCH", "host": platform.platform(), "machine": platform.machine(),
           "python": sys.version.split()[0], "compiler": tool_version(["c++", "--version"]), "make": tool_version(["make", "--version"]),
           "build_configuration": "make Release (sketches/experience_runtime, config.make as present in --root)"}
    if a.bin:
        if not os.path.isfile(a.bin):
            print(f"BINARY: {a.bin} does not exist")
            sys.exit(1)
        st = os.stat(a.bin)
        caps = scan_binary(a.bin)
        cap_ok = all(caps.values())
        srcs = [os.path.join(a.root, r["path"]) for r in rows if r["path"].startswith("sketches/") and os.path.isfile(os.path.join(a.root, r["path"]))]
        newer = [os.path.relpath(p, a.root) for p in srcs if os.path.getmtime(p) > st.st_mtime]
        rec["binary"] = {"path": os.path.abspath(a.bin), "sha256": sha256(a.bin), "size": st.st_size, "mtime": time.ctime(st.st_mtime),
                         "capability_strings": caps, "sources_newer_than_binary": newer}
        print(f"BINARY: {a.bin}\n  sha256 {rec['binary']['sha256']}\n  size {st.st_size}  mtime {rec['binary']['mtime']}")
        for k, v in caps.items():
            print(f"  {'present' if v else 'ABSENT ':7s} {k}")
        print("BINARY_CAPABILITY: " + ("PRESENT (the attribution probe is compiled in)" if cap_ok else
              "ABSENT -- this binary cannot produce the event columns; do not run the campaign with it (see Route B)"))
        print("BINARY_FRESHNESS: " + ("binary is newer than every checked source file" if not newer else
              "STALE? these sources are NEWER than the binary: " + ", ".join(newer)))
        ok &= cap_ok and not newer
        rec["binary_capability"] = "PRESENT" if cap_ok else "ABSENT"
        if a.build_log:
            rec["build_log"] = {"path": os.path.abspath(a.build_log), "sha256": sha256(a.build_log), "size": os.path.getsize(a.build_log)}
        print("PROVENANCE: the link between this binary and these sources is NOT PROVEN by this check "
              "(a matching manifest proves source identity only; capability + freshness are circumstantial evidence)")
    else:
        print("BINARY_CAPABILITY: NOT CHECKED (no --bin)")
    if a.record:
        with open(a.record, "w") as fh:
            json.dump(rec, fh, indent=2)
        print("record written:", a.record)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
