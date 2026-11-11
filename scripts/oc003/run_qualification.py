#!/usr/bin/env python3
"""OC-003-F automated qualification runner (orchestration only; it adds no experiment logic of its own).

Runs the OC-003-F qualification as ordered, fail-reporting stages and writes <out>/qualification.json + qualification.md. Every stage is a
command this repository already provides; the runner records its exact command line, exit status, duration and the tail of its output.

  run_qualification.py --out DIR --bin BIN --workdir DIR --media-id ID [--scene S] [--resolution WxH] [--no-service | --stop-service ...]
                       [--stages unit,tooling,plan,timing,palette] [--timing-duration 100] [--timing-rounds 1] [--palette-duration 300]
                       [--min-events 40] [--display :0] [--dry-run]

Stages (default: all)
  unit     OF-free C++ tests: make -f sketches/experience_runtime/test/Makefile.tests oc003f raw-video scene-color
  tooling  python3 -I -W error scripts/oc003/test_diag_tooling.py   (planner / audit / verifier / summary fixtures)
  plan     plan P0..P5 (timing) and PCMP+P4 (palette) with plan_interleaved.py, then verify_run.audit_plan through run_campaign --dry-run
  timing   run_campaign.py on the timing plan (P0..P5; NO palette capture so analysis I/O never contaminates timings)
  palette  run_campaign.py on the palette plan, then palette_report.py on every palette directory (replay, determinism, CPU-vs-GPU)
--dry-run prints the stage commands and runs nothing (not even the unit tests).

Safety: it never stages/commits, never touches git, never deploys, and starts the runtime only through run_campaign.py, which refuses to start
when any experience_runtime process or the production unit is active (unless --stop-service, which is the operator's explicit choice on the
Pi). On the ROCK Pi 4B it is the SUPERVISED OPERATOR who runs this; the coding agent never does.
Exit 0 only if every selected stage passed.
"""
import argparse
import json
import os
import shlex
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
PY = [sys.executable, "-I"]
SKETCH = os.path.join(ROOT, "sketches", "experience_runtime")
TESTDIR = os.path.join(SKETCH, "test")


