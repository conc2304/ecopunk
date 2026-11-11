#!/usr/bin/env python3
"""OC-003 three-condition interleaved experiment PLANNER. Prints commands; RUNS NOTHING.

Same binary / scene / media / resolution for every run; only the sampling environment differs. Order is
interleaved (P0 P1 P2 repeated). The startup media is PINNED with the verification control
EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID (+ EXPERIENCE_RUNTIME_VIDEO_DWELL so it never advances); --media-id is
REQUIRED. Run metadata (media id, expected scene, resolution, binary path + sha256, run list) is written to
<out>/plan.json and checked after the runs by verify_run.py.

  plan_interleaved.py --bin PATH --out DIR --media-id media.auto.29d9e437 --scene blob-region-prototype \
                      --resolution 1280x720 [--rounds 4] [--duration 300] [--order fixed|rotate]

OC-003-D stall-mechanism arms (diagnostic, opt-in): P2G = P2 + EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH=1 (presence-based in the runtime),
P2N = P2 + EXPERIENCE_RUNTIME_VSYNC=off (value-based: exactly "off"). The frame limiter stays ON and DEFER_FLUSH stays UNSET in every arm.
Typical: --conditions P2,P2G,P2N --order rotate --rounds 4 --duration 300

Find media ids: run the binary once with EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID=list; it rejects "list" and
prints the catalog (id + file) in the log.
"""
import argparse
import hashlib
import json
import os
import re
import shlex
import sys

