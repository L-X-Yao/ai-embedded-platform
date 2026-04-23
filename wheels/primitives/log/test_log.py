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
    assert len(passes) == 11, f"expected 11 cases, got {len(passes)}: {passes}"


# --- release build verification (LOG_LEVEL_MAX=LOG_LEVEL_WARN) ---

WHEEL_DIR = Path(__file__).parent
RELEASE_BIN = WHEEL_DIR / "build_release" / "log_test"


@pytest.fixture(scope="module")
def release_sim_output():
    """Runs the test binary built with LOG_LEVEL_MAX=LOG_LEVEL_WARN.
    Verifies DEBUG/INFO macros truly expanded to ((void)0) — no emit,
    and (crucially) no arg evaluation (proves compile-time elimination).
    Auto-builds if the release binary is missing."""
    if not RELEASE_BIN.exists():
        subprocess.run(
            ["cmake", "-GNinja", "-S", str(WHEEL_DIR), "-B", str(WHEEL_DIR / "build_release"),
             "-DCMAKE_C_FLAGS=-DLOG_LEVEL_MAX=LOG_LEVEL_WARN"],
            check=True, capture_output=True, text=True, timeout=120,
        )
        subprocess.run(
            ["cmake", "--build", str(WHEEL_DIR / "build_release")],
            check=True, capture_output=True, text=True, timeout=120,
        )
    assert RELEASE_BIN.exists(), f"release binary not found after build: {RELEASE_BIN}"
    r = subprocess.run([str(RELEASE_BIN)], capture_output=True, text=True, timeout=10)
    return r.returncode, r.stdout


def test_release_build_eliminates_debug_info(release_sim_output):
    """At LOG_LEVEL_MAX=WARN, test_side_effect_at_default_level's
    internal #if selects the 'warn max: 2 args evaluated' assertion.
    If compile-time filter were broken (DEBUG/INFO still calling their
    side_effect args), counter would be 4 and the assertion fails.
    Thus: exit 0 on release build == compile-time elimination works."""
    rc, out = release_sim_output
    assert rc == 0, f"release build verification failed: rc={rc}\n{out}"
    assert "PASS: test_side_effect_at_default_level" in out, \
        f"side-effect test did not run in release build:\n{out}"
    assert "DONE: 0 failures" in out, f"expected 0 failures:\n{out}"
