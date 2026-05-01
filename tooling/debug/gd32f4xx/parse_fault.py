#!/usr/bin/env python3
"""parse_fault.py — Cortex-M4 fault register decoder.

Offline alternative to cortex_m4.gdbinit's fault_full: feed register values
from serial log, GDB output, or manual input. Outputs fault classification,
probable cause, and suggested GDB commands.

Usage:
  parse_fault.py --cfsr 0x00020000 --hfsr 0x40000000
  parse_fault.py --cfsr 0x00000082 --mmfar 0x00000000
  parse_fault.py --cfsr 0x00020000 --lr 0xFFFFFFF9 --pc 0x08012345
  echo "CFSR=0x00020000 HFSR=0x40000000" | parse_fault.py --stdin

All register arguments are optional; only provided values are analyzed.
"""

import argparse
import re
import sys


def parse_hex(s: str) -> int:
    s = s.strip().lower()
    if s.startswith("0x"):
        return int(s, 16)
    return int(s, 16) if all(c in "0123456789abcdef" for c in s) else int(s)


# ---------- CFSR bit decode ---------------------------------------------------

MMFSR_BITS = [
    (0x01, "IACCVIOL", "Instruction fetch from non-executable region"),
    (0x02, "DACCVIOL", "Data access violation (MPU or unprivileged access)"),
    (0x08, "MUNSTKERR", "Fault during exception return (unstacking)"),
    (0x10, "MSTKERR", "Fault during exception entry (stacking — stack overflow?)"),
    (0x80, "MMARVALID", "MMFAR holds the faulting address"),
]

BFSR_BITS = [
    (0x01, "IBUSERR", "Instruction bus error (bad code fetch address)"),
    (0x02, "PRECISERR", "Precise data bus error — BFAR is valid"),
    (0x04, "IMPRECISERR", "Imprecise data bus error — BFAR may not be valid"),
    (0x08, "UNSTKERR", "Bus fault during unstacking"),
    (0x10, "STKERR", "Bus fault during stacking (stack pointer corrupt?)"),
    (0x80, "BFARVALID", "BFAR holds the faulting address"),
]

UFSR_BITS = [
    (0x0001, "UNDEFINSTR", "Undefined instruction (corrupt code or Thumb/ARM mismatch)"),
    (0x0002, "INVSTATE", "Invalid execution state (EPSR.T bit, or IT block error)"),
    (0x0004, "INVPC", "Invalid PC load (bad EXC_RETURN or branch to odd address)"),
    (0x0008, "NOCP", "Coprocessor access — FPU not enabled or not present"),
    (0x0100, "UNALIGNED", "Unaligned memory access"),
    (0x0200, "DIVBYZERO", "Integer divide by zero"),
]

HFSR_BITS = [
    (0x00000002, "VECTTBL", "Bus fault on vector table read"),
    (0x40000000, "FORCED", "Escalated from a configurable fault (check CFSR)"),
    (0x80000000, "DEBUGEVT", "Debug event (BKPT with no debugger attached)"),
]


def decode_bits(value, table):
    hits = []
    for mask, name, desc in table:
        if value & mask:
            hits.append((name, desc))
    return hits


# ---------- High-level classification -----------------------------------------

