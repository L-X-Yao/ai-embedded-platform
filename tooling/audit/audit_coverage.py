#!/usr/bin/env python3
"""
audit_coverage.py -- measure line coverage for wheel tests via gcc/gcov.

For each wheel with test/CMakeLists.txt, builds with
  -fprofile-arcs -ftest-coverage (--coverage)
runs the test, then invokes gcov on the wheel's .c file to extract
line coverage percentage.

Thresholds (default):
  L0 (primitives) : 80%
  L1-L3 (drivers/protocols/modules) : 60%

Reports per-wheel coverage table and flags wheels below threshold.

Flags:
  --quiet       Suppress per-wheel details, show only summary
  --json        Machine-readable JSON output
  --threshold N Override default threshold (all layers)

Exit 0 if all above threshold, 1 if any below.
"""

import json
import re
import subprocess
import sys
from pathlib import Path

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
    parse_args_common,
)

BUILD_DIR_NAME = "build_cov"
TIMEOUT_SEC = 30

# Default thresholds per layer
DEFAULT_THRESHOLDS = {
    "primitives": 80,
    "drivers": 60,
    "protocols": 60,
    "modules": 60,
    "_draft": 0,  # No threshold for drafts
}


def find_source_files(wheel_dir, name):
    """Find the wheel's .c source files (not test files)."""
    sources = []
    main_src = wheel_dir / f"{name}.c"
    if main_src.is_file():
        sources.append(main_src)
    # Also check for additional .c files in wheel root (not test/)
    for f in wheel_dir.glob("*.c"):
        if f.name.startswith("test_"):
            continue
        if f not in sources:
            sources.append(f)
    return sources


def run_test(binary):
    """Run the test binary. Returns (success: bool, output: str)."""
    try:
        result = subprocess.run(
            [str(binary)],
            capture_output=True, text=True,
            timeout=TIMEOUT_SEC,
            cwd=str(binary.parent),
        )
        return result.returncode == 0, result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        return False, f"TIMEOUT after {TIMEOUT_SEC}s"
    except OSError as e:
        return False, str(e)


def run_gcov(build_dir, source_file):
    """Run gcov on a source file and return line coverage percentage.

    Searches the build directory for the .gcno/.gcda pair corresponding to
    the source file, then runs gcov from that directory.
    """
    src_name = source_file.name
    stem = source_file.stem

    # Find .gcda files matching the source name
    gcda_files = list(build_dir.rglob(f"*{stem}*.gcda"))
    if not gcda_files:
        return None

    # Run gcov from the directory containing the .gcda
    gcda = gcda_files[0]
    gcda_dir = gcda.parent

    try:
        result = subprocess.run(
            ["gcov", str(source_file)],
            capture_output=True, text=True,
            timeout=30,
            cwd=str(gcda_dir),
        )
    except (subprocess.TimeoutExpired, OSError, FileNotFoundError):
        return None

    output = result.stdout + result.stderr

    # Parse gcov output for line coverage
    # Format: "Lines executed:85.71% of 14"  or  "File '<name>.c'"
    # We look for the coverage line following our source file name
    coverage = None
    found_file = False
    for line in output.splitlines():
        if src_name in line and ("File" in line or "file" in line.lower()):
            found_file = True
            continue
        if found_file and "Lines executed:" in line:
            m = re.search(r'Lines executed:\s*([\d.]+)%', line)
            if m:
                coverage = float(m.group(1))
            break

    # Fallback: just grab any "Lines executed" referencing our file
    if coverage is None:
        for line in output.splitlines():
            if "Lines executed:" in line:
                m = re.search(r'Lines executed:\s*([\d.]+)%', line)
                if m:
                    coverage = float(m.group(1))
                    break

    return coverage


