#!/usr/bin/env python3
"""RT-004 one-command Raspberry Pi validation campaign -- DIAGNOSTIC ONLY.

Every runtime scenario is a separate short process launched through
memattr_supervisor.py (external 1 Hz collector + independent watchdog; the
orchestrator cannot override its thresholds). Between scenarios the
orchestrator checks that no runtime process survived and that memory/CMA
recovered to the preflight levels. Any safety stop, ceiling stop, crash,
missing required measurement, watchdog malfunction, surviving process or
non-recovery ABORTS the remaining runtime scenarios (NOT RUN; nonzero exit).
No automatic retries.

Commands (run from sketches/experience_runtime on the Pi):
  provenance   (dev host, before deploy) write scripts/rt004/PROVENANCE.json
  plan         print the scenario matrix and exact supervisor commands; runs nothing
  preflight    all checks incl. the 21 Linux offline watchdog tests; no runtime launch
  run          preflight (always re-run) then the campaign
Offline tests use --fixture CONFIG.json (dummy processes only).

Limitations: Python cannot make a host reboot, lost HDMI, power failure or a
kernel/GPU hang safe; unattended safety is NOT proven. DRM/dma-buf counters
are unavailable without root (the app never runs as root).
"""

import argparse
import csv
import datetime
import glob
import hashlib
import json
import os
import platform
import random
import re
import shutil
import signal
import subprocess
import sys
import tarfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SUP = os.path.join(HERE, "memattr_supervisor.py")
OFFLINE_TESTS = os.path.join(HERE, "test_memattr_supervisor.py")
PROVENANCE = os.path.join(HERE, "PROVENANCE.json")
sys.path.insert(0, HERE)
import memattr_supervisor as ms  # noqa: E402

CAMPAIGN_VERSION = "rt004-pi-campaign/1"
EXPECTED_OFFLINE_TESTS = 21
RECOVERY_TIMEOUT_S = 30.0          # waiting time, not a resource threshold
LOW_CONFIDENCE_N = 100             # p99 from fewer frames is flagged
PER_RUN_LOG_BUDGET = ms.LOG_CAP_BYTES + (1 << 20)
RUNTIME_PATTERN = "bin/experience_runtime"

D = "EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT"

# ---------------------------------------------------------------------------
# Scenario matrix (required order). "requires" = earlier scenario that must
# have PASSED for this one to run.
SCENARIOS = [
    {"id": "S1", "title": "Combined production smoke", "env": {D: "smoke-circuit"},
     "purpose": "Blob->Temporal->Blob->Temporal->Blueprint(+RESEED)->Temporal: startup-to-first-frame, 1st/2nd/3rd Temporal activation worst frames, media sync, RESEED media advance, first effect snapshot, GL error lines",
     "predicate": "driver exit reached; scene-owner sequence blob,temporal,blob,temporal,blueprint,temporal; >=1 canonical-media change after RESEED"},
    {"id": "S2", "title": "Temporal memory attribution", "env": {D: "temporal-memattr"},
     "purpose": "history frames/capacity/pixel bytes vs RSS/RssAnon, playhead GPU estimate vs CMA, decoder events, NextMedia reset/refill",
     "predicate": "driver exit reached; canonical-media change then history-reset observed; residency WINDOW lines present"},
    {"id": "S3", "title": "Priority B fast vs legacy downscale",
     "env": {D: "ab-compact", "EXPERIENCE_RUNTIME_STALL_DRIVER_AB_ENV": "EXPERIENCE_RUNTIME_TEMPORAL_LEGACY_DOWNSCALE=1"},
     "purpose": "same process, same media: window ab-default (fast) then ab-toggled (legacy); frame p50/p95/p99, downscale mean/p95, path counters",
     "predicate": "both window frame-interval rows present (n>0); fastDownscale@ab-default and legacyDownscale@ab-toggled present"},
    {"id": "S4a", "title": "Priority C bounded residency (+mask cycles)",
     "env": {D: "blob-short", "EXPERIENCE_RUNTIME_STALL_DRIVER_MASK_CYCLES": "3"},
     "purpose": "Blob only: 19 tint / 0 eager mask, CPU 0 MiB, RSS/CMA; on-demand mask 0->1->0 x3 with load/release time and GL errors",
     "predicate": "residency=bounded tintTextures=19 maskTextures=0 cpu 0.0/0.0; 3 MASK-CYCLE lines masks=0->1->0, GL 0x0"},
    {"id": "S4b", "title": "Priority C legacy residency (short guarded comparison)", "requires": "S4a",
     "env": {D: "blob-short", "EXPERIENCE_RUNTIME_TEMPORAL_AMBIENT_RESIDENCY": "legacy"},
     "purpose": "Blob only, legacy residency: CPU/GPU residency, RSS/CMA vs S4a (supervisor preflight/floor still apply)",
     "predicate": "residency=legacy tintTextures=19 maskTextures=19"},
    {"id": "S5", "title": "GL error localization", "env": {D: "smoke-circuit", "EXPERIENCE_RUNTIME_GL_ERROR_TRACE": "1"},
     "purpose": "first 0x0500 scope(s), per-scope counts (self-test excluded); exact GL call unresolved unless instrumented",
     "predicate": "probe summary with glError rows written (self-test row present proves the tracer)"},
    {"id": "S6", "title": "Playhead upload skip vs legacy",
     "env": {D: "ab-compact", "EXPERIENCE_RUNTIME_STALL_DRIVER_AB_ENV": "EXPERIENCE_RUNTIME_TEMPORAL_PLAYHEAD_UPLOAD=legacy"},
     "purpose": "same process: ab-default (skip) then ab-toggled (legacy): frame p50/p95/p99, upload mean/p95/p99, uploads/frame, skips/frame",
     "predicate": "both window frame-interval rows and playheadUpload window rows present"},
    {"id": "S6v", "title": "Downscale + playhead-skip correctness verify",
     "env": {D: "ab-compact", "EXPERIENCE_RUNTIME_TEMPORAL_DOWNSCALE_VERIFY": "1", "EXPERIENCE_RUNTIME_TEMPORAL_PLAYHEAD_UPLOAD_VERIFY": "1"},
     "purpose": "byte comparison of fast downscale vs ofPixels::resize and of skipped playhead textures vs selected history frame",
     "predicate": "history.verifyMatch>0 and no verifyMismatch; no playhead skipVerifyMismatch"},
    {"id": "S7", "title": "RGB vs RGBA upload split", "env": {D: "upload-split"},
     "purpose": "existing measurement-only benchmark (two 640x360 textures, 300 uploads each, glFinish per upload)",
     "predicate": "RGB and RGBA UPLOAD-SPLIT lines with glError=0x0"},
    {"id": "S9a", "title": "Repeatability: smoke repeat", "requires": "S1", "env": {D: "smoke-circuit"},
     "purpose": "separate process repeat of S1: startup, baseline RSS/CMA, activation worst frames drift", "predicate": "as S1"},
    {"id": "S9b", "title": "Repeatability: memory attribution repeat", "requires": "S2", "env": {D: "temporal-memattr"},
     "purpose": "separate process repeat of S2: RSS/CMA/history drift", "predicate": "as S2"},
]

