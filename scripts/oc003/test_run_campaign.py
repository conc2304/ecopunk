#!/usr/bin/env python3
"""Failure-injection tests for run_campaign.py. NO hardware, NO GL, NO real systemd: a fake `systemctl`/`sudo` on PATH keeps the unit
state in a file, and the 'runtime' is a shell script that only sleeps / exits. Works from any location (the package or the repo).

  python3 scripts/oc003/test_run_campaign.py        (needs `timeout` and `pgrep` on PATH, like the runner)

Covered: refusal when the unit is active without --stop-service; ambiguous unit scope; unit absent; --no-service with a unit present;
binary without the instrumentation strings; tampered plan (DEFER_FLUSH=0, leaked managed variable); a run that exits non-zero; a run whose
evidence is missing; SIGTERM during a run (process tree ended, unit started exactly once, postflight matches).
NOT covered (needs real hardware): real systemd/sudo, the Pi's ps/pkill/dmesg, a real runtime's exit status under SIGINT.
"""
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, "run_campaign.py")
PLAN = os.path.join(HERE, "plan_interleaved.py")

SYSTEMCTL = r'''#!/bin/bash
st="${SHIM_STATE:?}"; a=("$@")
if [ "${a[0]}" = "--user" ]; then
  [ "${SHIM_USER_UNIT:-0}" = "1" ] || { echo "Unit not found" >&2; exit 4; }
  a=("${a[@]:1}")
else
  [ "${SHIM_NO_SYSTEM_UNIT:-0}" = "1" ] && { echo "Unit not found" >&2; exit 4; }
fi
case "${a[0]}" in
 cat) echo "[Service]"; echo "ExecStart=/fake/experience_runtime";;
 is-active) cat "$st";;
 is-enabled) echo enabled;;
 show) echo "Environment=FAKE=1 WorkingDirectory=/fake MainPID=1 NRestarts=0";;
 stop) echo inactive > "$st"; echo stop >> "$st.calls";;
 start) echo active > "$st"; echo start >> "$st.calls";;
esac
'''
def _read(path):
    with open(path) as fh:
        return fh.read()


def _write(path, text):
    with open(path, "w") as fh:
        fh.write(text)


def _jload(path):
    with open(path) as fh:
        return json.load(fh)


def _jdump(doc, path):
    with open(path, "w") as fh:
        json.dump(doc, fh)


MARKERS = "# raw_cfail EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH\n"


