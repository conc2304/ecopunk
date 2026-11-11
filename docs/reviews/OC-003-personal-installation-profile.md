# OC-003 — Personal-installation colour profile (ROCK Pi 4B) and operator instructions

Status: prepared and desktop-validated. NOT applied to the Pi; not ROCK Pi runtime-verified. Application code defaults are unchanged.
Everything on the Pi (install, restart, rollback) is operator-run: the coding agent may not SSH, deploy, or touch services.

## 1. Where the configuration lives
The runtime is configured by environment variables it already reads, and the Pi runs it as the device-owned systemd unit `ecopunk-runtime` (unit file not in this repository; confirm the name with `systemctl status ecopunk-runtime`). The supported mechanism is therefore a **systemd drop-in** on that unit. `installation-settings.json` is the on-screen settings file (UI-owned); no keys were added to it (that would be a new installation setting).

| Repo file | Install on the Pi as | Purpose |
|---|---|---|
| `scripts/oc003/rockpi/50-oc003-color.conf` | `/etc/systemd/system/ecopunk-runtime.service.d/50-oc003-color.conf` | the profile |
| `scripts/oc003/rockpi/90-oc003-color-off.conf` | `/etc/systemd/system/ecopunk-runtime.service.d/90-oc003-color-off.conf` | kill switch (install only when needed) |

Effective flags of the profile (exactly the seven approved, no others):
```
EXPERIENCE_RUNTIME_SCENE_COLOR=1            FX sampling on
EXPERIENCE_RUNTIME_SCENE_COLOR_MODE=deferred
EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER=fence  non-blocking fence completion
EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH=1  (glFlush right after the fence; presence-based in the runtime)
EXPERIENCE_RUNTIME_VIDEO_COLOR=1            RAW sampling on
EXPERIENCE_RUNTIME_VIDEO_COLOR_MODE=deferred
EXPERIENCE_RUNTIME_PREDOMINANT_COLOR=1      DOM/VIB for both FX and RAW
```
Result: FX and RAW AVG/DOM/VIB all published, RGBA32F GPU-linear deferred-fence path, 2 Hz (compiled constant, not configurable). Deliberately NOT set: `SCENE_COLOR_READBACK` (RGBA16F stays off), OC-004 (`SCENE_VISUAL_ACTIVITY`), `FRAME_PHASE_*`, `*_STATS`, `*_DIAG`, harness/fault/fallback switches.

Canonical (no-profile) defaults, for reference: FX AVG ON with **synchronous** readback, RAW off, DOM/VIB off, RGBA16F off. "Unset" is therefore not "off"; the kill switch sets `SCENE_COLOR=0`.

## 2. Enable on the Pi (operator)
1. Get the files there: a normal `scripts/deploy.sh` syncs `scripts/` (docs/ and bin/ are excluded), or copy them yourself.
2. Inspect first: `systemctl cat ecopunk-runtime` and `systemctl show ecopunk-runtime -p Environment`. STOP and review if the base unit or another drop-in already sets any `EXPERIENCE_RUNTIME_*` colour or diagnostic variable (a later `Environment=` of the same name wins; a leftover frame-phase probe or `SCENE_VISUAL_ACTIVITY` would stay on).
3. Install and apply:
```bash
sudo mkdir -p /etc/systemd/system/ecopunk-runtime.service.d
sudo install -m 0644 <REMOTE_PATH>/scripts/oc003/rockpi/50-oc003-color.conf /etc/systemd/system/ecopunk-runtime.service.d/50-oc003-color.conf
sudo systemctl daemon-reload
systemctl show ecopunk-runtime -p Environment | tr ' ' '\n' | grep EXPERIENCE_RUNTIME    # exactly the seven lines above (plus anything pre-existing)
sudo systemctl restart ecopunk-runtime
journalctl -u ecopunk-runtime -b --no-pager | grep -E "GPU linear-light|deferred readback|RAW VIDEO sampling ENABLED|DOMINANT/VIBRANT"
```
Expected log lines: `GPU linear-light reduction ready`, `deferred readback: ... strategy=fence`, `RAW VIDEO sampling ENABLED ... mode=deferred`, `DOMINANT/VIBRANT colour sampling REQUESTED`, and NO `EXPERIMENTAL readback format`. Then confirm in Orbit that all six FX/RAW x AVG/DOM/VIB selections show data (a missing clip keeps RAW unavailable by design).

