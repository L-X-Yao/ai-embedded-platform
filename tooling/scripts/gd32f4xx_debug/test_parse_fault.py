"""Tests for parse_fault.py — Cortex-M4 fault register decoder."""
import subprocess
import sys
import os

SCRIPT = os.path.join(os.path.dirname(__file__), "parse_fault.py")

def run(args, stdin_text=None):
    cmd = [sys.executable, SCRIPT] + args
    r = subprocess.run(cmd, capture_output=True, text=True, input=stdin_text)
    return r.stdout, r.stderr, r.returncode

# ---------- bit decode tests ----------

def test_stack_overflow():
    out, _, rc = run(["--cfsr", "0x00000010", "--hfsr", "0x40000000"])
    assert rc == 0
    assert "MemManage" in out or "MSTKERR" in out
    assert "STACK OVERFLOW" in out or "stacking" in out.lower()

def test_null_pointer_deref():
    out, _, rc = run(["--cfsr", "0x00000082", "--mmfar", "0x00000000"])
    assert rc == 0
    assert "DACCVIOL" in out
    assert "MMARVALID" in out
    assert "0x00000000" in out

def test_undefined_instruction():
    out, _, rc = run(["--cfsr", "0x00010000", "--hfsr", "0x40000000"])
    assert rc == 0
    assert "UNDEFINSTR" in out
    assert "UsageFault" in out or "HardFault" in out

def test_fpu_not_enabled():
    out, _, rc = run(["--cfsr", "0x00080000"])
    assert rc == 0
    assert "NOCP" in out
    assert "FPU" in out

def test_divide_by_zero():
    out, _, rc = run(["--cfsr", "0x02000000"])
    assert rc == 0
    assert "DIVBYZERO" in out

def test_unaligned_access():
    out, _, rc = run(["--cfsr", "0x01000000"])
    assert rc == 0
    assert "UNALIGNED" in out

def test_bus_fault_precise():
    out, _, rc = run(["--cfsr", "0x00000200", "--bfar", "0x60000000"])
    assert rc == 0
    assert "PRECISERR" in out
    assert "BusFault" in out

def test_bus_fault_stacking():
    out, _, rc = run(["--cfsr", "0x00001000"])
    assert rc == 0
    assert "STKERR" in out
    assert "stack" in out.lower()

def test_hardfault_escalated():
    out, _, rc = run(["--cfsr", "0x00020000", "--hfsr", "0x40000000"])
    assert rc == 0
    assert "FORCED" in out
    assert "HardFault" in out

def test_hardfault_vector_table():
    out, _, rc = run(["--hfsr", "0x00000002"])
    assert rc == 0
    assert "VECTTBL" in out
    assert "vector" in out.lower()

def test_hardfault_debug():
    out, _, rc = run(["--hfsr", "0x80000000"])
    assert rc == 0
    assert "DEBUGEVT" in out
    assert "BKPT" in out

def test_pc_and_lr_output():
    out, _, rc = run(["--cfsr", "0x00010000", "--pc", "0x08012345", "--lr", "0xFFFFFFF9"])
    assert rc == 0
    assert "0x08012345" in out
    assert "MSP" in out  # EXC_RETURN bit 2 = 0 → MSP

def test_lr_psp():
    out, _, rc = run(["--cfsr", "0x00010000", "--lr", "0xFFFFFFFD"])
    assert rc == 0
    assert "PSP" in out  # EXC_RETURN bit 2 = 1 → PSP

def test_all_zero_watchdog():
    out, _, rc = run(["--cfsr", "0x00000000", "--hfsr", "0x00000000"])
    assert rc == 0
    assert "watchdog" in out.lower() or "zero" in out.lower()

# ---------- stdin parsing ----------

def test_stdin_mode():
    stdin_text = "CFSR=0x00020000 HFSR=0x40000000 PC=0x08001234"
    out, _, rc = run(["--stdin"], stdin_text=stdin_text)
    assert rc == 0
    assert "INVSTATE" in out
    assert "0x08001234" in out

# ---------- error handling ----------

def test_no_args_fails():
    _, _, rc = run([])
    assert rc == 1

if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])
