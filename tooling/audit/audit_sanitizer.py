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

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules", "_draft"]
SKIP_DIRS = {"test", "build", "__pycache__", ".pytest_cache"}
BUILD_DIR_NAME = "build_san"
TIMEOUT_SEC = 30

# ANSI colors
GREEN = "\033[32m"
RED = "\033[31m"
YELLOW = "\033[33m"
RESET = "\033[0m"
BOLD = "\033[1m"


def color(text: str, code: str) -> str:
    if not sys.stdout.isatty():
        return text
    return f"{code}{text}{RESET}"


def find_wheels() -> list:
    """Return list of (layer, name, wheel_dir) for wheels with test/CMakeLists.txt."""
    wheels = []
    for layer in LAYERS:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for entry in sorted(layer_dir.iterdir()):
            if not entry.is_dir() or entry.name in SKIP_DIRS or entry.name.startswith("."):
                continue
            # Skip vendor wheels
            if (entry / "vendor").is_dir():
                continue
            cmake = entry / "test" / "CMakeLists.txt"
            if cmake.is_file():
                wheels.append((layer, entry.name, entry))
    return wheels


def get_changed_wheels() -> set:
    """Return set of (layer, name) tuples for wheels changed vs main."""
    try:
        result = subprocess.run(
            ["git", "diff", "--name-only", "origin/main", "HEAD"],
            capture_output=True, text=True, cwd=str(REPO)
        )
        if result.returncode != 0:
            # Fallback: diff against HEAD~1
            result = subprocess.run(
                ["git", "diff", "--name-only", "HEAD~1", "HEAD"],
                capture_output=True, text=True, cwd=str(REPO)
            )
    except (OSError, subprocess.SubprocessError):
        return set()

    changed = set()
    pat = re.compile(r'^wheels/([^/]+)/([^/]+)/')
    for line in result.stdout.splitlines():
        m = pat.match(line.strip())
        if m:
            changed.add((m.group(1), m.group(2)))
    return changed


def extract_test_target(cmake_path: Path) -> str:
    """Extract the test executable target name from CMakeLists.txt."""
    try:
        text = cmake_path.read_text(encoding="utf-8")
    except OSError:
        return ""
    m = re.search(r'add_executable\(\s*(\w+)', text)
    return m.group(1) if m else ""


def build_wheel(wheel_dir: Path, target_name: str) -> tuple:
    """Build wheel test with sanitizers. Returns (success: bool, stderr: str)."""
    test_dir = wheel_dir / "test"
    build_dir = test_dir / BUILD_DIR_NAME

    # Clean previous build
    if build_dir.exists():
        shutil.rmtree(build_dir, ignore_errors=True)
    build_dir.mkdir(parents=True, exist_ok=True)

    san_flags = "-fsanitize=address,undefined -fno-sanitize-recover=all -O1"

    # Configure
    cmake_cmd = [
        "cmake",
        "-G", "Ninja",
        f"-DCMAKE_C_FLAGS={san_flags}",
        f"-DCMAKE_EXE_LINKER_FLAGS={san_flags}",
        "-S", str(test_dir),
        "-B", str(build_dir),
    ]
    try:
        result = subprocess.run(
            cmake_cmd,
            capture_output=True, text=True, timeout=60,
            cwd=str(test_dir)
        )
        if result.returncode != 0:
            return False, result.stderr
    except (subprocess.TimeoutExpired, OSError) as e:
        return False, str(e)

    # Build
    build_cmd = ["cmake", "--build", str(build_dir), "--", "-j"]
    try:
        result = subprocess.run(
            build_cmd,
            capture_output=True, text=True, timeout=120,
            cwd=str(build_dir)
        )
        if result.returncode != 0:
            return False, result.stderr
    except (subprocess.TimeoutExpired, OSError) as e:
        return False, str(e)

    return True, ""


def find_test_binary(build_dir: Path, target_name: str) -> Path | None:
    """Locate the built test binary."""
    # Common locations
    candidates = [
        build_dir / target_name,
        build_dir / f"{target_name}.exe",
    ]
    for c in candidates:
        if c.is_file():
            return c
    # Search recursively
    for f in build_dir.rglob(target_name + "*"):
        if f.is_file() and f.suffix in ("", ".exe"):
            return f
    return None


def run_test(binary: Path) -> tuple:
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


def cleanup(wheel_dir: Path):
    """Remove the sanitizer build directory."""
    build_dir = wheel_dir / "test" / BUILD_DIR_NAME
    if build_dir.exists():
        shutil.rmtree(build_dir, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build and run wheel tests under AddressSanitizer + UBSan"
    )
    parser.add_argument("--changed", action="store_true",
                        help="Only test wheels changed vs main")
    parser.add_argument("--quiet", "-q", action="store_true",
                        help="Only show failures and summary")
    parser.add_argument("--json", action="store_true",
                        help="Output results as JSON")
    args = parser.parse_args()

    if not WHEELS.is_dir():
        print(f"ERROR: wheels/ not found at {WHEELS}", file=sys.stderr)
        return 2

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
        cmake_path = wheel_dir / "test" / "CMakeLists.txt"
        target = extract_test_target(cmake_path)

        if not target:
            entry = {"wheel": path, "status": "SKIP", "detail": "no target in CMakeLists.txt"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                print(f"  [{color('SKIP', YELLOW)}]  {path}: no target in CMakeLists.txt")
            continue

        # Build
        ok, err = build_wheel(wheel_dir, target)
        if not ok:
            entry = {"wheel": path, "status": "SKIP", "detail": f"build failed: {err[:200]}"}
            results.append(entry)
            skip_count += 1
            if not args.quiet and not args.json:
                short_err = err.strip().splitlines()[-1][:120] if err.strip() else "unknown"
                print(f"  [{color('SKIP', YELLOW)}]  {path}: build failed: {short_err}")
            cleanup(wheel_dir)
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
            cleanup(wheel_dir)
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

        cleanup(wheel_dir)

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