def run(name, cmd, cwd, results, dry, timeout=None):
    line = " ".join(shlex.quote(c) for c in cmd)
    if dry:
        print(f"[dry-run] {name}: (cd {cwd}) {line}")
        results.append({"stage": name, "command": line, "status": "DRY-RUN"})
        return True
    t0 = time.time()
    print(f"== {name}: {line}", flush=True)
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
        rc, out = p.returncode, (p.stdout + p.stderr)
    except subprocess.TimeoutExpired as e:
        rc, out = 124, f"TIMEOUT after {timeout}s\n{e.stdout or ''}"
    tail = "\n".join(out.strip().splitlines()[-25:])
    results.append({"stage": name, "command": line, "status": "PASS" if rc == 0 else "FAIL", "exit": rc,
                    "seconds": round(time.time() - t0, 1), "output_tail": tail})
    print(f"   -> {'PASS' if rc == 0 else 'FAIL'} (exit {rc}, {time.time() - t0:.1f}s)", flush=True)
    if rc != 0:
        print(tail, flush=True)
    return rc == 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True)
    ap.add_argument("--bin")
    ap.add_argument("--workdir", default=SKETCH)
    ap.add_argument("--media-id")
    ap.add_argument("--scene", default="blob-region-prototype")
    ap.add_argument("--resolution", default="1280x720")
    ap.add_argument("--stages", default="unit,tooling,plan,timing,palette")
    ap.add_argument("--timing-duration", type=int, default=100)
    ap.add_argument("--timing-rounds", type=int, default=1)
    ap.add_argument("--palette-duration", type=int, default=300, help="PCMP run length (s); 300 gave >100 same-frame CPU/GPU pairs on the desktop, 150 gave only a few")
    ap.add_argument("--min-events", type=int, default=40)
    ap.add_argument("--display", default=os.environ.get("DISPLAY", ":0"))
    ap.add_argument("--no-service", action="store_true", help="rehearsal machine with no production unit (desktop)")
    ap.add_argument("--stop-service", action="store_true", help="Pi: stop the production unit for the runs (restored by run_campaign.py)")
    ap.add_argument("--scope", default="auto")
    ap.add_argument("--unit", default="ecopunk-runtime")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    stages = [s for s in a.stages.split(",") if s]
    bad = [s for s in stages if s not in ("unit", "tooling", "plan", "timing", "palette")]
    if bad:
        sys.exit(f"unknown stage(s): {bad}")
    needs_bin = any(s in stages for s in ("plan", "timing", "palette"))
    if needs_bin and not (a.bin and a.media_id):
        sys.exit("--bin and --media-id are required for the plan/timing/palette stages")
    if needs_bin and not (a.no_service or a.stop_service):
        sys.exit("choose --no-service (desktop rehearsal) or --stop-service (Pi): the runner will not guess")
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    res = []
    ok = True
    svc = (["--no-service"] if a.no_service else ["--stop-service", "--scope", a.scope, "--unit", a.unit])
    camp = lambda plan, extra=(): PY + [os.path.join(HERE, "run_campaign.py"), plan, "--display", a.display, "--workdir", a.workdir,  # noqa: E731
                                         "--min-events", str(a.min_events)] + svc + list(extra)

    if "unit" in stages and ok:
        ok &= run("unit", ["make", "-f", "Makefile.tests", "oc003f", "raw-video", "scene-color"], TESTDIR, res, a.dry_run, 600)
    if "tooling" in stages and ok:
        ok &= run("tooling", PY + ["-W", "error", os.path.join(HERE, "test_diag_tooling.py")], HERE, res, a.dry_run, 600)

    timing_dir, palette_dir = os.path.join(out, "timing"), os.path.join(out, "palette")
    base = ["--bin", a.bin or "BIN", "--media-id", a.media_id or "MEDIA", "--scene", a.scene, "--resolution", a.resolution]
    if "plan" in stages and ok:
        ok &= run("plan.timing", PY + [os.path.join(HERE, "plan_interleaved.py"), "--out", timing_dir] + base +
                  ["--conditions", "P0,P1,P2,P3,P4,P5", "--order", "rotate", "--rounds", str(a.timing_rounds), "--duration", str(a.timing_duration)], HERE, res, a.dry_run)
        ok &= run("plan.palette", PY + [os.path.join(HERE, "plan_interleaved.py"), "--out", palette_dir] + base +
                  ["--conditions", "PCMP,P4", "--rounds", "1", "--duration", str(a.palette_duration), "--palette-diag", "--palette-grids", "600", "--palette-every", "0.5"],
                  HERE, res, a.dry_run)
        for nm, d in (("audit.timing", timing_dir), ("audit.palette", palette_dir)):
            ok &= run(nm, camp(os.path.join(d, "plan.json"), ["--dry-run"]), HERE, res, a.dry_run)
    if "timing" in stages and ok:
        ok &= run("timing", camp(os.path.join(timing_dir, "plan.json")), HERE, res, a.dry_run, a.timing_duration * 6 * a.timing_rounds + 1800)
    if "palette" in stages and ok:
        ok &= run("palette.campaign", camp(os.path.join(palette_dir, "plan.json")), HERE, res, a.dry_run, a.palette_duration * 2 + 900)
        replay = os.path.join(TESTDIR, "palette_replay")
        if not a.dry_run and os.path.isdir(palette_dir):
            for name in sorted(os.listdir(palette_dir)):
                d = os.path.join(palette_dir, name)
                if name.endswith(".palette") and os.path.isdir(d):
                    ok &= run("palette.report." + name, PY + [os.path.join(HERE, "palette_report.py"), d, "--replay", replay], HERE, res, False, 600)
        elif a.dry_run:
            run("palette.report.<each *.palette>", PY + [os.path.join(HERE, "palette_report.py"), "<dir>", "--replay", replay], HERE, res, True)

    summary = {"finished": time.ctime(), "ok": bool(ok), "stages": res, "out": out, "dry_run": a.dry_run}
    if not a.dry_run:
        with open(os.path.join(out, "qualification.json"), "w") as fh:
            json.dump(summary, fh, indent=2)
        L = ["# OC-003-F qualification run", "", f"finished: {summary['finished']}  overall: {'PASS' if ok else 'FAIL'}", "", "| stage | status | seconds |", "|---|---|---|"]
        L += [f"| {r['stage']} | {r['status']} | {r.get('seconds', '')} |" for r in res]
        with open(os.path.join(out, "qualification.md"), "w") as fh:
            fh.write("\n".join(L) + "\n")
    print(("QUALIFICATION PASS" if ok else "QUALIFICATION FAIL") + (" (dry run)" if a.dry_run else ""))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