COMMON = {
    "EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE": "1",
    "EXPERIENCE_RUNTIME_FRAME_PHASE_WINDOW_MS": "10000",
    "EXPERIENCE_RUNTIME_FRAME_PHASE_WARMUP_S": "30",
    "EXPERIENCE_RUNTIME_FRAME_PHASE_SETTLE_MS": "3000",
    # Pin the startup media for the whole run (verification overrides; selection stays with the shared policy).
    "EXPERIENCE_RUNTIME_VIDEO_DWELL": "100000,100000",
}
CONDITIONS = {
    "P0": {"EXPERIENCE_RUNTIME_SCENE_COLOR": "0"},
    "P1": {"EXPERIENCE_RUNTIME_SCENE_COLOR": "1", "EXPERIENCE_RUNTIME_SCENE_COLOR_MODE": "deferred",
           "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER": "fence", "EXPERIENCE_RUNTIME_SCENE_COLOR_STATS": "1"},
    "P2": {"EXPERIENCE_RUNTIME_SCENE_COLOR": "1", "EXPERIENCE_RUNTIME_SCENE_COLOR_MODE": "deferred",
           "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER": "fence", "EXPERIENCE_RUNTIME_SCENE_COLOR_STATS": "1",
           "EXPERIENCE_RUNTIME_PREDOMINANT_COLOR": "1",
           "EXPERIENCE_RUNTIME_VIDEO_COLOR": "1", "EXPERIENCE_RUNTIME_VIDEO_COLOR_MODE": "deferred",
           "EXPERIENCE_RUNTIME_VIDEO_COLOR_STATS": "1"},
}
# Every variable that selects colour sampling / its diagnostics. A diagnostic run UNSETS (env -u) each one the condition does
# not set itself, so nothing is inherited from a shell that exported the installation profile (docs/reviews/OC-003-personal-
# installation-profile.md). DEFER_FLUSH is presence-based in the runtime, so it can only be unset, never set to 0.
MANAGED = [
    "EXPERIENCE_RUNTIME_SCENE_COLOR", "EXPERIENCE_RUNTIME_SCENE_COLOR_MODE", "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER",
    "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH", "EXPERIENCE_RUNTIME_SCENE_COLOR_STATS", "EXPERIENCE_RUNTIME_SCENE_COLOR_READBACK",
    "EXPERIENCE_RUNTIME_SCENE_COLOR_PROBE", "EXPERIENCE_RUNTIME_SCENE_COLOR_FALLBACK", "EXPERIENCE_RUNTIME_SCENE_COLOR_FAULT",
    "EXPERIENCE_RUNTIME_SCENE_COLOR_SELFTEST", "EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM",
    "EXPERIENCE_RUNTIME_VIDEO_COLOR", "EXPERIENCE_RUNTIME_VIDEO_COLOR_MODE", "EXPERIENCE_RUNTIME_VIDEO_COLOR_STATS", "EXPERIENCE_RUNTIME_VIDEO_COLOR_DIAG",
    "EXPERIENCE_RUNTIME_PREDOMINANT_COLOR", "EXPERIENCE_RUNTIME_PREDOMINANT_HARNESS",
    # OC-003-F: CPU RAW + palette analysis controls (diagnostic, off unless a condition sets them)
    "EXPERIENCE_RUNTIME_CPU_RAW", "EXPERIENCE_RUNTIME_CPU_RAW_STATS",
    "EXPERIENCE_RUNTIME_PALETTE_DIAG", "EXPERIENCE_RUNTIME_PALETTE_DIAG_GRIDS", "EXPERIENCE_RUNTIME_PALETTE_DIAG_EVERY",
    "EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY", "EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE",
    # startup-scene selectors (OC-003-F platform-cost arm): exactly one may be set by --scene-env, all others explicitly unset
    "EXPERIENCE_RUNTIME_TEMPORAL_SCENE", "EXPERIENCE_RUNTIME_BLUEPRINT_SCENE", "EXPERIENCE_RUNTIME_FRAGMENT_SCENE", "EXPERIENCE_RUNTIME_QUADRANT_SCENE",
    "EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH", "EXPERIENCE_RUNTIME_FRAME_PHASE_CONTROL", "EXPERIENCE_RUNTIME_FRAME_PHASE_MS",
    "EXPERIENCE_RUNTIME_FRAME_LIMITER", "EXPERIENCE_RUNTIME_VSYNC",
]
# OC-003 experimental RGBA16F readback A/B. P2H = P2 + half-float transfer. P2V = P2 + half-verify (reads BOTH ways and
# counts bitwise differences: a CORRECTNESS pass, not a timing condition, because it does two readbacks per sample).
CONDITIONS["P2H"] = dict(CONDITIONS["P2"], EXPERIENCE_RUNTIME_SCENE_COLOR_READBACK="half")
CONDITIONS["P2V"] = dict(CONDITIONS["P2"], EXPERIENCE_RUNTIME_SCENE_COLOR_READBACK="half-verify")
# OC-003 sampling-frame attribution: P2 (DEFER_FLUSH deliberately UNSET: the runtime treats mere presence as "on") vs P2F = P2 + DEFER_FLUSH=1
# (the installed profile). Nothing else differs. Use with the event columns of the frame stream (ev, fx_issue, ...).
CONDITIONS["P2F"] = dict(CONDITIONS["P2"], EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH="1")
# OC-003-D stall-mechanism discrimination (diagnostic arms; each differs from P2 by exactly ONE variable):
#   P2G = P2 + FRAME_PHASE_GLFINISH=1   (presence-based in the runtime: a timed glFinish() after the app's draw, before the swap)
#   P2N = P2 + VSYNC=off                (value-based: ONLY the exact string "off" acts; 0/false/OFF are silently ignored)
# FRAME_LIMITER is never set (the limiter stays on) and DEFER_FLUSH stays unset in every arm.
CONDITIONS["P2G"] = dict(CONDITIONS["P2"], EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH="1")
CONDITIONS["P2N"] = dict(CONDITIONS["P2"], EXPERIENCE_RUNTIME_VSYNC="off")
SAMPLING_WITH_RAW = ("P2", "P2F", "P2H", "P2V", "P2G", "P2N", "P3")
# OC-003-F acquisition-isolation arms (diagnostic only; each is a distinct acquisition combination at 2 Hz, PREDOMINANT on everywhere
# sampling is on so analysis cost is included; the GPU FX / GPU RAW definitions are the UNCHANGED P1/P2 ones):
#   P3 = GPU RAW only            (FX off)              P4 = CPU RAW only (no GPU colour sampling at all)
#   P5 = CPU RAW + FX GPU        (FX on, GPU RAW off)
# P0 (nothing), P1 (FX GPU), P2 (FX GPU + GPU RAW) are as before. P3 is DEFINED here by OC-003-F as the missing symmetric arm; it was not
# specified in the authorisation text (recorded as an assumption in docs/reviews/OC-003-F-acquisition-qualification.md).
_FX = {"EXPERIENCE_RUNTIME_SCENE_COLOR": "1", "EXPERIENCE_RUNTIME_SCENE_COLOR_MODE": "deferred",
       "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER": "fence", "EXPERIENCE_RUNTIME_SCENE_COLOR_STATS": "1"}