STATIC_NOT_RUN = [
    ("S8-resume", "Shared Video short loop / medium completion / long progressive resume",
     "ResumeWindowHarness needs automatic-advance windows (10-20 s dwell override) and a ~100 s clip: exceeds the 45 s per-run ceiling. Requires a separately authorized endurance campaign."),
    ("S8-manual", "Manual Next/Previous preserving positions",
     "Position preservation is only asserted by ResumeWindowHarness (>45 s). Not reducible without defeating supervision."),
    ("S8-long240", "Production long-clip (240 s threshold) behavior",
     "Never faked: a short override is not production long-clip validation."),
    ("S8-cadence", "Blueprint cadence: no timer advance within cycle, one boundary advance, fresh dwell on exit, no overdue jump, perpetual hold",
     "CadenceHarness ran ~735 s on desktop: exceeds the 45 s ceiling. Requires an endurance campaign."),
    ("S8-stale", "Temporal stale previous-visit imagery after Blueprint RESEED (BLUE-002 open defect)",
     "Not observable: no pixel/readback identity instrumentation on the Pi path. Correctness defect remains open."),
    ("S8-hudchips", "Curated HUD effect-chip rendering",
     "HUD drawing is not instrumented on the Pi path; S1 records HudFrameData effect slots/display names only."),
    ("RT006-four-scene", "Four-scene ring incl. Fragment Trail (RT-006)",
     "Not in this build: RT-006 lives in an isolated worktree pending integration (handoff 2026-10-11)."),
]


# ---------------------------------------------------------------------------
def utc():
    return datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")


def atomic_json(path, obj):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(obj, f, indent=2, sort_keys=True, default=str)
        f.write("\n")
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def run_cmd(cmd, timeout=10):
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout, universal_newlines=True)
        return p.returncode, p.stdout
    except (OSError, subprocess.TimeoutExpired) as e:
        return None, repr(e)


def read_meminfo(fixture):
    path = os.path.join(fixture["proc_root"], "meminfo") if fixture else "/proc/meminfo"
    return ms.parse_kb_fields(ms.read_text(path), ms.MEMINFO_KEYS)


def runtime_processes(fixture):
    """PIDs of live runtime processes (never signalled by the orchestrator)."""
    marker = fixture.get("process_marker") if fixture else RUNTIME_PATTERN
    code, out = run_cmd(["ps", "-A", "-o", "pid=,command="])
    if code != 0:
        return None
    pids = []
    for line in out.splitlines():
        parts = line.strip().split(None, 1)
        if len(parts) == 2 and marker in parts[1] and "pi_validation.py" not in parts[1] and "memattr_supervisor.py" not in parts[1] \
                and int(parts[0]) != os.getpid():
            pids.append(int(parts[0]))
    return pids


