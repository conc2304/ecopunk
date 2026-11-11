#!/usr/bin/env python3
"""OC-003 sampling-frame attribution: per-round AND pooled campaign summary (machine-readable JSON + short Markdown report).

  summarize_campaign.py <dir>/plan.json [--out DIR] [--long-ms 40]

Reads only what the planner and the runtime wrote (plan.json, <tag>.frames.csv / .phase.log / .stdout.log) and reuses the existing
tools: aggregate_frames.summarize (exclusions, nearest-rank percentiles, independent vs exclusive event populations, lag views),
sampler_stats (last sampler stat line of each stream) and reconcile_events.py (exact event reconciliation). No new profiling.

Writes <out>/summary.json and <out>/report.md (default <out> = <dir>/analysis). Exit 0 always when it could read the runs; the verdicts
(RECONCILED, gl errors, failed attempts) are IN the output, and verify_run.py remains the gate.
Rules the numbers follow:
  * pooled = every steady-state frame of every round of a condition, percentiles over individual frames (never averaged percentiles);
  * per round = the same computation for that round's run alone;
  * overlapping event populations are never added together (independent) and exclusive categories always sum to the frame total;
  * a difference (P2-P0, P2F-P0, P2F-P2) is reported pooled AND per round (same round = same position in the interleaved order),
    with the min/max across rounds; `sign_consistent` says whether every round agrees on the direction. A difference whose per-round
    values straddle zero or are not sign-consistent is NOT a result.
"""
import argparse
import json
import os
import re
import subprocess
import sys
from types import SimpleNamespace

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import aggregate_frames as agg  # noqa: E402
import sampler_stats as sst  # noqa: E402
import stall_analysis as sta  # noqa: E402

BAD_GL = re.compile(r"GL error|(?i:gl[A-Za-z]*errors?[A-Za-z]*)=(?:0x)?0*[1-9a-fA-F]")
PAIRS = [("P2", "P0"), ("P2F", "P0"), ("P2F", "P2")]
# OC-003-D arms (only compared when both conditions are in the plan; existing campaigns are unaffected)
DIAG_PAIRS = [("P2G", "P2"), ("P2N", "P2"),
              # OC-003-F acquisition isolation: each arm against the no-sampling baseline, then the CPU-vs-GPU and FX-added comparisons
              ("P3", "P0"), ("P4", "P0"), ("P5", "P0"), ("P4", "P3"), ("P5", "P4"), ("P5", "P1"), ("P4", "P2")]
# Stall-analysis metrics compared between conditions (measured values only): (name, path into the stall_analysis.analyze() result)
STALL_METRICS = [("effective_fps", ("effective_fps",)), ("p95", ("frame_ms", "p95")), ("p99", ("frame_ms", "p99")), ("worst", ("frame_ms", "worst")),
                 ("long_frames", ("long_frames",)), ("after_issue_long_rate", ("after_issue", "long_rate")),
                 ("no_event_after_issue_long_rate", ("no_event_after_issue_long_rate",)),
                 ("postDraw_wall_p95", ("postDraw", "all", "wall", "p95")), ("postDraw_blocked_p95", ("postDraw", "all", "blocked", "p95")),
                 ("postDraw_cpu_p50", ("postDraw", "all", "cpu", "p50")),
                 ("glFinish_wall_p95", ("glFinish", "all", "wall", "p95")), ("glFinish_wall_max", ("glFinish", "all", "wall", "max")),
                 ("glFinish_cpu_p95", ("glFinish", "all", "cpu", "p95")),
                 ("glFinish_issue_frame_wall_p50", ("glFinish", "issue_frame", "wall", "p50")),
                 ("fence_not_ready_excess", ("fence_not_ready_excess",))]
METRICS = [("p50", ("frame_ms", "p50")), ("p95", ("frame_ms", "p95")), ("p99", ("frame_ms", "p99")), ("worst", ("frame_ms", "worst")),
           ("mean", ("frame_ms", "mean")), ("mean_draw_cpu_ms", ("mean_draw_cpu_ms",)), ("long_frames", ("events", "long_frames_total"))]


def get(d, path):
    for k in path:
        if not isinstance(d, dict) or k not in d:
            return None
        d = d[k]
    return d


def delta(a, b):
    return None if a is None or b is None else round(a - b, 4)


