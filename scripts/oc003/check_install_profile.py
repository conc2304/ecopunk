#!/usr/bin/env python3
"""OC-003 personal-installation profile: local, non-hardware validation. Runs nothing on the Pi.

Checks
  1. syntax of the systemd drop-ins in scripts/oc003/rockpi/ ([Service] + Environment=NAME=VALUE only);
  2. every variable is actually read by the runtime source (no invented variables);
  3. the 50- profile does not set any forbidden/diagnostic variable (RGBA16F readback, OC-004, frame-phase logging, stats, harnesses);
  4. effective flags (emulating the runtime's own parsing, cross-checked against real launch logs, see the procedure doc) for:
       absent profile (canonical defaults), normal installation, kill switch, and every diagnostic condition produced by
       plan_interleaved.py, each applied on top of a shell that EXPORTED the installation profile (the isolation test).
Exit 0 only if every expectation holds.

Parsing rules mirrored from the source (ExperienceRuntime.cpp / SceneColorSampler.cpp / RawVideoGate.h):
  FX enabled   : SCENE_COLOR unset or != "0"          FX mode : "deferred" | "async" | else sync (default sync)
  defer        : DEFER "frames*" -> frames; unset/"auto"/"fence" -> fence if sync objects exist; DEFER_FLUSH: PRESENCE only
  RAW enabled  : VIDEO_COLOR == "1" exactly           RAW mode: "sync" | "async" | else deferred (default deferred)
  DOM/VIB      : PREDOMINANT_COLOR == "1" exactly (both samplers)
  RGBA16F      : SCENE_COLOR_READBACK in {half, half-verify}
"""
import os
import re
import shlex
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
DROPIN_DIR = os.path.join(HERE, "rockpi")
SRC = os.path.join(ROOT, "sketches", "experience_runtime", "src")

FORBIDDEN_IN_PROFILE = [
    "EXPERIENCE_RUNTIME_SCENE_COLOR_READBACK", "EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY", "EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE",
    "EXPERIENCE_RUNTIME_SCENE_COLOR_STATS", "EXPERIENCE_RUNTIME_VIDEO_COLOR_STATS", "EXPERIENCE_RUNTIME_VIDEO_COLOR_DIAG",
    "EXPERIENCE_RUNTIME_PREDOMINANT_HARNESS", "EXPERIENCE_RUNTIME_SCENE_COLOR_FAULT", "EXPERIENCE_RUNTIME_SCENE_COLOR_FALLBACK",
    "EXPERIENCE_RUNTIME_SCENE_COLOR_SELFTEST", "EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM", "EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID",
    "EXPERIENCE_RUNTIME_VIDEO_DWELL", "EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS",
]

failures = []


def check(ok, msg):
    print(("ok   " if ok else "FAIL ") + msg)
    if not ok:
        failures.append(msg)


