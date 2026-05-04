#!/usr/bin/env python3
"""
mutation_test_batch2.py — Fast mutation testing for 50 wheels (batch 2).

For each wheel:
  1. Compile baseline (gcc -std=c99 or gnu99)
  2. For each line in source .c, try ~18 mutation operators
  3. If mutation compiles and tests still pass → survived (test gap)
  4. If mutation causes test failure → killed (test effective)

Usage (from WSL):
  python3 tooling/test/mutation_test_batch2.py
"""

import os
import re
import subprocess
import sys
import tempfile
import shutil

REPO = "/mnt/d/temp/project/ai-embedded-platform"
HARNESS = os.path.join(REPO, "tooling/test")

# 50 wheels: (layer, name)
WHEELS = [
    # L0 primitives (17)
    ("primitives", "checksum"),
    ("primitives", "crc8"),
    ("primitives", "crc32"),
    ("primitives", "endian"),
    ("primitives", "hash_fnv"),
    ("primitives", "mahony_ahrs"),
    ("primitives", "mag_cal"),
    ("primitives", "moving_window"),
    ("primitives", "cli_parser"),
    ("primitives", "log_dedup"),
    ("primitives", "log_ringbuf"),
    ("primitives", "log_file"),
    ("primitives", "log_filter"),
    ("primitives", "log_rate"),
    ("primitives", "log_color"),
    ("primitives", "log_rtt"),
    ("primitives", "log_uart"),
    # L1 drivers (18)
    ("drivers", "bme280"),
    ("drivers", "bme680"),
    ("drivers", "sht30"),
    ("drivers", "lis3dh"),
    ("drivers", "ads1115"),
    ("drivers", "tmp102"),
    ("drivers", "ina219"),
    ("drivers", "w25qxx"),
    ("drivers", "ds3231"),
    ("drivers", "servo"),
    ("drivers", "stepper"),
    ("drivers", "gpio_button"),
    ("drivers", "rotary_encoder"),
    ("drivers", "ws2812"),
    ("drivers", "pca9685"),
    ("drivers", "drv8833"),
    ("drivers", "pcf8574"),
    ("drivers", "mcp23017"),
    # L2 protocols (10)
    ("protocols", "modbus_rtu"),
    ("protocols", "mqtt5_packet"),
    ("protocols", "lorawan_packet"),
    ("protocols", "nmea_parser"),
    ("protocols", "websocket_frame"),
    ("protocols", "at_command"),
    ("protocols", "bencode"),
    ("protocols", "protobuf_lite"),
    ("protocols", "syslog_packet"),
    ("protocols", "dns_packet"),
    # L3 modules (5)
    ("modules", "cli_shell"),
    ("modules", "data_logger"),
    ("modules", "sensor_hub"),
    ("modules", "error_handler"),
    ("modules", "config_store"),
]

# Patterns that indicate a line is not real code (skip mutation)
SKIP_LINE_RE = re.compile(
    r'^\s*($|//|/\*|\*|#include|#define|#ifndef|#endif|#ifdef|#else|#elif|#pragma|#undef|#if\b)'
)

