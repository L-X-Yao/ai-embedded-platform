"""Pytest regression harness around the mps2_bare QEMU sim.

Guards the full integration chain: incremental ninja build + link +
VSF kernel schedule + teda+sem business + log wheel sink routing +
semihosting exit. A regression in any layer shows up here first.

First-time setup (build dir must exist before pytest can run ninja):
    cmake -GNinja -S . -B build

Then:
    python3 -m pytest test_mps2_bare.py -v
"""
import subprocess
import re
from pathlib import Path
import pytest

SCAFFOLD = Path(__file__).parent
BUILD = SCAFFOLD / "build"
ELF = BUILD / "vsf_mps2_bare.elf"

QEMU_CMD = [
    "qemu-system-arm",
    "-M", "mps2-an500",
    "-cpu", "cortex-m7",
    "-nographic",
    "-semihosting",
    "-kernel", str(ELF),
]


@pytest.fixture(scope="module")
def sim_output():
    assert BUILD.exists(), (
        f"build dir missing — first run requires: "
        f"cmake -GNinja -S {SCAFFOLD} -B {BUILD}"
    )
    subprocess.run(
        ["ninja"], cwd=BUILD, check=True, capture_output=True, text=True,
    )
    assert ELF.exists(), f"elf missing after ninja: {ELF}"
    r = subprocess.run(QEMU_CMD, capture_output=True, text=True, timeout=10)
    # -nographic + -semihosting: SYS_WRITEC (bkpt 0xAB / r0=0x03) bytes emerge
    # on stderr, not stdout. (Verified QEMU 8.2.2.)
    return r.returncode, r.stderr


def test_qemu_exits_zero(sim_output):
    rc, out = sim_output
    assert rc == 0, f"non-zero exit rc={rc}\n{out}"


def test_every_line_uses_log_wheel_prefix(sim_output):
    _, out = sim_output
    lines = [l for l in out.splitlines() if l.strip()]
    assert lines, "no output"
    # [XXXXXXXX] [L] [module] — proves bytes went through log_emit + our sink,
    # not raw printf / vsf_stdout_putchar.
    pattern = re.compile(r"^\[\d{8}\] \[[EWID]\] \[kerneltest\] ")
    bad = [l for l in lines if not pattern.match(l)]
    assert not bad, f"{len(bad)} line(s) missing log wheel prefix: {bad}"


def test_three_ping_pongs(sim_output):
    _, out = sim_output
    posts = re.findall(r"sender post #(\d+)", out)
    gets = re.findall(r"recver got #(\d+)", out)
    assert posts == ["1", "2", "3"], f"sender posts: {posts}"
    assert gets == ["1", "2", "3"], f"recver gets: {gets}"


def test_pass_line_present(sim_output):
    _, out = sim_output
    assert "pass: 3 sem ping-pongs completed" in out, \
        f"missing pass line:\n{out}"


def test_timestamps_monotonic(sim_output):
    _, out = sim_output
    ts = [int(m) for m in re.findall(r"^\[(\d{8})\]", out, re.MULTILINE)]
    assert ts, "no timestamped log lines"
    assert ts == sorted(ts), f"timestamps not monotonic: {ts}"