def parse_dropin(path):
    env, section = {}, None
    for n, raw in enumerate(open(path), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("["):
            section = line
            check(section == "[Service]", f"{os.path.basename(path)}:{n} only a [Service] section ({section})")
            continue
        m = re.fullmatch(r"Environment=(EXPERIENCE_RUNTIME_[A-Z0-9_]+)=([A-Za-z0-9_.,/:+-]*)", line)
        check(bool(m) and section == "[Service]", f"{os.path.basename(path)}:{n} well-formed Environment= line: {line}")
        if m:
            env[m.group(1)] = m.group(2)
    return env


def runtime_reads(name):
    out = subprocess.run(["grep", "-rlF", f'"{name}"', SRC, "--include=*.cpp", "--include=*.h"], capture_output=True, text=True).stdout
    return bool(out.strip())


def resolve(env):
    fx = env.get("EXPERIENCE_RUNTIME_SCENE_COLOR") != "0"
    m = env.get("EXPERIENCE_RUNTIME_SCENE_COLOR_MODE")
    fx_mode = "async" if m == "async" else "deferred" if m == "deferred" else "sync"
    d = env.get("EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER", "auto")
    defer = "frames" if d.startswith("frames") else "fence" if d in ("auto", "fence") else "frames"
    raw = env.get("EXPERIENCE_RUNTIME_VIDEO_COLOR") == "1"
    rm = env.get("EXPERIENCE_RUNTIME_VIDEO_COLOR_MODE")
    raw_mode = "sync" if rm == "sync" else "async" if rm == "async" else "deferred"
    pred = env.get("EXPERIENCE_RUNTIME_PREDOMINANT_COLOR") == "1"
    return {
        "FX AVG": fx, "FX DOM/VIB": fx and pred, "RAW AVG": raw, "RAW DOM/VIB": raw and pred,
        "fx_mode": fx_mode if fx else "-", "fx_defer": defer if fx and fx_mode == "deferred" else "-",
        "flush": "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH" in env and fx and fx_mode == "deferred",
        "raw_mode": raw_mode if raw else "-",
        "RGBA16F": env.get("EXPERIENCE_RUNTIME_SCENE_COLOR_READBACK") in ("half", "half-verify"),
        "OC-004": any(k in env for k in ("EXPERIENCE_RUNTIME_SCENE_VISUAL_ACTIVITY", "EXPERIENCE_RUNTIME_SCENE_ACTIVITY_PROBE")),
        "frame-phase": "EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE" in env,
    }


def apply_command(base, line):
    """Apply `env [-u NAME]... [NAME=VALUE]... <command>` to a copy of base."""
    env = dict(base)
    toks = shlex.split(line)
    i = 1  # skip "env"
    while i < len(toks):
        if toks[i] == "-u":
            env.pop(toks[i + 1], None)
            i += 2
        elif re.fullmatch(r"[A-Z_][A-Z0-9_]*=.*", toks[i]):
            k, v = toks[i].split("=", 1)
            env[k] = v
            i += 1
        else:
            break
    return env


def show(name, r):
    obs = " ".join(f"{k}={'ON' if r[k] else 'off'}" for k in ("FX AVG", "FX DOM/VIB", "RAW AVG", "RAW DOM/VIB"))
    print(f"   {name:<22}{obs} | fx={r['fx_mode']}/{r['fx_defer']} flush={'1' if r['flush'] else '-'} raw={r['raw_mode']} | RGBA16F={'ON' if r['RGBA16F'] else 'off'} OC-004={'ON' if r['OC-004'] else 'off'}")


def main():
    on_path = os.path.join(DROPIN_DIR, "50-oc003-color.conf")
    off_path = os.path.join(DROPIN_DIR, "90-oc003-color-off.conf")
    profile, kill = parse_dropin(on_path), parse_dropin(off_path)

    for name in sorted(set(profile) | set(kill)):
        check(runtime_reads(name), f"runtime source reads {name}")
    for name in FORBIDDEN_IN_PROFILE:
        check(name not in profile, f"profile does not set {name}")
    expected_profile = {
        "EXPERIENCE_RUNTIME_SCENE_COLOR": "1", "EXPERIENCE_RUNTIME_SCENE_COLOR_MODE": "deferred", "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER": "fence",
        "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH": "1", "EXPERIENCE_RUNTIME_VIDEO_COLOR": "1", "EXPERIENCE_RUNTIME_VIDEO_COLOR_MODE": "deferred",
        "EXPERIENCE_RUNTIME_PREDOMINANT_COLOR": "1",
    }
    check(profile == expected_profile, "profile contains exactly the seven approved flags")

    print("\nEffective flags")
    absent, normal = resolve({}), resolve(profile)
    killed = resolve({**profile, **kill})
    show("absent (defaults)", absent); show("normal installation", normal); show("kill switch (50+90)", killed)
    check(absent["FX AVG"] and absent["fx_mode"] == "sync" and not any(absent[k] for k in ("FX DOM/VIB", "RAW AVG", "RAW DOM/VIB", "RGBA16F", "OC-004")),
          "canonical defaults unchanged when the profile is absent (FX AVG sync only)")
    check(all(normal[k] for k in ("FX AVG", "FX DOM/VIB", "RAW AVG", "RAW DOM/VIB")) and normal["fx_mode"] == "deferred" and normal["fx_defer"] == "fence"
          and normal["flush"] and normal["raw_mode"] == "deferred", "normal installation: six observations ON, deferred fence + flush")
    check(not normal["RGBA16F"] and not normal["OC-004"] and not normal["frame-phase"], "normal installation: RGBA16F, OC-004, frame-phase logging all OFF")
    check(not any(killed[k] for k in ("FX AVG", "FX DOM/VIB", "RAW AVG", "RAW DOM/VIB")), "kill switch turns all six observations off")

    print("\nDiagnostic conditions applied on top of a shell that exported the installation profile")
    planner = os.path.join(HERE, "plan_interleaved.py")
    tmp = os.path.join(os.environ.get("TMPDIR", "/tmp"), "oc003_profile_check")
    out = subprocess.run([sys.executable, planner, "--bin", sys.executable, "--out", tmp, "--media-id", "media.auto.00000000", "--scene", "s",
                          "--resolution", "1x1", "--conditions", "P0,P1,P2,P2H", "--rounds", "1", "--duration", "1"], capture_output=True, text=True)
    check(out.returncode == 0, "planner produces the P0/P1/P2/P2H commands")
    want = {
        "P0":  dict(fx=False, fxdv=False, raw=False, rawdv=False, half=False),
        "P1":  dict(fx=True,  fxdv=False, raw=False, rawdv=False, half=False),
        "P2":  dict(fx=True,  fxdv=True,  raw=True,  rawdv=True,  half=False),
        "P2H": dict(fx=True,  fxdv=True,  raw=True,  rawdv=True,  half=True),
    }
    cond = None
    for line in out.stdout.splitlines():
        m = re.match(r"# run \d+_(P\w+)_r\d", line)
        if m:
            cond = m.group(1)
        elif line.startswith("env ") and cond:
            r = resolve(apply_command(profile, line))   # polluted shell = profile exported
            show(cond, r)
            w = want[cond]
            check((r["FX AVG"], r["FX DOM/VIB"], r["RAW AVG"], r["RAW DOM/VIB"], r["RGBA16F"]) == (w["fx"], w["fxdv"], w["raw"], w["rawdv"], w["half"]),
                  f"{cond} resolves to its intended flags despite the exported profile")
            resolved = apply_command(profile, line)
            flush_ok = ("EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH" not in resolved) or ("EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH=1" in line)
            check(flush_ok and not r["OC-004"], f"{cond}: DEFER_FLUSH only if the condition itself sets it (never inherited), OC-004 off")
            cond = None

    print("\n" + ("PROFILE VALID" if not failures else f"{len(failures)} FAILURE(S)"))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
