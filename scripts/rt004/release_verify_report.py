#!/usr/bin/env python3
"""RT-004 Temporal inactive-release verification -- read-only analyzer.

Turns one supervised `release-verify` run directory into the Markdown
transition table + mechanical PASS/FAIL/INCONCLUSIVE verdict required by the
focused Pi verification brief. Runs nothing; never modifies the run.

  python3 scripts/rt004/release_verify_report.py RUN_DIR > report-section.md

Semantics: CmaFree / RSS / dma-buf / bo_stats = measured (collector, 1 Hz,
plus the watchdog's own 0.25 s reads in watchdog.tsv); "userspace BO cache"
is Mesa's purgeable subset of V3D (never added to V3D); history pixel bytes =
measured payload; playhead MiB = estimate. A drop in app-side ownership does
not prove CMA was returned (Mesa may cache freed BOs); CmaFree alone does not
identify which resource was released.
"""

import csv
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from s1_cma_timeline import bo_fields  # noqa: E402

# Prior S1 (campaign-20261012T023119Z-747b), CmaFree MiB at equivalent phases.
S1 = {"prelaunch": 231.9, "blob-baseline": 71.7, "temporal-1": 54.5, "blob-recovery": 54.5, "temporal-2": 49.6,
      "blueprint": "48.5 -> 33.5 (watchdog stop inside first setup)"}


def load(run_dir):
    man = json.load(open(os.path.join(run_dir, "manifest.json")))
    samples = []
    p = os.path.join(run_dir, "samples.tsv")
    if os.path.exists(p):
        samples = list(csv.DictReader(open(p), delimiter="\t"))
    wd = list(csv.DictReader(open(os.path.join(run_dir, "watchdog.tsv")), delimiter="\t")) if os.path.exists(os.path.join(run_dir, "watchdog.tsv")) else []
    notes = []
    sp = os.path.join(run_dir, "stall-probe.log")
    if os.path.exists(sp):
        for line in open(sp):
            m = re.match(r"\s*NOTE frame=\d+ mono=([\d.]+) t=\S+ ([^:]+): (.*)", line)
            if m:
                notes.append((float(m.group(1)), m.group(2), m.group(3).strip()))
    windows = []
    fp = os.path.join(run_dir, "frame-phase.log")
    if os.path.exists(fp):
        for line in open(fp):
            if line.startswith("WINDOW"):
                m = re.search(r"mono=([\d.]+)", line)
                kv = dict(re.findall(r"(\w+)=([^\s|]+)", line))
                if m:
                    windows.append((float(m.group(1)), kv))
    out = ""
    for name in ("app-stdout.log", "app-stderr.log"):
        q = os.path.join(run_dir, name)
        if os.path.exists(q):
            out += open(q, errors="replace").read()
    return man, samples, wd, notes, windows, out


def f(v, d=1):
    try:
        return ("%." + str(d) + "f") % float(v)
    except (TypeError, ValueError):
        return "unavailable"


def kbm(v):
    try:
        return "%.1f" % (float(v) / 1024.0)
    except (TypeError, ValueError):
        return "unavailable"


