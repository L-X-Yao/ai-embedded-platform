#!/usr/bin/env python3
"""
audit_compile.py -- compile every wheel's test with strict warning flags
and parse gcc output into structured findings.

Iterates wheels under wheels/<layer>/<name>/ that have test/CMakeLists.txt,
runs cmake + ninja with extra -W flags injected via CMAKE_C_FLAGS, captures
compiler warnings, and reports per-wheel PASS/WARN status.

Skips vendor wheels (those with vendor/ subdir).
Skips warnings originating from system headers.

Exit 0 if no warnings found; 1 if any wheel has warnings.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules", "_draft"]
SKIP_DIRS = {"test", "build", "build_audit", "__pycache__", ".pytest_cache"}

# Extra warning flags injected on top of whatever the wheel already uses.
# Deliberately excludes -fanalyzer (too slow for daily runs).
EXTRA_WARNINGS = [
    "-Wconversion",
    "-Wsign-conversion",
    "-Wfloat-conversion",
    "-Wdouble-promotion",
    "-Wswitch-default",
    "-Wswitch-enum",
    "-Wunused-function",
    "-Wunused-parameter",
    "-Wformat=2",
    "-Wshadow",
    "-Wredundant-decls",
]

# Pattern for a gcc/clang warning line:
#   path/file.c:42:10: warning: some message [-Wflag]
WARNING_RE = re.compile(
    r'^(?P<file>[^\s:]+):(?P<line>\d+):(?P<col>\d+):\s+'
    r'warning:\s+(?P<message>.+?)(?:\s+\[(?P<flag>-W[^\]]+)\])?\s*$'
)

EXCEPTIONS_PATH = REPO / "tooling" / "audit" / "known_exceptions.json"


# ---------------------------------------------------------------------------
# Discovery
# ---------------------------------------------------------------------------

def find_wheels() -> list[dict]:
    """Return list of wheel info dicts for wheels with test/CMakeLists.txt."""
    wheels = []
    for layer in LAYERS:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for entry in sorted(layer_dir.iterdir()):
            if not entry.is_dir() or entry.name in SKIP_DIRS or entry.name.startswith("."):
                continue
            test_cmake = entry / "test" / "CMakeLists.txt"
            if not test_cmake.exists():
                continue
            # Skip vendor wheels
            if (entry / "vendor").is_dir():
                continue
            wheels.append({
                "layer": layer,
                "name": entry.name,
                "path": f"wheels/{layer}/{entry.name}",
                "dir": entry,
                "test_dir": entry / "test",
            })
    return wheels


def detect_changed_wheels(wheels: list[dict]) -> list[dict]:
    """Filter wheels to only those with files changed vs HEAD."""
    try:
        result = subprocess.run(
            ["git", "diff", "--name-only", "HEAD"],
            capture_output=True, text=True, cwd=str(REPO),
            timeout=30,
        )
        changed = result.stdout.strip().splitlines()
    except (OSError, subprocess.TimeoutExpired):
        # If git fails, compile everything
        return wheels

    if not changed:
        # Also check untracked files
        try:
            result = subprocess.run(
                ["git", "ls-files", "--others", "--exclude-standard"],
                capture_output=True, text=True, cwd=str(REPO),
                timeout=30,
            )
            changed = result.stdout.strip().splitlines()
        except (OSError, subprocess.TimeoutExpired):
            return wheels

    if not changed:
        return []

    changed_set = set(changed)
    filtered = []
    for w in wheels:
        prefix = w["path"] + "/"
        if any(f.startswith(prefix) for f in changed_set):
            filtered.append(w)
    return filtered


# ---------------------------------------------------------------------------
# Known exceptions
# ---------------------------------------------------------------------------

def load_exceptions() -> list[dict]:
    """Load known_exceptions.json if present. Returns list of exception dicts."""
    if not EXCEPTIONS_PATH.exists():
        return []
    try:
        data = json.loads(EXCEPTIONS_PATH.read_text(encoding="utf-8"))
        return data.get("exceptions", [])
    except (json.JSONDecodeError, OSError):
        return []


def is_excepted(wheel_path: str, flag: str, exceptions: list[dict]) -> bool:
    """Check if a (wheel, flag) combination is in the exceptions list."""
    for exc in exceptions:
        if exc.get("wheel") == wheel_path and exc.get("warning") == flag:
            return True
    return False


# ---------------------------------------------------------------------------
# Compilation
# ---------------------------------------------------------------------------

def compile_wheel(wheel: dict) -> tuple[bool, str, str]:
    """Run cmake + build for a single wheel. Returns (success, stdout, stderr).

    Builds in a temp directory to avoid polluting the wheel tree.
    """
    test_dir = wheel["test_dir"]

    # Use a temp directory for the build
    build_dir = tempfile.mkdtemp(prefix=f"audit_compile_{wheel['name']}_")

    extra_flags = " ".join(EXTRA_WARNINGS)

    try:
        # Configure
        cmake_cmd = [
            "cmake",
            str(test_dir),
            f"-DCMAKE_C_FLAGS={extra_flags}",
            "-G", "Ninja",
        ]

        env = os.environ.copy()
        # Ensure consistent locale for warning parsing
        env["LC_ALL"] = "C"

        cfg_result = subprocess.run(
            cmake_cmd,
            capture_output=True, text=True,
            cwd=build_dir, env=env,
            timeout=60,
        )

        if cfg_result.returncode != 0:
            return False, cfg_result.stdout, cfg_result.stderr

        # Build
        build_cmd = ["ninja", "-j1"]

        build_result = subprocess.run(
            build_cmd,
            capture_output=True, text=True,
            cwd=build_dir, env=env,
            timeout=120,
        )

        combined_stdout = cfg_result.stdout + build_result.stdout
        combined_stderr = cfg_result.stderr + build_result.stderr

        return build_result.returncode == 0, combined_stdout, combined_stderr

    except subprocess.TimeoutExpired:
        return False, "", "TIMEOUT: compilation exceeded time limit"
    except OSError as e:
        return False, "", f"OS error: {e}"
    finally:
        shutil.rmtree(build_dir, ignore_errors=True)


# ---------------------------------------------------------------------------
# Warning parsing
# ---------------------------------------------------------------------------

def is_system_header(filepath: str) -> bool:
    """Return True if the file path looks like a system/compiler header."""
    norm = filepath.replace("\\", "/")
    system_markers = [
        "/usr/include/",
        "/usr/lib/",
        "/usr/local/include/",
        "/lib/gcc/",
        "/include/c++/",
        "/bits/",
        "gcc-",
        "/mingw",
        "/msys",
    ]
    return any(m in norm for m in system_markers)


def parse_warnings(stderr: str, wheel: dict) -> list[dict]:
    """Parse gcc/clang warning lines from stderr into structured findings.

    Skips warnings from system headers.
    """
    findings = []
    seen = set()

    for line in stderr.splitlines():
        m = WARNING_RE.match(line.strip())
        if not m:
            continue

        filepath = m.group("file")
        if is_system_header(filepath):
            continue

        lineno = int(m.group("line"))
        col = int(m.group("col"))
        message = m.group("message").strip()
        flag = m.group("flag") or ""

        # Deduplicate identical warnings (same file+line+flag)
        key = (filepath, lineno, flag)
        if key in seen:
            continue
        seen.add(key)

        findings.append({
            "file": filepath,
            "line": lineno,
            "col": col,
            "message": message,
            "flag": flag,
        })

    return findings


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------

def print_text_report(results: list[dict], quiet: bool) -> None:
    """Print human-readable report to stdout."""
    total_wheels = len(results)
    total_warnings = sum(len(r["findings"]) for r in results)
    warn_wheels = [r for r in results if r["findings"]]
    pass_wheels = [r for r in results if not r["findings"]]
    error_wheels = [r for r in results if r.get("build_error")]

    if not quiet:
        print(f"=== Compile audit ({total_wheels} wheels) ===\n")

    for r in results:
        path = r["wheel"]
        findings = r["findings"]
        build_ok = not r.get("build_error")

        if r.get("build_error"):
            print(f"  [ERR ]  {path}: build failed")
            if not quiet:
                # Show first few lines of the error
                for errline in r["build_error"].splitlines()[:5]:
                    print(f"          {errline}")
            continue

        if findings:
            print(f"  [WARN]  {path}: {len(findings)} warning(s)")
            for f in findings:
                flag_str = f" [{f['flag']}]" if f["flag"] else ""
                print(f"          {f['file']}:{f['line']}:{f['col']}: {f['message']}{flag_str}")
        elif not quiet:
            print(f"  [PASS]  {path}")

    # Summary
    print(f"\n--- Summary ---")
    print(f"  Wheels scanned: {total_wheels}")
    print(f"  PASS:           {len(pass_wheels) - len(error_wheels)}")
    if error_wheels:
        print(f"  ERR (no build): {len(error_wheels)}")
    print(f"  WARN:           {len(warn_wheels)}")
    print(f"  Total warnings: {total_warnings}")


def build_json_output(results: list[dict]) -> dict:
    """Build the JSON output structure."""
    return {
        "tool": "audit_compile",
        "wheels_scanned": len(results),
        "wheels_with_warnings": sum(1 for r in results if r["findings"]),
        "total_warnings": sum(len(r["findings"]) for r in results),
        "results": [
            {
                "wheel": r["wheel"],
                "layer": r["layer"],
                "status": "ERR" if r.get("build_error") else ("WARN" if r["findings"] else "PASS"),
                "warning_count": len(r["findings"]),
                "findings": r["findings"],
                **({"build_error": r["build_error"]} if r.get("build_error") else {}),
            }
            for r in results
        ],
    }


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def check_tools() -> bool:
    """Verify cmake and ninja are available."""
    for tool in ("cmake", "ninja"):
        try:
            subprocess.run(
                [tool, "--version"],
                capture_output=True, timeout=10,
            )
        except (OSError, subprocess.TimeoutExpired):
            print(f"ERROR: '{tool}' not found in PATH", file=sys.stderr)
            return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile-audit: build each wheel's test with strict warnings "
                    "and report findings.",
    )
    parser.add_argument(
        "--changed", action="store_true",
        help="Only compile wheels whose files differ from HEAD",
    )
    parser.add_argument(
        "--json", action="store_true",
        help="Output results as JSON (machine-readable)",
    )
    parser.add_argument(
        "--quiet", "-q", action="store_true",
        help="Only show warnings, not PASS lines",
    )
    args = parser.parse_args()

    if not WHEELS.is_dir():
        print(f"ERROR: wheels/ not found at {WHEELS}", file=sys.stderr)
        return 2

    if not check_tools():
        return 2

    # Discover wheels
    wheels = find_wheels()
    if args.changed:
        wheels = detect_changed_wheels(wheels)

    if not wheels:
        if args.json:
            print(json.dumps(build_json_output([]), indent=2))
        else:
            print("No wheels to compile.")
        return 0

    # Load known exceptions
    exceptions = load_exceptions()

    # Compile each wheel and collect results
    results = []
    for i, wheel in enumerate(wheels):
        if not args.json and not args.quiet:
            # Progress indicator on stderr so it does not pollute stdout
            print(
                f"  [{i+1}/{len(wheels)}] compiling {wheel['path']}...",
                file=sys.stderr, flush=True,
            )

        success, stdout, stderr = compile_wheel(wheel)

        if not success and not stderr.strip():
            stderr = stdout  # cmake errors sometimes go to stdout

        findings = parse_warnings(stderr, wheel) if stderr else []

        # Filter out excepted findings
        if exceptions:
            findings = [
                f for f in findings
                if not is_excepted(wheel["path"], f["flag"], exceptions)
            ]

        entry = {
            "wheel": wheel["path"],
            "layer": wheel["layer"],
            "findings": findings,
        }

        # Mark build errors (distinct from warnings)
        if not success:
            # Check if there are actual errors (not just warnings)
            error_lines = [
                ln for ln in stderr.splitlines()
                if ": error:" in ln or "FAILED" in ln or "Error" in ln
            ]
            if error_lines:
                entry["build_error"] = "\n".join(error_lines[:10])

        results.append(entry)

    # Output
    if args.json:
        print(json.dumps(build_json_output(results), indent=2))
    else:
        print_text_report(results, quiet=args.quiet)

    # Exit code: 1 if any warnings found, 0 otherwise
    has_warnings = any(r["findings"] for r in results)
    return 1 if has_warnings else 0


if __name__ == "__main__":
    sys.exit(main())
