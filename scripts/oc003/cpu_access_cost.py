#!/usr/bin/env python3
"""OC-003-F: CPU RAW decoder-pixel ACCESS cost per scene (desktop measurement; also usable on a supervised Pi).

  cpu_access_cost.py --bin BIN --media-id media.auto.xxxx --out DIR [--seconds 75] [--scenes blob,temporal,blueprint,fragment,quadrant] [--workdir DIR]
  cpu_access_cost.py --parse STDOUT.log            (print the last CPURAW stat line as JSON; no run)

Why: on macOS `ofAVFoundationPlayer::getPixels()` converts lazily, once per new frame, on the FIRST call. A scene that already reads the decoder pixels
(e.g. Temporal's time-offset buffer) pays that conversion itself, so CPU RAW's `access` time there is a cache hit; in a scene that does not, CPU RAW pays it.
That difference is the "scene dependence" of the access cost. On the GStreamer path (ROCK Pi) getPixels() is a reference and the cost is expected to be ~0 everywhere (hypothesis,
measured by the same script on the Pi under the hardware owner's supervision).

One run per scene, CPU RAW ONLY (no FX/RAW GPU work), media pinned, the production service untouched. Refuses to start while ANY experience_runtime process exists
(another campaign may be using the GPU/display). Output per scene: <out>/<scene>.stdout.log, .frames.csv, .phase.log and <out>/access_cost.json with
accessWall/accessCpu/reduce/total p50/p95/max (ring of the latest 256 samples) and the pooled draw CPU / frame time from the frame stream.
Scene selection uses the runtime's existing startup switches (EXPERIENCE_RUNTIME_TEMPORAL_SCENE / _BLUEPRINT_SCENE / _FRAGMENT_SCENE / _QUADRANT_SCENE; none = Blob).
"""
import argparse
import csv
import json
import os
import re
import subprocess
import sys

SCENES = {"blob": None, "temporal": "EXPERIENCE_RUNTIME_TEMPORAL_SCENE", "blueprint": "EXPERIENCE_RUNTIME_BLUEPRINT_SCENE",
          "fragment": "EXPERIENCE_RUNTIME_FRAGMENT_SCENE", "quadrant": "EXPERIENCE_RUNTIME_QUADRANT_SCENE"}
TRI = re.compile(r"(\w+)\[p50/p95/max\]=([\d.eE+-]+)/([\d.eE+-]+)/([\d.eE+-]+)")


def parse_stat_line(text):
    last = None
    for line in text.splitlines():
        if "stream=CPURAW" in line:
            last = line
    if last is None:
        return None
    out = {k: int(v) for k, v in re.findall(r"\b(due|samples|gaps|noPixels|badFormat|dimMismatch|droppedGen)=(\d+)", last)}
    for name, a, b, c in TRI.findall(last):
        out[name] = {"p50": float(a), "p95": float(b), "max": float(c)}
    return out


def frame_summary(path):
    tot, draw = [], []
    with open(path, newline="") as fh:
        for r in csv.DictReader(fh):
            if r["exclude"] == "1":
                continue
            tot.append(float(r["total_ms"])); draw.append(float(r["draw_cpu"]))
    if not tot:
        return None
    pct = lambda v, p: sorted(v)[min(len(v) - 1, int(p * len(v)))]  # noqa: E731  (nearest rank, same rule as the campaign tools)
    return {"frames": len(tot), "frame_ms_p50": pct(tot, .5), "frame_ms_p95": pct(tot, .95), "draw_cpu_ms_mean": sum(draw) / len(draw)}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin")
    ap.add_argument("--media-id")
    ap.add_argument("--out")
    ap.add_argument("--workdir")
    ap.add_argument("--seconds", type=int, default=75)
    ap.add_argument("--scenes", default="blob,temporal")
    ap.add_argument("--parse")
    a = ap.parse_args()
    if a.parse:
        print(json.dumps(parse_stat_line(open(a.parse, errors="replace").read()), indent=2))
        return
    if not (a.bin and a.media_id and a.out):
        sys.exit("--bin, --media-id and --out are required")
    if subprocess.run(["pgrep", "-x", "experience_runtime"], capture_output=True).returncode == 0:
        sys.exit("an experience_runtime process exists (another campaign?): refusing to start a GPU workload")
    unknown = [s for s in a.scenes.split(",") if s not in SCENES]
    if unknown:
        sys.exit(f"unknown scenes {unknown}; choose from {sorted(SCENES)}")
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    workdir = os.path.abspath(a.workdir or os.getcwd())  # the runtime is started from here (<repo>/sketches/experience_runtime)
    base = {k: v for k, v in os.environ.items() if not k.startswith("EXPERIENCE_RUNTIME_")}
    # private settings copy with both auto-cycles OFF (same rule as the planner), so the scene/media cannot move mid-run
    src = os.path.join(workdir, "bin", "data", "installation-settings.json")
    inst = json.load(open(src)) if os.path.isfile(src) else {}
    for k in ("sceneAutoCycle", "mediaAutoCycle"):
        inst[k] = dict(inst.get(k) or {"durationSeconds": 300.0}, enabled=False)
    settings = os.path.join(out, "settings.json")
    json.dump(inst, open(settings, "w"), indent=2)
    result = {"bin": os.path.abspath(a.bin), "media_id": a.media_id, "seconds": a.seconds, "scenes": {}}
    for scene in a.scenes.split(","):
        env = dict(base, EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE="1", EXPERIENCE_RUNTIME_FRAME_PHASE_WARMUP_S="15",
                   EXPERIENCE_RUNTIME_FRAME_PHASE_SETTLE_MS="3000", EXPERIENCE_RUNTIME_VIDEO_DWELL="100000,100000",
                   EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID=a.media_id, EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS=settings,
                   EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES=f"{out}/{scene}.frames.csv", EXPERIENCE_RUNTIME_FRAME_PHASE_LOG=f"{out}/{scene}.phase.log",
                   EXPERIENCE_RUNTIME_SCENE_COLOR="0", EXPERIENCE_RUNTIME_PREDOMINANT_COLOR="1",
                   EXPERIENCE_RUNTIME_CPU_RAW="1", EXPERIENCE_RUNTIME_CPU_RAW_STATS="1")
        if SCENES[scene]:
            env[SCENES[scene]] = "1"
        with open(f"{out}/{scene}.stdout.log", "w") as fh:
            rc = subprocess.run(["timeout", "-s", "INT", str(a.seconds), a.bin], env=env, cwd=workdir, stdout=fh, stderr=subprocess.STDOUT).returncode
        text = open(f"{out}/{scene}.stdout.log", errors="replace").read()
        m = re.search(r"AcquisitionMode: (.*)", text)
        sc = None
        if os.path.isfile(f"{out}/{scene}.frames.csv"):
            with open(f"{out}/{scene}.frames.csv", newline="") as fh:
                sc = sorted({r["scene"] for r in csv.DictReader(fh)} - {"-"})
        result["scenes"][scene] = {"rc": rc, "acquisition_mode": m.group(1) if m else None, "scene_ids_seen": sc,
                                   "cpu_raw": parse_stat_line(text), "frames": frame_summary(f"{out}/{scene}.frames.csv") if sc is not None else None}
    json.dump(result, open(f"{out}/access_cost.json", "w"), indent=2)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
