"""Pytest wrapper around the in-sim log test harness."""
import subprocess
import re
from pathlib import Path
import pytest

BIN = Path(__file__).parent / "build" / "log_test"


@pytest.fixture(scope="module")
def sim_output():
    assert BIN.exists(), f"binary not built: {BIN}"
    r = subprocess.run([str(BIN)], capture_output=True, text=True, timeout=10)
    return r.returncode, r.stdout


def test_sim_exits_cleanly(sim_output):
    rc, out = sim_output
    assert "DONE:" in out, f"harness did not complete: {out!r}"
    assert rc == 0, f"non-zero exit rc={rc}\n{out}"


def test_all_cases_report_pass(sim_output):
    _, out = sim_output
    fails = re.findall(r"^FAIL: (\w+): (.+)$", out, re.MULTILINE)
    assert not fails, f"failures: {fails}"


def test_expected_case_count(sim_output):
    _, out = sim_output
    passes = re.findall(r"^PASS: (\w+)$", out, re.MULTILINE)
    assert len(passes) == 10, f"expected 10 cases, got {len(passes)}: {passes}"