_GPU_RAW = {"EXPERIENCE_RUNTIME_VIDEO_COLOR": "1", "EXPERIENCE_RUNTIME_VIDEO_COLOR_MODE": "deferred", "EXPERIENCE_RUNTIME_VIDEO_COLOR_STATS": "1"}
_CPU_RAW = {"EXPERIENCE_RUNTIME_CPU_RAW": "1", "EXPERIENCE_RUNTIME_CPU_RAW_STATS": "1"}
CONDITIONS["P3"] = dict({"EXPERIENCE_RUNTIME_SCENE_COLOR": "0", "EXPERIENCE_RUNTIME_PREDOMINANT_COLOR": "1"}, **_GPU_RAW)
CONDITIONS["P4"] = dict({"EXPERIENCE_RUNTIME_SCENE_COLOR": "0", "EXPERIENCE_RUNTIME_PREDOMINANT_COLOR": "1"}, **_CPU_RAW)
CONDITIONS["P5"] = dict(_FX, EXPERIENCE_RUNTIME_PREDOMINANT_COLOR="1", **_CPU_RAW)
# PCMP = GPU RAW + CPU RAW together, FX off: the output-comparison arm (CPU RAW vs GPU RAW on the SAME frames, scripts/oc003/palette_report.py).
# It is a correctness arm, NOT a timing arm: running both RAW paths together is not a production combination and must not be used for costs.
CONDITIONS["PCMP"] = dict(CONDITIONS["P3"], **_CPU_RAW)
SAMPLING_WITH_RAW = SAMPLING_WITH_RAW + ("PCMP",)
SAMPLING_WITH_CPU_RAW = ("P4", "P5", "PCMP")
FX_OFF_CONDITIONS = ("P0", "P3", "P4", "PCMP")
# Optional palette-analysis capture (--palette-diag): the SAME env for every run that has any acquisition path, so it is never a variable
# between conditions; palette files go to <out>/<tag>.palette/.



