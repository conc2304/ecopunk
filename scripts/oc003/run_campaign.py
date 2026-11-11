#!/usr/bin/env python3
"""OC-003 sampling-frame attribution: SUPERVISED sequential campaign runner (operator-run; the coding agent never runs it on hardware).

Executes the commands of an existing plan (plan_interleaved.py -> <dir>/plan.json) strictly one after another, with pre-flight
capture, per-run stop conditions, guaranteed restoration of the production service, and the validation/analysis commands at the end.
It adds no experiment logic: the environment of every run is exactly the planner's `env -u ... KEY=VAL ... timeout -s INT N BIN` line.

  python3 scripts/oc003/run_campaign.py <dir>/plan.json --display :0 --workdir <REPO>/sketches/experience_runtime \
         [--stop-service] [--scope auto|system|user] [--unit ecopunk-runtime] [--min-events 100] [--dry-run]

Service handling (never silent):
  * If the production unit is ACTIVE the runner refuses to start, unless --stop-service is given; then it records the unit, stops it
    (system scope: `sudo systemctl stop`, which may prompt), and starts it again at the end on EVERY exit path (normal, stop condition,
    exception, SIGINT/SIGTERM) -- only if it was the one that stopped it.
  * --no-service: for a machine that has no such unit (desktop rehearsal). Refused if the unit exists.
  * No `experience_runtime` process may exist before any run (pgrep -x); a second graphical process aborts the campaign.
Stop conditions (checked after every run; the first hit aborts the campaign, restores, and exits 2):
  exit status other than the expected `timeout` end (124/130); unexpected process present; new `GL error`, `telemetry DISABLED`,
  `discardTimeout>0`, `discardWaitFailed>0`, `halfFailures>0` in a log; `verify_run.py` failing on the runs so far (binary hash drift, wrong
  media/scene/resolution, flushAfterFence mismatch, insufficient events, failed sampling attempts); kernel throttling / OOM lines in dmesg
  (best effort: dmesg may be unreadable without privileges, which is recorded, not hidden).
Output: <dir>/campaign/ (preflight.json, service_before.txt, state.json, run_status.json, restoration.json, campaign.log) and, after the last run,
<dir>/analysis/ (verify.txt, reconcile.txt, aggregate.txt, aggregate.json, sampler.csv, summary.json, report.md).
Pre-flight also runs the fail-closed isolation audit (verify_run.audit_plan): identical environment per condition, every managed variable set or
unset, DEFER_FLUSH presence-based, only the intended variables differ between P0/P2/P2F, campaign settings copy intact with both auto-cycles off.
Inherited EXPERIENCE_RUNTIME_* variables are scrubbed from the runs' environment (names recorded in campaign.log).
Exit 0 only if every run completed, verify_run.py printed VERIFIED, every run reconciled, and restoration matched the pre-flight record.
"""
import argparse
import glob
import hashlib
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import verify_run  # noqa: E402  (audit_plan: the same fail-closed isolation audit the verifier uses)
MARKERS = (b"raw_cfail", b"EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES", b"EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH")
# Nonzero GL error values only: the runtime prints a healthy `glError=0x00000000` status field, which must not match.
BAD_LOG = [r"GL error", r"(?i:gl[A-Za-z]*errors?[A-Za-z]*)=(?:0x)?0*[1-9a-fA-F]", r"telemetry DISABLED", r"discardTimeout=[1-9]", r"discardWaitFailed=[1-9]", r"halfFailures=[1-9]"]
BAD_DMESG = re.compile(r"throttl|Out of memory|oom-kill|over-?temp|thermal.*(critical|shutdown)", re.I)

state = {"we_stopped": False, "child": None, "restored": False, "name": "INIT", "dir": None}
log_fh = None
# Explicit state machine (written to campaign/state.json on every transition, so an interrupted campaign says where it stopped):
#   INIT -> PREFLIGHT -> [REFUSED | DRY_RUN | STOPPING_SERVICE -> RUNNING:<tag> -> CHECKING:<tag> -> (next run)...] -> RESTORING -> ANALYSING -> DONE
#   Any failure: -> ABORTED (restoration still runs). The only transitions that touch the system are STOPPING_SERVICE and RESTORING.


