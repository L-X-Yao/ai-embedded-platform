"""pytest harness for log wheel.

Builds the C test binary via CMake+Ninja and runs it. Parses PASS/FAIL
lines into individual pytest cases. Also verifies compile-time level
elimination via a separate release build.
"""

import shutil
import subprocess
from pathlib import Path

import pytest

TEST_DIR = Path(__file__).parent
BUILD_DIR = TEST_DIR / "build"
TEST_BIN = BUILD_DIR / "test_log"

RELEASE_BUILD_DIR = TEST_DIR / "build_release"
RELEASE_BIN = RELEASE_BUILD_DIR / "test_log"


@pytest.fixture(scope="session", autouse=True)
def build():
    """Configure + build debug binary once per pytest session."""
    BUILD_DIR.mkdir(exist_ok=True)
    cmake = shutil.which("cmake")
    assert cmake, "cmake not on PATH"
    subprocess.run(
        [cmake, "-GNinja", "-S", str(TEST_DIR), "-B", str(BUILD_DIR)],
        check=True, capture_output=True,
    )
    subprocess.run(
        [cmake, "--build", str(BUILD_DIR)],
        check=True, capture_output=True,
    )
    assert TEST_BIN.exists() or (BUILD_DIR / "test_log.exe").exists(), \
        f"test binary not produced at {TEST_BIN}"


def _run_bin(bin_path):
    """Invoke a test binary and return (stdout, returncode)."""
    actual = bin_path if bin_path.exists() else bin_path.with_suffix(".exe")
    proc = subprocess.run([str(actual)], capture_output=True, text=True, timeout=10)
    return proc.stdout, proc.returncode


def test_all_c_tests_pass():
    """The C test harness must exit 0 (zero failures)."""
    stdout, rc = _run_bin(TEST_BIN)
    assert rc == 0, f"C tests reported failures:\n{stdout}"


@pytest.mark.parametrize("marker", [
    "test_runtime_level_get_set",
    "test_add_remove_sink",
    "test_emit_basic",
    "test_level_chars",
    "test_runtime_level_filter",
    "test_level_none_disables_all",
    "test_multi_sink",
    "test_no_sinks_no_crash",
    "test_time_fn_disable",
    "test_side_effect_at_default_level",
    "test_sink_slots_exhausted",
])
def test_c_case_passed(marker):
    """Each named C test case must report PASS."""
    stdout, _ = _run_bin(TEST_BIN)
    assert f"PASS {marker}" in stdout, \
        f"missing or failing: {marker}\n--- output ---\n{stdout}"


# --- release build verification (LOG_LEVEL_MAX=LOG_LEVEL_WARN) ---

@pytest.fixture(scope="session")
def release_build():
    """Build with LOG_LEVEL_MAX=WARN to verify compile-time elimination."""
    RELEASE_BUILD_DIR.mkdir(exist_ok=True)
    cmake = shutil.which("cmake")
    subprocess.run(
        [cmake, "-GNinja", "-S", str(TEST_DIR), "-B", str(RELEASE_BUILD_DIR),
         "-DCMAKE_C_FLAGS=-DLOG_LEVEL_MAX=LOG_LEVEL_WARN"],
        check=True, capture_output=True,
    )
    subprocess.run(
        [cmake, "--build", str(RELEASE_BUILD_DIR)],
        check=True, capture_output=True,
    )


def test_release_eliminates_debug_info(release_build):
    """At LOG_LEVEL_MAX=WARN, side_effect counter must be 2 (not 4).
    This proves DEBUG/INFO macros expanded to ((void)0) and argument
    evaluation was eliminated at compile time."""
    stdout, rc = _run_bin(RELEASE_BIN)
    assert rc == 0, f"release build failed: rc={rc}\n{stdout}"
    assert "PASS test_side_effect_at_default_level" in stdout, \
        f"side-effect test missing:\n{stdout}"
