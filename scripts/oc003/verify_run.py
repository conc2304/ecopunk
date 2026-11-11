#!/usr/bin/env python3
"""OC-003 per-run identity verification. Reads <out>/plan.json (from plan_interleaved.py) and, for every run whose
files exist, checks that the run really measured the planned configuration. Exit 0 only if every run found passes
and at least one run was found; missing runs are reported (and fail with --require-all).

Checks per run:
  media       stdout log has 'startup media ACTIVE id=<planned>' and NO 'REJECTED'; every steady-state CSV row
              (exclude=0) has media == planned id
  scene       every steady-state CSV row has scene == planned scene
  resolution  the phase log CONFIG line has window=<planned WxH>
  binary      sha256 of the planned binary path still equals the sha256 recorded at planning time
  flush       (sampling runs) every 'flushAfterFence=' in the stdout log equals the planned DEFER_FLUSH state (P2 no, P2F yes)
  events      (event-column streams) P0 has ZERO sampler events; sampling runs have >= --min-events steady-state successful
              issue and collect events per expected source, and no failed attempts (fail counts are reported, any failure fails)
  offset      records the activation start offset (VideoResume 'start=' value) and the media size/duration
Across runs: media size/duration/start offset must be identical in every run.
"""
import argparse
import csv
import hashlib
import json
import os
import re
import sys


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_command(cmd):
    """(set, unset, rest) of a planner line `env [-u K]... [K=V]... timeout -s INT N BIN > log 2>&1`."""
    import shlex
    toks = shlex.split(cmd)
    if not toks or toks[0] != "env":
        raise ValueError("command does not start with `env`")
    i, setv, unset = 1, {}, set()
    while i < len(toks):
        if toks[i] == "-u":
            unset.add(toks[i + 1])
            i += 2
        elif "=" in toks[i] and not toks[i].startswith("/"):
            k, v = toks[i].split("=", 1)
            setv[k] = v
            i += 1
        else:
            break
    return setv, unset, toks[i:]


PER_RUN_KEYS = ("EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES", "EXPERIENCE_RUNTIME_FRAME_PHASE_LOG", "EXPERIENCE_RUNTIME_PALETTE_DIAG")
# Planner-added variables that every run sets (besides COMMON, its condition's own variables and the two per-run output paths).
PLANNER_RUN_KEYS = ("EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID", "EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS",
                    "EXPERIENCE_RUNTIME_PALETTE_DIAG_GRIDS", "EXPERIENCE_RUNTIME_PALETTE_DIAG_EVERY",
                    "EXPERIENCE_RUNTIME_TEMPORAL_SCENE", "EXPERIENCE_RUNTIME_BLUEPRINT_SCENE", "EXPERIENCE_RUNTIME_FRAGMENT_SCENE", "EXPERIENCE_RUNTIME_QUADRANT_SCENE")
DEFER_FLUSH_KEY = "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH"
# OC-003-D diagnostic controls. GLFINISH is presence-based in the runtime (so it is set to exactly "1" or left unset); VSYNC acts only on the
# exact value "off"; the frame limiter is never touched (stays on); the present-only CONTROL is never used in a campaign.
CONTROL_VARS = ("EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH", "EXPERIENCE_RUNTIME_VSYNC", "EXPERIENCE_RUNTIME_FRAME_LIMITER", "EXPERIENCE_RUNTIME_FRAME_PHASE_CONTROL")
CONTROL_EXPECT = {"P2G": {"EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH": "1"}, "P2N": {"EXPERIENCE_RUNTIME_VSYNC": "off"}}
# Exact set of variables that may differ between two conditions (in addition to the derived check against the planner's CONDITIONS table).
EXACT_PAIR_DIFFS = {("P2", "P2G"): {"EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH"}, ("P2", "P2N"): {"EXPERIENCE_RUNTIME_VSYNC"},
                    ("P2", "P2F"): {DEFER_FLUSH_KEY}}