def set_state(name):
    state["name"] = name
    say("STATE " + name)
    if state["dir"]:
        try:
            with open(os.path.join(state["dir"], "state.json"), "w") as fh:
                json.dump({"state": name, "time": time.ctime()}, fh)
        except OSError:
            pass


def say(msg):
    line = time.strftime("%H:%M:%S ") + msg
    print(line, flush=True)
    if log_fh:
        log_fh.write(line + "\n")
        log_fh.flush()


def sh(cmd, timeout=60):
    try:
        r = subprocess.run(cmd, shell=isinstance(cmd, str), capture_output=True, text=True, timeout=timeout, errors="replace")
        return r.returncode, (r.stdout + r.stderr).strip()
    except Exception as e:  # noqa: BLE001  (a missing tool must be recorded, not crash the capture)
        return 127, f"{type(e).__name__}: {e}"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sysctl(scope, *args):
    return ["systemctl"] + (["--user"] if scope == "user" else []) + list(args)


def detect_scope(unit, want):
    scopes = [want] if want != "auto" else ["system", "user"]
    found = []
    for s in scopes:
        rc, out = sh(sysctl(s, "cat", unit))
        if rc == 0 and out:
            found.append(s)
    return found


def unit_snapshot(scope, unit):
    """Everything that must be identical after the campaign."""
    snap = {}
    if not scope:
        return snap
    snap["active"] = sh(sysctl(scope, "is-active", unit))[1]
    snap["enabled"] = sh(sysctl(scope, "is-enabled", unit))[1]
    snap["cat"] = sh(sysctl(scope, "cat", unit))[1]
    snap["environment"] = sh(sysctl(scope, "show", unit, "-p", "Environment", "-p", "WorkingDirectory", "-p", "ExecStart"))[1]
    snap["main_pid_restarts"] = sh(sysctl(scope, "show", unit, "-p", "MainPID", "-p", "NRestarts"))[1]
    return snap


def runtime_processes():
    rc, out = sh(["pgrep", "-x", "experience_runtime"])
    return [p for p in out.split() if p.isdigit()] if rc == 0 else []


def file_sha(path):
    return sha256(path) if path and os.path.isfile(path) else None


def descendants(root):
    """All live descendants of `root` (pid/ppid table from `ps`, identical on Linux and macOS). `timeout` puts itself in its own
    process group and pkill -s is not portable, so the parent/child chain is the only reliable handle on everything a run started."""
    rc, out = sh(["ps", "-axo", "pid=,ppid="])
    kids = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0].isdigit() and parts[1].isdigit():
            kids.setdefault(int(parts[1]), []).append(int(parts[0]))
    found, todo = [], [root]
    while todo:
        for k in kids.get(todo.pop(), []):
            found.append(k)
            todo.append(k)
    return found


def start_sysmon(child, path, say):
    """OC-003-F: best-effort external RSS/thermal/frequency monitor for the run's runtime process (sysmon.py, OC-003-F). Never raises: a
    monitoring problem must not change or fail a run; it is reported and the run continues without a .sys.csv."""
    try:
        import sysmon  # noqa: E402  (same directory; absent in packages that predate OC-003-F)
    except ImportError:
        return None
    holder = {"mon": None}

    def find_and_start():
        end = time.time() + 12
        while time.time() < end and holder["mon"] is None:
            for pid in descendants(child.pid):
                rc, comm = sh(["ps", "-o", "comm=", "-p", str(pid)])
                if rc == 0 and os.path.basename(comm.strip()) == "experience_runtime":
                    try:
                        holder["mon"] = sysmon.SysMonitor(pid, path, 5.0)
                        holder["mon"].start()
                    except Exception as e:  # noqa: BLE001
                        say(f"sysmon: could not start ({e}); continuing without it")
                        holder["mon"] = False
                    return
            time.sleep(0.5)
        if holder["mon"] is None:
            say("sysmon: runtime pid not found within 12 s; continuing without it")

    import threading
    th = threading.Thread(target=find_and_start, daemon=True)
    th.start()
    holder["thread"] = th
    return holder


