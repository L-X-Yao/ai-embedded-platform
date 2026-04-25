#!/usr/bin/env python3
"""
HIL (Hardware-in-the-Loop) script for flybox VSF firmware.

Flash via OpenOCD (CMSIS-DAP) -> capture debug UART -> report result.

Usage:
    python hil_flash_and_verify.py [--port COM25] [--bin path/to/flybox_merged.bin]
"""

import argparse
import subprocess
import sys
import time
import serial


def flash_firmware(bin_path, base_addr=0x08000000):
    print(f"[HIL] Flashing {bin_path} @ 0x{base_addr:08X} ...")
    cmd = [
        "openocd",
        "-f", "interface/cmsis-dap.cfg",
        "-f", "target/stm32f4x.cfg",
        "-c", f"program {bin_path} verify 0x{base_addr:08X} reset exit",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    output = result.stderr + result.stdout
    if "verified" in output.lower() or result.returncode == 0:
        print(f"[HIL] Flash OK")
        return True
    print(f"[HIL] Flash FAILED:\n{output[-500:]}")
    return False


def reset_target():
    print("[HIL] Resetting target ...")
    cmd = [
        "openocd",
        "-f", "interface/cmsis-dap.cfg",
        "-f", "target/stm32f4x.cfg",
        "-c", "init; reset run; shutdown",
    ]
    subprocess.run(cmd, capture_output=True, text=True, timeout=10)


def capture_serial(port, baudrate=115200, duration=8):
    lines = []
    print(f"[HIL] Capturing {port} @ {baudrate} for {duration}s ...")
    try:
        with serial.Serial(port, baudrate, timeout=1) as ser:
            ser.reset_input_buffer()
            deadline = time.time() + duration
            while time.time() < deadline:
                raw = ser.readline()
                if raw:
                    line = raw.decode("utf-8", errors="replace").rstrip()
                    lines.append(line)
                    print(f"  > {line}")
    except serial.SerialException as e:
        print(f"[HIL] Serial error: {e}")
    return lines


def analyze_output(lines):
    text = "\n".join(lines)

    if not lines:
        return False, "NO_OUTPUT", "MCU may be stuck in reset, UART not connected, or wrong COM port"

    non_printable = sum(1 for c in text if ord(c) < 0x20 and c not in "\r\n\t")
    if non_printable > len(text) * 0.3:
        return False, "GARBAGE", "Likely baud rate mismatch or clock config wrong"

    if "[SYS] Flybox V0.1-vsf" in text:
        if "All tasks created" in text:
            return True, "BOOT_OK", "VSF kernel + Linux shell + tasks all started"
        return False, "PARTIAL_BOOT", "Banner seen but tasks not confirmed"

    if "HardFault" in text or "MemManage" in text or "BusFault" in text:
        return False, "CRASH", f"Fault handler triggered"

    return False, "UNEXPECTED", f"Got {len(lines)} lines but no boot banner"


def main():
    parser = argparse.ArgumentParser(description="Flybox HIL flash & verify")
    parser.add_argument("--port", default="COM25", help="Debug UART COM port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--bin", default="projects/flybox/vsf_build/build/flybox_merged.bin")
    parser.add_argument("--addr", default="0x08000000", help="Flash base address")
    parser.add_argument("--duration", type=int, default=8, help="Serial capture seconds")
    parser.add_argument("--skip-flash", action="store_true")
    args = parser.parse_args()

    if not args.skip_flash:
        if not flash_firmware(args.bin, int(args.addr, 16)):
            sys.exit(1)
    else:
        reset_target()

    time.sleep(0.5)
    lines = capture_serial(args.port, args.baud, args.duration)
    success, code, summary = analyze_output(lines)

    print(f"\n{'='*60}")
    print(f"[HIL] {code}: {summary}")
    if lines and not success:
        print(f"[HIL] Raw output ({len(lines)} lines):")
        for l in lines[:20]:
            print(f"  | {l}")
    print(f"{'='*60}")

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