# The five production scenes in NextScene ring order (ExperienceRuntime::installProductionScenes): ids as the frame stream reports them.
FIVE_SCENES = ("blob-region-prototype", "temporal-fields", "blueprint_emergence", "fragment-trail", "quadrant-crosshair")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--media-id", required=True, help="canonical mediaId to pin as the startup media (REQUIRED)")
    ap.add_argument("--scene", required=True, help="expected active scene id (checked per run)")
    ap.add_argument("--resolution", required=True, help="expected window size WxH (checked per run)")
    ap.add_argument("--conditions", default="P0,P1,P2", help="comma list from P0,P1,P2,P2F,P2H,P2V (OC-003-F isolation arms: P3 GPU RAW only, P4 CPU RAW only, P5 CPU RAW + FX GPU) in run order, e.g. P2,P2H for the readback A/B, P0,P2,P2F for the sampling-frame attribution, P2,P2G,P2N for the OC-003-D stall-mechanism arms (also P1, P2V)")
    ap.add_argument("--defer-flush", action="store_true", help="also set EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH=1 on P1/P2/P2H/P2V to mirror the installation profile (default off = recorded baseline)")
    ap.add_argument("--installation-settings", help="installation-settings.json to copy as the campaign's settings (default: <bin dir>/data/installation-settings.json if it exists, else runtime defaults). "
                    "Its scene/media AUTO-CYCLES are switched off in the COPY; the installed file is only read, never written.")
    ap.add_argument("--order", choices=("fixed", "rotate"), default="fixed",
                    help="condition order inside every round: fixed (default, as before) or rotate = round r starts at condition ((r-1) mod n) "
                         "and wraps (deterministic; round 1 equals the listed order). The executed order is recorded in plan.json")
    ap.add_argument("--palette-diag", action="store_true", help="OC-003-F: also set EXPERIENCE_RUNTIME_PALETTE_DIAG=<out>/<tag>.palette on every run that has an acquisition path (P0 never; analysis cost is recorded in the frame CSV pal_us column). Off by default so timing arms stay clean")
    ap.add_argument("--palette-grids", type=int, default=48, help="with --palette-diag: bounded grid snapshots per stream (default 48; use 240 with --palette-every 0.5 for the CPU-vs-GPU comparison)")
    ap.add_argument("--palette-every", type=float, default=10.0, help="with --palette-diag: seconds between grid snapshots (default 10)")
    ap.add_argument("--scene-env", choices=("TEMPORAL", "BLUEPRINT", "FRAGMENT", "QUADRANT"),
                    help="OC-003-F: start in another production scene by setting EXPERIENCE_RUNTIME_<NAME>_SCENE=1 on EVERY run (same for all conditions). "
                         "Needed to measure the macOS first-access pixel conversion: blob-region-prototype already calls currentPixels() every frame, so there the "
                         "conversion is not CPU RAW's cost. --scene must then be that scene's id (it is checked per run)")
    ap.add_argument("--kind", choices=("campaign", "correctness"), default="campaign",
                    help="campaign (default, unchanged): scene and media pinned, steady-state timing comparison. correctness (OC-003-F five-scene workflow): the runtime's scene "
                         "auto-cycle is ON (--cycle-scenes N seconds), media stays pinned, and the run is judged by verify_run.py (scene SET, mode, events) + verify_generations.py "
                         "(generation isolation); it is NOT a timing comparison")
    ap.add_argument("--cycle-scenes", type=float, metavar="SECONDS",
                    help="with --kind correctness: sceneAutoCycle.durationSeconds in the campaign settings COPY (>= 10, the runtime's minimum). The run duration must cover "
                         "four transitions: --duration >= 4*SECONDS + 20")
    ap.add_argument("--rounds", type=int, default=4)
    ap.add_argument("--duration", type=int, default=300, help="seconds per run (incl. the excluded warm-up)")
    a = ap.parse_args()
    # The runner executes every command from the binary's working directory, so output paths MUST be absolute (a relative --out made every run fail).
    a.out = os.path.abspath(a.out)

    if not re.fullmatch(r"media\.[A-Za-z0-9_.\-]+", a.media_id) or a.media_id == "media.":
        sys.exit(f"--media-id '{a.media_id}' is not a canonical media id (expected like media.auto.29d9e437)")
    if not re.fullmatch(r"\d+x\d+", a.resolution):
        sys.exit("--resolution must be WxH, e.g. 1280x720")
    if not os.path.isfile(a.bin):
        sys.exit(f"--bin {a.bin} does not exist: the binary identity (sha256) is recorded at planning time")

    conds = [c for c in a.conditions.split(",") if c]
    unknown = [c for c in conds if c not in CONDITIONS]
    if unknown or not conds:
        sys.exit(f"--conditions: unknown {unknown}; choose from {sorted(CONDITIONS)}")
    if len(set(conds)) != len(conds):
        sys.exit(f"--conditions: duplicate condition in {conds}")
    if a.defer_flush and any(c in ("P2G", "P2N") for c in conds):
        sys.exit("--defer-flush is not allowed with P2G/P2N: DEFER_FLUSH must stay UNSET in every OC-003-D arm")
    if a.defer_flush and any(c in ("P3", "P4", "PCMP") for c in conds):
        sys.exit("--defer-flush has no meaning for FX-off conditions P3/P4 (it is an FX GPU-readback control); drop it")
    if a.defer_flush and "P2" in conds and "P2F" in conds:
        sys.exit("--defer-flush would turn P2 into P2F; drop it (P2F already sets DEFER_FLUSH=1, P2 must leave it unset)")
    if a.kind == "correctness":
        if a.cycle_scenes is None or a.cycle_scenes < 10.0:
            sys.exit("--kind correctness needs --cycle-scenes SECONDS >= 10 (the runtime's minimum scene cycle)")
        if a.duration < 4 * a.cycle_scenes + 20:
            sys.exit(f"--duration {a.duration} s cannot show five scenes with --cycle-scenes {a.cycle_scenes:g}: need >= {4 * a.cycle_scenes + 20:g} s (four transitions + warm-up)")
        if a.scene_env:
            sys.exit("--scene-env is a campaign option: a correctness run starts in the default scene and cycles through all five")
    elif a.cycle_scenes is not None:
        sys.exit("--cycle-scenes only applies to --kind correctness (campaign runs pin the scene)")
    # In a normal (non-harness) run the runtime loads installation-settings.json and its mediaAutoCycle / sceneAutoCycle OVERRIDE the
    # pinned startup media + EXPERIENCE_RUNTIME_VIDEO_DWELL (observed: the pinned media was left after exactly the configured cycle time).
    # Every run therefore gets a private COPY of the settings with both auto-cycles disabled via the runtime's own explicit-path override;
    # all other keys (look, orbit observations, ...) are kept so the workload equals the installed one and is identical in every condition.
    src = a.installation_settings or os.path.join(os.path.dirname(os.path.abspath(a.bin)), "data", "installation-settings.json")
    inst = {}
    src_sha = None
    if os.path.isfile(src):
        src_sha = sha256(src)
        with open(src) as fh:
            inst = json.load(fh)
    elif a.installation_settings:
        sys.exit(f"--installation-settings {src} does not exist")
    for key in ("sceneAutoCycle", "mediaAutoCycle"):
        inst[key] = dict(inst.get(key) or {"durationSeconds": 300.0}, enabled=False)
    if a.kind == "correctness":  # the ONLY difference from a campaign copy: the scene cycle is on (media stays pinned/off)
        inst["sceneAutoCycle"] = dict(inst["sceneAutoCycle"], enabled=True, durationSeconds=a.cycle_scenes)
    os.makedirs(a.out, exist_ok=True)
    settings_out = os.path.abspath(os.path.join(a.out, "campaign-installation-settings.json"))
    with open(settings_out, "w") as fh:
        json.dump(inst, fh, indent=4, sort_keys=True)
    runs = []
    n = 0
    round_orders = {}
    for r in range(1, a.rounds + 1):
        off = ((r - 1) % len(conds)) if a.order == "rotate" else 0
        order = conds[off:] + conds[:off]
        round_orders[str(r)] = order
        for pos, cond in enumerate(order, 1):
            n += 1
            tag = f"{n:02d}_{cond}_r{r}"
            env = dict(COMMON, **CONDITIONS[cond])
            if a.defer_flush and cond not in FX_OFF_CONDITIONS:
                env["EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH"] = "1"
            if a.palette_diag and cond != "P0":
                env["EXPERIENCE_RUNTIME_PALETTE_DIAG"] = f"{a.out}/{tag}.palette"
                env["EXPERIENCE_RUNTIME_PALETTE_DIAG_GRIDS"] = str(a.palette_grids)
                env["EXPERIENCE_RUNTIME_PALETTE_DIAG_EVERY"] = str(a.palette_every)
            env["EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID"] = a.media_id
            if a.scene_env:
                env[f"EXPERIENCE_RUNTIME_{a.scene_env}_SCENE"] = "1"
            env["EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS"] = settings_out
            env["EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES"] = f"{a.out}/{tag}.frames.csv"
            env["EXPERIENCE_RUNTIME_FRAME_PHASE_LOG"] = f"{a.out}/{tag}.phase.log"
            unsets = " ".join(f"-u {k}" for k in MANAGED if k not in env)
            envs = (unsets + " " if unsets else "") + " ".join(f"{k}={shlex.quote(v)}" for k, v in env.items())
            cmd = (f"env {envs} timeout -s INT {a.duration} {shlex.quote(a.bin)} "
                   f"> {shlex.quote(a.out + '/' + tag + '.stdout.log')} 2>&1")
            flush = None if env.get("EXPERIENCE_RUNTIME_SCENE_COLOR") == "0" else ("yes" if "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH" in env else "no")
            runs.append({"tag": tag, "condition": cond, "round": r, "position": pos, "command": cmd, "expect_flush_after_fence": flush,
                         "expect_fx_events": cond not in FX_OFF_CONDITIONS, "expect_raw_events": cond in SAMPLING_WITH_RAW,
                         # OC-003-F: acquisition path expected per run (verify_run.py checks the runtime's AcquisitionMode line and the event columns)
                         "expect_fx_gpu": env.get("EXPERIENCE_RUNTIME_SCENE_COLOR") == "1", "expect_raw_gpu": cond in SAMPLING_WITH_RAW,
                         "expect_cpu_raw": cond in SAMPLING_WITH_CPU_RAW, "expect_palette": "EXPERIENCE_RUNTIME_PALETTE_DIAG" in env,
                         # what the probe's CONFIG line must report for this run (verify_run.py checks it when present)
                         "expect_glfinish": "on" if "EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH" in env else "off",
                         "expect_vsync": "off" if env.get("EXPERIENCE_RUNTIME_VSYNC") == "off" else "on",
                         "expect_limiter": "off" if env.get("EXPERIENCE_RUNTIME_FRAME_LIMITER") == "off" else "on"})

    os.makedirs(a.out, exist_ok=True)
    meta = {
        "media_id": a.media_id, "scene": a.scene, "resolution": a.resolution,
        "binary": os.path.abspath(a.bin), "binary_sha256": sha256(a.bin), "binary_size": os.path.getsize(a.bin),
        "installation_settings_source": src if src_sha else None, "installation_settings_source_sha256": src_sha,
        "installation_settings_used": settings_out, "installation_settings_used_sha256": sha256(settings_out),
        "duration_s": a.duration, "rounds": a.rounds, "conditions": conds, "order": a.order, "round_orders": round_orders,
        "defer_flush": a.defer_flush, "runs": runs,
        # OC-003-F five-scene correctness workflow (absent/"campaign" for every earlier plan; verify_run.py treats a missing key as a campaign)
        "kind": a.kind, "scene_cycle_seconds": a.cycle_scenes,
    }
    if a.kind == "correctness":
        meta["expected_scenes"] = sorted(FIVE_SCENES)
        meta["min_events"] = 20  # the steady state is shorter (every scene change excludes its settle window); per-run event floor for verify_run.py
    with open(os.path.join(a.out, "plan.json"), "w") as fh:
        json.dump(meta, fh, indent=2)

    print(f"# media={a.media_id} scene={a.scene} resolution={a.resolution}\n# binary={meta['binary']} sha256={meta['binary_sha256']}")
    for run in runs:
        print(f"# run {run['tag']}")
        print(run["command"])
    print(f"\n# after the runs:\n#   python3 scripts/oc003/verify_run.py {a.out}/plan.json      (must exit 0 before aggregating)\n"
          f"#   python3 scripts/oc003/aggregate_frames.py P0=<csvs> P1=<csvs> P2=<csvs> --sampler-log <stdout logs>")


if __name__ == "__main__":
    main()