## 3. Disable all colour instrumentation immediately (operator)
```bash
sudo install -m 0644 <REMOTE_PATH>/scripts/oc003/rockpi/90-oc003-color-off.conf /etc/systemd/system/ecopunk-runtime.service.d/90-oc003-color-off.conf
sudo systemctl daemon-reload && sudo systemctl restart ecopunk-runtime
```
(Sets SCENE_COLOR=0, VIDEO_COLOR=0, PREDOMINANT_COLOR=0; Orbit shows NOT AVAILABLE.) A restart is required: the runtime reads these once at startup.
**Full rollback**: `sudo rm /etc/systemd/system/ecopunk-runtime.service.d/50-oc003-color.conf` (and the 90- file if present), `sudo systemctl daemon-reload && sudo systemctl restart ecopunk-runtime` -> canonical defaults (FX AVG synchronous only). To go from "off" back to "on", remove only the 90- file.

## 4. Diagnostic conditions (P0/P1/P2/P2H)
Run with the service STOPPED (`sudo systemctl stop ecopunk-runtime`; never a second graphical instance), then restore it (`sudo systemctl start ecopunk-runtime`), as in the smoke-gate runbook. The commands come from the planner and run in a shell, so the unit's `Environment=` is not inherited; additionally every command explicitly `env -u`-unsets all colour/diagnostic variables the condition does not set, so even a shell that exported the profile cannot leak measurements into a condition.

| Condition | FX | RAW | DOM/VIB | RGBA16F | Planner |
|---|---|---|---|---|---|
| Normal installation | ON | ON | ON | off | (service + 50- profile) |
| P0 | off | off | off | off | `--conditions P0` (`SCENE_COLOR=0`) |
| P1 | AVG only | off | off | off | `--conditions P1` |
| P2 | ON | ON | ON | off | `--conditions P2` |
| P2H | ON | ON | ON | ON | `--conditions P2H` |

`python3 scripts/oc003/plan_interleaved.py --bin <abs binary> --out <dir> --media-id <ID> --scene <scene> --resolution <WxH> --conditions P0,P1,P2,P2H --rounds 4 --duration 300`. Recorded baselines are unchanged (same variables and values as before); the only additions are the explicit unsets. P1/P2 do not set `DEFER_FLUSH` unless `--defer-flush` is given; pass it to make P2 identical to the installation profile (the installation sets it; the earlier baseline did not).

## 5. Persistence through future deployments
- The drop-ins live in `/etc/systemd/system/...`, outside `REMOTE_PATH`. `scripts/deploy.sh` only rsyncs inside `REMOTE_PATH` (and, with `--clean`, deletes only there), so a deployment or rebuild never removes or changes them. The Pi-side copies of the repo files are updated by deploys, but the installed `/etc` copies change only when the operator re-installs them.
- A deployment replaces the binary; the unit's environment (and so the profile) applies at the next service restart. `bin/` (including `installation-settings.json`) is excluded from sync and untouched.
- If the repo copy of a drop-in changes, re-install it manually (`systemctl show ... -p Environment` shows what is effective).

## 6. Local validation (desktop, non-hardware)
- `python3 scripts/oc003/check_install_profile.py` -> **PROFILE VALID** (drop-in syntax; every variable is read by the runtime; no forbidden variable in the profile; effective flags for absent/normal/kill-switch; P0/P1/P2/P2H each resolved on top of an exported profile).
- Real runtime launches (desktop), runtime-logged flags: absent profile = FX sync only; normal profile = FX deferred/fence, RAW ENABLED deferred, DOM/VIB REQUESTED, RGBA16F off; P0 = no sampling; P1 = FX deferred AVG only; P2 = all on; P2H = all on + EXPERIMENTAL half, each launched under an exported profile.
- Real five-scene harness under exactly the profile: **44/44** checks, `rb=float`, 230,400 B per readback, FX and RAW DOM/VIB published, no generation leakage.
- No application source changed in this task (planner/validator/config/docs only), so scene/media navigation, generation handling and Orbit A/B configuration are unchanged by construction; HUD-004/HUD-006, OC-004 and measurement code untouched.

## 7. Unresolved risks (honest limits)
- Not applied or run on the Pi. The unit name, base-unit environment and whether the base unit already sets `EXPERIENCE_RUNTIME_*` are unverified (the unit is device-owned).
- Deferred fence + `DEFER_FLUSH` + RAW + DOM/VIB on Panfrost at living-room load has not been measured; the smoke gate and the P0/P1/P2 campaign remain open. This profile is an owner-approved operating configuration, not a ROCK Pi performance or qualification claim.
- `DEFER_FLUSH` is presence-based: setting it to `0` still enables it. Never "disable" it by value.
- The profile applies only at service start; changing it needs a restart.
- Disabling FX/RAW sampling makes the affected Orbit observations show NOT AVAILABLE (existing behaviour).

## 8. Architecture coordination
No contract, ownership or default change, so none is required for this configuration. Recommended: tell Architecture that the personal installation now runs the opt-in Tier C DOM/VIB path operationally before its Pi qualification is complete.