def stop_sysmon(holder):
    if not holder:
        return
    try:
        holder["thread"].join(15)
        if holder.get("mon"):
            holder["mon"].stop()
    except Exception:  # noqa: BLE001
        pass


def kill_session(child, grace=10):
    """End EVERYTHING the run started (child = the shell we spawned). INT first (the planned graceful end), then TERM, then KILL.
    Returns True when nothing of the tree is left."""
    for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGKILL):
        end = time.time() + (grace if sig != signal.SIGKILL else 3)
        while True:
            tree = descendants(child.pid)
            alive_root = child.poll() is None
            if not tree and not alive_root:
                return True
            for pid in ([child.pid] + tree if alive_root else tree):
                try:
                    os.kill(pid, sig)
                except OSError:
                    pass
            if sig == signal.SIGKILL or time.time() >= end:
                break
            time.sleep(1.0)
    return not descendants(child.pid)


def restore(scope, unit, pre, outdir, dry):
    """Idempotent. Starts the unit again only if this runner stopped it; then compares with the pre-flight record."""
    if state["restored"]:
        return True
    state["restored"] = True
    ok = True
    rec = {}
    child = state["child"]
    if child is not None:
        say("restore: ending the run in progress (whole session)")
        if not kill_session(child):
            ok = False
            say("RESTORE PROBLEM: processes of the run are still alive; check `pgrep -a experience_runtime` before trusting the display")
        try:
            child.wait(5)
        except subprocess.TimeoutExpired:
            pass
    if state["we_stopped"] and not dry:
        cmd = sysctl(scope, "start", unit)
        if scope == "system" and os.geteuid() != 0:
            cmd = ["sudo"] + cmd
        say("restore: " + " ".join(cmd))
        rc, out = sh(cmd, timeout=120)
        rec["start_rc"], rec["start_out"] = rc, out
        time.sleep(5)
    # Postflight is ALWAYS taken and compared with the pre-flight record: restoration is never assumed.
    post = unit_snapshot(scope, unit)
    rec["post"] = post
    for k in ("active", "enabled", "cat", "environment"):
        if post.get(k) != pre.get(k):
            ok = False
            say(f"POSTFLIGHT DIFFERENCE in '{k}': before={str(pre.get(k))[:80]!r} after={str(post.get(k))[:80]!r}; report it, do not fix silently")
    rec["post_sha256"] = sha256(pre["_bin"]) if os.path.isfile(pre["_bin"]) else None
    if rec["post_sha256"] != pre["_sha"]:
        ok = False
        say("POSTFLIGHT DIFFERENCE: installed binary hash changed")
    rec["installed_settings_sha256_before"], rec["installed_settings_sha256_after"] = pre.get("_inst_sha"), file_sha(pre.get("_inst"))
    if rec["installed_settings_sha256_before"] != rec["installed_settings_sha256_after"]:
        ok = False
        say("RESTORE DIFFERENCE: the installed installation-settings.json changed during the campaign")
    rec["watched_files_after"] = {w: file_sha(w) for w in pre.get("_watch", {})}
    for w, h in pre.get("_watch", {}).items():
        if rec["watched_files_after"][w] != h:
            ok = False
            say(f"POSTFLIGHT DIFFERENCE: watched file changed during the campaign: {w}")
    rec["we_stopped_service"] = state["we_stopped"]
    rec["ok"] = ok
    rec["stray_processes"] = runtime_processes()
    try:
        with open(os.path.join(outdir, "restoration.json"), "w") as fh:
            json.dump(rec, fh, indent=2)
    except OSError:
        pass
    say("restoration " + ("MATCHES the pre-flight record" if ok else "DOES NOT MATCH -- STOP AND REPORT"))
    return ok


