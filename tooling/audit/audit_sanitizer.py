#!/usr/bin/env python3
"""
audit_sanitizer.py -- build and run wheel tests under AddressSanitizer + UBSan.

For each wheel with test/CMakeLists.txt, builds with
  -fsanitize=address,undefined -fno-sanitize-recover=all -O1
then runs the test binary with a 30-second timeout.

Reports:
  PASS  -- clean run, exit 0
  FAIL  -- sanitizer error, crash, or non-zero exit
  SKIP  -- build failed or no test CMakeLists.txt

Flags:
  --changed   Only test wheels changed vs main (git diff)
  --quiet     Suppress per-wheel details, show only summary
  --json      Machine-readable JSON output

Exit 0 if all pass, 1 if any fail.
"""

import json
import os
import subprocess
import sys

from _audit_build_base import (
    BOLD,
    GREEN,
    RED,
    RESET,
    WHEELS,
    YELLOW,
    cleanup_build,
    cmake_build,
    color,
    extract_test_target,
    find_test_binary,
    find_wheels,
    get_changed_wheels,
    parse_args_common,
)

BUILD_DIR_NAME = "build_san"
SAN_FLAGS = "-fsanitize=address,undefined -fno-sanitize-recover=all -O1"
SAN_LINKER_FLAGS = "-fsanitize=address,undefined"
TIMEOUT_SEC = 30


def run_test(binary):
    """Run the test binary. Returns (passed: bool, output: str)."""
    env = os.environ.copy()
    # Make ASAN print full traces
    env["ASAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"

    try:
        result = subprocess.run(
            [str(binary)],
            capture_output=True, text=True,
            timeout=TIMEOUT_SEC,
            cwd=str(binary.parent),
            env=env,
        )
        output = result.stdout + result.stderr
        if result.returncode != 0:
            return False, output
        # Check for sanitizer keywords in output even on exit 0
        san_keywords = ["ERROR: AddressSanitizer", "ERROR: LeakSanitizer",
                        "runtime error:", "SUMMARY: ", "undefined behavior"]
        for kw in san_keywords:
            if kw in output:
                return False, output
        return True, output
    except subprocess.TimeoutExpired:
        return False, f"TIMEOUT after {TIMEOUT_SEC}s"
    except OSError as e:
        return False, str(e)


def main():
    parser = parse_args_common(
        "Build and run wheel tests under AddressSanitizer + UBSan"
    )
    args = parser.parse_args()

    if not WHEELS.is_dir():
        print(f"ERROR: wheels/ not found at {WHEELS}", file=sys.stderr)
        return 2

    if sys.platform == "win32":
        print("NOTE: ASan/UBSan require Linux gcc. Run in WSL: wsl python3 tooling/audit/audit_sanitizer.py")
        print("      Windows MinGW lacks -lasan/-lubsan. Skipping.\n")

    all_wheels = find_wheels()

    if args.changed:
        changed = get_changed_wheels()
        wheels = [(l, n, d) for l, n, d in all_wheels if (l, n) in changed]
    else:
        wheels = all_wheels

    if not wheels:
        if not args.json:
            print("No wheels to test.")
        else:
            print(json.dumps({"wheels": 0, "pass": 0, "fail": 0, "skip": 0, "results": []}))
        return 0

    results = []
    pass_count = 0
    fail_count = 0
    skip_count = 0

    if not args.quiet and not args.json:
        print(f"{BOLD}=== Sanitizer audit ({len(wheels)} wheels) ==={RESET}\n")

    for layer, name, wheel_dir in wheels:
        path = f"wheels/{layer}/{name}"
        target = extract_test_target(wheel_dir)

        if not target:
            entry = {"wheel": path, "status": "SKIP", "detail": "no target in CMakeLists.txt"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                print(f"  [{color('SKIP', YELLOW)}]  {path}: no target in CMakeLists.txt")
            continue

        # Build with sanitizers
        ok, err = cmake_build(wheel_dir, target, BUILD_DIR_NAME, SAN_FLAGS, SAN_LINKER_FLAGS)
        if not ok:
            entry = {"wheel": path, "status": "SKIP", "detail": f"build failed: {err[:200]}"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                short_err = err.strip().splitlines()[-1][:120] if err.strip() else "unknown"
                print(f"  [{color('SKIP', YELLOW)}]  {path}: build failed: {short_err}")
            continue

        # Find binary
        build_dir = wheel_dir / "test" / BUILD_DIR_NAME
        binary = find_test_binary(build_dir, target)
        if not binary:
            entry = {"wheel": path, "status": "SKIP", "detail": "binary not found after build"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                print(f"  [{color('SKIP', YELLOW)}]  {path}: binary not found after build")
            cleanup_build(build_dir)
            continue

        # Run
        passed, output = run_test(binary)
        if passed:
            entry = {"wheel": path, "status": "PASS"}
            results.append(entry)
            pass_count += 1
            if not args.quiet and not args.json:
                print(f"  [{color('PASS', GREEN)}]  {path}")
        else:
            detail = output.strip().splitlines()[-1][:200] if output.strip() else "non-zero exit"
            entry = {"wheel": path, "status": "FAIL", "detail": detail}
            results.append(entry)
            fail_count += 1
            if not args.json:
                print(f"  [{color('FAIL', RED)}]  {path}: {detail}")

        cleanup_build(build_dir)

    # Summary
    if args.json:
        output = {
            "wheels": len(wheels),
            "pass": pass_count,
            "fail": fail_count,
            "skip": skip_count,
            "results": results,
        }
        print(json.dumps(output, indent=2, ensure_ascii=False))
    else:
        print(f"\n{BOLD}=== Summary ==={RESET}")
        print(f"  wheels: {len(wheels)}")
        print(f"  {color('PASS', GREEN)}: {pass_count}")
        print(f"  {color('FAIL', RED)}: {fail_count}")
        print(f"  {color('SKIP', YELLOW)}: {skip_count}")

        if fail_count:
            print(f"\nFailed wheels:")
            for r in results:
                if r["status"] == "FAIL":
                    print(f"  - {r['wheel']}: {r.get('detail', '')}")

    return 1 if fail_count else 0


if __name__ == "__main__":
    sys.exit(main())