@unittest.skipUnless(shutil.which("timeout") and shutil.which("pgrep"), "needs timeout and pgrep")
class RunnerFailureInjection(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp(prefix="oc003_rc_")
        os.makedirs(os.path.join(self.d, "shim"))
        for name, body in (("systemctl", SYSTEMCTL), ("sudo", '#!/bin/bash\nexec "$@"\n')):
            p = os.path.join(self.d, "shim", name)
            _write(p, body)
            os.chmod(p, 0o755)
        self.state = os.path.join(self.d, "unit.state")
        _write(self.state, "active\n")
        _write(self.state + ".calls", "")
        self.env = dict(os.environ, PATH=os.path.join(self.d, "shim") + os.pathsep + os.environ["PATH"], SHIM_STATE=self.state)
        for k in [k for k in self.env if k.startswith("EXPERIENCE_RUNTIME_")]:
            del self.env[k]

    def tearDown(self):
        subprocess.run(["pkill", "-KILL", "-f", self.d], capture_output=True)
        shutil.rmtree(self.d, ignore_errors=True)

    def plan(self, body="sleep 100", markers=True, duration=3, conds="P0,P2,P2F"):
        binp = os.path.join(self.d, "fakebin.sh")
        _write(binp, "#!/bin/bash\n" + (MARKERS if markers else "") + body + "\n")
        os.chmod(binp, 0o755)
        out = os.path.join(self.d, "out")
        r = subprocess.run([sys.executable, PLAN, "--bin", binp, "--out", out, "--media-id", "media.auto.x", "--scene", "s", "--resolution", "1280x720",
                            "--conditions", conds, "--rounds", "1", "--duration", str(duration)], capture_output=True, text=True, env=self.env)
        self.assertEqual(r.returncode, 0, r.stderr)
        return os.path.join(out, "plan.json")

    def run_runner(self, plan, *extra, env=None, wait=True):
        cmd = [sys.executable, RUN, plan, "--workdir", self.d, "--display", ":0", "--grace", "5", "--settle", "1"] + list(extra)
        if not wait:
            return subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env or self.env)
        r = subprocess.run(cmd, capture_output=True, text=True, env=env or self.env, timeout=120)
        return r.returncode, r.stdout + r.stderr

    def calls(self):
        return _read(self.state + ".calls").split()

    def test_refuses_active_unit_without_stop_service(self):
        rc, out = self.run_runner(self.plan())
        self.assertEqual(rc, 1)
        self.assertIn("ACTIVE", out)
        self.assertEqual(self.calls(), [])
        self.assertEqual(_read(self.state).strip(), "active")

    def test_refuses_ambiguous_scope(self):
        env = dict(self.env, SHIM_USER_UNIT="1")
        rc, out = self.run_runner(self.plan(), "--stop-service", env=env)
        self.assertEqual(rc, 1)
        self.assertIn("need exactly one", out)
        self.assertEqual(self.calls(), [])

    def test_refuses_when_unit_not_found(self):
        env = dict(self.env, SHIM_NO_SYSTEM_UNIT="1")
        rc, out = self.run_runner(self.plan(), "--stop-service", env=env)
        self.assertEqual(rc, 1)
        self.assertIn("need exactly one", out)

    def test_no_service_refused_when_unit_exists(self):
        rc, out = self.run_runner(self.plan(), "--no-service")
        self.assertEqual(rc, 1)
        self.assertIn("exists in scope", out)

    def test_binary_without_probe_is_a_stop_with_route_b(self):
        rc, out = self.run_runner(self.plan(markers=False), "--stop-service")
        self.assertEqual(rc, 1)
        self.assertIn("NOT the attribution build", out)
        self.assertIn("Route B", out)
        self.assertEqual(self.calls(), [])

    def test_tampered_plan_flush_zero_is_refused(self):
        plan = self.plan()
        doc = _jload(plan)
        r = doc["runs"][1]
        r["command"] = r["command"].replace("-u EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH ", "EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH=0 ")
        _jdump(doc, plan)
        rc, out = self.run_runner(plan, "--stop-service")
        self.assertEqual(rc, 1)
        self.assertIn("presence-based", out)
        self.assertEqual(self.calls(), [])

    def test_leaked_managed_variable_is_refused(self):
        plan = self.plan()
        doc = _jload(plan)
        doc["runs"][0]["command"] = doc["runs"][0]["command"].replace("-u EXPERIENCE_RUNTIME_VSYNC", "")
        _jdump(doc, plan)
        rc, out = self.run_runner(plan, "--stop-service")
        self.assertEqual(rc, 1)
        self.assertIn("neither set nor unset", out)

    def test_nonzero_run_exit_aborts_and_restores(self):
        rc, out = self.run_runner(self.plan(body="exit 3"), "--stop-service")
        self.assertEqual(rc, 2, out)
        self.assertIn("exit status 3", out)
        self.assertEqual(self.calls(), ["stop", "start"])
        self.assertEqual(_read(self.state).strip(), "active")
        self.assertIn("MATCHES the pre-flight record", out)

    def test_missing_evidence_aborts_and_restores(self):
        rc, out = self.run_runner(self.plan(), "--stop-service")  # sleeps, ends by timeout (124), writes no CSV/phase log
        self.assertEqual(rc, 2, out)
        self.assertIn("missing or empty", out)
        self.assertEqual(self.calls(), ["stop", "start"])
        self.assertEqual(_jload(os.path.join(self.d, "out", "campaign", "state.json"))["state"], "ABORTED")

    def test_sigterm_during_run_ends_tree_and_restores(self):
        p = self.run_runner(self.plan(body="sleep 100", duration=60, conds="P0"), "--stop-service", wait=False)
        deadline = time.time() + 30
        while time.time() < deadline and "STATE RUNNING" not in (_read(os.path.join(self.d, "out", "campaign", "campaign.log"))
                                                                  if os.path.exists(os.path.join(self.d, "out", "campaign", "campaign.log")) else ""):
            time.sleep(0.5)
        time.sleep(1.5)
        p.send_signal(signal.SIGTERM)
        out, _ = p.communicate(timeout=90)
        self.assertEqual(p.returncode, 2, out)
        self.assertEqual(self.calls(), ["stop", "start"])
        self.assertEqual(_read(self.state).strip(), "active")
        self.assertIn("MATCHES the pre-flight record", out)
        left = subprocess.run(["pgrep", "-f", os.path.join(self.d, "fakebin.sh")], capture_output=True, text=True).stdout.split()
        self.assertEqual(left, [], "the run's process tree must be gone")


if __name__ == "__main__":
    unittest.main(verbosity=1)