def classify(cfsr, hfsr, mmfar, bfar, lr, pc):
    """Return (fault_type, probable_causes, gdb_commands)."""
    causes = []
    gdb = []
    fault_type = "Unknown"

    mmfsr = cfsr & 0xFF if cfsr else 0
    bfsr = (cfsr >> 8) & 0xFF if cfsr else 0
    ufsr = (cfsr >> 16) & 0xFFFF if cfsr else 0

    if ufsr:
        fault_type = "UsageFault"
        if ufsr & 0x0001:
            causes.append("Undefined instruction — corrupted code, wrong Flash bank, or ARM/Thumb mismatch")
        if ufsr & 0x0002:
            causes.append("Invalid state — likely branched to even address (bit 0 = 0 means ARM mode on M-profile)")
        if ufsr & 0x0004:
            causes.append("Invalid PC load — corrupt function pointer or bad EXC_RETURN")
        if ufsr & 0x0008:
            causes.append("FPU access with FPU disabled — check CPACR (0xE000ED88)")
            gdb.append("x/x 0xE000ED88  # CPACR — should have bits [23:20] = 0xF for full FPU access")
        if ufsr & 0x0100:
            causes.append("Unaligned access — packed struct or cast to wider pointer without __attribute__((packed))")
        if ufsr & 0x0200:
            causes.append("Integer divide by zero — check DIV_0_TRP in CCR, or caller passed 0 as divisor")

    if bfsr:
        fault_type = "BusFault"
        if bfsr & 0x10:
            causes.append("Bus fault during stacking — stack pointer likely corrupt or overflowed into invalid memory")
            gdb.append("info reg msp psp  # check if stack pointer is in valid SRAM range")
        if bfsr & 0x02:
            causes.append(f"Precise bus error at {'0x%08X' % bfar if bfar else 'BFAR'} — read/write to unmapped address")
            gdb.append("info symbol *(uint32_t*)($psp + 0x18)  # faulting PC")
        if bfsr & 0x04:
            causes.append("Imprecise bus error — happened 1-3 instructions before the fault was taken; BFAR unreliable")
        if bfsr & 0x01:
            causes.append("Instruction bus error — PC points to invalid memory (bad jump or corrupted return address)")

    if mmfsr:
        fault_type = "MemManage"
        if mmfsr & 0x10:
            causes.append("MemManage stacking fault — STACK OVERFLOW (SP crossed MPU boundary)")
            gdb.append("info reg msp psp sp  # check if SP is below _estack or thread stack base")
            gdb.append("# Compare with: python3 stack_analyzer.py build --task-config stack_tasks.flybox.json")
        if mmfsr & 0x02:
            causes.append(f"Data access violation at {'0x%08X' % mmfar if mmfar else 'MMFAR'} — MPU or NULL deref")
        if mmfsr & 0x01:
            causes.append("Instruction access violation — executing from non-executable region")

    if hfsr:
        if hfsr & 0x40000000:
            if not causes:
                fault_type = "HardFault (escalated)"
                causes.append("Escalated from configurable fault — CFSR should have the root cause")
            else:
                fault_type = f"HardFault <- {fault_type}"
        if hfsr & 0x00000002:
            fault_type = "HardFault (vector table)"
            causes.append("Vector table read failed — VTOR corrupt or Flash not accessible")
            gdb.append("x/x 0xE000ED08  # VTOR — should point to valid Flash")
        if hfsr & 0x80000000:
            fault_type = "HardFault (debug)"
            causes.append("BKPT hit with no debugger — stale breakpoint in Flash?")

    if not causes:
        if cfsr == 0 and hfsr == 0:
            causes.append("All fault status registers are zero — might be watchdog reset, not a fault")
        else:
            causes.append("Unrecognized fault pattern")

    # Standard GDB commands
    gdb.append("fault_full  # if cortex_m4.gdbinit is sourced")
    if pc:
        gdb.append(f"info symbol 0x{pc:08X}  # look up faulting PC")
        gdb.append(f"disas 0x{pc:08X}-16, 0x{pc:08X}+16  # disassemble around fault")
    if lr and (lr & 0xFFFFFFF0) == 0xFFFFFFF0:
        sp_hint = "PSP" if lr & 0x4 else "MSP"
        gdb.append(f"# EXC_RETURN=0x{lr:08X} → using {sp_hint} after return")
    elif lr:
        gdb.append(f"info symbol 0x{lr:08X}  # caller (if LR is return address, not EXC_RETURN)")

    return fault_type, causes, gdb


# ---------- stdin parsing -----------------------------------------------------