def main():
    parser = parse_args_common(
        "Measure line coverage for wheel tests via gcc/gcov"
    )
    parser.add_argument("--threshold", type=int, default=None,
                        help="Override default threshold for all layers (0-100)")
    args = parser.parse_args()

    if not WHEELS.is_dir():
        print(f"ERROR: wheels/ not found at {WHEELS}", file=sys.stderr)
        return 2

    if sys.platform == "win32":
        print("NOTE: gcov --coverage requires Linux gcc. Run in WSL: wsl python3 tooling/audit/audit_coverage.py")
        print("      Windows MinGW gcov output format may differ. Skipping.\n")

    wheels = find_wheels()
    if not wheels:
        if not args.json:
            print("No wheels with tests found.")
        else:
            print(json.dumps({"wheels": 0, "above": 0, "below": 0, "skip": 0, "results": []}))
        return 0

    results = []
    above_count = 0
    below_count = 0
    skip_count = 0

    if not args.quiet and not args.json:
        print(f"{BOLD}=== Coverage audit ({len(wheels)} wheels) ==={RESET}\n")
        print(f"  {'Wheel':<40s}  {'Coverage':>8s}  {'Threshold':>9s}  Status")
        print(f"  {'-'*40}  {'-'*8}  {'-'*9}  {'-'*6}")

    for layer, name, wheel_dir in wheels:
        path = f"wheels/{layer}/{name}"
        target = extract_test_target(wheel_dir)

        # Determine threshold
        if args.threshold is not None:
            threshold = args.threshold
        else:
            threshold = DEFAULT_THRESHOLDS.get(layer, 60)

        if not target:
            entry = {"wheel": path, "layer": layer, "status": "SKIP",
                     "coverage": None, "threshold": threshold,
                     "detail": "no target in CMakeLists.txt"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                print(f"  {path:<40s}  {'---':>8s}  {threshold:>8d}%  {color('SKIP', YELLOW)}")
            continue

        # Build with coverage
        cov_flags = "--coverage"
        ok, err = cmake_build(wheel_dir, target, BUILD_DIR_NAME, cov_flags, cov_flags)
        if not ok:
            entry = {"wheel": path, "layer": layer, "status": "SKIP",
                     "coverage": None, "threshold": threshold,
                     "detail": f"build failed: {err[:200]}"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                print(f"  {path:<40s}  {'---':>8s}  {threshold:>8d}%  {color('SKIP', YELLOW)}")
            continue

        # Find and run binary
        build_dir = wheel_dir / "test" / BUILD_DIR_NAME
        binary = find_test_binary(build_dir, target)
        if not binary:
            entry = {"wheel": path, "layer": layer, "status": "SKIP",
                     "coverage": None, "threshold": threshold,
                     "detail": "binary not found after build"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                print(f"  {path:<40s}  {'---':>8s}  {threshold:>8d}%  {color('SKIP', YELLOW)}")
            cleanup_build(build_dir)
            continue

        test_ok, _ = run_test(binary)
        if not test_ok:
            entry = {"wheel": path, "layer": layer, "status": "SKIP",
                     "coverage": None, "threshold": threshold,
                     "detail": "test failed (cannot measure coverage of failing test)"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                print(f"  {path:<40s}  {'---':>8s}  {threshold:>8d}%  {color('SKIP', YELLOW)}")
            cleanup_build(build_dir)
            continue

        # Run gcov on each source file, take the main one
        sources = find_source_files(wheel_dir, name)
        coverage = None
        for src in sources:
            cov = run_gcov(build_dir, src)
            if cov is not None:
                # Use the main source file's coverage (first match)
                if src.name == f"{name}.c" or coverage is None:
                    coverage = cov

        if coverage is None:
            entry = {"wheel": path, "layer": layer, "status": "SKIP",
                     "coverage": None, "threshold": threshold,
                     "detail": "gcov produced no output"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                print(f"  {path:<40s}  {'---':>8s}  {threshold:>8d}%  {color('SKIP', YELLOW)}")
            cleanup_build(build_dir)
            continue

        # Evaluate against threshold
        passed = coverage >= threshold
        status = "PASS" if passed else "FAIL"
        entry = {"wheel": path, "layer": layer, "status": status,
                 "coverage": round(coverage, 1), "threshold": threshold}
        results.append(entry)

        if passed:
            above_count += 1
            if not args.quiet and not args.json:
                print(f"  {path:<40s}  {coverage:>7.1f}%  {threshold:>8d}%  {color('PASS', GREEN)}")
        else:
            below_count += 1
            if not args.json:
                print(f"  {path:<40s}  {coverage:>7.1f}%  {threshold:>8d}%  {color('FAIL', RED)}")

        cleanup_build(build_dir)

    # Summary
    if args.json:
        output = {
            "wheels": len(wheels),
            "above": above_count,
            "below": below_count,
            "skip": skip_count,
            "results": results,
        }
        print(json.dumps(output, indent=2, ensure_ascii=False))
    else:
        print(f"\n{BOLD}=== Summary ==={RESET}")
        print(f"  wheels:    {len(wheels)}")
        print(f"  {color('PASS', GREEN)} (above threshold): {above_count}")
        print(f"  {color('FAIL', RED)} (below threshold): {below_count}")
        print(f"  {color('SKIP', YELLOW)} (build/test/gcov failed): {skip_count}")

        if below_count:
            print(f"\nBelow threshold:")
            for r in results:
                if r["status"] == "FAIL":
                    print(f"  - {r['wheel']}: {r['coverage']:.1f}% (threshold: {r['threshold']}%)")

    return 1 if below_count else 0


if __name__ == "__main__":
    sys.exit(main())