def reconcile(csvp, phase, stdout, cond):
    cmd = [sys.executable, os.path.join(HERE, "reconcile_events.py"), csvp, phase] + ([] if cond == "P0" else [stdout])
    r = subprocess.run(cmd, capture_output=True, text=True)
    return {"reconciled": r.returncode == 0, "last_line": (r.stdout.strip().splitlines() or [""])[-1]}


def _f(v, spec="%.2f"):
    return "—" if v is None else spec % v


def stall_report(stall, conds, long_ms):
    """Markdown for the OC-003-D sections. Part 1 is MEASURED values only; the interpretation guide is a separate, clearly labelled section."""
    cs = [c for c in conds if c in stall["conditions"]]
    L = ["", f"## OC-003-D stall analysis — MEASURED (steady state; a frame is related to an earlier frame only when frame ids are consecutive and nothing between is excluded)", "",
         "### Frames and long frames (> %g ms), pooled" % long_ms, "",
         "| cond | frames | eff. fps | p50 | p95 | p99 | worst | long | long: event / after-issue / after-collect / lag+2 / quiet | frames after an issue (long, rate) |",
         "|---|---|---|---|---|---|---|---|---|---|"]
    for c in cs:
        p = stall["conditions"][c]["pooled"]
        if not p.get("frames"):
            continue
        k = p["classes"]
        split = " / ".join(str(k[n]["long"]) for n in sta.CLASSES)
        ai = p["after_issue"]
        L.append(f"| {c} | {p['frames']} | {_f(p['effective_fps'], '%.3f')} | {p['frame_ms']['p50']:.2f} | {p['frame_ms']['p95']:.2f} | {p['frame_ms']['p99']:.2f} | "
                 f"{p['frame_ms']['worst']:.2f} | {p['long_frames']} | {split} | {ai['frames']} ({ai['long']}, {_f(None if ai['long_rate'] is None else 100 * ai['long_rate'], '%.1f%%')}) |")
    L += ["", "### Segment distributions, pooled (ms): postDraw (swap + loop) and glFinish (probe-timed, only when GLFINISH is on)", "",
          "| cond | population | n | postDraw wall p50/p95/max | postDraw cpu p50 | postDraw blocked p95 | glFinish wall p50/p95/max | glFinish cpu p50/p95 |", "|---|---|---|---|---|---|---|---|"]
    for c in cs:
        p = stall["conditions"][c]["pooled"]
        for pop in ("all", "quiet", "issue_frame", "no_event_after_issue"):
            pd, gd = (p.get("postDraw") or {}).get(pop), (p.get("glFinish") or {}).get(pop)
            if not pd:
                continue
            w, cp, bl = pd["wall"], pd["cpu"], pd["blocked"]
            gw, gc = gd["wall"], gd["cpu"]
            L.append(f"| {c} | {pop} | {w['n']} | {w['p50']:.2f}/{w['p95']:.2f}/{w['max']:.2f} | {cp['p50']:.2f} | {bl['p95']:.2f} | "
                     f"{gw['p50']:.3f}/{gw['p95']:.3f}/{gw['max']:.3f} | {gc['p50']:.3f}/{gc['p95']:.3f} |")
    L += ["", "### Per round: long frames, frames after an issue that were long, fence not-ready excess (derived)", "",
          "| cond | round | frames | long | after-issue frames | after-issue long | not-ready excess (whole run) |", "|---|---|---|---|---|---|---|"]
    for c in cs:
        for rd, p in sorted(stall["conditions"][c]["per_round"].items(), key=lambda kv: int(kv[0])):
            if not p.get("frames"):
                continue
            ai = p["after_issue"]
            L.append(f"| {c} | r{rd} | {p['frames']} | {p['long_frames']} | {ai['frames']} | {ai['long']} | {p.get('fence_not_ready_excess', '—')} |")
    L += ["", "The not-ready excess is `notReady - issued` from the last cumulative sampler stat line of each stream (whole run including warm-up; per-run totals only, "
          "not per frame); it assumes one unsignalled poll per sample by construction.", ""]
    for k, block in stall["differences"].items():
        if k.split("-")[0] not in ("P2G", "P2N") and k.split("-")[1] not in ("P2G", "P2N"):
            continue
        L += [f"### {k} (pooled; per-round min..max; sign-consistent = every round agrees on the direction)", "",
              "| metric | pooled | per-round min..max | sign-consistent |", "|---|---|---|---|"]
        for name, _p in STALL_METRICS:
            b = block[name]
            L.append(f"| {name} | {b['pooled']} | {b['min']}..{b['max']} | {'n/a' if b['sign_consistent'] is None else b['sign_consistent']} |")
        L.append("")
    L += ["## Interpretation guide (NOT measured — hypotheses the measured values above can support or weaken)", "",
          "| if... | late GPU completion | swap / present scheduling |", "|---|---|---|",
          "| P2G (glFinish on): `glFinish` wall on issue frames | large (the wait moves into the measured segment) | small |",
          "| P2G: after-issue stall rate and postDraw blocked time | fall toward 0 | persist |",
          "| P2N (vsync off): after-issue stall rate and postDraw blocked time | unchanged | fall |",
          "", "Limits: glFinish serialises CPU and GPU, so a vanished stall alone is not proof; its time is the drain of ALL earlier GPU work, not only the sampler's. "
          "The probe CONFIG line records the REQUESTED vsync state, not the driver's swap interval, so no change in P2N cannot tell \"ignored by the driver\" from "
          "\"not the mechanism\". The not-ready excess is expected to fall under P2G by construction (a drained GPU signals every fence), so it is not independent evidence there. "
          "Only differences that are sign-consistent across all rounds and larger than the round-to-round range of the baseline are results.", ""]
    return L


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("plan")
    ap.add_argument("--out")
    ap.add_argument("--long-ms", type=float, default=40.0)
    ap.add_argument("--stall", action="store_true", help="append the OC-003-D stall-analysis sections to report.md (always appended when the plan has P2G or P2N; "
                    "summary.json always carries the additive `stall` key)")
    a = ap.parse_args()
    plan = json.load(open(a.plan))
    base = os.path.dirname(os.path.abspath(a.plan))
    out = a.out or os.path.join(base, "analysis")
    os.makedirs(out, exist_ok=True)
    args = SimpleNamespace(include_excluded=False, threshold_ms=100.0, long_ms=a.long_ms)

    runs = plan["runs"]
    conds = plan["conditions"]
    path = lambda r, s: os.path.join(base, r["tag"] + s)  # noqa: E731
    present = [r for r in runs if os.path.isfile(path(r, ".frames.csv"))]
    summary = {"plan": os.path.abspath(a.plan), "binary_sha256": plan.get("binary_sha256"), "media_id": plan["media_id"], "scene": plan["scene"],
               "resolution": plan["resolution"], "duration_s": plan["duration_s"], "rounds": plan["rounds"], "long_ms": a.long_ms,
               "installation_settings_used_sha256": plan.get("installation_settings_used_sha256"),
               "runs_planned": len(runs), "runs_found": len(present), "conditions": {}, "differences": {}, "runs": {}, "notes": []}
    if len(present) != len(runs):
        summary["notes"].append(f"INCOMPLETE: {len(present)} of {len(runs)} runs found; do not pool an incomplete campaign")

    if plan["rounds"] < 2:
        summary["notes"].append("SINGLE ROUND: there is no round-to-round spread, so no difference in this summary is a result (smoke/rehearsal data only)")
    for cond in conds:
        cruns = [r for r in present if r["condition"] == cond]
        entry = {"pooled": agg.summarize(cond, [path(r, ".frames.csv") for r in cruns], args) if cruns else None, "per_round": {}}
        for r in cruns:
            entry["per_round"][str(r["round"])] = agg.summarize(f"{cond}/r{r['round']}", [path(r, ".frames.csv")], args)
        spread = {}
        for name, p in METRICS:
            vals = [v for v in (get(x, p) for x in entry["per_round"].values()) if v is not None]
            spread[name] = {"per_round": vals, "min": min(vals), "max": max(vals), "range": round(max(vals) - min(vals), 4)} if vals else None
        entry["round_to_round"] = spread
        summary["conditions"][cond] = entry

    for x, y in PAIRS + DIAG_PAIRS:
        if x not in conds or y not in conds:
            continue
        px, py = summary["conditions"][x], summary["conditions"][y]
        block = {}
        for name, p in METRICS:
            pooled = delta(get(px["pooled"] or {}, p), get(py["pooled"] or {}, p))
            per = []
            for rd in sorted(set(px["per_round"]) & set(py["per_round"]), key=int):
                per.append(delta(get(px["per_round"][rd], p), get(py["per_round"][rd], p)))
            per = [v for v in per if v is not None]
            block[name] = {"pooled": pooled, "per_round": per, "min": min(per) if per else None, "max": max(per) if per else None,
                           "rounds_compared": len(per),
                           "sign_consistent": (all(v > 0 for v in per) or all(v < 0 for v in per)) if len(per) >= 2 else None}
        summary["differences"][f"{x}-{y}"] = block

    # ---- OC-003-D stall analysis: an ADDITIVE key of summary.json ("stall"); nothing above is changed by it.
    stall = {"conditions": {}, "differences": {}}
    for cond in conds:
        cruns = [r for r in present if r["condition"] == cond]
        if not cruns:
            continue
        by_round = {r["round"]: sta.read_frames(path(r, ".frames.csv")) for r in cruns}
        ent = {"pooled": sta.analyze(list(by_round.values()), a.long_ms),
               "per_round": {str(rd): sta.analyze([rows], a.long_ms) for rd, rows in sorted(by_round.items())}, "fence_not_ready": {}}
        if cond != "P0":
            tot = 0
            for r in cruns:
                fz = sta.fence_notready([path(r, ".stdout.log")])
                ent["fence_not_ready"][str(r["round"])] = fz
                ex = sum(v["excess"] for v in fz.values())
                ent["per_round"][str(r["round"])]["fence_not_ready_excess"] = ex
                tot += ex
            ent["pooled"]["fence_not_ready_excess"] = tot
        stall["conditions"][cond] = ent
    for x, y in PAIRS + DIAG_PAIRS:
        if x not in stall["conditions"] or y not in stall["conditions"]:
            continue
        px, py = stall["conditions"][x], stall["conditions"][y]
        block = {}
        for name, p in STALL_METRICS:
            pooled = delta(get(px["pooled"], p), get(py["pooled"], p))
            per = [delta(get(px["per_round"][rd], p), get(py["per_round"][rd], p)) for rd in sorted(set(px["per_round"]) & set(py["per_round"]), key=int)]
            per = [v for v in per if v is not None]
            block[name] = {"pooled": pooled, "per_round": per, "min": min(per) if per else None, "max": max(per) if per else None, "rounds_compared": len(per),
                           "sign_consistent": (all(v > 0 for v in per) or all(v < 0 for v in per)) if len(per) >= 2 else None}
        stall["differences"][f"{x}-{y}"] = block
    summary["stall"] = stall

    gl_hits = 0
    for r in present:
        cond = r["condition"]
        info = {"condition": cond, "round": r["round"]}
        info["reconcile"] = reconcile(path(r, ".frames.csv"), path(r, ".phase.log"), path(r, ".stdout.log"), cond)
        hits = []
        for s in (".stdout.log", ".phase.log"):
            if os.path.isfile(path(r, s)):
                hits += [f"{r['tag']}{s}: {l.strip()[:160]}" for l in open(path(r, s), errors="replace") if BAD_GL.search(l)]
        info["gl_error_lines"] = hits
        gl_hits += len(hits)
        if os.path.isfile(path(r, ".stdout.log")) and cond != "P0":
            info["sampler"] = {st: sst.parse(line) for st, line in sorted(sst.last_lines(path(r, ".stdout.log")).items())}
        summary["runs"][r["tag"]] = info
    summary["gl_error_lines_total"] = gl_hits
    summary["all_reconciled"] = all(v["reconcile"]["reconciled"] for v in summary["runs"].values()) if summary["runs"] else False
    with open(os.path.join(out, "summary.json"), "w") as fh:
        json.dump(summary, fh, indent=2, sort_keys=True)

    # ---------------------------------------------------------------- human report (short; the JSON has everything)
    L = [f"# OC-003 sampling-frame attribution — campaign summary", "",
         f"media `{plan['media_id']}`, scene `{plan['scene']}`, {plan['resolution']}, {plan['rounds']} round(s) x {plan['duration_s']} s, binary sha256 `{plan.get('binary_sha256')}`",
         f"runs found {len(present)}/{len(runs)}; all event streams reconciled: **{summary['all_reconciled']}**; GL-error lines: **{gl_hits}**", ""]
    L += summary["notes"] and ["**" + n + "**" for n in summary["notes"]] + [""] or []
    L += ["## Frame time (ms), pooled over rounds; steady state only (warm-up, media/scene change, transitions excluded)", "",
          "| cond | frames | p50 | p95 | p99 | worst | long > %g ms | mean draw CPU |" % a.long_ms, "|---|---|---|---|---|---|---|---|"]
    for c in conds:
        p = summary["conditions"][c]["pooled"]
        if p:
            f = p["frame_ms"]
            L.append(f"| {c} | {p['frames']} | {f['p50']:.2f} | {f['p95']:.2f} | {f['p99']:.2f} | {f['worst']:.2f} | {get(p, ('events', 'long_frames_total'))} | {p['mean_draw_cpu_ms']:.2f} |")
    L += ["", "## Per round (p95 / p99 / worst ms)", "", "| cond | " + " | ".join(f"r{i}" for i in range(1, plan["rounds"] + 1)) + " |", "|---|" + "---|" * plan["rounds"]]
    for c in conds:
        cells = []
        for i in range(1, plan["rounds"] + 1):
            x = summary["conditions"][c]["per_round"].get(str(i))
            cells.append("%.2f / %.2f / %.2f" % (x["frame_ms"]["p95"], x["frame_ms"]["p99"], x["frame_ms"]["worst"]) if x else "—")
        L.append(f"| {c} | " + " | ".join(cells) + " |")
    L += ["", "## Differences (pooled; per-round min..max; sign-consistent = every round agrees on the direction; n/a with fewer than 2 rounds)", "",
          "| diff | metric | pooled | per-round min..max | sign-consistent |", "|---|---|---|---|---|"]
    for k, block in summary["differences"].items():
        for m in ("p50", "p95", "p99", "worst", "long_frames"):
            b = block[m]
            L.append(f"| {k} | {m} | {b['pooled']} | {b['min']}..{b['max']} | {'n/a' if b['sign_consistent'] is None else b['sign_consistent']} |")
    L += ["", "A difference that is not sign-consistent across rounds, or is smaller than the round-to-round range of the condition, is not a result.", "",
          "## Event-conditioned frames (pooled, steady state). Independent populations OVERLAP; exclusive categories sum to the frame total", ""]
    for c in conds:
        e = get(summary["conditions"][c]["pooled"] or {}, ("events",))
        if not e:
            continue
        L += [f"### {c}", "", "events: " + ", ".join(f"{k}={v}" for k, v in e["event_counts"].items()), "",
              "| population | frames | p50 | p95 | p99 | worst | long |", "|---|---|---|---|---|---|---|"]
        for kind, grp in (("with ", e["independent"]), ("", e["exclusive"])):
            for name, d in grp.items():
                if d["n"]:
                    L.append(f"| {kind}{name} | {d['n']} | {d['p50']:.2f} | {d['p95']:.2f} | {d['p99']:.2f} | {d['worst']:.2f} | {d['long_frames']} |")
        L.append("")
    L += ["## Sampler (last stat line per run; cumulative counters since start, ring p50/p95/max over the latest 256 samples)", "",
          "| run | stream | total p50/p95/max | latency p50/p95/max | issued | completed | gaps | dropGen | invalid | discardTimeout | discardWaitFailed | glErrors |",
          "|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for tag, info in summary["runs"].items():
        for st, d in info.get("sampler", {}).items():
            L.append(f"| {tag} | {st} | {d.get('total', '')} | {d.get('latency', '')} | {d.get('issued', '')} | {d.get('completed', '')} | {d.get('gaps', '')} | "
                     f"{d.get('dropGen', d.get('droppedGen', ''))} | {d.get('invalid', '')} | {d.get('discardTimeout', '')} | {d.get('discardWaitFailed', '')} | {d.get('glErrors', '')} |")
    L += ["", "Association is not GPU causation: the probe observes wall/CPU time per frame segment, not GPU timestamps.", ""]
    if a.stall or any(c in conds for c in ("P2G", "P2N")):
        L += stall_report(stall, conds, a.long_ms)
    with open(os.path.join(out, "report.md"), "w") as fh:
        fh.write("\n".join(L))
    print(f"wrote {os.path.join(out, 'summary.json')} and {os.path.join(out, 'report.md')}")


if __name__ == "__main__":
    main()