# Mutation operators: (name, regex_pattern, replacement)
# Each tries to match on a code line and produce a single mutation
MUTATIONS = [
    # Relational boundary
    ("LE_to_LT",  re.compile(r'(?<![<>!])<=(?!=)'), '<' ),
    ("GE_to_GT",  re.compile(r'(?<![<>!])>=(?!=)'), '>' ),
    ("EQ_to_NE",  re.compile(r'(?<![!=])=='      ), '!='),
    ("NE_to_EQ",  re.compile(r'!=(?!=)'           ), '=='),
    # Arithmetic
    ("PLUS_to_MINUS",  re.compile(r'(?<!\+)\+ (?!\+|=)'), '- '),
    ("MINUS_to_PLUS",  re.compile(r'(?<!-)- (?![-=>])' ), '+ '),
    # Increment/decrement
    ("INC_to_DEC", re.compile(r'\+\+'), '--'),
    ("DEC_to_INC", re.compile(r'(?<!-)--(?!>)'), '++'),
    # Logical
    ("AND_to_OR",  re.compile(r'&&'), '||'),
    ("OR_to_AND",  re.compile(r'\|\|'), '&&'),
    # Shift
    ("LSHIFT_to_RSHIFT", re.compile(r'<<(?!=)'), '>>'),
    ("RSHIFT_to_LSHIFT", re.compile(r'>>(?!=)'), '<<'),
    # Bitwise
    ("BITOR_to_BITAND",  re.compile(r'(?<!\|)\|(?!\||=)'), '&'),
    ("BITAND_to_BITOR",  re.compile(r'(?<!&)&(?!&|=)'   ), '|'),
    # Return value
    ("RET0_to_RET1",   re.compile(r'return\s+0\s*;' ), 'return 1;'),
    ("RETn1_to_RET0",  re.compile(r'return\s+-1\s*;'), 'return 0;'),
    ("RET1_to_RET0",   re.compile(r'return\s+1\s*;' ), 'return 0;'),
    # Negate condition
    ("TRUE_to_FALSE", re.compile(r'return\s+true\s*;'), 'return false;'),
    ("FALSE_to_TRUE", re.compile(r'return\s+false\s*;'), 'return true;'),
]


def try_compile(src_path, test_path, extra_includes, extra_srcs, out_bin, std="c99"):
    """Try to compile; return True on success."""
    cmd = ["gcc", f"-std={std}", "-w", "-O0"] + extra_includes + \
          [test_path, src_path] + extra_srcs + ["-o", out_bin, "-lm"]
    result = subprocess.run(cmd, capture_output=True, timeout=30)
    return result.returncode == 0


def run_test(bin_path):
    """Run test binary; return True if tests pass (exit 0)."""
    try:
        result = subprocess.run([bin_path], capture_output=True, timeout=10)
        return result.returncode == 0
    except subprocess.TimeoutExpired:
        return False


def test_wheel(layer, name, tmpdir):
    """Run mutation testing on a single wheel. Returns dict with results."""
    wheel_dir = os.path.join(REPO, "wheels", layer, name)
    src_path  = os.path.join(wheel_dir, f"{name}.c")
    test_path = os.path.join(wheel_dir, "test", f"test_{name}.c")

    extra_includes = [f"-I{wheel_dir}", f"-I{HARNESS}"]
    extra_srcs = []

    # Handle cross-wheel dependencies
    if name == "modbus_rtu":
        crc16_dir = os.path.join(REPO, "wheels/primitives/crc16")
        extra_includes.append(f"-I{crc16_dir}")
        extra_srcs.append(os.path.join(crc16_dir, "crc16.c"))

    work = os.path.join(tmpdir, name)
    os.makedirs(work, exist_ok=True)
    base_bin = os.path.join(work, "base")
    mut_src  = os.path.join(work, "mut.c")
    mut_bin  = os.path.join(work, "mut_bin")

    # Determine working std
    std = "c99"
    if not try_compile(src_path, test_path, extra_includes, extra_srcs, base_bin, "c99"):
        if not try_compile(src_path, test_path, extra_includes, extra_srcs, base_bin, "gnu99"):
            return {"name": name, "layer": layer, "total": 0, "killed": 0,
                    "survived": 0, "skip": 0, "status": "COMPILE_FAIL"}
        std = "gnu99"

    # Verify baseline passes
    if not run_test(base_bin):
        return {"name": name, "layer": layer, "total": 0, "killed": 0,
                "survived": 0, "skip": 0, "status": "TEST_FAIL"}

    # Read source
    with open(src_path, "r") as f:
        lines = f.readlines()

    total = 0
    killed = 0
    survived = 0
    skip_compile = 0
    survived_details = []

    for i, line in enumerate(lines):
        # Skip non-code lines
        if SKIP_LINE_RE.match(line):
            continue

        for mut_name, pattern, replacement in MUTATIONS:
            m = pattern.search(line)
            if not m:
                continue

            # Apply mutation (first match only)
            mutated_line = line[:m.start()] + replacement + line[m.end():]
            if mutated_line == line:
                continue

            # Write mutated source
            mutated_lines = list(lines)
            mutated_lines[i] = mutated_line
            with open(mut_src, "w") as f:
                f.writelines(mutated_lines)

            # Try compile
            if not try_compile(mut_src, test_path, extra_includes, extra_srcs, mut_bin, std):
                skip_compile += 1
                continue

            total += 1
            if run_test(mut_bin):
                survived += 1
                survived_details.append((i+1, mut_name, line.strip(), mutated_line.strip()))
            else:
                killed += 1

    rate = (killed / total * 100) if total > 0 else 0.0
    return {"name": name, "layer": layer, "total": total, "killed": killed,
            "survived": survived, "skip": skip_compile, "rate": rate,
            "status": "OK", "survived_details": survived_details}


