#!/usr/bin/env python3
"""RT-004 memory-attribution supervisor -- DIAGNOSTIC ONLY.

Runs ONE bounded ExperienceRuntime diagnostic on the Raspberry Pi under an
independent watchdog, with an external 1 Hz memory collector. Everything here
lives outside the app process: the app is a child in its own process group,
and the collector keeps sampling even while the app's main loop is stalled.

Time basis: time.monotonic() == CLOCK_MONOTONIC on Linux, the same clock as
the app's steady_clock "mono=" stamps (frame-phase WINDOW/EVENT/SLOW lines
and stall-probe NOTE/LONG FRAME lines). Wall-clock UTC is secondary metadata.

CMA policy (Architecture conditional approval, 2026-10-11):
  * preflight: the app is NOT launched unless CmaFree > 96 MiB (and
    MemAvailable > 256 MiB) before launch;
  * from launch onward, continuously (no startup grace period): stop when
    CmaFree <= 40 MiB (absolute floor);
  * additionally, once the driver logs its "blob-baseline" marker, the
    median CmaFree over the next 3 s is the stabilized Blob baseline; stop
    when CmaFree has declined >= 48 MiB below it. The floor always applies,
    with or without a baseline (a relative-drop-only rule is not used).
Other stop conditions (unchanged): MemAvailable <= 256 MiB, app RSS >= 300
MiB, wall clock >= 45 s since launch, app exit, collector heartbeat stale
> 3 s, a required metric unreadable, app log volume > 32 MiB, the operator
STOP file, or SIGINT/SIGTERM/SIGHUP to this supervisor.

On stop: TERM to the app's process group (after a PID-identity check),
wait up to 5 s, then KILL. On Linux the app also gets PR_SET_PDEATHSIG
(SIGKILL), so it cannot outlive a crashed supervisor.

Usage (real run, on the Pi, from sketches/experience_runtime):
  python3 ../../scripts/rt004/memattr_supervisor.py run [--build-id ID]

Offline tests only (never launches the runtime):
  python3 scripts/rt004/memattr_supervisor.py fixture --proc-root DIR \
      --cmd 'sleep 600' [--thresholds JSON] [--run-root DIR]
"""

import argparse
import datetime
import hashlib
import json
import os
import platform
import random
import shlex
import signal
import subprocess
import sys
import threading
import time

MiB = 1024 * 1024
KiB_PER_MiB = 1024
UNAVAILABLE = "unavailable"

REAL_THRESHOLDS = {
    "preflight_cma_free_min_mib": 96.0,
    "cma_free_floor_mib": 40.0,
    "cma_max_decline_mib": 48.0,
    "mem_available_min_mib": 256.0,
    "rss_max_mib": 300.0,
    "wall_max_s": 45.0,
}
SAMPLE_PERIOD_S = 1.0
WATCH_PERIOD_S = 0.25
COLLECTOR_STALE_S = 3.0
TERM_GRACE_S = 5.0
LOG_CAP_BYTES = 32 * MiB
PRE_LAUNCH_SAMPLES = 3
BASELINE_MARKER = "blob-baseline"  # Rt004StallDriver temporal-memattr window mark
BASELINE_WINDOW_S = 3.0
POST_EXIT_SAMPLES = 3

STATUS_KEYS = ["VmRSS", "RssAnon", "RssFile", "RssShmem", "VmSwap"]
ROLLUP_KEYS = ["Rss", "Pss", "Pss_Anon", "Pss_File", "Pss_Shmem", "Anonymous", "Swap"]
MEMINFO_KEYS = ["MemAvailable", "SwapFree", "CmaTotal", "CmaFree"]

COLUMNS = (
    ["run_id", "seq", "mono_s", "wall_utc", "phase", "pid", "pid_alive", "identity"]
    + ["status_" + k.lower() + "_kb" for k in STATUS_KEYS]
    + ["rollup_" + k.lower() + "_kb" for k in ROLLUP_KEYS]
    + ["meminfo_" + k.lower() + "_kb" for k in MEMINFO_KEYS]
    + ["drm_bo_stats", "drm_clients_lines", "dmabuf_objects", "dmabuf_bytes", "sample_cost_ms"]
)

