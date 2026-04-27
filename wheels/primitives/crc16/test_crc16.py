"""pytest harness for crc16 wheel.

Builds the C test binary in `build/` (CMake + Ninja) and runs it. Parses
PASS/FAIL lines from stdout into individual pytest cases for nice reporting.
"""

import shutil
import subprocess
from pathlib import Path

import pytest

WHEEL_DIR = Path(__file__).parent
BUILD_DIR = WHEEL_DIR / "build"
TEST_BIN = BUILD_DIR / "test_crc16"


@pytest.fixture(scope="session", autouse=True)
def build():
    """Configure + build once per pytest session."""
    BUILD_DIR.mkdir(exist_ok=True)
    cmake = shutil.which("cmake")
    assert cmake, "cmake not on PATH"
    subprocess.run(
        [cmake, "-GNinja", "-S", str(WHEEL_DIR), "-B", str(BUILD_DIR)],
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
        "empty buffer returns 0xFFFF",
        "empty with non-NULL pointer",
        "123456789 -> 0x4B37",
        "modbus rtu read-10-regs frame -> 0xCDC5",
        "single 0x00 -> 0x40BF",
        "single 0xFF -> 0x00FF",
        "step API equals bulk API",
        "chunked step matches bulk",
        "flybox 34-byte deterministic frame CRC stable",
    ],
)
def test_c_case_passed(marker):
    """Each named C test case must report [PASS]."""
    stdout, _ = _run_tests()
    line = f"[PASS] {marker}"
    assert line in stdout, f"missing or failing: {marker}\n--- output ---\n{stdout}"