def main():
    tmpdir = tempfile.mkdtemp(prefix="mutation_b2_")
    print("=" * 72)
    print(" Mutation Testing — Batch 2 (50 wheels)")
    print("=" * 72)
    print(f" Temp dir: {tmpdir}")
    print()

    results = []
    for idx, (layer, name) in enumerate(WHEELS, 1):
        label = f"[{idx:2d}/50]"
        sys.stdout.write(f"{label} {layer}/{name}... ")
        sys.stdout.flush()
        try:
            r = test_wheel(layer, name, tmpdir)
        except Exception as e:
            r = {"name": name, "layer": layer, "total": 0, "killed": 0,
                 "survived": 0, "skip": 0, "rate": 0, "status": f"ERROR: {e}",
                 "survived_details": []}
        results.append(r)
        if r.get("status") in ("COMPILE_FAIL", "TEST_FAIL"):
            print(f"SKIP ({r['status']})")
        else:
            print(f"{r['total']:3d} mutations, {r['killed']:3d} killed, "
                  f"{r['survived']:3d} survived  -> {r.get('rate', 0):.1f}%")

    # Print summary table
    print()
    print("=" * 72)
    print(" SUMMARY TABLE")
    print("=" * 72)
    print()
    header = f"{'Wheel':<22} {'Layer':<12} {'Total':>6} {'Killed':>7} {'Surv':>6} {'Rate':>7}"
    print(header)
    print("-" * len(header))

    total_all = 0
    killed_all = 0
    survived_all = 0
    tested_count = 0
    skip_count = 0

    for r in results:
        if r.get("status") in ("COMPILE_FAIL", "TEST_FAIL") or "ERROR" in r.get("status", ""):
            print(f"{r['name']:<22} {r['layer']:<12} {'SKIP':>6} {'':>7} {'':>6} {'N/A':>7}")
            skip_count += 1
        else:
            rate_str = f"{r.get('rate', 0):.1f}%"
            print(f"{r['name']:<22} {r['layer']:<12} {r['total']:>6d} {r['killed']:>7d} {r['survived']:>6d} {rate_str:>7}")
            total_all += r['total']
            killed_all += r['killed']
            survived_all += r['survived']
            tested_count += 1

    print("-" * len(header))
    overall_rate = (killed_all / total_all * 100) if total_all > 0 else 0
    print(f"{'TOTAL':<22} {'':12} {total_all:>6d} {killed_all:>7d} {survived_all:>6d} {overall_rate:>6.1f}%")
    print(f"\nWheels tested: {tested_count}, Skipped: {skip_count}")

    # Print survived mutants for investigation
    print()
    print("=" * 72)
    print(" SURVIVING MUTANTS (test gaps)")
    print("=" * 72)
    for r in results:
        details = r.get("survived_details", [])
        if not details:
            continue
        print(f"\n--- {r['layer']}/{r['name']} ({len(details)} survivors) ---")
        for lineno, mut_name, orig, mutated in details[:10]:
            print(f"  L{lineno} [{mut_name}]")
            print(f"    orig:    {orig}")
            print(f"    mutant:  {mutated}")
        if len(details) > 10:
            print(f"  ... and {len(details) - 10} more")

    # Cleanup
    shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    main()
