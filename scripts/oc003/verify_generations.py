#!/usr/bin/env python3
"""OC-003-F five-scene correctness / generation-isolation verifier (read-only; works on the files a run already wrote).

  verify_generations.py <plan.json | run-dir> [--tags TAG,TAG] [--min-scenes 5] [--require-all]

A correctness run lets the runtime cycle through its production scenes (scene auto-cycle on, media pinned) with the acquisition paths of one
mode active, then checks that generations and observations were never mixed:

  scenes        the frame stream (<tag>.frames.csv, ALL rows incl. excluded ones) shows >= --min-scenes distinct scene ids and >= min-scenes-1 changes
  mode          the stdout log's `AcquisitionMode: fxGpu= rawGpu= cpuRaw= palette=` line equals what the plan expects for the run
                (expect_fx_events / expect_raw_events / expect_cpu_raw / expect_palette; keys the plan does not carry are not checked)
  generations   every `generation N begins` line sequence of each stream (FX `SceneColor:`, GPU RAW `VideoColor:`, CPU RAW `CpuRaw:`) is strictly
                increasing; each RAW / CPU RAW stream begins a generation for EVERY scene generation the runtime logged (scene_gen coverage), i.e. a
                scene change can never leave a stream on its old generation
  palette       (runs with <tag>.palette/) per stream: generation never decreases; the FIRST row of every generation has driftHue = nan
                (temporal drift never crosses a generation); rows are time-ordered; every row's generation was announced by a `begins` line
  samples       the stdout stat lines show gaps/droppedGen/invalid counters but no negative or decreasing cumulative counters
Exit 0 only if every selected run passes (and, with --require-all, every run of the plan was found).
"""
import argparse
import csv
import json
import os
import re
import sys

FIVE = {"blob-region-prototype", "temporal-fields", "blueprint_emergence", "fragment-trail", "quadrant-crosshair"}
FX_RE = re.compile(r"SceneColor: generation (\d+) begins: scene=(\S+)")
RAW_RE = re.compile(r"VideoColor: generation (\d+) begins: media=(\S+) activation=(\d+) scene_gen=(\d+)")
CPU_RE = re.compile(r"CpuRaw: generation (\d+) begins: media=(\S+) activation=(\d+) scene_gen=(\d+)")
MODE_RE = re.compile(r"AcquisitionMode: fxGpu=(\w+) rawGpu=(\w+) cpuRaw=(\w+) palette=(\w+)")
STREAM_FILES = {"fx": "palette_fx.csv", "raw_gpu": "palette_raw_gpu.csv", "raw_cpu": "palette_raw_cpu.csv"}


def _strictly_increasing(v):
    return all(b > a for a, b in zip(v, v[1:]))