# ---------------------------------------------------------------------------
# Preflight
def preflight(args, fixture, out_dir):
    checks = []

    def add(name, status, detail):
        checks.append({"check": name, "status": status, "detail": detail})

    fx = fixture.get("preflight", {}) if fixture else {}
    add("not_root", "ok" if os.geteuid() != 0 else "fail", "euid=%d" % os.geteuid())
    add("python", "ok" if sys.version_info >= (3, 7) else "fail", sys.version.split()[0])
    if not fixture:
        add("platform", "ok" if sys.platform.startswith("linux") else "fail", platform.platform())
    add("kernel", "ok", " ".join(platform.uname()))

    app_dir = os.path.abspath(args.app_dir)
    binary = fx.get("binary") or os.path.join(app_dir, "bin", "experience_runtime")
    bin_info = {}
    if os.path.isfile(binary) and (fixture or os.access(binary, os.X_OK)):
        st = os.stat(binary)
        bin_info = {"path": binary, "size": st.st_size, "mtime": st.st_mtime, "sha256": sha256_file(binary)}
        add("binary", "ok", bin_info)
    else:
        add("binary", "fail", "missing or not executable: %s" % binary)

    prov_path = fx.get("provenance") or PROVENANCE
    prov = None
    try:
        with open(prov_path) as f:
            prov = json.load(f)
        prov_mtime = os.stat(prov_path).st_mtime
        if bin_info and bin_info["mtime"] + 1.0 < prov_mtime:
            add("provenance", "fail", "binary predates the synced source provenance (rebuild after deploy): %s" % prov)
        else:
            add("provenance", "ok", prov)
        if prov.get("dirty"):
            add("source_dirty", "warn", "working tree was dirty at deploy; diff sha256 %s" % prov.get("diff_sha256"))
    except (OSError, ValueError) as e:
        add("provenance", "fail", "binary identity uncertain: no readable %s (%r)" % (prov_path, e))

    media_dir = fx.get("media_dir") or os.path.normpath(os.path.join(app_dir, "..", "..", "assets", "shared", "media"))
    files = sorted(glob.glob(os.path.join(media_dir, "*.mp4")))
    listing = [(os.path.basename(p), os.path.getsize(p), int(os.stat(p).st_mtime)) for p in files]
    media = {"dir": media_dir, "count": len(listing), "listing_sha256": hashlib.sha256(json.dumps(listing).encode()).hexdigest(),
             "content_hash": "unavailable (names/sizes/mtimes only)"}
    add("media_catalog", "ok" if listing else "fail", media)

    display = fx.get("display", os.environ.get("DISPLAY"))
    add("display_env", "ok" if display else "fail", "DISPLAY=%s" % display)
    if fixture:
        xr = (0, fx.get("xrandr", ""))
    else:
        env = dict(os.environ, DISPLAY=display or ":0")
        try:
            p = subprocess.run(["xrandr", "--query"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=5, env=env, universal_newlines=True)
            xr = (p.returncode, p.stdout)
        except (OSError, subprocess.TimeoutExpired) as e:
            xr = (None, repr(e))
    if xr[0] != 0:
        add("display_mode", "unavailable", "xrandr: %s" % xr[1][:200])
    else:
        connected = [l for l in xr[1].splitlines() if " connected" in l]
        active = [l for l in xr[1].splitlines() if "*" in l]
        add("display_mode", "ok" if connected and active else "fail", {"connected": connected, "active_mode": active[:1]})

    run_root = os.path.expanduser(args.run_root)
    try:
        os.makedirs(run_root, exist_ok=True)
        probe = os.path.join(run_root, ".write-test-%d" % os.getpid())
        open(probe, "w").close()
        os.remove(probe)
        add("run_root_writable", "ok", run_root)
    except OSError as e:
        add("run_root_writable", "fail", repr(e))
    budget = 2 * len(SCENARIOS) * PER_RUN_LOG_BUDGET
    try:
        du = shutil.disk_usage(run_root)
        add("disk_free", "ok" if du.free >= budget else "fail", "free %.0f MiB, campaign worst-case budget %.0f MiB (2 x %d runs x per-run log cap)" % (du.free / ms.MiB, budget / ms.MiB, len(SCENARIOS)))
    except OSError as e:
        add("disk_free", "fail", repr(e))

    def vc(argv, key):
        if fixture:
            return (0, fx.get(key, "")) if key in fx else (None, "unavailable")
        return run_cmd(["vcgencmd"] + argv, timeout=5)
    code, thr = vc(["get_throttled"], "throttled")
    m = re.search(r"0x[0-9a-fA-F]+", thr or "")
    if code == 0 and m:
        bits = int(m.group(0), 16)
        add("throttled", "fail" if bits & 0xF else "ok", "%s (current bits 0x%x; historical bits 0x%x recorded only)" % (m.group(0), bits & 0xF, bits & ~0xF))
    else:
        add("throttled", "fail", "vcgencmd get_throttled unavailable: %s" % (thr or "")[:200])
    code, temp = vc(["measure_temp"], "temp")
    add("temperature", "ok" if code == 0 else "unavailable", (temp or "").strip())
    code, gpu = vc(["get_mem", "gpu"], "gpu_mem")
    add("gpu_mem_split", "ok" if code == 0 else "unavailable", (gpu or "").strip())

    mem = read_meminfo(fixture)
    th = ms.REAL_THRESHOLDS
    if "CmaFree" not in mem or "MemAvailable" not in mem:
        add("meminfo", "fail", "required meminfo fields unavailable: %s" % mem)
    else:
        ok = mem["CmaFree"] / 1024 > th["preflight_cma_free_min_mib"] and mem["MemAvailable"] / 1024 > th["mem_available_min_mib"]
        add("meminfo", "ok" if ok else "fail", "CmaFree %.1f MiB (>%.0f), MemAvailable %.1f MiB (>%.0f), SwapFree %s kB, CmaTotal %s kB"
            % (mem["CmaFree"] / 1024, th["preflight_cma_free_min_mib"], mem["MemAvailable"] / 1024, th["mem_available_min_mib"], mem.get("SwapFree"), mem.get("CmaTotal")))
    drm = ms.ProcSource(fixture["proc_root"] if fixture else None).drm()
    add("debugfs_drm", "ok" if drm["drm_bo_stats"] != ms.UNAVAILABLE else "unavailable", drm)

    procs = runtime_processes(fixture)
    if procs is None:
        add("no_runtime_running", "fail", "cannot list processes")
    else:
        add("no_runtime_running", "ok" if not procs else "fail", "running runtime pids: %s" % procs)

    tests_cmd = fx.get("offline_tests_cmd")
    argv = tests_cmd if tests_cmd else [sys.executable, OFFLINE_TESTS, "-v"]
    t0 = time.monotonic()
    code, out = run_cmd(argv if isinstance(argv, list) else ["sh", "-c", argv], timeout=900)
    with open(os.path.join(out_dir, "offline-tests.log"), "w") as f:
        f.write(out or "")
    ran = re.search(r"Ran (\d+) tests?", out or "")
    n = int(ran.group(1)) if ran else -1
    skipped = re.search(r"skipped=(\d+)", out or "")
    ok_line = re.search(r"^OK(\s|$)", out or "", re.M)
    passed = code == 0 and n == EXPECTED_OFFLINE_TESTS and ok_line and not skipped
    add("offline_watchdog_tests", "ok" if passed else "fail",
        "ran=%d expected=%d skipped=%s rc=%s %.0fs (Linux PID-identity + parent-death tests must run, not skip)"
        % (n, EXPECTED_OFFLINE_TESTS, skipped.group(1) if skipped else 0, code, time.monotonic() - t0))

    failed = [c for c in checks if c["status"] == "fail"]
    result = {"version": CAMPAIGN_VERSION, "at_utc": utc(), "ok": not failed, "failed": [c["check"] for c in failed], "checks": checks,
              "binary": bin_info, "provenance": prov, "media": media}
    atomic_json(os.path.join(out_dir, "preflight.json"), result)
    return result


# ---------------------------------------------------------------------------
# Log parsing (formats from Rt004FramePhaseProbe / Rt004StallProbe / driver)
KV = re.compile(r"(\w+)=([^\s|]+)")


def parse_frame_phase(path):
    events, windows, slow, config = [], [], [], {}
    text = ms.read_text(path, 64 << 20) or ""
    for line in text.splitlines():
        if line.startswith("CONFIG"):
            config = dict(KV.findall(line))
            g = re.search(r'glRenderer="([^"]*)" glVersion="([^"]*)"', line)
            if g:
                config["glRenderer"], config["glVersion"] = g.group(1), g.group(2)
        elif line.startswith("EVENT"):
            head, _, res = line.partition(" | ")
            kv = dict(KV.findall(head))
            kv.update({"res_" + k: v for k, v in KV.findall(res)})
            kv["raw"] = head
            events.append(kv)
        elif line.startswith("WINDOW"):
            parts = line.split(" | ")
            w = dict(KV.findall(parts[0]))
            m = re.search(r"frame p50=([\d.]+) p95=([\d.]+) max=([\d.]+)", parts[0])
            if m:
                w["frame_p50"], w["frame_p95"], w["frame_max"] = m.groups()
            for p in parts[1:]:
                p = p.strip()
                if p.startswith("historyFrames") or p.startswith("rssMiB"):
                    w.update(dict(KV.findall(p)))
            windows.append(w)
        elif line.startswith("SLOW"):
            slow.append(dict(KV.findall(line.split(" | ")[0])))
    return {"config": config, "events": events, "windows": windows, "slow": slow}


def parse_stall(path):
    text = ms.read_text(path, 64 << 20) or ""
    out = {"notes": [], "long": [], "intervals": {}, "window": {}, "windowCounter": {}, "counter": {}, "glError": [], "glFirst": [], "summary": "RT-004 SUMMARY" in text}
    for line in text.splitlines():
        s = line.strip()
        m = re.match(r"NOTE frame=(\d+) mono=([\d.]+) t=\S+ ([^:]+): (.*)", s)
        if m:
            out["notes"].append({"frame": int(m.group(1)), "mono": float(m.group(2)), "label": m.group(3), "detail": m.group(4)})
            continue
        m = re.match(r"--- LONG FRAME #\d+ frame=(\d+) mono=([\d.]+) .*interval=([\d.]+)ms", s)
        if m:
            out["long"].append({"frame": int(m.group(1)), "mono": float(m.group(2)), "interval_ms": float(m.group(3))})
            continue
        m = re.match(r"frameIntervalMs\[owner=([^\]]+)\] n=(\d+) p50=([\d.]+) p95=([\d.]+) p99=([\d.]+) max=([\d.]+)", s)
        if m:
            out["intervals"][m.group(1)] = {"n": int(m.group(2)), "p50": float(m.group(3)), "p95": float(m.group(4)), "p99": float(m.group(5)), "max": float(m.group(6))}
            continue
        if s.startswith("window,"):
            f = s.split(",")
            if len(f) == 8:
                out["window"][f[1]] = {"count": int(f[2]), "mean": float(f[4]), "p95": float(f[5]), "p99": float(f[6]), "max": float(f[7])}
        elif s.startswith("windowCounter,"):
            f = s.split(",")
            out["windowCounter"][f[1]] = int(f[2])
        elif s.startswith("counter,"):
            f = s.split(",")
            out["counter"][f[1]] = int(f[2])
        elif s.startswith("glError,"):
            f = s.split(",")
            out["glError"].append({"scope": ",".join(f[1:-2]), "code": f[-2], "count": int(f[-1])})
        elif s.startswith("GLERROR first"):
            out["glFirst"].append(s)
    return out


def parse_driver(path):
    text = ms.read_text(path, 8 << 20) or ""
    upload = []
    for m in re.finditer(r"UPLOAD-SPLIT (\d+)x(\d+) (RGBA|RGB) internalFormat=(\S+) bytes=(\d+) n=(\d+) meanMs=([\d.e+-]+) p50Ms=([\d.e+-]+) p95Ms=([\d.e+-]+) p99Ms=([\d.e+-]+) glError=(\S+)", text):
        upload.append({"format": m.group(3), "internalFormat": m.group(4), "bytes": int(m.group(5)), "n": int(m.group(6)), "mean_ms": float(m.group(7)),
                       "p50_ms": float(m.group(8)), "p95_ms": float(m.group(9)), "p99_ms": float(m.group(10)), "glError": m.group(11)})
    masks = [dict(KV.findall(l)) for l in text.splitlines() if l.startswith("MASK-CYCLE")]
    actions = re.findall(r"ACTION t=([\d.]+)s probeFrame=\d+ (.*)", text)
    return {"upload": upload, "masks": masks, "actions": actions, "exit_action": any(a[1] == "exit" for a in actions)}


def read_samples(path):
    try:
        with open(path) as f:
            return list(csv.DictReader(f, delimiter="\t"))
    except OSError:
        return []


def num(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


# ---------------------------------------------------------------------------
# Per-scenario evaluation. Returns (status, metrics, notes, missing).
def evaluate(sid, run_dir, manifest, fixture):
    fp = parse_frame_phase(os.path.join(run_dir, "frame-phase.log"))
    st = parse_stall(os.path.join(run_dir, "stall-probe.log"))
    dr = parse_driver(os.path.join(run_dir, "driver.log"))
    stdout = ms.read_text(os.path.join(run_dir, "app-stdout.log"), 64 << 20) or ""
    stdout += ms.read_text(os.path.join(run_dir, "app-stderr.log"), 64 << 20) or ""
    samples = [r for r in read_samples(os.path.join(run_dir, "samples.tsv")) if r.get("phase") == "running"]
    launch = manifest.get("launch_mono_s")
    metrics = {"gl": fp["config"].get("glRenderer"), "glVersion": fp["config"].get("glVersion"),
               "gstreamer_critical": stdout.count("GStreamer-CRITICAL"), "eos_timeout_warnings": stdout.count("didn't received EOS"),
               "gl_error_after_ambient_load": bool(re.search(r"GL error after ambient load", stdout))}
    rss = [num(r["status_vmrss_kb"]) for r in samples if num(r["status_vmrss_kb"]) is not None]
    cma = [num(r["meminfo_cmafree_kb"]) for r in samples if num(r["meminfo_cmafree_kb"]) is not None]
    if rss:
        metrics["rss_mib_first_max"] = [round(rss[0] / 1024, 1), round(max(rss) / 1024, 1)]
    if cma:
        metrics["cma_free_mib_first_min"] = [round(cma[0] / 1024, 1), round(min(cma) / 1024, 1)]
    notes, missing = [], []
    owners = [e["to"] for e in fp["events"] if e.get("kind") == "scene-owner" and e.get("to") not in ("-", None)]
    first_frame = next((float(e["mono"]) for e in fp["events"] if e.get("kind") == "scene-owner" and e.get("to") != "-"), None)
    if first_frame and launch:
        metrics["startup_to_first_live_frame_s"] = round(first_frame - launch, 2)

    def worst_after(mono, span=3.0):
        vals = [num(w.get("frame_max")) for w in fp["windows"] if num(w.get("mono")) is not None and mono - 0.5 <= num(w["mono"]) <= mono + span + 1.0]
        vals += [num(s.get("total")[:-2]) for s in fp["slow"] if s.get("total") and num(s.get("mono")) is not None and mono - 0.5 <= num(s["mono"]) <= mono + span]
        vals = [v for v in vals if v is not None]
        return max(vals) if vals else None

    status = "PASS"
    if sid in ("S1", "S5", "S9a"):
        expected = ["blob-region-prototype", "temporal-fields", "blob-region-prototype", "temporal-fields", "blueprint_emergence", "temporal-fields"]
        metrics["scene_owner_sequence"] = owners
        acts = [float(e["mono"]) for e in fp["events"] if e.get("kind") == "scene-owner" and e.get("to") == "temporal-fields"]
        metrics["temporal_activation_worst_frame_ms"] = [worst_after(a) for a in acts]
        reseed = [n["mono"] for n in st["notes"] if n["label"] == "driver.action" and "RESEED" in n["detail"]]
        media_events = [(float(e["mono"]), e.get("to")) for e in fp["events"] if e.get("kind") == "canonical-media"]
        metrics["canonical_media_events"] = [m[1] for m in media_events]
        metrics["temporal_loadExplicit_notes"] = len([n for n in st["notes"] if n["label"] == "temporal.loadExplicit"])
        eff = [e for e in fp["events"] if e.get("kind") == "effects"]
        metrics["first_effect_snapshot"] = eff[0]["raw"].split("kind=effects ", 1)[-1] if eff else None
        if not fp["events"]:
            missing.append("frame-phase EVENT lines")
        if owners[:len(expected)] != expected:
            status = "FAIL"
            notes.append("scene-owner sequence differs from expected")
        if reseed and not any(m[0] > reseed[0] for m in media_events):
            status = "FAIL"
            notes.append("no canonical-media change after RESEED")
        if not reseed and not missing:
            missing.append("RESEED driver note")
        if sid == "S5":
            if not st["summary"]:
                missing.append("stall-probe summary (needed for glError rows)")
            real = [g for g in st["glError"] if "glTraceSelfTest" not in g["scope"]]
            metrics["gl_error_scopes"] = real
            metrics["gl_first"] = [l for l in st["glFirst"] if "glTraceSelfTest" not in l][:10]
            metrics["gl_tracer_selftest_seen"] = any("glTraceSelfTest" in g["scope"] for g in st["glError"])
            metrics["gl_exact_call"] = "unresolved (scope-level attribution only)"
            if st["summary"] and not metrics["gl_tracer_selftest_seen"]:
                missing.append("tracer self-test row (tracer not proven active)")
    elif sid in ("S2", "S9b"):
        res = [w for w in fp["windows"] if "historyFrames" in w]
        if not res:
            missing.append("residency WINDOW lines")
        media = [float(e["mono"]) for e in fp["events"] if e.get("kind") == "canonical-media"]
        resets = [float(e["mono"]) for e in fp["events"] if e.get("kind") == "history-reset"]
        act = next((float(e["mono"]) for e in fp["events"] if e.get("kind") == "scene-owner" and e.get("to") == "temporal-fields"), None)
        metrics["temporal_activation_mono"] = act
        metrics["history_reset_after_media_change"] = bool(media and resets and any(r >= media[-1] - 0.5 for r in resets))
        if act and samples:
            pre_reset = resets[-1] if resets else float(samples[-1]["mono_s"])
            seg = [r for r in samples if act <= float(r["mono_s"]) <= pre_reset]
            hist = [w for w in res if num(w.get("mono")) and act <= num(w["mono"]) <= pre_reset]
            if seg and hist:
                anon0, anon1 = num(seg[0]["status_rssanon_kb"]), num(seg[-1]["status_rssanon_kb"])
                h0, h1 = num(hist[0].get("historyPixMiB")), num(hist[-1].get("historyPixMiB"))
                metrics["activation_to_reset"] = {
                    "rss_anon_delta_mib": round((anon1 - anon0) / 1024, 1) if anon0 is not None and anon1 is not None else None,
                    "history_pixel_delta_mib": round(h1 - h0, 1) if h0 is not None and h1 is not None else None,
                    "history_frames_capacity_end": hist[-1].get("historyFrames"),
                    "cma_free_delta_mib": round((num(seg[-1]["meminfo_cmafree_kb"]) - num(seg[0]["meminfo_cmafree_kb"])) / 1024, 1) if num(seg[0]["meminfo_cmafree_kb"]) is not None else None,
                    "playhead_est_rgba_mib_end": hist[-1].get("playheadEstRgbaMiB"),
                    "note": "correlation only; RssAnon includes allocator retention and non-history heap"}
        metrics["decoder_events"] = [n["label"] + ":" + n["detail"] for n in st["notes"] if n["label"] in ("canonical.load", "temporal.loadExplicit")]
        if manifest.get("stop_class") == "completed" and not metrics["history_reset_after_media_change"]:
            status = "FAIL"
            notes.append("NextMedia reset not observed")
    elif sid in ("S3", "S6", "S6v"):
        a = st["intervals"].get("temporal-fields/ab-default")
        b = st["intervals"].get("temporal-fields/ab-toggled") or st["intervals"].get("temporal-fields/ab-default-2")
        metrics["frame_interval_A_default"] = a
        metrics["frame_interval_B"] = b
        if not st["summary"] or not a or not b or a["n"] == 0 or b["n"] == 0:
            missing.append("per-window frame-interval rows (probe summary)")
        for w in (a, b):
            if w and w["n"] < LOW_CONFIDENCE_N:
                notes.append("p99 low confidence (n=%d < %d)" % (w["n"], LOW_CONFIDENCE_N))
        hist = [w for w in fp["windows"] if "historyFrames" in w]
        metrics["history_state_end"] = hist[-1].get("historyFrames") if hist else None
        if sid == "S3":
            metrics["fastDownscale@A"] = st["window"].get("TOVB.history.fastDownscale@ab-default")
            metrics["legacyDownscale@B"] = st["window"].get("TOVB.history.legacyDownscale@ab-toggled")
            metrics["path_counts"] = {k: v for k, v in st["windowCounter"].items() if k.startswith(("TOVB.history.fastPath", "TOVB.history.legacyPath"))}
            if st["summary"] and (not metrics["fastDownscale@A"] or not metrics["legacyDownscale@B"]):
                missing.append("downscale window scopes")
        elif sid == "S6":
            metrics["upload@A"] = st["window"].get("TimeOffsetVideoBuffer.playheadUpload@ab-default")
            metrics["upload@B"] = st["window"].get("TimeOffsetVideoBuffer.playheadUpload@ab-toggled")
            wc = st["windowCounter"]
            for tag in ("ab-default", "ab-toggled"):
                frames = wc.get("TOVB.update.calls@" + tag) or 0
                metrics["uploads_per_frame@" + tag] = round(wc.get("TOVB.playhead.upload@" + tag, 0) / frames, 2) if frames else None
                metrics["skips_per_frame@" + tag] = round(wc.get("TOVB.playhead.uploadSkippedRedundant@" + tag, 0) / frames, 3) if frames else None
            if st["summary"] and (not metrics["upload@A"] or not metrics["upload@B"]):
                missing.append("playhead upload window scopes")
        else:
            c = st["counter"]
            metrics["verify"] = {k: c.get(k, 0) for k in ("TOVB.history.verifyMatch", "TOVB.history.verifyMismatch", "TOVB.playhead.skipVerifyMatch", "TOVB.playhead.skipVerifyMismatch")}
            if st["summary"] and (c.get("TOVB.history.verifyMismatch", 0) or c.get("TOVB.playhead.skipVerifyMismatch", 0) or not c.get("TOVB.history.verifyMatch", 0)):
                status = "FAIL"
                notes.append("verify mismatch or no verified frames")
            if st["summary"] and not c.get("TOVB.playhead.skipVerifyMatch", 0):
                notes.append("no skipped uploads occurred to verify (skip path not exercised)")
    elif sid in ("S4a", "S4b"):
        m = re.search(r"residency=(\w+) pairs=(\d+) tintTextures=(\d+) maskTextures=(\d+) cpuTintMiB=([\d.]+) cpuMaskMiB=([\d.]+) gpuEstMiB=([\d.]+).*?rssBeforeMiB=(\d+) rssAfterMiB=(\d+) glError=(\S+)", stdout)
        if not m:
            missing.append("TFAmbientTextureLayer residency line")
        else:
            metrics["residency"] = dict(zip(["mode", "pairs", "tint", "mask", "cpuTintMiB", "cpuMaskMiB", "gpuEstMiB", "rssBeforeMiB", "rssAfterMiB", "glError"], m.groups()))
            want = ("bounded", "19", "0") if sid == "S4a" else ("legacy", "19", "19")
            if (m.group(1), m.group(3), m.group(4)) != want or (sid == "S4a" and (m.group(5), m.group(6)) != ("0.0", "0.0")):
                status = "FAIL"
                notes.append("residency differs from expectation %s" % (want,))
        if sid == "S4a":
            metrics["mask_cycles"] = dr["masks"]
            okc = [c for c in dr["masks"] if c.get("masks") == "0->1->0" and c.get("glErrorLoad") == "0x0" and c.get("glErrorRelease") == "0x0"]
            if not dr["masks"]:
                missing.append("MASK-CYCLE lines")
            elif len(okc) != 3:
                status = "FAIL"
                notes.append("mask cycles not all 0->1->0 without GL error")
    elif sid == "S7":
        metrics["upload_split"] = dr["upload"]
        fmts = {u["format"] for u in dr["upload"]}
        if fmts != {"RGB", "RGBA"}:
            missing.append("UPLOAD-SPLIT RGB and RGBA lines")
        elif any(u["glError"] not in ("0x0", "0x0\n") for u in dr["upload"]):
            status = "FAIL"
            notes.append("GL error during upload split")
        else:
            rgb = [u for u in dr["upload"] if u["format"] == "RGB"][0]
            rgba = [u for u in dr["upload"] if u["format"] == "RGBA"][0]
            metrics["rgb_over_rgba_mean"] = round(rgb["mean_ms"] / rgba["mean_ms"], 2) if rgba["mean_ms"] else None
    if sid != "S7" and not dr["exit_action"] and manifest.get("stop_class") == "completed":
        missing.append("driver exit action")
    if missing:
        status = "INCONCLUSIVE"
    if fixture and fixture.get("assume_measurements_ok") and status == "INCONCLUSIVE":
        status, missing = "PASS", []
        notes.append("fixture: measurements assumed")
    return status, metrics, notes, missing


# ---------------------------------------------------------------------------
class Campaign:
    def __init__(self, args, fixture):
        self.args = args
        self.fixture = fixture
        root = os.path.expanduser(args.run_root)
        os.makedirs(root, exist_ok=True)
        self.id = "campaign-%s-%04x" % (datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ"), random.getrandbits(16))
        self.dir = os.path.join(root, self.id)
        os.makedirs(self.dir, exist_ok=False)
        self.results = []
        self.interrupted = None
        self.current_sup = None
        self.abort_reason = None

    def status(self, state, extra=None):
        obj = {"campaign": self.id, "state": state, "at_utc": utc(), "abort_reason": self.abort_reason,
               "results": [{k: r[k] for k in ("id", "status", "stop_class")} for r in self.results]}
        if extra:
            obj.update(extra)
        atomic_json(os.path.join(self.dir, "status.json"), obj)

    def on_signal(self, signum, _frame):
        self.interrupted = signal.Signals(signum).name
        if self.current_sup is not None and self.current_sup.poll() is None:
            try:
                self.current_sup.send_signal(signal.SIGINT)  # supervisor performs the guarded app stop
            except ProcessLookupError:
                pass

    def supervisor_cmd(self, sc, scen_dir):
        env_args = []
        for k, v in sc["env"].items():
            env_args += ["--env", "%s=%s" % (k, v)]
        if self.fixture:
            dummy = self.fixture.get("dummy_cmds", {}).get(sc["id"]) or self.fixture.get("dummy_cmds", {}).get("default", "sh -c 'sleep 1'")
            cmd = [sys.executable, SUP, "fixture", "--cmd", dummy, "--proc-root", self.fixture["proc_root"], "--run-root", scen_dir, "--label", sc["id"]]
            if self.fixture.get("thresholds"):
                cmd += ["--thresholds", json.dumps(self.fixture["thresholds"])]
            return cmd + env_args
        return [sys.executable, SUP, "run", "--app-dir", os.path.abspath(self.args.app_dir), "--run-root", scen_dir, "--label", sc["id"],
                "--build-id", self.build_id, "--source-revision", self.source_rev] + env_args

    def wait_recovery(self):
        th = ms.REAL_THRESHOLDS
        t0 = time.monotonic()
        while True:
            procs = runtime_processes(self.fixture)
            mem = read_meminfo(self.fixture)
            cma = mem.get("CmaFree", 0) / 1024
            avail = mem.get("MemAvailable", 0) / 1024
            if procs:
                return False, "unexpected runtime process survival: pids %s (not signalled; investigate)" % procs
            if cma > th["preflight_cma_free_min_mib"] and avail > th["mem_available_min_mib"]:
                return True, "recovered in %.1fs (CmaFree %.1f MiB, MemAvailable %.1f MiB)" % (time.monotonic() - t0, cma, avail)
            if time.monotonic() - t0 > RECOVERY_TIMEOUT_S or self.interrupted:
                return False, "resources did not recover within %.0fs (CmaFree %.1f MiB, MemAvailable %.1f MiB)" % (RECOVERY_TIMEOUT_S, cma, avail)
            time.sleep(1.0)

    def run_scenario(self, sc):
        scen_dir = os.path.join(self.dir, sc["id"])
        os.makedirs(scen_dir)
        cmd = self.supervisor_cmd(sc, scen_dir)
        rec = {"id": sc["id"], "title": sc["title"], "command": cmd, "status": "NOT RUN", "stop_class": None}
        with open(os.path.join(scen_dir, "supervisor-stdout.log"), "w") as so, open(os.path.join(scen_dir, "supervisor-stderr.log"), "w") as se:
            self.current_sup = subprocess.Popen(cmd, stdout=so, stderr=se)
            while self.current_sup.poll() is None:
                if os.path.exists(os.path.join(self.dir, "STOP")):
                    for d in glob.glob(os.path.join(scen_dir, "*", "")):
                        open(os.path.join(d, "STOP"), "w").close()
                time.sleep(0.25)
        rc = self.current_sup.returncode
        self.current_sup = None
        runs = [d for d in glob.glob(os.path.join(scen_dir, "*")) if os.path.isdir(d)]
        manifest = {}
        if runs:
            try:
                with open(os.path.join(runs[0], "manifest.json")) as f:
                    manifest = json.load(f)
            except (OSError, ValueError):
                pass
        rec["run_dir"] = runs[0] if runs else None
        rec["supervisor_rc"] = rc
        rec["stop_reason"] = manifest.get("stop_reason")
        rec["stop_class"] = manifest.get("stop_class") if manifest.get("status") == "finished" else "error"
        if rc != 0 or not runs or manifest.get("status") != "finished":
            rec["status"] = "FAIL"
            rec["notes"] = ["watchdog/supervisor malfunction (rc=%s, manifest status=%s)" % (rc, manifest.get("status"))]
            return rec, "watchdog malfunction"
        status, metrics, notes, missing = evaluate(sc["id"], runs[0], manifest, self.fixture)
        rec.update({"metrics": metrics, "notes": notes, "missing": missing, "launch_mono_s": manifest.get("launch_mono_s"),
                    "app_returncode": manifest.get("app_returncode"), "cma_baseline_mib": manifest.get("cma_baseline_mib")})
        sclass = rec["stop_class"]
        if sclass != "completed":
            rec["status"] = "FAIL" if sclass in ("safety", "crash", "ceiling", "error") else ("NOT RUN" if sclass == "refused" else "INCONCLUSIVE")
            return rec, "%s stop: %s" % (sclass, rec["stop_reason"])
        rec["status"] = status
        if status == "INCONCLUSIVE":
            return rec, "missing required measurement: %s" % ", ".join(missing)
        if status == "FAIL" and not self.args.continue_on_functional_failure:
            return rec, "functional assertion failed (policy: abort)"
        return rec, None

    def run(self):
        for s in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            signal.signal(s, self.on_signal)
        self.status("preflight")
        pf = preflight(self.args, self.fixture, self.dir)
        prov = pf.get("provenance") or {}
        self.build_id = (pf.get("binary") or {}).get("sha256", "unavailable")[:16]
        self.source_rev = "%s%s" % (prov.get("git_head", "unavailable"), "+dirty" if prov.get("dirty") else "")
        self.write_manifest(pf)
        if not pf["ok"]:
            self.abort_reason = "preflight failed: %s" % ", ".join(pf["failed"])
            for sc in SCENARIOS:
                self.results.append({"id": sc["id"], "title": sc["title"], "status": "NOT RUN", "stop_class": None, "notes": [self.abort_reason]})
            return self.finish(3)
        selected = [sc for sc in SCENARIOS if not self.args.only or sc["id"] in self.args.only]
        for sc in SCENARIOS:
            if sc not in selected:
                self.results.append({"id": sc["id"], "title": sc["title"], "status": "NOT RUN", "stop_class": None, "notes": ["not selected (--only)"]})
                continue
            if self.abort_reason or self.interrupted:
                self.results.append({"id": sc["id"], "title": sc["title"], "status": "NOT RUN", "stop_class": None,
                                     "notes": ["campaign aborted: %s" % (self.abort_reason or "interrupted " + str(self.interrupted))]})
                continue
            req = sc.get("requires")
            if req and not any(r["id"] == req and r["status"] == "PASS" for r in self.results):
                self.results.append({"id": sc["id"], "title": sc["title"], "status": "NOT RUN", "stop_class": None, "notes": ["prerequisite %s did not PASS" % req]})
                continue
            self.status("running", {"current": sc["id"]})
            rec, abort = self.run_scenario(sc)
            self.results.append(rec)
            if self.interrupted:
                self.abort_reason = "interrupted (%s)" % self.interrupted
                continue
            if abort:
                self.abort_reason = "%s: %s" % (sc["id"], abort)
                continue
            ok, detail = self.wait_recovery()
            rec["recovery"] = detail
            if not ok:
                self.abort_reason = "%s: %s" % (sc["id"], detail)
        code = 0 if not (self.abort_reason or self.interrupted) else 2
        return self.finish(code)

    def write_manifest(self, pf):
        atomic_json(os.path.join(self.dir, "manifest.json"), {
            "campaign": self.id, "version": CAMPAIGN_VERSION, "created_utc": utc(), "mode": "fixture" if self.fixture else "real",
            "host": list(platform.uname()), "python": sys.version.split()[0], "app_dir": os.path.abspath(self.args.app_dir),
            "preflight_ok": pf["ok"], "binary": pf.get("binary"), "provenance": pf.get("provenance"), "media": pf.get("media"),
            "thresholds": ms.REAL_THRESHOLDS, "supervisor_constants": {"sample_period_s": ms.SAMPLE_PERIOD_S, "collector_stale_s": ms.COLLECTOR_STALE_S,
                                                                       "term_grace_s": ms.TERM_GRACE_S, "log_cap_bytes": ms.LOG_CAP_BYTES},
            "recovery_timeout_s": RECOVERY_TIMEOUT_S, "continue_on_functional_failure": self.args.continue_on_functional_failure,
            "scenarios": SCENARIOS, "static_not_run": STATIC_NOT_RUN,
            "randomness": "media start selection and Temporal/Blueprint composition seeds are not controlled by the campaign; actual media ids/seeds are logged per scenario (nondeterministic)",
        })

    def finish(self, code):
        # Merged watchdog + metrics timelines (CSV for plotting).
        with open(os.path.join(self.dir, "watchdog.tsv"), "w") as out:
            out.write("scenario\tmono_s\twall_utc\tevent\tdetail\n")
            for r in self.results:
                if r.get("run_dir"):
                    for row in read_samples(os.path.join(r["run_dir"], "watchdog.tsv")):
                        out.write("%s\t%s\t%s\t%s\t%s\n" % (r["id"], row.get("mono_s"), row.get("wall_utc"), row.get("event"), row.get("detail")))
        with open(os.path.join(self.dir, "metrics.csv"), "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["scenario", "source", "mono_s", "rel_launch_s", "rss_mib", "rss_anon_mib", "rss_file_mib", "rss_shmem_mib", "mem_available_mib", "cma_free_mib",
                        "history_frames", "history_pix_mib", "playhead_tex", "playhead_est_rgba_mib", "frame_p50_ms", "frame_p95_ms", "frame_max_ms", "kind"])
            for r in self.results:
                if not r.get("run_dir"):
                    continue
                launch = r.get("launch_mono_s") or 0.0
                for s in read_samples(os.path.join(r["run_dir"], "samples.tsv")):
                    m = num(s["mono_s"])
                    kb = lambda k: round(num(s[k]) / 1024, 2) if num(s.get(k)) is not None else ""  # noqa: E731
                    w.writerow([r["id"], "collector", s["mono_s"], round(m - launch, 3) if launch else "", kb("status_vmrss_kb"), kb("status_rssanon_kb"), kb("status_rssfile_kb"),
                                kb("status_rssshmem_kb"), kb("meminfo_memavailable_kb"), kb("meminfo_cmafree_kb"), "", "", "", "", "", "", "", s["phase"]])
                fp = parse_frame_phase(os.path.join(r["run_dir"], "frame-phase.log"))
                for win in fp["windows"]:
                    m = num(win.get("mono"))
                    w.writerow([r["id"], "app-window", win.get("mono"), round(m - launch, 3) if (m and launch) else "", win.get("rssMiB"), win.get("rssAnonMiB"), win.get("rssFileMiB"),
                                win.get("rssShmemMiB"), win.get("memAvailMiB"), win.get("cmaFreeMiB"), win.get("historyFrames"), win.get("historyPixMiB"), win.get("playheadTex"),
                                win.get("playheadEstRgbaMiB"), win.get("frame_p50"), win.get("frame_p95"), win.get("frame_max"), "window"])
                for ev in fp["events"]:
                    m = num(ev.get("mono"))
                    w.writerow([r["id"], "app-event", ev.get("mono"), round(m - launch, 3) if (m and launch) else "", "", "", "", "", "", "", ev.get("res_historyFrames"),
                                ev.get("res_historyPixMiB"), ev.get("res_playheadTex"), ev.get("res_playheadEstRgbaMiB"), "", "", "", ev.get("kind")])
        atomic_json(os.path.join(self.dir, "scenario-results.json"), {"campaign": self.id, "abort_reason": self.abort_reason, "interrupted": self.interrupted,
                                                                       "results": self.results, "static_not_run": STATIC_NOT_RUN})
        executed = [r for r in self.results if r["status"] != "NOT RUN"]
        all_pass = executed and all(r["status"] == "PASS" for r in self.results if r["id"] in [s["id"] for s in SCENARIOS])
        verdict = "RT-004 PI VALIDATION COMPLETE" if (all_pass and not self.abort_reason and not STATIC_NOT_RUN) else "RT-004 PI FOLLOW-UP REQUIRED"
        with open(os.path.join(self.dir, "not-run.md"), "w") as f:
            f.write("# NOT RUN\n\n")
            for r in self.results:
                if r["status"] == "NOT RUN":
                    f.write("- **%s %s**: %s\n" % (r["id"], r["title"], "; ".join(r.get("notes") or [])))
            for sid, title, why in STATIC_NOT_RUN:
                f.write("- **%s %s**: %s\n" % (sid, title, why))
        with open(os.path.join(self.dir, "summary.md"), "w") as f:
            f.write("# RT-004 Pi validation campaign %s\n\n" % self.id)
            f.write("Mode: %s. Abort: %s.\n\n" % ("fixture" if self.fixture else "real", self.abort_reason or "none"))
            f.write("| Scenario | Status | Stop | Key metrics (measured unless noted) |\n|---|---|---|---|\n")
            for r in self.results:
                mshort = json.dumps(r.get("metrics", {}), default=str)[:600].replace("|", "/")
                f.write("| %s %s | %s | %s | %s |\n" % (r["id"], r["title"], r["status"], (r.get("stop_reason") or "").replace("|", "/"), mshort))
            f.write("\nStatic coverage gaps: see not-run.md (%d items).\n\n" % len(STATIC_NOT_RUN))
            f.write("Semantics: RSS/RssAnon/CMA/MemAvailable = measured (collector); historyPixMiB = measured pixel payload; playhead/ambient GPU MiB = estimates; DRM/dma-buf = unavailable without root; GL errors = scope-level attribution only.\n\n")
            f.write("**%s**\n" % verdict)
        self.status("finished", {"verdict": verdict, "exit_code": code})
        archive = os.path.join(os.path.dirname(self.dir), self.id + ".tar.gz")
        with tarfile.open(archive, "w:gz") as tar:
            tar.add(self.dir, arcname=self.id)
        print(self.dir)
        print("archive: %s" % archive)
        print("abort: %s" % (self.abort_reason or "none"))
        print(verdict)
        return code


def cmd_plan(args):
    print("RT-004 Pi campaign plan (%s) -- nothing is launched.\n" % CAMPAIGN_VERSION)
    print("Preflight: not root, provenance/binary identity, media catalog, DISPLAY/xrandr, disk budget, vcgencmd throttled/temp/gpu mem,")
    print("           /proc/meminfo (CmaFree > %.0f, MemAvailable > %.0f MiB), debugfs, no running runtime, %d offline watchdog tests (no skips).\n"
          % (ms.REAL_THRESHOLDS["preflight_cma_free_min_mib"], ms.REAL_THRESHOLDS["mem_available_min_mib"], EXPECTED_OFFLINE_TESTS))
    per = 3 + ms.REAL_THRESHOLDS["wall_max_s"] + ms.TERM_GRACE_S + 3 + RECOVERY_TIMEOUT_S
    for sc in SCENARIOS:
        print("%-4s %s%s\n     purpose:   %s\n     pass if:   %s\n     command:   python3 %s run --app-dir . --run-root <campaign>/%s --label %s %s\n"
              % (sc["id"], sc["title"], (" (requires %s PASS)" % sc["requires"]) if sc.get("requires") else "", sc["purpose"], sc["predicate"],
                 os.path.relpath(SUP), sc["id"], sc["id"], " ".join("--env %s=%s" % kv for kv in sc["env"].items())))
    print("NOT RUN by design:")
    for sid, title, why in STATIC_NOT_RUN:
        print("  %s %s: %s" % (sid, title, why))
    print("\nLimits per run (supervisor, not overridable): %s" % json.dumps(ms.REAL_THRESHOLDS))
    print("Max per scenario incl. recovery: %.0f s; worst-case campaign: %.1f min (+ ~3 min offline tests); disk budget %.0f MiB"
          % (per, per * len(SCENARIOS) / 60.0, 2 * len(SCENARIOS) * PER_RUN_LOG_BUDGET / ms.MiB))
    return 0


def cmd_provenance(_args):
    root = os.path.normpath(os.path.join(HERE, "..", ".."))
    code, head = run_cmd(["git", "-C", root, "rev-parse", "HEAD"])
    code2, porcelain = run_cmd(["git", "-C", root, "status", "--porcelain"])
    code3, diff = run_cmd(["git", "-C", root, "diff", "HEAD"], timeout=60)
    if code != 0:
        raise SystemExit("provenance must be written from the git checkout on the development host")
    prov = {"git_head": head.strip(), "dirty": bool(porcelain.strip()), "status_porcelain": porcelain.splitlines(),
            "diff_sha256": hashlib.sha256((diff or "").encode()).hexdigest(), "created_utc": utc(), "host": platform.node()}
    atomic_json(PROVENANCE, prov)
    print("wrote %s (head %s, dirty=%s)" % (PROVENANCE, prov["git_head"][:12], prov["dirty"]))
    return 0


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("command", choices=["plan", "preflight", "run", "provenance"])
    p.add_argument("--app-dir", default=".")
    p.add_argument("--run-root", default="~/rt004fp")
    p.add_argument("--only", default=None, help="comma-separated scenario ids (others NOT RUN)")
    p.add_argument("--continue-on-functional-failure", action="store_true",
                   help="continue after an isolated functional FAIL when the run completed normally and resources recovered (default: abort)")
    p.add_argument("--fixture", default=None, help="offline test config JSON (dummy processes only)")
    args = p.parse_args(argv)
    args.only = [s.strip() for s in args.only.split(",")] if args.only else None
    fixture = None
    if args.fixture:
        with open(args.fixture) as f:
            fixture = json.load(f)
    if args.command == "plan":
        return cmd_plan(args)
    if args.command == "provenance":
        return cmd_provenance(args)
    if not fixture and not sys.platform.startswith("linux"):
        raise SystemExit("preflight/run need the Pi (Linux); use --fixture for offline tests")
    if args.command == "preflight":
        out = os.path.join(os.path.expanduser(args.run_root), "preflight-%s-%04x" % (datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ"), random.getrandbits(16)))
        os.makedirs(out, exist_ok=False)
        pf = preflight(args, fixture, out)
        for c in pf["checks"]:
            print("%-24s %-11s %s" % (c["check"], c["status"].upper(), json.dumps(c["detail"], default=str)[:160]))
        print("\nPREFLIGHT %s -> %s" % ("OK" if pf["ok"] else "FAILED: " + ", ".join(pf["failed"]), out))
        return 0 if pf["ok"] else 3
    return Campaign(args, fixture).run()


if __name__ == "__main__":
    sys.exit(main())