APP_ENV = {
    # Diagnostic switches only; production behavior is otherwise unchanged.
    "EXPERIENCE_RUNTIME_VIDEO_DWELL": "900,900",
    "EXPERIENCE_RUNTIME_STALL_PROBE": "1",
    "EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE": "1",
    "EXPERIENCE_RUNTIME_FRAME_PHASE_WINDOW_MS": "1000",
    "EXPERIENCE_RUNTIME_STALL_DRIVER": "1",
    "EXPERIENCE_RUNTIME_STALL_DRIVER_SCRIPT": "temporal-memattr",
}


# Scenario environment accepted via --env (orchestrator): diagnostic switches
# only. Log paths are always set by the supervisor itself; nothing here can
# change a watchdog threshold (thresholds are not environment-driven).
ENV_ALLOWED_PREFIX = "EXPERIENCE_RUNTIME_"
ENV_FORBIDDEN_SUFFIX = ("_LOG",)


def parse_env_overrides(items):
    out = {}
    for item in items or []:
        if "=" not in item:
            raise SystemExit("--env expects KEY=VALUE: %r" % item)
        k, v = item.split("=", 1)
        if not k.startswith(ENV_ALLOWED_PREFIX) or k.endswith(ENV_FORBIDDEN_SUFFIX):
            raise SystemExit("--env key not allowed: %s" % k)
        out[k] = v
    return out


def classify_stop(reason, returncode):
    """One stop class for gating: completed | crash | ceiling | safety |
    refused | interrupted | error."""
    r = reason or ""
    if r.startswith("app_exited"):
        return "completed" if returncode == 0 else "crash"
    if r.startswith("wall_clock"):
        return "ceiling"
    if r.startswith("preflight_refused"):
        return "refused"
    if r.startswith(("supervisor_signal", "operator_stop_file")):
        return "interrupted"
    if r.startswith(("cma_free", "cma_decline", "mem_available", "rss>=", "metric_unavailable", "collector_", "log_volume", "identity")):
        return "safety"
    return "error"


def utc_now():
    return datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%S.%fZ")


def read_text(path, limit=1 << 20):
    try:
        with open(path, "r", errors="replace") as f:
            return f.read(limit)
    except (OSError, IOError):
        return None


def parse_kb_fields(text, keys):
    """'Key:   1234 kB' lines -> {key: int kB}; keys not present are absent."""
    out = {}
    if text is None:
        return out
    for line in text.splitlines():
        if ":" not in line:
            continue
        k, v = line.split(":", 1)
        k = k.strip()
        if k in keys:
            parts = v.split()
            if parts:
                try:
                    out[k] = int(parts[0])
                except ValueError:
                    pass
    return out


class ProcSource:
    """Reads process/system memory. Real mode: /proc and debugfs. Fixture
    mode: pid-agnostic files under proc_root (status, smaps_rollup, meminfo,
    stat) so offline tests can drive threshold crossings."""

    def __init__(self, fixture_root=None):
        self.fixture_root = fixture_root
        self.has_proc = os.path.isdir("/proc/self")

    def _pid_file(self, pid, name):
        if self.fixture_root:
            return os.path.join(self.fixture_root, name)
        return "/proc/%d/%s" % (pid, name)

    def status(self, pid):
        return parse_kb_fields(read_text(self._pid_file(pid, "status")), STATUS_KEYS)

    def rollup(self, pid):
        return parse_kb_fields(read_text(self._pid_file(pid, "smaps_rollup")), ROLLUP_KEYS)

    def meminfo(self):
        path = os.path.join(self.fixture_root, "meminfo") if self.fixture_root else "/proc/meminfo"
        return parse_kb_fields(read_text(path), MEMINFO_KEYS)

    def starttime(self, pid):
        """Field 22 of /proc/<pid>/stat (always the real /proc: identity is
        about the real child). None where /proc is unavailable."""
        if not self.has_proc:
            return None
        text = read_text("/proc/%d/stat" % pid)
        if not text or ")" not in text:
            return None
        fields = text.rsplit(")", 1)[1].split()
        try:
            return int(fields[19])
        except (IndexError, ValueError):
            return None

    def drm(self):
        """Readable DRM/dma-buf diagnostics (usually root-only). Never added
        to RSS; not total GPU memory."""
        out = {"drm_bo_stats": UNAVAILABLE, "drm_clients_lines": UNAVAILABLE,
               "dmabuf_objects": UNAVAILABLE, "dmabuf_bytes": UNAVAILABLE}
        if self.fixture_root:
            base_dri = [os.path.join(self.fixture_root, "dri")]
            bufinfo = os.path.join(self.fixture_root, "dma_buf_bufinfo")
        else:
            base_dri = ["/sys/kernel/debug/dri/soc:gpu", "/sys/kernel/debug/dri/0"]
            bufinfo = "/sys/kernel/debug/dma_buf/bufinfo"
        for d in base_dri:
            bo = read_text(os.path.join(d, "bo_stats"), 8192)
            if bo is not None:
                out["drm_bo_stats"] = ";".join(l.strip() for l in bo.splitlines() if l.strip())[:400].replace("\t", " ")
                clients = read_text(os.path.join(d, "clients"), 65536)
                if clients is not None:
                    out["drm_clients_lines"] = str(max(0, len(clients.strip().splitlines()) - 1))
                break
        info = read_text(bufinfo, 4 << 20)
        if info is not None:
            for line in reversed(info.splitlines()):
                # "Total 12 objects, 345678 bytes"
                if line.strip().startswith("Total"):
                    parts = line.replace(",", " ").split()
                    try:
                        out["dmabuf_objects"] = str(int(parts[1]))
                        out["dmabuf_bytes"] = str(int(parts[3]))
                    except (IndexError, ValueError):
                        pass
                    break
        return out