def main():
    global log_fh
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("plan")
    ap.add_argument("--display", default=os.environ.get("DISPLAY", ""), help="X display for the runtime, e.g. :0 (required unless --dry-run)")
    ap.add_argument("--workdir", required=True, help="directory the runtime is started from (<repo>/sketches/experience_runtime on the Pi)")
    ap.add_argument("--unit", default="ecopunk-runtime")
    ap.add_argument("--scope", default="auto", choices=["auto", "system", "user"])
    ap.add_argument("--stop-service", action="store_true", help="stop the production unit (recorded; restored on every exit path)")
    ap.add_argument("--no-service", action="store_true", help="rehearsal on a machine with no production unit (refused if the unit exists)")
    ap.add_argument("--watch", action="append", default=[], metavar="FILE",
                    help="extra file whose sha256 must be identical before and after (repeatable). Use it for the INSTALLED production binary when the campaign "
                         "runs a staged diagnostic binary (Route B), so 'the installed binary was not touched' is proven, not assumed")
    ap.add_argument("--min-events", type=int, default=100)
    ap.add_argument("--grace", type=int, default=45, help="seconds beyond the planned duration before a run is killed as hung")
    ap.add_argument("--settle", type=int, default=5, help="seconds between runs")
    ap.add_argument("--dry-run", action="store_true", help="print the pre-flight result and the exact run list; start nothing, stop nothing")
    a = ap.parse_args()

    if sys.version_info < (3, 7):
        sys.exit("run_campaign.py needs Python >= 3.7 (found %s)" % sys.version.split()[0])
    plan = json.load(open(a.plan))
    out = os.path.dirname(os.path.abspath(a.plan))
    cdir, adir = os.path.join(out, "campaign"), os.path.join(out, "analysis")
    os.makedirs(cdir, exist_ok=True)
    os.makedirs(adir, exist_ok=True)
    log_fh = open(os.path.join(cdir, "campaign.log"), "a")
    state["dir"] = cdir
    set_state("PREFLIGHT")
    problems = []

    # ---------------------------------------------------------------- pre-flight (read-only)
    binp = plan["binary"]
    pre = {"_bin": binp, "_sha": None, "_inst": os.path.join(a.workdir, "bin", "data", "installation-settings.json")}
    pre["_inst_sha"] = file_sha(pre["_inst"])
    pre["_watch"] = {os.path.abspath(w): file_sha(w) for w in a.watch}
    if not os.path.isfile(binp):
        problems.append(f"binary {binp} missing")
    else:
        pre["_sha"] = sha256(binp)
        if pre["_sha"] != plan["binary_sha256"]:
            problems.append(f"binary sha256 {pre['_sha']} != planned {plan['binary_sha256']}")
        data = open(binp, "rb").read()
        missing = [m.decode() for m in MARKERS if m not in data]
        if missing:
            problems.append(f"binary lacks instrumentation strings {missing}: it is NOT the attribution build. STOP: this is a prerequisite, not something the runner fixes -- "
                            "see the operator package, Route B (isolated diagnostic build), which needs Manager approval")
    for w, h in pre["_watch"].items():
        if h is None:
            problems.append(f"--watch {w}: file does not exist")
    if not os.path.isdir(a.workdir):
        problems.append(f"--workdir {a.workdir} is not a directory")
    if not a.display and not a.dry_run:
        problems.append("no X display: pass --display :0 (the runtime needs the real display)")
    if shutil.which("timeout") is None:
        problems.append("`timeout` not found (the planned commands use it)")
    if len(plan.get("conditions", [])) < 1:
        problems.append("plan has no conditions")
    problems += ["isolation audit: " + e for e in verify_run.audit_plan(plan)]
    scopes = detect_scope(a.unit, a.scope)
    if a.no_service:
        if scopes:
            problems.append(f"--no-service given but unit {a.unit} exists in scope {scopes}")
        scope = None
    else:
        if len(scopes) != 1:
            problems.append(f"unit {a.unit}: found in scopes {scopes}; need exactly one (use --scope system|user, or --no-service on a machine without the unit)")
        scope = scopes[0] if len(scopes) == 1 else None
    pre.update(unit_snapshot(scope, a.unit))
    flush_installed = "unknown"
    if "environment" in pre:
        flush_installed = "SET" if "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH" in pre["environment"] else "NOT SET"
    procs = runtime_processes()
    unit_active = pre.get("active") == "active"
    if procs and not (unit_active and a.stop_service):
        problems.append(f"experience_runtime already running (pids {procs}); only the production unit may be stopped by --stop-service")
    if unit_active and not a.stop_service:
        problems.append("production unit is ACTIVE; pass --stop-service to have the runner stop and later restore it")
    rec = {k: v for k, v in pre.items() if not k.startswith("_")}
    rec.update({"binary": binp, "binary_sha256": pre["_sha"], "scope": scope, "installed_DEFER_FLUSH": flush_installed, "installed_settings_file": pre["_inst"], "installed_settings_sha256": pre["_inst_sha"], "watched_files_before": pre["_watch"],
                "display": a.display, "workdir": a.workdir, "uname": sh("uname -a")[1], "python": sys.version.split()[0],
                "dmesg_tail": sh("dmesg 2>&1 | tail -20")[1], "problems": problems,
                "unit_dropins": sh(f"ls -l /etc/systemd/system/{a.unit}.service.d/ 2>&1")[1], "time": time.ctime()})
    with open(os.path.join(cdir, "preflight.json"), "w") as fh:
        json.dump(rec, fh, indent=2)
    with open(os.path.join(cdir, "service_before.txt"), "w") as fh:
        fh.write(json.dumps({k: v for k, v in pre.items() if not k.startswith("_")}, indent=2))
    say(f"pre-flight: binary sha256={pre['_sha']}  installed DEFER_FLUSH={flush_installed} (the campaign never changes it)  scope={scope}  unit_active={unit_active}")
    for p in problems:
        say("PRE-FLIGHT PROBLEM: " + p)
    if problems:
        set_state("REFUSED")
        say("not starting: nothing was stopped or run")
        sys.exit(1)

    runs = plan["runs"]
    est = len(runs) * (plan["duration_s"] + a.settle) / 60
    say(f"{len(runs)} runs x {plan['duration_s']} s, sequential, about {est:.0f} min; order: " + " ".join(r["tag"] for r in runs))
    if a.dry_run:
        for r in runs:
            say("WOULD RUN (cwd=%s DISPLAY=%s): %s" % (a.workdir, a.display or "<unset>", r["command"]))
        set_state("DRY_RUN")
        say("dry run: nothing started, nothing stopped")
        sys.exit(0)

    # ---------------------------------------------------------------- run
    def on_signal(signum, _frame):
        say(f"signal {signum}: aborting, restoring")
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGHUP, on_signal)  # a closed terminal must restore the service too
    status = {}
    abort = None
    try:
        if unit_active:
            cmd = sysctl(scope, "stop", a.unit)
            if scope == "system" and os.geteuid() != 0:
                cmd = ["sudo"] + cmd
            set_state("STOPPING_SERVICE")
            say("stopping the production unit: " + " ".join(cmd))
            rc, o = sh(cmd, timeout=120)
            state["we_stopped"] = True  # set before verifying: restoration must run even if the stop half-failed
            if rc != 0:
                raise RuntimeError(f"stop failed rc={rc}: {o}")
            time.sleep(3)
            if sh(sysctl(scope, "is-active", a.unit))[1] == "active" or runtime_processes():
                raise RuntimeError("unit still active or experience_runtime still running after stop")
        # Inherited EXPERIENCE_RUNTIME_* variables are scrubbed (the planned `env` line sets every variable a condition needs), so nothing the
        # operator's shell exported can reach a run. Their NAMES are recorded; values may be paths/secrets-free but are not needed.
        scrubbed = sorted(k for k in os.environ if k.startswith("EXPERIENCE_RUNTIME_"))
        env = {k: v for k, v in os.environ.items() if not k.startswith("EXPERIENCE_RUNTIME_")}
        env["DISPLAY"] = a.display
        say("scrubbed inherited variables: " + (", ".join(scrubbed) if scrubbed else "none"))
        dmesg0 = sh("dmesg 2>&1")[1].splitlines()
        for run in runs:
            tag = run["tag"]
            if runtime_processes():
                raise RuntimeError(f"before {tag}: an experience_runtime process exists")
            set_state("RUNNING:" + tag)
            say(f"=== {tag} ({run['condition']}, round {run['round']}) flushAfterFence expected={run['expect_flush_after_fence']}")
            t0 = time.time()
            child = subprocess.Popen(["bash", "-c", run["command"]], cwd=a.workdir, env=env, start_new_session=True)
            state["child"] = child
            mon = start_sysmon(child, os.path.join(out, tag + ".sys.csv"), say)
            try:
                rc = child.wait(plan["duration_s"] + a.grace)
            except subprocess.TimeoutExpired:
                kill_session(child)
                stop_sysmon(mon)
                raise RuntimeError(f"{tag}: hung beyond duration+{a.grace}s, killed")
            stop_sysmon(mon)
            state["child"] = None
            status[tag] = {"rc": rc, "seconds": round(time.time() - t0, 1)}
            if rc not in (124, 130):
                raise RuntimeError(f"{tag}: exit status {rc} (expected 124/130 = ended by the planned SIGINT)")
            set_state("CHECKING:" + tag)
            leftovers = runtime_processes()
            if leftovers:
                raise RuntimeError(f"{tag}: runtime still alive after the run (pids {leftovers})")
            for suffix in (".frames.csv", ".phase.log", ".stdout.log"):
                fp = os.path.join(out, tag + suffix)
                if not os.path.isfile(fp) or os.path.getsize(fp) == 0:
                    raise RuntimeError(f"{tag}: expected output {tag + suffix} is missing or empty (invalid evidence; not continuing)")
            for suffix in (".stdout.log", ".phase.log"):
                p = os.path.join(out, tag + suffix)
                txt = open(p, errors="replace").read() if os.path.isfile(p) else ""
                for pat in BAD_LOG:
                    if re.search(pat, txt):
                        raise RuntimeError(f"{tag}{suffix}: stop condition '{pat}'")
            fresh = [l for l in sh("dmesg 2>&1")[1].splitlines() if l not in set(dmesg0)]
            bad = [l for l in fresh if BAD_DMESG.search(l)]
            if bad:
                raise RuntimeError(f"{tag}: kernel throttling/OOM lines: {bad[:3]}")
            dmesg0 += fresh
            rcv, vout = sh([sys.executable, os.path.join(HERE, "verify_run.py"), a.plan, "--min-events", str(a.min_events)], timeout=300)
            status[tag]["verify_rc"] = rcv
            with open(os.path.join(cdir, "run_status.json"), "w") as fh:
                json.dump(status, fh, indent=2)
            if rcv != 0:
                say(vout[-1500:])
                raise RuntimeError(f"{tag}: verify_run.py failed on the runs so far")
            say(f"{tag}: ok ({status[tag]['seconds']} s, verify ok)")
            time.sleep(a.settle)
    except (RuntimeError, KeyboardInterrupt, OSError) as e:
        abort = f"{type(e).__name__}: {e}"
        say("STOP CONDITION: " + abort)
    finally:
        set_state("RESTORING")
        restored = restore(scope, a.unit, pre, cdir, False)

    with open(os.path.join(cdir, "run_status.json"), "w") as fh:
        json.dump({"runs": status, "abort": abort, "restored": restored}, fh, indent=2)
    if abort:
        set_state("ABORTED")
        say("campaign ABORTED; runs completed so far are kept for diagnosis; do not pool a partial campaign")
        sys.exit(2)

    # ---------------------------------------------------------------- validate + analyse (no production service interaction)
    set_state("ANALYSING")
    ok = True
    rc, o = sh([sys.executable, os.path.join(HERE, "verify_run.py"), a.plan, "--require-all", "--min-events", str(a.min_events)], timeout=600)
    open(os.path.join(adir, "verify.txt"), "w").write(o)
    ok &= rc == 0 and o.rstrip().endswith("VERIFIED")
    # OC-003-F: correctness plans (--kind correctness) must also pass the multi-scene generation checks; a failure is treated like a verify failure
    if plan.get("kind") == "correctness":
        rc, o = sh([sys.executable, os.path.join(HERE, "verify_generations.py"), a.plan, "--require-all"], timeout=600)
        open(os.path.join(adir, "generations.txt"), "w").write(o)
        ok &= rc == 0
        say("verify_generations.py: " + ("VERIFIED" if rc == 0 else "FAILED (see analysis/generations.txt)"))
    # OC-003-F: external RSS/thermal/frequency observation summaries (written only for runs that have a .sys.csv; blank values stay blank)
    try:
        import sysmon  # noqa: E402
        sysm = {}
        for run in runs:
            sp = os.path.join(out, run["tag"] + ".sys.csv")
            if os.path.isfile(sp):
                sysm[run["tag"]] = sysmon.summarize(sp)
        if sysm:
            with open(os.path.join(adir, "sysmon.json"), "w") as fh:
                json.dump(sysm, fh, indent=2)
    except Exception as e:  # noqa: BLE001
        say(f"sysmon summary skipped: {e}")
    rec_lines = []
    for csvp in sorted(glob.glob(os.path.join(out, "*.frames.csv"))):
        b = csvp[: -len(".frames.csv")]
        cond = next((r["condition"] for r in runs if os.path.basename(b) == r["tag"]), "?")
        # P0 has no sampler stat lines (sampling is off): reconcile its (all-zero) event totals against the phase log only
        extra_log = [] if cond == "P0" else [b + ".stdout.log"]
        rc, o = sh([sys.executable, os.path.join(HERE, "reconcile_events.py"), csvp, b + ".phase.log"] + extra_log, timeout=600)
        rec_lines.append(f"{os.path.basename(b)}: {o.splitlines()[-1] if o else 'no output'}")
        ok &= rc == 0
    open(os.path.join(adir, "reconcile.txt"), "w").write("\n".join(rec_lines) + "\n")
    groups = {}
    for run in runs:
        groups.setdefault(run["condition"], []).append(os.path.join(out, run["tag"] + ".frames.csv"))
    stdout_logs = [os.path.join(out, r["tag"] + ".stdout.log") for r in runs if r["condition"] != "P0"]
    garg = [f"{c}={','.join(v)}" for c, v in groups.items()]
    for name, extra in (("aggregate.json", ["--json"]), ("aggregate.txt", [x for l in stdout_logs for x in ("--sampler-log", l)])):
        rc, o = sh([sys.executable, os.path.join(HERE, "aggregate_frames.py")] + garg + ["--long-ms", "40"] + extra, timeout=900)
        open(os.path.join(adir, name), "w").write(o)
        ok &= rc == 0
    rc, o = sh([sys.executable, os.path.join(HERE, "sampler_stats.py")] + stdout_logs + ["--csv"], timeout=300)
    open(os.path.join(adir, "sampler.csv"), "w").write(o)
    ok &= rc == 0
    rc, o = sh([sys.executable, os.path.join(HERE, "summarize_campaign.py"), a.plan, "--out", adir], timeout=900)
    ok &= rc == 0
    if rc != 0:
        say("summarize_campaign.py failed: " + o[-400:])
    say("analysis written to " + adir)
    set_state("DONE" if (ok and restored) else "DONE_WITH_FAILURES")
    say("CAMPAIGN " + ("COMPLETE: verified, reconciled, restored" if (ok and restored) else "FINISHED WITH FAILURES: read analysis/*.txt and campaign/restoration.json"))
    sys.exit(0 if (ok and restored) else 3)


if __name__ == "__main__":
    main()
