#!/usr/bin/env python3
"""
audit_coverage.py -- measure line coverage for wheel tests via gcc/gcov.

For each wheel with test/CMakeLists.txt, builds with
  -fprofile-arcs -ftest-coverage (--coverage)
runs every add_executable target, then invokes gcov on the wheel's .c file in
each target and takes the union, as coverage_wheel.sh does.

Thresholds (default):
  L0 (primitives) : 80%
  L1-L3 (drivers/protocols/modules) : 60%

Reports per-wheel coverage table and flags wheels below threshold. A wheel that
fails to build, has a failing target or yields no coverage data is a FAIL: its
coverage is unknown, not fine. SKIP means there is nothing to measure.

Flags:
  --quiet       Suppress per-wheel details, show only summary
  --json        Machine-readable JSON output
  --threshold N Override default threshold (all layers)

Exit 0 if all above threshold, 1 if any below or any FAIL.
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
    REPO,
    RESET,
    WHEELS,
    YELLOW,
    cleanup_build,
    cmake_build,
    color,
    extract_test_targets,
    find_test_binary,
    find_wheels,
    parse_args_common,
)

BUILD_DIR_NAME = "build_cov"
TIMEOUT_SEC = 30
GCOV_UNION = REPO / "tooling" / "test" / "gcov_union.py"

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
    """Return the line coverage of source_file over all targets, or None.

    Each target compiles the source into its own <file>.c.gcda. The name must
    match exactly: a wildcard also finds test_<file>.c.gcda, and the test
    file's own coverage is near 100% whatever the wheel does. Targets can
    compile the source with different defines, so gcov_union.py merges the
    reports line by line.
    """
    src_name = source_file.name
    reports = []
    for gcda in sorted(build_dir.rglob(f"{src_name}.gcda")):
        try:
            subprocess.run(
                ["gcov", gcda.name],
                capture_output=True, text=True,
                timeout=30,
                cwd=str(gcda.parent),
            )
        except (subprocess.TimeoutExpired, OSError):
            return None
        report = gcda.parent / f"{src_name}.gcov"
        if report.is_file():
            reports.append(str(report))

    if not reports:
        return None

    try:
        result = subprocess.run(
            [sys.executable, str(GCOV_UNION), *reports],
            capture_output=True, text=True,
            timeout=30,
        )
    except (subprocess.TimeoutExpired, OSError):
        return None
    if result.returncode != 0:
        return None

    # gcov_union.py prints "LINE_PCT BRANCH_PCT"
    return float(result.stdout.split()[0])


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
        return 0

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
    fail_count = 0

    if not args.quiet and not args.json:
        print(f"{BOLD}=== Coverage audit ({len(wheels)} wheels) ==={RESET}\n")
        print(f"  {'Wheel':<40s}  {'Coverage':>8s}  {'Threshold':>9s}  Status")
        print(f"  {'-'*40}  {'-'*8}  {'-'*9}  {'-'*6}")

    for layer, name, wheel_dir in wheels:
        path = f"wheels/{layer}/{name}"
        targets = extract_test_targets(wheel_dir)

        # Determine threshold
        if args.threshold is not None:
            threshold = args.threshold
        else:
            threshold = DEFAULT_THRESHOLDS.get(layer, 60)

        if not targets:
            entry = {"wheel": path, "layer": layer, "status": "SKIP",
                     "coverage": None, "threshold": threshold,
                     "detail": "no target in CMakeLists.txt"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                print(f"  {path:<40s}  {'---':>8s}  {threshold:>8d}%  {color('SKIP', YELLOW)}")
            continue

        def report_fail(detail):
            nonlocal fail_count
            results.append({"wheel": path, "layer": layer, "status": "FAIL",
                            "coverage": None, "threshold": threshold,
                            "detail": detail})
            fail_count += 1
            if not args.json:
                print(f"  {path:<40s}  {'---':>8s}  {threshold:>8d}%  "
                      f"{color('FAIL', RED)}  {detail[:100]}")

        # Build with coverage
        cov_flags = "--coverage"
        ok, err = cmake_build(wheel_dir, targets[0], BUILD_DIR_NAME, cov_flags, cov_flags)
        if not ok:
            lines = err.strip().splitlines()
            reason = next((ln for ln in lines if "error" in ln),
                          lines[-1] if lines else "unknown")
            report_fail(f"build failed: {reason.strip()[:200]}")
            continue

        # Run every target
        build_dir = wheel_dir / "test" / BUILD_DIR_NAME
        failures = []
        for target in targets:
            binary = find_test_binary(build_dir, target)
            if not binary:
                failures.append(f"{target}: binary not found after build")
                continue
            test_ok, _ = run_test(binary)
            if not test_ok:
                failures.append(f"{target}: test failed")
        if failures:
            report_fail("; ".join(failures))
            cleanup_build(build_dir)
            continue

        # Run gcov on each source file, take the main one
        sources = find_source_files(wheel_dir, name)
        if not sources:
            entry = {"wheel": path, "layer": layer, "status": "SKIP",
                     "coverage": None, "threshold": threshold,
                     "detail": "no .c file in the wheel"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                print(f"  {path:<40s}  {'---':>8s}  {threshold:>8d}%  {color('SKIP', YELLOW)}")
            cleanup_build(build_dir)
            continue

        coverage = None
        for src in sources:
            cov = run_gcov(build_dir, src)
            if cov is not None:
                # Use the main source file's coverage (first match)
                if src.name == f"{name}.c" or coverage is None:
                    coverage = cov

        if coverage is None:
            report_fail("no coverage data for the wheel's source")
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
            "failed": fail_count,
            "skip": skip_count,
            "results": results,
        }
        print(json.dumps(output, indent=2, ensure_ascii=False))
    else:
        print(f"\n{BOLD}=== Summary ==={RESET}")
        print(f"  wheels:    {len(wheels)}")
        print(f"  {color('PASS', GREEN)} (above threshold): {above_count}")
        print(f"  {color('FAIL', RED)} (below threshold): {below_count}")
        print(f"  {color('FAIL', RED)} (build/test/gcov failed): {fail_count}")
        print(f"  {color('SKIP', YELLOW)} (nothing to measure): {skip_count}")

        if below_count:
            print(f"\nBelow threshold:")
            for r in results:
                if r["status"] == "FAIL" and r["coverage"] is not None:
                    print(f"  - {r['wheel']}: {r['coverage']:.1f}% (threshold: {r['threshold']}%)")

        if fail_count:
            print(f"\nNot measured:")
            for r in results:
                if r["status"] == "FAIL" and r["coverage"] is None:
                    print(f"  - {r['wheel']}: {r['detail'][:200]}")

    return 1 if below_count or fail_count else 0


if __name__ == "__main__":
    sys.exit(main())
