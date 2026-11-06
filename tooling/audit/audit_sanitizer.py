#!/usr/bin/env python3
"""
audit_sanitizer.py -- build and run wheel tests under AddressSanitizer + UBSan.

For each wheel with test/CMakeLists.txt, builds with
  -fsanitize=address,undefined -fno-sanitize-recover=all -O1 -w
then runs every add_executable target, each with a 120-second timeout.
A wheel has several: the main test, test_null, test_boundary, the hardware
model test and the property test.

Reports:
  PASS  -- every target ran clean
  FAIL  -- sanitizer error, crash, non-zero exit, missing binary or a build
           that fails under the sanitizer flags
  SKIP  -- no test CMakeLists.txt or no target in it

Flags:
  --changed   Only test wheels changed vs main (git diff)
  --jobs N    Audit N wheels at once (default 1)
  --quiet     Suppress per-wheel details, show only summary
  --json      Machine-readable JSON output

Exit 0 if all pass, 1 if any fail.
"""

import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed

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
    extract_test_targets,
    find_test_binary,
    find_wheels,
    get_changed_wheels,
    parse_args_common,
)

BUILD_DIR_NAME = "build_san"
# -w: the instrumentation sets off -Wconversion and -Wformat-truncation under
# the wheels' -Werror. wheel_gate.sh checks warnings in a separate strict build.
SAN_FLAGS = "-fsanitize=address,undefined -fno-sanitize-recover=all -O1 -w"
SAN_LINKER_FLAGS = "-fsanitize=address,undefined"
TIMEOUT_SEC = 120
BUILD_TIMEOUT_SEC = 300


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


def audit_wheel(layer, name, wheel_dir):
    """Build one wheel's tests and run every target. Returns a result dict."""
    path = f"wheels/{layer}/{name}"
    targets = extract_test_targets(wheel_dir)
    if not targets:
        return {"wheel": path, "status": "SKIP", "targets": 0,
                "detail": "no target in CMakeLists.txt"}

    ok, err = cmake_build(wheel_dir, targets[0], BUILD_DIR_NAME, SAN_FLAGS,
                          SAN_LINKER_FLAGS, build_timeout=BUILD_TIMEOUT_SEC)
    if not ok:
        short_err = err.strip().splitlines()[-1][:160] if err.strip() else "unknown"
        return {"wheel": path, "status": "FAIL", "targets": len(targets),
                "detail": f"build failed: {short_err}"}

    build_dir = wheel_dir / "test" / BUILD_DIR_NAME
    failures = []
    for target in targets:
        binary = find_test_binary(build_dir, target)
        if not binary:
            failures.append(f"{target}: binary not found after build")
            continue
        passed, output = run_test(binary)
        if not passed:
            lines = output.strip().splitlines()
            reason = next((ln for ln in lines if "runtime error:" in ln
                           or "ERROR: " in ln), lines[-1] if lines else "non-zero exit")
            failures.append(f"{target}: {reason.strip()[:200]}")
    cleanup_build(build_dir)

    if failures:
        return {"wheel": path, "status": "FAIL", "targets": len(targets),
                "detail": "; ".join(failures)}
    return {"wheel": path, "status": "PASS", "targets": len(targets)}


def main():
    parser = parse_args_common(
        "Build and run wheel tests under AddressSanitizer + UBSan"
    )
    parser.add_argument(
        "--jobs", "-j", type=int, default=1,
        help="Audit this many wheels at once",
    )
    args = parser.parse_args()

    if not WHEELS.is_dir():
        print(f"ERROR: wheels/ not found at {WHEELS}", file=sys.stderr)
        return 2

    if sys.platform == "win32":
        print("NOTE: ASan/UBSan require Linux gcc. Run in WSL: wsl python3 tooling/audit/audit_sanitizer.py")
        print("      Windows MinGW lacks -lasan/-lubsan. Skipping.\n")
        return 0

    all_wheels = find_wheels(skip_vendor=False)

    if args.changed:
        changed = get_changed_wheels()
        wheels = [(l, n, d) for l, n, d in all_wheels if (l, n) in changed]
    else:
        wheels = all_wheels

    if not wheels:
        if not args.json:
            print("No wheels to test.")
        else:
            print(json.dumps({"wheels": 0, "targets": 0, "pass": 0, "fail": 0,
                              "skip": 0, "results": []}))
        return 0

    if not args.quiet and not args.json:
        print(f"{BOLD}=== Sanitizer audit ({len(wheels)} wheels) ==={RESET}\n")

    results = []
    labels = {"PASS": color("PASS", GREEN), "FAIL": color("FAIL", RED),
              "SKIP": color("SKIP", YELLOW)}
    with ThreadPoolExecutor(max_workers=max(args.jobs, 1)) as pool:
        futures = [pool.submit(audit_wheel, *w) for w in wheels]
        for future in as_completed(futures):
            entry = future.result()
            results.append(entry)
            show = not args.json and (not args.quiet or entry["status"] == "FAIL")
            if show:
                detail = entry.get("detail", f"{entry['targets']} targets")
                print(f"  [{labels[entry['status']]}]  {entry['wheel']}: {detail}", flush=True)
    results.sort(key=lambda r: r["wheel"])

    pass_count = sum(r["status"] == "PASS" for r in results)
    fail_count = sum(r["status"] == "FAIL" for r in results)
    skip_count = sum(r["status"] == "SKIP" for r in results)
    target_count = sum(r["targets"] for r in results)

    # Summary
    if args.json:
        output = {
            "wheels": len(wheels),
            "targets": target_count,
            "pass": pass_count,
            "fail": fail_count,
            "skip": skip_count,
            "results": results,
        }
        print(json.dumps(output, indent=2, ensure_ascii=False))
    else:
        print(f"\n{BOLD}=== Summary ==={RESET}")
        print(f"  wheels: {len(wheels)}")
        print(f"  targets: {target_count}")
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