def check_run(tag, base, run=None, min_scenes=5):
    """Returns (errors, facts) for one run whose files live at <base>/<tag>.*"""
    errs, facts = [], {}
    log = os.path.join(base, tag + ".stdout.log")
    frames = os.path.join(base, tag + ".frames.csv")
    pal = os.path.join(base, tag + ".palette")
    if not os.path.isfile(log) or not os.path.isfile(frames):
        return [f"{tag}: missing stdout log or frame stream"], facts
    text = open(log, errors="replace").read()

    scenes, changes, prev = [], 0, None
    with open(frames, newline="") as fh:
        for row in csv.DictReader(fh):
            s = row.get("scene", "-")
            if s != prev:
                if prev is not None:
                    changes += 1
                prev = s
            if s not in scenes:
                scenes.append(s)
    real = [s for s in scenes if s != "-"]
    facts["scenes"], facts["scene_changes"] = real, changes
    if len(set(real)) < min_scenes:
        errs.append(f"scenes: only {len(set(real))} distinct scene id(s) {real}; need >= {min_scenes} (scene auto-cycle off, or run too short)")
    unknown = [s for s in real if s not in FIVE]
    if unknown:
        errs.append(f"scenes: unexpected scene ids {unknown}")

    m = MODE_RE.findall(text)
    if not m:
        errs.append("mode: no 'AcquisitionMode:' line in the stdout log (binary lacks the OC-003-F contract)")
    else:
        if len(set(m)) != 1:
            errs.append(f"mode: AcquisitionMode lines differ within the run: {sorted(set(m))}")
        fx, raw, cpu, pl = m[0]
        facts["mode"] = {"fxGpu": fx, "rawGpu": raw, "cpuRaw": cpu, "palette": pl}
        if run:
            want = {"fxGpu": run.get("expect_fx_events"), "rawGpu": run.get("expect_raw_events"), "cpuRaw": run.get("expect_cpu_raw"), "palette": run.get("expect_palette")}
            for k, v in want.items():
                if v is not None and facts["mode"][k] != ("on" if v else "off"):
                    errs.append(f"mode: {k}={facts['mode'][k]} but the plan expects {'on' if v else 'off'}")

    fx = [(int(g), s) for g, s in FX_RE.findall(text)]
    raw = [(int(g), med, int(a), int(sg)) for g, med, a, sg in RAW_RE.findall(text)]
    cpu = [(int(g), med, int(a), int(sg)) for g, med, a, sg in CPU_RE.findall(text)]
    facts["generations"] = {"fx": len(fx), "raw_gpu": len(raw), "raw_cpu": len(cpu)}
    for name, seq in (("fx", [g for g, _ in fx]), ("raw_gpu", [r[0] for r in raw]), ("raw_cpu", [c[0] for c in cpu])):
        if seq and not _strictly_increasing(seq):
            errs.append(f"generations: {name} generation numbers not strictly increasing: {seq}")
    scene_gens = {g for g, _ in fx}
    for name, seq in (("raw_gpu", raw), ("raw_cpu", cpu)):
        if not seq:
            continue
        have = {r[3] for r in seq}
        if scene_gens:
            miss = sorted(g for g in scene_gens if g >= min(have) and g not in have)
            if miss:
                errs.append(f"generations: {name} never began a generation for scene generation(s) {miss}: a scene change left it on its old generation")
        sg = [r[3] for r in seq]
        if any(b < a for a, b in zip(sg, sg[1:])):
            errs.append(f"generations: {name} scene_gen went backwards")

    announced = {"fx": {g for g, _ in fx}, "raw_gpu": {r[0] for r in raw}, "raw_cpu": {c[0] for c in cpu}}
    # The palette sink records the generation the SAMPLER holds (FX: scene generation; RAW: its own raw generation), so the announced sets are comparable.
    if os.path.isdir(pal):
        for stream, fn in STREAM_FILES.items():
            p = os.path.join(pal, fn)
            if not os.path.isfile(p):
                continue
            last_gen, last_t, first_of_gen, n = None, None, set(), 0
            with open(p, newline="") as fh:
                for row in csv.DictReader(fh):
                    n += 1
                    g, t = int(row["gen"]), float(row["t"])
                    if last_gen is not None and g < last_gen:
                        errs.append(f"palette/{stream}: generation went {last_gen} -> {g}")
                    if last_t is not None and t < last_t - 1e-6:
                        errs.append(f"palette/{stream}: time went backwards at row {row['idx']}")
                    if g != last_gen and g not in first_of_gen:
                        first_of_gen.add(g)
                        if row["driftHue"].lower() != "nan":
                            errs.append(f"palette/{stream}: first row of generation {g} has driftHue={row['driftHue']} (drift crossed a generation)")
                    if announced[stream] and g not in announced[stream]:
                        errs.append(f"palette/{stream}: row generation {g} was never announced by a 'generation N begins' line")
                        announced[stream].add(g)  # report once
                    last_gen, last_t = g, t
            facts.setdefault("palette_rows", {})[stream] = n
            if n == 0:
                errs.append(f"palette/{stream}: no rows")
    return errs, facts


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("target", help="plan.json, or a directory holding <tag>.stdout.log / .frames.csv")
    ap.add_argument("--tags", help="comma list of run tags (default: every run of the plan / every *.frames.csv)")
    ap.add_argument("--min-scenes", type=int, default=5)
    ap.add_argument("--require-all", action="store_true")
    a = ap.parse_args()
    plan = None
    if os.path.isdir(a.target):
        base = a.target
        tags = sorted(f[:-len(".frames.csv")] for f in os.listdir(base) if f.endswith(".frames.csv"))
        runs = {}
    else:
        base = os.path.dirname(os.path.abspath(a.target))
        plan = json.load(open(a.target))
        runs = {r["tag"]: r for r in plan["runs"]}
        tags = list(runs)
    if a.tags:
        tags = a.tags.split(",")
    failures, found = [], 0
    for tag in tags:
        if not os.path.isfile(os.path.join(base, tag + ".frames.csv")):
            print("MISSING", tag)
            if a.require_all:
                failures.append(f"{tag}: not found")
            continue
        found += 1
        errs, facts = check_run(tag, base, runs.get(tag), a.min_scenes)
        print(("FAIL " if errs else "OK   ") + f"{tag} scenes={len(set(facts.get('scenes', [])))} changes={facts.get('scene_changes')} generations={facts.get('generations')} mode={facts.get('mode')}")
        failures += errs
    if found == 0:
        failures.append("no runs found")
    for f in failures:
        print("FAILURE:", f)
    print("VERIFIED" if not failures else "NOT VERIFIED")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