class Supervisor:
    def __init__(self, args, fixture):
        self.args = args
        self.fixture = fixture
        self.src = ProcSource(args.proc_root if fixture else None)
        self.thresholds = dict(REAL_THRESHOLDS)
        if fixture and args.thresholds:
            self.thresholds.update(json.loads(args.thresholds))
        self.env_overrides = parse_env_overrides(getattr(args, "env", None))
        label = getattr(args, "label", None) or "memattr"
        if not label.replace("-", "").replace("_", "").isalnum():
            raise SystemExit("--label must be alphanumeric/-/_")
        self.run_id = "%s-%s-%04x" % (label, datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ"), random.getrandbits(16))
        self.run_dir = os.path.join(os.path.expanduser(args.run_root), self.run_id)
        os.makedirs(self.run_dir, exist_ok=False)  # unique; never reuse or overwrite
        self.lock = threading.Lock()
        self.samples_f = open(os.path.join(self.run_dir, "samples.tsv"), "w", buffering=1)
        self.samples_f.write("\t".join(COLUMNS) + "\n")
        self.events_f = open(os.path.join(self.run_dir, "watchdog.tsv"), "w", buffering=1)
        self.events_f.write("mono_s\twall_utc\tevent\tdetail\n")
        self.seq = 0
        self.proc = None
        self.pid = None
        self.pgid = None
        self.child_starttime = None
        self.launch_mono = None
        self.phase = "prelaunch"
        self.heartbeat = None
        self.last_row = None
        self.collector_stop = threading.Event()
        self.collector_error = None
        self.signal_name = None
        self.stop_reason = None
        self.exit_code = None
        self.fail_after = args.collector_fail_after if fixture else None
        self.cma_baseline = None
        self.baseline_marker_mono = None
        self.baseline_readings = []

    # ---- logging -----------------------------------------------------------
    def event(self, name, detail=""):
        with self.lock:
            self.events_f.write("%.3f\t%s\t%s\t%s\n" % (time.monotonic(), utc_now(), name, str(detail).replace("\t", " ").replace("\n", " | ")))
            self.events_f.flush()

    # ---- collector ---------------------------------------------------------
    def sample_row(self):
        t0 = time.monotonic()
        row = {c: UNAVAILABLE for c in COLUMNS}
        row["run_id"] = self.run_id
        row["seq"] = str(self.seq)
        row["mono_s"] = "%.3f" % t0
        row["wall_utc"] = utc_now()
        row["phase"] = self.phase
        if self.pid is not None:
            row["pid"] = str(self.pid)
            alive = self.proc is not None and self.proc.poll() is None
            row["pid_alive"] = "1" if alive else "0"
            st = self.src.starttime(self.pid)
            if st is None or self.child_starttime is None:
                row["identity"] = UNAVAILABLE
            else:
                row["identity"] = "ok" if st == self.child_starttime else "MISMATCH"
            if alive:
                for k, v in self.src.status(self.pid).items():
                    row["status_" + k.lower() + "_kb"] = str(v)
                for k, v in self.src.rollup(self.pid).items():
                    row["rollup_" + k.lower() + "_kb"] = str(v)
        else:
            row["pid"] = "-"
            row["pid_alive"] = "-"
            row["identity"] = "-"
        for k, v in self.src.meminfo().items():
            row["meminfo_" + k.lower() + "_kb"] = str(v)
        row.update(self.src.drm())
        row["sample_cost_ms"] = "%.2f" % ((time.monotonic() - t0) * 1000.0)
        return row

    def write_row(self, row):
        with self.lock:
            self.samples_f.write("\t".join(row[c] for c in COLUMNS) + "\n")
            self.samples_f.flush()
            self.last_row = row
            self.seq += 1

    def collector_loop(self):
        next_t = time.monotonic()
        n = 0
        try:
            while not self.collector_stop.is_set():
                if self.fail_after is not None and n >= self.fail_after:
                    raise RuntimeError("fixture: injected collector failure")
                self.write_row(self.sample_row())
                self.heartbeat = time.monotonic()
                n += 1
                next_t += SAMPLE_PERIOD_S
                self.collector_stop.wait(max(0.0, next_t - time.monotonic()))
        except Exception as e:  # noqa: BLE001 -- any collector failure must stop the run
            self.collector_error = repr(e)
            self.event("collector_failed", repr(e))

    # ---- watchdog ----------------------------------------------------------
    def required_reading(self):
        """The watchdog's OWN reads (independent of the collector thread)."""
        mem = self.src.meminfo()
        st = self.src.status(self.pid) if self.pid is not None else {}
        return {
            "cma_free_mib": mem["CmaFree"] / KiB_PER_MiB if "CmaFree" in mem else None,
            "mem_available_mib": mem["MemAvailable"] / KiB_PER_MiB if "MemAvailable" in mem else None,
            "rss_mib": st["VmRSS"] / KiB_PER_MiB if "VmRSS" in st else None,
        }

    def threshold_violation(self, r, need_rss, preflight=False):
        th = self.thresholds
        for key in ("cma_free_mib", "mem_available_mib") + (("rss_mib",) if need_rss else ()):
            if r[key] is None:
                return "metric_unavailable:" + key
        if preflight:
            if r["cma_free_mib"] <= th["preflight_cma_free_min_mib"]:
                return "preflight_cma_free<=%.0fMiB (%.1f)" % (th["preflight_cma_free_min_mib"], r["cma_free_mib"])
        else:
            if r["cma_free_mib"] <= th["cma_free_floor_mib"]:
                return "cma_free<=%.0fMiB_floor (%.1f)" % (th["cma_free_floor_mib"], r["cma_free_mib"])
            if self.cma_baseline is not None and self.cma_baseline - r["cma_free_mib"] >= th["cma_max_decline_mib"]:
                return "cma_decline>=%.0fMiB_from_baseline_%.1f (%.1f)" % (th["cma_max_decline_mib"], self.cma_baseline, r["cma_free_mib"])
        if r["mem_available_mib"] <= th["mem_available_min_mib"]:
            return "mem_available<=%.0fMiB (%.1f)" % (th["mem_available_min_mib"], r["mem_available_mib"])
        if need_rss and r["rss_mib"] >= th["rss_max_mib"]:
            return "rss>=%.0fMiB (%.1f)" % (th["rss_max_mib"], r["rss_mib"])
        return None

    def track_baseline(self, r):
        """Stabilized Blob baseline: median CmaFree over BASELINE_WINDOW_S
        after the driver's blob-baseline marker appears in driver.log."""
        if self.cma_baseline is not None:
            return
        now = time.monotonic()
        if self.baseline_marker_mono is None:
            text = read_text(os.path.join(self.run_dir, "driver.log"), 65536)
            if text and BASELINE_MARKER in text:
                self.baseline_marker_mono = now
                self.event("cma_baseline_marker", "driver logged %s" % BASELINE_MARKER)
            return
        if r["cma_free_mib"] is not None:
            self.baseline_readings.append(r["cma_free_mib"])
        if now - self.baseline_marker_mono >= BASELINE_WINDOW_S and self.baseline_readings:
            v = sorted(self.baseline_readings)
            self.cma_baseline = v[len(v) // 2]
            self.event("cma_baseline", "%.1f MiB (median of %d readings; decline limit %.0f MiB -> stop at %.1f, floor %.0f)"
                       % (self.cma_baseline, len(v), self.thresholds["cma_max_decline_mib"],
                          self.cma_baseline - self.thresholds["cma_max_decline_mib"], self.thresholds["cma_free_floor_mib"]))

    def log_bytes(self):
        total = 0
        for name in ("app-stdout.log", "app-stderr.log", "frame-phase.log", "stall-probe.log", "driver.log"):
            try:
                total += os.path.getsize(os.path.join(self.run_dir, name))
            except OSError:
                pass
        return total

    def identity_ok(self):
        if self.child_starttime is None:
            return True  # no /proc (offline fixture on macOS): we still hold the unreaped child
        return self.src.starttime(self.pid) == self.child_starttime

    def group_alive(self):
        try:
            os.killpg(self.pgid, 0)
            return True
        except ProcessLookupError:
            return False
        except PermissionError:
            return False

    def terminate(self, reason):
        if self.stop_reason is None:
            self.stop_reason = reason
        self.event("stop", reason)
        r = self.required_reading()
        self.event("stop_last_reading", json.dumps(r))
        if self.last_row is not None:
            self.event("stop_last_sample", json.dumps({k: self.last_row[k] for k in COLUMNS if k.startswith(("status_vmrss", "status_rss", "meminfo_"))}))
        if self.proc is None:
            return
        if self.proc.poll() is None:
            if not self.identity_ok():
                self.event("identity_mismatch", "pid %d starttime changed; not signalling" % self.pid)
            else:
                self.event("signal", "SIGTERM -> pgid %d" % self.pgid)
                try:
                    os.killpg(self.pgid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                deadline = time.monotonic() + TERM_GRACE_S
                while self.proc.poll() is None and time.monotonic() < deadline:
                    time.sleep(0.05)
                if self.proc.poll() is None:
                    self.event("signal", "SIGKILL -> pgid %d (TERM grace %.0fs expired)" % (self.pgid, TERM_GRACE_S))
                    try:
                        os.killpg(self.pgid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    self.proc.wait()
        self.exit_code = self.proc.returncode
        # Children/grandchildren left in the app's own process group.
        if self.group_alive():
            self.event("signal", "SIGTERM -> remaining group members of pgid %d" % self.pgid)
            try:
                os.killpg(self.pgid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            deadline = time.monotonic() + TERM_GRACE_S
            while self.group_alive() and time.monotonic() < deadline:
                time.sleep(0.05)
            if self.group_alive():
                self.event("signal", "SIGKILL -> remaining group members of pgid %d" % self.pgid)
                try:
                    os.killpg(self.pgid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        self.event("app_exit", "returncode=%s" % self.exit_code)

    def on_signal(self, signum, _frame):
        self.signal_name = signal.Signals(signum).name

    # ---- launch ------------------------------------------------------------
    def build_command(self):
        if self.fixture:
            cmd = shlex.split(self.args.cmd)
            if any("experience_runtime" in c for c in cmd):
                raise SystemExit("fixture mode refuses to launch the runtime")
            env = dict(os.environ)
            env.update(self.env_overrides)
            env["MEMATTR_RUN_DIR"] = self.run_dir  # lets fixture dummies write a driver.log marker
            return cmd, os.getcwd(), env
        app_dir = os.path.abspath(self.args.app_dir)
        binary = os.path.join(app_dir, "bin", "experience_runtime")
        if not os.access(binary, os.X_OK):
            raise SystemExit("not executable: %s" % binary)
        env = dict(os.environ)
        env.update(APP_ENV)
        env.update(self.env_overrides)
        env["EXPERIENCE_RUNTIME_STALL_PROBE_LOG"] = os.path.join(self.run_dir, "stall-probe.log")
        env["EXPERIENCE_RUNTIME_FRAME_PHASE_LOG"] = os.path.join(self.run_dir, "frame-phase.log")
        env["EXPERIENCE_RUNTIME_STALL_DRIVER_LOG"] = os.path.join(self.run_dir, "driver.log")
        env.setdefault("DISPLAY", ":0")
        env["MEMATTR_RUN_DIR"] = self.run_dir
        return [binary], app_dir, env

    @staticmethod
    def child_preexec():
        # Linux: the app dies with the supervisor (no orphaned runtime).
        if sys.platform.startswith("linux"):
            try:
                import ctypes
                libc = ctypes.CDLL("libc.so.6", use_errno=True)
                libc.prctl(1, signal.SIGKILL, 0, 0, 0)  # PR_SET_PDEATHSIG
            except Exception:  # noqa: BLE001
                pass

    def write_manifest(self, cmd, cwd, env, extra):
        binary = cmd[0]
        bin_info = {}
        if not self.fixture:
            try:
                h = hashlib.sha256()
                with open(binary, "rb") as f:
                    for chunk in iter(lambda: f.read(1 << 20), b""):
                        h.update(chunk)
                st = os.stat(binary)
                bin_info = {"path": binary, "size": st.st_size, "mtime_utc": datetime.datetime.utcfromtimestamp(st.st_mtime).isoformat() + "Z", "sha256": h.hexdigest()}
            except OSError as e:
                bin_info = {"path": binary, "error": repr(e)}
        manifest = {
            "run_id": self.run_id,
            "mode": "fixture" if self.fixture else "real",
            "created_utc": utc_now(),
            "host": list(platform.uname()),
            "python": sys.version.split()[0],
            "cwd": cwd,
            "command": cmd,
            "env_diagnostic": {k: v for k, v in env.items() if k.startswith("EXPERIENCE_RUNTIME_") or k in ("DISPLAY",)},
            "thresholds": self.thresholds,
            "sample_period_s": SAMPLE_PERIOD_S,
            "watch_period_s": WATCH_PERIOD_S,
            "collector_stale_s": COLLECTOR_STALE_S,
            "term_grace_s": TERM_GRACE_S,
            "log_cap_bytes": LOG_CAP_BYTES,
            "binary": bin_info,
            "build_id": self.args.build_id or UNAVAILABLE,
            "source_revision": self.args.source_revision or UNAVAILABLE,
            "time_basis": "mono_s = time.monotonic() = CLOCK_MONOTONIC (Linux); app mono= = steady_clock = CLOCK_MONOTONIC",
        }
        manifest.update(extra)
        with open(os.path.join(self.run_dir, "manifest.json"), "w") as f:
            json.dump(manifest, f, indent=2, sort_keys=True)
            f.write("\n")

    def run(self):
        for s in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            signal.signal(s, self.on_signal)
        cmd, cwd, env = self.build_command()
        self.write_manifest(cmd, cwd, env, {"status": "started"})
        self.event("supervisor_start", "run_dir=%s" % self.run_dir)

        # Pre-launch baseline, sampled synchronously (no threads before the
        # fork: preexec_fn is only safe in a single-threaded parent).
        for _ in range(PRE_LAUNCH_SAMPLES):
            self.write_row(self.sample_row())
            time.sleep(SAMPLE_PERIOD_S)
        pre = self.required_reading()
        self.event("preflight_reading", json.dumps(pre))
        violation = self.threshold_violation(pre, need_rss=False, preflight=True)
        if violation or self.signal_name:
            self.stop_reason = "preflight_refused: " + (violation or "signal " + str(self.signal_name))
            self.event("preflight_refused", self.stop_reason)
            return self.finish(cmd, cwd, env)

        out = open(os.path.join(self.run_dir, "app-stdout.log"), "wb")
        err = open(os.path.join(self.run_dir, "app-stderr.log"), "wb")
        self.launch_mono = time.monotonic()
        self.proc = subprocess.Popen(cmd, cwd=cwd, env=env, stdout=out, stderr=err, stdin=subprocess.DEVNULL,
                                     start_new_session=True, preexec_fn=self.child_preexec)
        self.pid = self.proc.pid
        self.pgid = self.pid
        self.child_starttime = self.src.starttime(self.pid)
        self.phase = "running"
        self.event("launch", "pid=%d pgid=%d starttime=%s launch_mono=%.3f cmd=%s" % (self.pid, self.pgid, self.child_starttime, self.launch_mono, shlex.join(cmd) if hasattr(shlex, "join") else " ".join(cmd)))
        self.heartbeat = time.monotonic()
        collector = threading.Thread(target=self.collector_loop, name="collector", daemon=True)
        collector.start()

        try:
            while True:
                now = time.monotonic()
                if self.signal_name:
                    self.terminate("supervisor_signal:" + self.signal_name)
                    break
                if self.proc.poll() is not None:
                    self.terminate("app_exited returncode=%s" % self.proc.returncode)
                    break
                if now - self.launch_mono >= self.thresholds["wall_max_s"]:
                    self.terminate("wall_clock>=%.0fs" % self.thresholds["wall_max_s"])
                    break
                if os.path.exists(os.path.join(self.run_dir, "STOP")):
                    self.terminate("operator_stop_file")
                    break
                if self.collector_error or self.heartbeat is None or now - self.heartbeat > COLLECTOR_STALE_S:
                    self.terminate("collector_stale_or_failed: %s" % (self.collector_error or "no heartbeat for %.1fs" % (now - (self.heartbeat or now))))
                    break
                reading = self.required_reading()
                self.track_baseline(reading)
                v = self.threshold_violation(reading, need_rss=True)
                if v and not (v.startswith("metric_unavailable:rss") and self.proc.poll() is not None):
                    self.terminate(v)
                    break
                if self.log_bytes() > LOG_CAP_BYTES:
                    self.terminate("log_volume>%dMiB" % (LOG_CAP_BYTES // MiB))
                    break
                time.sleep(WATCH_PERIOD_S)
        except BaseException as e:  # never leave the runtime running
            self.terminate("supervisor_exception: %r" % (e,))
            raise
        finally:
            if self.proc is not None and self.proc.poll() is None:
                self.terminate("supervisor_finally")
            out.close()
            err.close()
        # A few post-exit samples (system recovery), then stop collecting.
        self.phase = "postexit"
        time.sleep(POST_EXIT_SAMPLES * SAMPLE_PERIOD_S)
        self.collector_stop.set()
        collector.join(timeout=3.0)
        return self.finish(cmd, cwd, env)

    def finish(self, cmd, cwd, env):
        files = {}
        for name in sorted(os.listdir(self.run_dir)):
            try:
                files[name] = os.path.getsize(os.path.join(self.run_dir, name))
            except OSError:
                pass
        app_config_mono = None
        try:
            with open(os.path.join(self.run_dir, "frame-phase.log")) as f:
                first = f.readline()
            if " mono=" in first:
                app_config_mono = float(first.split(" mono=", 1)[1].split()[0])
        except (OSError, ValueError, IndexError):
            pass
        extra = {
            "status": "finished",
            "stop_reason": self.stop_reason,
            "stop_class": classify_stop(self.stop_reason, self.exit_code),
            "app_returncode": self.exit_code,
            "launch_mono_s": self.launch_mono,
            "cma_baseline_mib": self.cma_baseline,
            "end_mono_s": time.monotonic(),
            "end_utc": utc_now(),
            "samples_written": self.seq,
            "app_config_mono_s": app_config_mono,
            "app_config_minus_launch_s": (app_config_mono - self.launch_mono) if (app_config_mono and self.launch_mono) else None,
            "files": files,
        }
        self.event("supervisor_end", self.stop_reason)
        self.write_manifest(cmd, cwd, env, extra)
        self.samples_f.flush()
        os.fsync(self.samples_f.fileno())
        self.samples_f.close()
        self.events_f.close()
        print(self.run_dir)
        print("stop_reason: %s" % self.stop_reason)
        return 0


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="mode", required=True)
    r = sub.add_parser("run", help="the one real supervised run (Pi only)")
    r.add_argument("--app-dir", default=".", help="sketches/experience_runtime (contains bin/experience_runtime)")
    r.add_argument("--run-root", default="~/rt004fp")
    r.add_argument("--build-id", default=None)
    r.add_argument("--source-revision", default=None)
    r.add_argument("--env", action="append", default=[], help="scenario switch KEY=VALUE (EXPERIENCE_RUNTIME_* only, not *_LOG)")
    r.add_argument("--label", default=None, help="run directory prefix (scenario id)")
    f = sub.add_parser("fixture", help="offline test mode: dummy command + fixture /proc files")
    f.add_argument("--cmd", required=True)
    f.add_argument("--proc-root", required=True)
    f.add_argument("--thresholds", default=None, help="JSON overrides (fixture mode only)")
    f.add_argument("--run-root", required=True)
    f.add_argument("--collector-fail-after", type=int, default=None)
    f.add_argument("--build-id", default=None)
    f.add_argument("--source-revision", default=None)
    f.add_argument("--env", action="append", default=[])
    f.add_argument("--label", default=None)
    args = p.parse_args(argv)
    if args.mode == "run" and not sys.platform.startswith("linux"):
        raise SystemExit("real mode needs Linux /proc (the Pi)")
    return Supervisor(args, fixture=(args.mode == "fixture")).run()


if __name__ == "__main__":
    sys.exit(main())