def parse_stdin():
    """Parse 'KEY=VALUE' pairs from stdin text."""
    text = sys.stdin.read()
    vals = {}
    for m in re.finditer(r'(CFSR|HFSR|MMFAR|BFAR|LR|PC)\s*[=:]\s*(0x[0-9a-fA-F]+|\d+)', text, re.I):
        vals[m.group(1).upper()] = parse_hex(m.group(2))
    return vals


# ---------- main --------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(description="Cortex-M4 fault register decoder")
    p.add_argument("--cfsr",  help="CFSR value (hex)")
    p.add_argument("--hfsr",  help="HFSR value (hex)")
    p.add_argument("--mmfar", help="MMFAR value (hex)")
    p.add_argument("--bfar",  help="BFAR value (hex)")
    p.add_argument("--lr",    help="LR value (hex)")
    p.add_argument("--pc",    help="PC value (hex)")
    p.add_argument("--stdin", action="store_true",
                   help="Parse register values from stdin (KEY=0xVALUE format)")
    args = p.parse_args()

    if args.stdin:
        vals = parse_stdin()
        cfsr  = vals.get("CFSR")
        hfsr  = vals.get("HFSR")
        mmfar = vals.get("MMFAR")
        bfar  = vals.get("BFAR")
        lr    = vals.get("LR")
        pc    = vals.get("PC")
    else:
        cfsr  = parse_hex(args.cfsr) if args.cfsr else None
        hfsr  = parse_hex(args.hfsr) if args.hfsr else None
        mmfar = parse_hex(args.mmfar) if args.mmfar else None
        bfar  = parse_hex(args.bfar) if args.bfar else None
        lr    = parse_hex(args.lr) if args.lr else None
        pc    = parse_hex(args.pc) if args.pc else None

    if all(v is None for v in (cfsr, hfsr, mmfar, bfar, lr, pc)):
        print("Error: no register values provided. Use --cfsr, --hfsr, etc. or --stdin.")
        sys.exit(1)

    print("=== Input ===")
    if cfsr is not None:  print(f"  CFSR  = 0x{cfsr:08X}")
    if hfsr is not None:  print(f"  HFSR  = 0x{hfsr:08X}")
    if mmfar is not None: print(f"  MMFAR = 0x{mmfar:08X}")
    if bfar is not None:  print(f"  BFAR  = 0x{bfar:08X}")
    if lr is not None:    print(f"  LR    = 0x{lr:08X}")
    if pc is not None:    print(f"  PC    = 0x{pc:08X}")

    # Bit-level decode
    if cfsr is not None:
        mmfsr_val = cfsr & 0xFF
        bfsr_val = (cfsr >> 8) & 0xFF
        ufsr_val = (cfsr >> 16) & 0xFFFF

        if mmfsr_val:
            print(f"\n--- MemManage (MMFSR=0x{mmfsr_val:02X}) ---")
            for name, desc in decode_bits(mmfsr_val, MMFSR_BITS):
                print(f"  {name:14s}  {desc}")
        if bfsr_val:
            print(f"\n--- BusFault (BFSR=0x{bfsr_val:02X}) ---")
            for name, desc in decode_bits(bfsr_val, BFSR_BITS):
                print(f"  {name:14s}  {desc}")
        if ufsr_val:
            print(f"\n--- UsageFault (UFSR=0x{ufsr_val:04X}) ---")
            for name, desc in decode_bits(ufsr_val, UFSR_BITS):
                print(f"  {name:14s}  {desc}")

    if hfsr is not None and hfsr:
        print(f"\n--- HardFault (HFSR=0x{hfsr:08X}) ---")
        for name, desc in decode_bits(hfsr, HFSR_BITS):
            print(f"  {name:14s}  {desc}")

    # High-level classification
    fault_type, causes, gdb_cmds = classify(
        cfsr or 0, hfsr or 0, mmfar, bfar, lr, pc)

    print(f"\n=== Classification: {fault_type} ===")
    for i, c in enumerate(causes, 1):
        print(f"  {i}. {c}")

    print("\n=== Suggested GDB commands ===")
    for cmd in gdb_cmds:
        print(f"  {cmd}")


if __name__ == "__main__":
    main()
