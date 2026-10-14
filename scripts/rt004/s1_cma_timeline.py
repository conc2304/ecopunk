#!/usr/bin/env python3
"""RT-004 S1 GPU/CMA timeline -- read-only offline parser (no runtime).

Merges one supervisor run directory (samples.tsv, watchdog.tsv,
frame-phase.log, stall-probe.log) into a single TSV on the shared
CLOCK_MONOTONIC basis (mono_s), relative to launch (rel_s).

VC4 bo_stats semantics used here: per-label lines ("V3D", "binner", "dumb",
"V3D shader", "RCL", "BCL", "kernel BO cache") plus "userspace BO cache",
which is the purgeable subset Mesa has freed but retains for reuse (it is
NOT disjoint from "V3D": summing both would exceed the 256 MiB CMA pool).
dma-buf totals (decoder buffers, window buffer) overlap CMA too. None of
these are added together or to RSS.

  python3 scripts/rt004/s1_cma_timeline.py RUN_DIR > timeline.tsv
"""

import csv
import json
import os
import re
import sys

BO = re.compile(r"\s*([A-Za-z0-9 ]+?):\s+(\d+)kb BOs \((\d+)\)")


def bo_fields(s):
    out = {}
    for part in (s or "").split(";"):
        m = BO.match(part)
        if m:
            out[m.group(1).strip()] = (int(m.group(2)), int(m.group(3)))
    return out


def kb_mib(v):
    try:
        return "%.1f" % (float(v) / 1024.0)
    except (TypeError, ValueError):
        return ""


def main(run_dir):
    man = json.load(open(os.path.join(run_dir, "manifest.json")))
    launch = man["launch_mono_s"]
    rows = []
    for r in csv.DictReader(open(os.path.join(run_dir, "samples.tsv")), delimiter="\t"):
        b = bo_fields(r["drm_bo_stats"])
        rows.append({"mono_s": float(r["mono_s"]), "source": "collector", "event": r["phase"],
                     "cma_free_mib": kb_mib(r["meminfo_cmafree_kb"]), "mem_available_mib": kb_mib(r["meminfo_memavailable_kb"]),
                     "rss_mib": kb_mib(r["status_vmrss_kb"]), "rss_anon_mib": kb_mib(r["status_rssanon_kb"]),
                     "v3d_kb": b.get("V3D", ("", ""))[0], "v3d_bos": b.get("V3D", ("", ""))[1],
                     "purgeable_userspace_cache_kb": b.get("userspace BO cache", (0, 0))[0],
                     "binner_kb": b.get("binner", ("", ""))[0], "shader_kb": b.get("V3D shader", (0, 0))[0],
                     "dumb_kb": b.get("dumb", ("", ""))[0], "dmabuf_objects": r["dmabuf_objects"], "dmabuf_bytes": r["dmabuf_bytes"],
                     "detail": ""})
    for r in csv.DictReader(open(os.path.join(run_dir, "watchdog.tsv")), delimiter="\t"):
        rows.append({"mono_s": float(r["mono_s"]), "source": "watchdog", "event": r["event"], "detail": r["detail"]})
    for line in open(os.path.join(run_dir, "frame-phase.log")):
        m = re.search(r"mono=([\d.]+)", line)
        if line.startswith(("EVENT", "SLOW", "CONFIG")) and m:
            head, _, res = line.strip().partition(" | ")
            kv = dict(re.findall(r"(\w+)=([^\s|]+)", res))
            rows.append({"mono_s": float(m.group(1)), "source": "frame-phase", "event": head.split()[0] + ":" + (re.search(r"kind=(\S+)", head).group(1) if "kind=" in head else ""),
                         "history_pix_mib": kv.get("historyPixMiB", ""), "playhead_tex": kv.get("playheadTex", ""), "detail": head[:200]})
    for line in open(os.path.join(run_dir, "stall-probe.log")):
        m = re.match(r"\s*NOTE frame=\d+ mono=([\d.]+) t=\S+ ([^:]+): (.*)", line)
        if m:
            rows.append({"mono_s": float(m.group(1)), "source": "stall-note", "event": m.group(2), "detail": m.group(3)[:160]})
        m = re.match(r"--- LONG FRAME #\d+ frame=(\d+) mono=([\d.]+) .*interval=(\S+)", line.strip())
        if m:
            rows.append({"mono_s": float(m.group(2)), "source": "stall-long", "event": "long-frame", "detail": "frame=%s interval=%s" % (m.group(1), m.group(3))})
    rows.sort(key=lambda r: r["mono_s"])
    # Scene state from SceneManager notes.
    scene = "-"
    cols = ["rel_s", "mono_s", "source", "scene", "event", "cma_free_mib", "mem_available_mib", "rss_mib", "rss_anon_mib", "v3d_kb", "v3d_bos",
            "purgeable_userspace_cache_kb", "binner_kb", "shader_kb", "dumb_kb", "dmabuf_objects", "dmabuf_bytes", "history_pix_mib", "playhead_tex", "detail"]
    w = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    w.writerow(["# run_id=%s source_revision=%s build_id=%s stop=%s launch_mono_s=%.3f" % (man["run_id"], man.get("source_revision"), man.get("build_id"), man.get("stop_reason"), launch)])
    w.writerow(cols)
    for r in rows:
        if r["source"] == "stall-note" and r["event"] in ("scene.activate", "scene.setup"):
            scene = r["detail"] + (" (setup)" if r["event"] == "scene.setup" else "")
        elif r["source"] == "stall-note" and r["event"] == "scene.deactivate":
            scene = "transition"
        elif r["source"] == "watchdog" and r["event"] == "signal":
            scene = "terminating"
        r["rel_s"] = "%.3f" % (r["mono_s"] - launch)
        r["mono_s"] = "%.3f" % r["mono_s"]
        r["scene"] = scene
        w.writerow([r.get(c, "") for c in cols])


if __name__ == "__main__":
    main(sys.argv[1])