def audit_plan(plan):
    """Fail-closed static audit of the CONDITIONS: every run of a condition has an identical environment (except its two output paths),
    every managed variable is either set or explicitly unset, DEFER_FLUSH is presence-based (unset or =1, never 0), the binary and
    duration are identical everywhere, the campaign settings copy has both auto-cycles off and is unchanged, and the sets of keys that
    differ between conditions are exactly the intended ones (P0->P2: sampling switches; P2->P2F: DEFER_FLUSH only)."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import plan_interleaved as pi  # noqa: E402
    errs = []
    envs = {}
    for run in plan["runs"]:
        try:
            setv, unset, rest = parse_command(run["command"])
        except (ValueError, IndexError) as e:
            errs.append(f"{run['tag']}: unparsable command ({e})")
            continue
        key = {k: v for k, v in setv.items() if k not in PER_RUN_KEYS}
        sig = (tuple(sorted(key.items())), tuple(sorted(unset)), tuple(rest[:5]))
        envs.setdefault(run["condition"], {}).setdefault(sig, []).append(run["tag"])
        for k in pi.MANAGED:
            if k not in setv and k not in unset:
                errs.append(f"{run['tag']}: managed variable {k} is neither set nor unset (inherited environment could leak)")
            if k in setv and k in unset:
                errs.append(f"{run['tag']}: {k} both set and unset")
        if setv.get("EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH") == "0":
            errs.append(f"{run['tag']}: DEFER_FLUSH=0 is presence-based ON in the runtime; disabled means unset")
        cond = run["condition"]
        if cond not in pi.CONDITIONS:
            errs.append(f"{run['tag']}: condition {cond!r} is not known to the planner")
        want_ctl = CONTROL_EXPECT.get(cond, {})
        for var in CONTROL_VARS:
            want = want_ctl.get(var)
            if want is None and var in setv:
                errs.append(f"{run['tag']}: {var}={setv[var]!r} must be unset in condition {cond}")
            elif want is not None and setv.get(var) != want:
                errs.append(f"{run['tag']}: {var} must be exactly {want!r} in condition {cond} (got {setv.get(var)!r})")
        if cond in ("P2", "P2G", "P2N") and not plan.get("defer_flush") and DEFER_FLUSH_KEY in setv:
            errs.append(f"{run['tag']}: DEFER_FLUSH must stay UNSET in condition {cond}")
        allowed = set(pi.COMMON) | set(pi.CONDITIONS.get(cond, {})) | set(PER_RUN_KEYS) | set(PLANNER_RUN_KEYS) | ({DEFER_FLUSH_KEY} if plan.get("defer_flush") and cond not in pi.FX_OFF_CONDITIONS else set())
        for k in setv:
            if k.startswith("EXPERIENCE_RUNTIME_") and k not in allowed:
                errs.append(f"{run['tag']}: unexpected variable {k}={setv[k]!r} (not part of condition {cond})")
        if rest[:4] != ["timeout", "-s", "INT", str(plan["duration_s"])] or rest[4] != plan["binary"]:
            errs.append(f"{run['tag']}: not `timeout -s INT {plan['duration_s']} {plan['binary']}` (got {rest[:5]})")
        if setv.get("EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS") != plan.get("installation_settings_used"):
            errs.append(f"{run['tag']}: EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS is not the plan's campaign settings copy")
        if setv.get("EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID") != plan["media_id"]:
            errs.append(f"{run['tag']}: startup media is not the planned media id")
    for cond, sigs in envs.items():
        if len(sigs) != 1:
            errs.append(f"condition {cond}: runs do not share one environment: {[v for v in sigs.values()]}")
    flat = {c: dict(next(iter(s))[0]) for c, s in envs.items() if len(s) >= 1}
    for a, b in (("P0", "P2"), ("P2", "P2F"), ("P2", "P2G"), ("P2", "P2N"), ("P0", "P3"), ("P0", "P4"), ("P4", "P5"), ("P0", "P5"), ("P1", "P5")):
        if a in flat and b in flat:
            # PALETTE_DIAG* are planner-level capture controls (absent on P0 by design, checked against expect_palette elsewhere)
            got = {k for k in set(flat[a]) | set(flat[b]) if flat[a].get(k) != flat[b].get(k) and not k.startswith("EXPERIENCE_RUNTIME_PALETTE_DIAG")}
            want = {k for k in set(pi.CONDITIONS[a]) | set(pi.CONDITIONS[b]) if pi.CONDITIONS[a].get(k) != pi.CONDITIONS[b].get(k)}
            if got != want:
                errs.append(f"{a} vs {b}: differing variables {sorted(got)} != intended {sorted(want)}")
            if (a, b) in EXACT_PAIR_DIFFS and got != EXACT_PAIR_DIFFS[(a, b)]:
                errs.append(f"{a} vs {b}: must differ by exactly {sorted(EXACT_PAIR_DIFFS[(a, b)])}, differs by {sorted(got)}")
    if "P2" in flat and "P2F" in flat:
        if "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH" in flat["P2"] or flat["P2F"].get("EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH") != "1":
            errs.append("P2 must leave DEFER_FLUSH unset and P2F must set it to 1")
    used = plan.get("installation_settings_used")
    if not used or not os.path.isfile(used):
        errs.append(f"campaign installation settings copy missing: {used}")
    else:
        if sha256(used) != plan.get("installation_settings_used_sha256"):
            errs.append("campaign installation settings copy changed since planning (sha256 differs)")
        try:
            with open(used) as fh:
                doc = json.load(fh)
            correctness = plan.get("kind") == "correctness"
            for k in ("sceneAutoCycle", "mediaAutoCycle"):
                want = True if (correctness and k == "sceneAutoCycle") else False
                if (doc.get(k) or {}).get("enabled") is not want:
                    errs.append(f"campaign settings copy: {k}.enabled is not {str(want).lower()}")
            if correctness:
                if (doc.get("sceneAutoCycle") or {}).get("durationSeconds") != plan.get("scene_cycle_seconds"):
                    errs.append(f"correctness settings copy: sceneAutoCycle.durationSeconds {(doc.get('sceneAutoCycle') or {}).get('durationSeconds')} != plan {plan.get('scene_cycle_seconds')}")
                if len(plan.get("expected_scenes") or []) < 5:
                    errs.append("correctness plan lacks the five expected scene ids")
        except ValueError as e:
            errs.append(f"campaign settings copy unreadable: {e}")
    return errs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("plan")
    ap.add_argument("--require-all", action="store_true")
    ap.add_argument("--min-events", type=int, default=100, help="steady-state FX/RAW issue and collect events required per sampling run (default 100; a correctness plan carries its own lower floor)")
    a = ap.parse_args()
    with open(a.plan) as fh:
        plan = json.load(fh)
    correctness = plan.get("kind") == "correctness"  # OC-003-F five-scene workflow; every earlier plan is a campaign
    if correctness:
        a.min_events = plan.get("min_events", a.min_events)
    out = os.path.dirname(os.path.abspath(a.plan))
    failures = []
    found = 0
    seen = {}

    for e in audit_plan(plan):
        failures.append("isolation: " + e)

    cur = sha256(plan["binary"]) if os.path.isfile(plan["binary"]) else None
    if cur != plan["binary_sha256"]:
        failures.append(f"binary: {plan['binary']} sha256 is {cur}, planned {plan['binary_sha256']}")

    for run in plan["runs"]:
        tag = run["tag"]
        csv_path, log_path, out_path = (os.path.join(out, f"{tag}{s}") for s in (".frames.csv", ".phase.log", ".stdout.log"))
        if not (os.path.isfile(csv_path) and os.path.isfile(out_path)):
            msg = f"{tag}: missing files (run not executed?)"
            print("MISSING", msg)
            if a.require_all:
                failures.append(msg)
            continue
        found += 1
        errs = []
        with open(out_path, errors="replace") as fh:
            text = fh.read()
        if "startup media REJECTED" in text:
            errs.append("media: startup media REJECTED in stdout log")
        m = re.search(r"startup media ACTIVE id=(\S+) file=(\S+) size=(\S+) duration=(\S+)", text)
        if not m or m.group(1) != plan["media_id"]:
            errs.append(f"media: no 'startup media ACTIVE id={plan['media_id']}' line (got {m.group(1) if m else None})")
        want_scene_cycle = "on" if correctness else "off"
        if f"Installation: applied: sceneAutoCycle {want_scene_cycle}" not in text or "mediaAutoCycle off" not in text:
            errs.append(f"autocycle: stdout log lacks 'Installation: applied: sceneAutoCycle {want_scene_cycle} ... mediaAutoCycle off' "
                        "(installation auto-cycles would move the media/scene unplanned; plan_interleaved.py sets EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS)")
        start = re.search(r"VideoResume\S*:? start media=(\S+).*? start=(\S+)", text)
        info = (m.group(2, 3, 4) if m else None, start.group(2) if start else None)
        seen[tag] = info
        media, scene, rows = set(), set(), 0
        with open(csv_path, newline="") as fh:
            for row in csv.DictReader(fh):
                if row["exclude"] == "1":
                    continue
                rows += 1
                media.add(row["media"])
                scene.add(row["scene"])
        if rows == 0:
            errs.append("csv: no steady-state rows")
        if media != {plan["media_id"]}:
            errs.append(f"media: steady-state CSV media {sorted(media)} != {plan['media_id']}")
        if correctness:
            want_scenes = set(plan.get("expected_scenes") or [])
            if not scene <= want_scenes:
                errs.append(f"scene: steady-state CSV scenes {sorted(scene - want_scenes)} are not production scenes")
            if len(scene & want_scenes) < len(want_scenes):
                errs.append(f"scene: only {len(scene & want_scenes)} of {len(want_scenes)} production scenes have steady-state frames {sorted(scene)} "
                            f"(scene cycle {plan.get('scene_cycle_seconds')} s, duration {plan.get('duration_s')} s)")
        elif scene != {plan["scene"]}:
            errs.append(f"scene: steady-state CSV scene {sorted(scene)} != {plan['scene']}")
        want_flush = run.get("expect_flush_after_fence")
        if want_flush is not None:
            got = set(re.findall(r"flushAfterFence=(\w+)", text))
            if got != {want_flush}:
                errs.append(f"flush: flushAfterFence={sorted(got)} != planned {want_flush}")
        with open(csv_path, newline="") as fh:
            rd = csv.DictReader(fh)
            have_ev = "ev" in (rd.fieldnames or [])
            tot = {k: 0 for k in ("fx_issue", "fx_collect", "raw_issue", "raw_collect", "fx_ifail", "fx_cfail", "raw_ifail", "raw_cfail", "cpu_raw", "cpu_raw_gap")}
            have_cpu = "cpu_raw" in (rd.fieldnames or [])
            for row in rd:
                if row["exclude"] == "1" or not have_ev:
                    continue
                for k in tot:
                    if k.startswith("cpu_raw") and not have_cpu:
                        continue
                    tot[k] += int(row[k])
        if not have_ev:
            errs.append("events: the frame stream has no event columns (binary predates the attribution probe)")
        else:
            want_fx = run.get("expect_fx_events", True)
            want_raw = run.get("expect_raw_events", False)
            want_cpu = run.get("expect_cpu_raw", False)
            if not want_fx and not want_raw and not want_cpu:
                if any(tot[k] for k in tot):
                    errs.append(f"events: all acquisition disabled (P0) but events recorded {tot}")
            else:
                need = [("fx_issue", want_fx), ("fx_collect", want_fx), ("raw_issue", want_raw), ("raw_collect", want_raw), ("cpu_raw", want_cpu)]
                for k, req in need:
                    if req and tot[k] < a.min_events:
                        errs.append(f"events: {k}={tot[k]} < --min-events {a.min_events} (insufficient events)")
                if not want_fx and (tot["fx_issue"] or tot["fx_collect"]):
                    errs.append(f"events: FX events recorded in a condition without FX GPU sampling {tot}")
                if not want_raw and (tot["raw_issue"] or tot["raw_collect"]):
                    errs.append(f"events: RAW events recorded in a condition without RAW sampling {tot}")
                if not want_cpu and (tot["cpu_raw"] or tot["cpu_raw_gap"]):
                    errs.append(f"events: CPU RAW samples recorded in a condition without CPU RAW {tot}")
                if want_cpu and not have_cpu:
                    errs.append("events: CPU RAW expected but the frame stream has no cpu_raw column (binary predates OC-003-F)")
                if want_cpu and tot["cpu_raw_gap"] > max(2, tot["cpu_raw"] // 20):
                    errs.append(f"events: CPU RAW gaps {tot['cpu_raw_gap']} vs samples {tot['cpu_raw']} (more than 5% gaps in the steady state)")
                if any(tot[k] for k in ("fx_ifail", "fx_cfail", "raw_ifail", "raw_cfail")):
                    errs.append(f"events: failed sampling attempts in the steady state {tot}")
        # OC-003-F: the runtime states which acquisition paths it really has; it must equal the plan (plans without expect_* skip this).
        if "expect_fx_gpu" in run:
            line = next((l for l in text.splitlines() if "AcquisitionMode" in l and "fxGpu=" in l), "")
            m = re.search(r"fxGpu=(\w+) rawGpu=(\w+) cpuRaw=(\w+) palette=(\w+)", line)
            if not m:
                errs.append("acquisition: no AcquisitionMode line in the log (binary predates OC-003-F)")
            else:
                yn = lambda b: "on" if b else "off"
                want = (yn(run["expect_fx_gpu"]), yn(run["expect_raw_gpu"]), yn(run["expect_cpu_raw"]), yn(run.get("expect_palette", False)))
                if m.groups() != want:
                    errs.append(f"acquisition: runtime reports fxGpu/rawGpu/cpuRaw/palette={m.groups()} != planned {want}")
        cfg = ""
        if os.path.isfile(log_path):
            with open(log_path, errors="replace") as fh:
                cfg = next((l for l in fh if l.startswith("CONFIG")), "")
        w = re.search(r"window=(\d+x\d+)", cfg)
        if not w or w.group(1) != plan["resolution"]:
            errs.append(f"resolution: window={w.group(1) if w else None} != {plan['resolution']}")
        # OC-003-D: what the probe reports it APPLIED must equal the plan (plans from before OC-003-D carry no expect_* keys and skip this).
        # The CONFIG line states the REQUEST the probe honoured, not the driver's actual swap interval.
        for key, field in (("expect_glfinish", "glFinish"), ("expect_vsync", "vsync"), ("expect_limiter", "limiter")):
            want = run.get(key)
            if want is not None:
                got = re.search(r"\b" + field + r"=(\w+)", cfg)
                if not got or got.group(1) != want:
                    errs.append(f"control: phase log CONFIG {field}={got.group(1) if got else None} != planned {want}")
        print(("FAIL " if errs else "OK   ") + f"{tag} rows={rows} media_file/size/duration={info[0]} start_offset={info[1]}")
        failures += [f"{tag}: {e}" for e in errs]

    if len({v for v in seen.values()}) > 1:
        failures.append(f"cross-run: media file/size/duration/start offset differ between runs: {sorted(set(map(str, seen.values())))}")
    if found == 0:
        failures.append("no runs found")
    for f in failures:
        print("FAILURE:", f)
    print("VERIFIED" if not failures else "NOT VERIFIED")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
