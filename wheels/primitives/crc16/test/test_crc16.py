"""pytest harness for crc16 wheel.

Builds the C test binary in `build/` (CMake + Ninja) and runs it. Parses
PASS/FAIL lines from stdout into individual pytest cases for nice reporting.
"""

import shutil
import subprocess
from pathlib import Path

import pytest

TEST_DIR = Path(__file__).parent
BUILD_DIR = TEST_DIR / "build"
TEST_BIN = BUILD_DIR / "test_crc16"


@pytest.fixture(scope="session", autouse=True)
def build():
    """Configure + build once per pytest session."""
    BUILD_DIR.mkdir(exist_ok=True)
    cmake = shutil.which("cmake")
    assert cmake, "cmake not on PATH"
    subprocess.run(
        [cmake, "-GNinja", "-S", str(TEST_DIR), "-B", str(BUILD_DIR)],
        check=True,
        capture_output=True,
    )
    subprocess.run([cmake, "--build", str(BUILD_DIR)], check=True, capture_output=True)
    assert TEST_BIN.exists() or (BUILD_DIR / "test_crc16.exe").exists(), \
        f"test binary not produced at {TEST_BIN}"


def _run_tests():
    """Invoke the test binary and return its stdout + exit code."""
    bin_path = TEST_BIN if TEST_BIN.exists() else (BUILD_DIR / "test_crc16.exe")
    proc = subprocess.run([str(bin_path)], capture_output=True, text=True)
    return proc.stdout, proc.returncode


def test_all_c_tests_pass():
    """The C test harness must exit 0 (zero failures)."""
    stdout, rc = _run_tests()
    assert rc == 0, f"C tests reported failures:\n{stdout}"


@pytest.mark.parametrize(
    "marker",
    [
        "test_empty_buffer",
        "test_canonical_vector",
        "test_modbus_rtu_frame",
        "test_single_byte",
        "test_step_equivalence",
        "test_step_chunked",
        "test_fixed_pattern_regression",
        "test_all_zeros_buffer",
        "test_all_ones_buffer",
        "test_single_byte_all_distinct",
        "test_crc_residue_zero",
        "test_long_buffer_step",
    ],
)
def test_c_case_passed(marker):
    """Each named C test case must report PASS."""
    stdout, _ = _run_tests()
    line = f"PASS {marker}"
    assert line in stdout, f"missing or failing: {marker}\n--- output ---\n{stdout}"