def main(run_dir):
    man, samples, wd, notes, windows, stdout = load(run_dir)
    launch = man.get("launch_mono_s") or 0.0
    rel = lambda t: "%.2f" % (t - launch)  # noqa: E731
    marks = [(t, d) for t, l, d in notes if l == "driver.action" and d in ("blob-baseline", "temporal-1", "blob-recovery", "temporal-2", "blueprint")]
    # Samples after the watchdog's first signal are app teardown, not scene state.
    sig = [float(r["mono_s"]) for r in wd if r.get("event") == "signal"]
    cutoff = min(sig) if sig else float("inf")
    running = [s for s in samples if s.get("phase") == "running" and float(s["mono_s"]) < cutoff]
    pre = [s for s in samples if s.get("phase") == "prelaunch"]
    print("## Run identity\n")
    print("- run: `%s` · stop: `%s` (class `%s`) · app returncode %s" % (man.get("run_id"), man.get("stop_reason"), man.get("stop_class"), man.get("app_returncode")))
    print("- source: `%s` · build id (binary sha256 prefix): `%s` · binary: %s" % (man.get("source_revision"), man.get("build_id"), json.dumps(man.get("binary", {}))))
    print("- driver script: `%s` · release toggle: `%s`" % (man.get("env_diagnostic", {}).get("EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT"),
                                                          man.get("env_diagnostic", {}).get("EXPERIENCE_RUNTIME_TEMPORAL_INACTIVE_RELEASE", "default (release)")))
    print("- stabilized Blob CMA baseline (supervisor): %s MiB\n" % f(man.get("cma_baseline_mib")))

    print("## Transition table (rel = seconds since launch, CLOCK_MONOTONIC)\n")
    print("| Phase | start rel | end-of-phase CmaFree | min CmaFree in phase | V3D kB (BOs) | purgeable cache kB | binner kB | dma-buf objs / MiB | RSS / RssAnon MiB | history frames · MiB | playheads | S1 CmaFree |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|")
    if pre:
        s = pre[-1]
        print("| prelaunch | — | %s | — | — | — | — | %s / %s | — | — | — | %s |" % (kbm(s["meminfo_cmafree_kb"]), s["dmabuf_objects"], kbm(float(s["dmabuf_bytes"]) / 1024) if s["dmabuf_bytes"].isdigit() else "unavailable", S1["prelaunch"]))
    end_mono = float(running[-1]["mono_s"]) if running else launch
    for i, (t, label) in enumerate(marks):
        t_next = marks[i + 1][0] if i + 1 < len(marks) else end_mono + 0.001
        seg = [s for s in running if t <= float(s["mono_s"]) < t_next]
        if not seg:
            print("| %s | %s | unavailable | unavailable | | | | | | | | %s |" % (label, rel(t), S1.get(label)))
            continue
        last = seg[-1]
        b = bo_fields(last["drm_bo_stats"])
        cmas = [float(s["meminfo_cmafree_kb"]) for s in seg if s["meminfo_cmafree_kb"].isdigit()]
        win = [kv for (m, kv) in windows if t <= m < t_next]
        w = win[-1] if win else {}
        dmab = last["dmabuf_bytes"]
        print("| %s | %s | %s | %s | %s (%s) | %s | %s | %s / %s | %s / %s | %s · %s | %s | %s |" % (
            label, rel(t), kbm(last["meminfo_cmafree_kb"]), f(min(cmas) / 1024) if cmas else "unavailable",
            b.get("V3D", ("unavailable", "-"))[0], b.get("V3D", ("-", "-"))[1], b.get("userspace BO cache", (0, 0))[0], b.get("binner", ("unavailable",))[0],
            last["dmabuf_objects"], kbm(float(dmab) / 1024) if dmab.isdigit() else "unavailable", kbm(last["status_vmrss_kb"]), kbm(last["status_rssanon_kb"]),
            w.get("historyFrames", "?"), w.get("historyPixMiB", "?"), w.get("playheadTex", "?"), S1.get(label)))
    stops = [r for r in wd if r.get("event") in ("stop", "stop_last_reading")]
    if stops:
        print("\nWatchdog: " + "; ".join("%s @%s %s" % (r["event"], rel(float(r["mono_s"])), r["detail"]) for r in stops))

    print("\n## Lifecycle and release markers\n")
    keep = ("scene.setup", "scene.activate", "scene.deactivate", "temporal.releaseForInactive", "temporal.loadExplicit", "canonical.load")
    for t, l, d in notes:
        if l in keep:
            print("- %s s `%s` %s" % (rel(t), l, d[:220]))
    gl = [l.strip() for l in stdout.splitlines() if "GL error" in l or "GL_INVALID" in l]
    print("\nGL error lines in app output: %s" % (gl if gl else "none"))

    # ---- mechanical verdict -------------------------------------------------
    releases = [d for t, l, d in notes if l == "temporal.releaseForInactive"]
    rel_ok = [d for d in releases if "after: historyFrames=0 playheadTex=0 decoderLoaded=0" in d and "decoderWasLoaded=1" in d]
    bp_setup = any(l == "scene.setup" and d == "blueprint_emergence" for t, l, d in notes)
    bp_active = any(l == "scene.activate" and d == "blueprint_emergence" for t, l, d in notes)
    reloads = [t for t, l, d in notes if l == "temporal.loadExplicit"]
    dm = [(float(s["mono_s"]), int(s["dmabuf_objects"])) for s in running if s["dmabuf_objects"].isdigit()]
    rel_marks = [t for t, l, d in notes if l == "temporal.releaseForInactive"]
    dmabuf_dropped = None
    if dm and rel_marks:
        before = [n for (m, n) in dm if m < rel_marks[0]]
        after = [n for (m, n) in dm if rel_marks[0] < m < rel_marks[0] + 3.5]
        if before and after:
            dmabuf_dropped = min(after) < max(before)
    problems, missing = [], []
    sc = man.get("stop_class")
    if sc in ("safety", "crash", "ceiling", "error"):
        problems.append("supervisor stop: %s" % man.get("stop_reason"))
    if len(releases) < 2:
        (problems if sc == "completed" else missing).append("expected 2 release markers (T->Blob, T->Blueprint), got %d" % len(releases))
    if releases and len(rel_ok) != len(releases):
        problems.append("a release left history/playheads/decoder resident")
    if len(reloads) < 2:
        (problems if sc == "completed" else missing).append("Temporal reactivation reload not observed (loadExplicit x%d)" % len(reloads))
    if dmabuf_dropped is None:
        missing.append("dma-buf counters unavailable around the first release")
    elif not dmabuf_dropped:
        problems.append("decoder dma-buf pool not reduced after release (retention contrary to patch)")
    if not bp_setup or not bp_active:
        missing.append("Blueprint %s" % ("setup not reached" if not bp_setup else "setup reached but activation not logged"))
    if not samples or not any(s["drm_bo_stats"] not in ("", "unavailable") for s in running):
        missing.append("bo_stats counters unavailable")
    verdict = "FAIL" if problems else ("INCONCLUSIVE" if missing else "PASS")
    cmas_all = [float(s["meminfo_cmafree_kb"]) / 1024 for s in running if s["meminfo_cmafree_kb"].isdigit()]
    wd_min = [json.loads(r["detail"]).get("cma_free_mib") for r in wd if r.get("event") == "stop_last_reading"]
    print("\n## Verdict (mechanical)\n")
    print("- releases: %d (clean %d) · reloads: %d · dma-buf reduced after first release: %s · Blueprint setup/activate: %s/%s" % (
        len(releases), len(rel_ok), len(reloads), dmabuf_dropped, bp_setup, bp_active))
    print("- minimum CmaFree: collector %s MiB%s" % (f(min(cmas_all)) if cmas_all else "unavailable", ("; watchdog stop reading %s MiB" % f(wd_min[0])) if wd_min else ""))
    for p in problems:
        print("- FAIL reason: %s" % p)
    for m in missing:
        print("- missing: %s" % m)
    print("\n**%s**" % verdict)
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
