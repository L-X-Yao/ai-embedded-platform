#!/usr/bin/env python3
"""
audit_compile.py -- compile every wheel's test with strict warning flags
and parse gcc output into structured findings.

Iterates wheels under wheels/<layer>/<name>/ that have test/CMakeLists.txt,
runs cmake + ninja with extra -W flags injected via CMAKE_C_FLAGS, captures
compiler warnings, and reports per-wheel PASS/WARN/ERR status. Most wheels build
with -Werror, so most warnings arrive as errors tagged [-Werror=<flag>]; those
count as warnings. Ninja prints compiler output on stdout.

Skips vendor wheels (those with vendor/ subdir).
Skips warnings originating from system headers.

Exit 0 if no warnings found; 1 if any wheel has warnings or fails to build,
a timeout included.
"""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from _audit_build_base import (
    REPO,
    WHEELS,
    find_wheels as _find_wheels_base,
    get_changed_wheels,
    parse_args_common,
)

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
#   path/file.c:42:10: error: some message [-Werror=flag]
WARNING_RE = re.compile(
    r'^(?P<file>[^\s:]+):(?P<line>\d+):(?P<col>\d+):\s+'
    r'(?P<kind>warning|error):\s+(?P<message>.+?)(?:\s+\[(?P<flag>-W[^\]]+)\])?\s*$'
)

EXCEPTIONS_PATH = REPO / "tooling" / "audit" / "known_exceptions.json"


# ---------------------------------------------------------------------------
# Discovery (wraps base module into dict format used by this tool)
# ---------------------------------------------------------------------------

def find_wheels() -> list[dict]:
    """Return list of wheel info dicts for wheels with test/CMakeLists.txt."""
    wheels = []
    for layer, name, wheel_dir in _find_wheels_base():
        wheels.append({
            "layer": layer,
            "name": name,
            "path": f"wheels/{layer}/{name}",
            "dir": wheel_dir,
            "test_dir": wheel_dir / "test",
        })
    return wheels


def detect_changed_wheels(wheels: list[dict]) -> list[dict]:
    """Filter wheels to only those with files changed vs HEAD.

    Returns empty list when nothing has changed.
    Returns all wheels when git is unavailable (compile-all fallback).
    """
    changed = get_changed_wheels()
    if changed:
        return [w for w in wheels if (w["layer"], w["name"]) in changed]

    # get_changed_wheels() returns empty both on git failure AND when nothing
    # changed vs origin/main.  Distinguish by checking working-tree changes.
    git_ok = False
    head_changed: list[str] = []
    try:
        result = subprocess.run(
            ["git", "diff", "--name-only", "HEAD"],
            capture_output=True, text=True, cwd=str(REPO),
            timeout=30,
        )
        git_ok = result.returncode == 0
        head_changed = result.stdout.strip().splitlines()
    except (OSError, subprocess.TimeoutExpired):
        pass  # git unavailable

    if not git_ok:
        return wheels  # git broken → compile everything

    if not head_changed:
        # Check untracked files that are wheel sources
        try:
            result = subprocess.run(
                ["git", "ls-files", "--others", "--exclude-standard"],
                capture_output=True, text=True, cwd=str(REPO),
                timeout=30,
            )
            head_changed = result.stdout.strip().splitlines()
        except (OSError, subprocess.TimeoutExpired):
            return wheels  # git broken → compile everything

    if not head_changed:
        return []  # nothing changed at all

    changed_set = set(head_changed)
    return [w for w in wheels
            if any(f.startswith(w["path"] + "/") for f in changed_set)]


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
        if exc.get("wheel") == wheel_path and exc.get("check") == flag:
            return True
    return False


# ---------------------------------------------------------------------------
# Compilation (uses tempdir -- distinct from cmake_build in base module)
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

        # Build. -k 0 keeps going past a -Werror failure, so one pass shows
        # the warnings of every file.
        build_cmd = ["ninja", "-j1", "-k", "0"]

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


def parse_warnings(output: str, wheel: dict) -> list[dict]:
    """Parse gcc/clang warning lines from build output into structured findings.

    A warning promoted by -Werror counts as a warning of its own flag; any
    other error line is left to plain_errors().
    Skips warnings from system headers.
    """
    findings = []
    seen = set()

    for line in output.splitlines():
        m = WARNING_RE.match(line.strip())
        if not m:
            continue

        flag = m.group("flag") or ""
        if flag.startswith("-Werror="):
            flag = "-W" + flag[len("-Werror="):]
        elif m.group("kind") == "error":
            continue

        filepath = m.group("file")
        if is_system_header(filepath):
            continue

        lineno = int(m.group("line"))
        col = int(m.group("col"))
        message = m.group("message").strip()

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


def plain_errors(output: str) -> list[str]:
    """Return the error lines of a failed build that -Werror does not explain."""
    return [
        ln for ln in output.splitlines()
        if (": error:" in ln or "CMake Error" in ln) and "[-Werror=" not in ln
    ]


def build_failure(output: str, findings: list[dict]) -> str:
    """Describe a failed build, or return "" when its warnings fully explain it.

    A timeout or OS error prints no error line and no warning, so it falls
    through to the last lines of the output.
    """
    errors = plain_errors(output)
    if errors:
        return "\n".join(errors[:10])
    if findings:
        return ""
    tail = output.strip().splitlines()[-5:]
    return "\n".join(tail) or "build failed with no output"


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

        if findings:
            print(f"  [WARN]  {path}: {len(findings)} warning(s)")
            for f in findings:
                flag_str = f" [{f['flag']}]" if f["flag"] else ""
                print(f"          {f['file']}:{f['line']}:{f['col']}: {f['message']}{flag_str}")
        elif not quiet and not r.get("build_error"):
            print(f"  [PASS]  {path}")

    # Summary
    print(f"\n--- Summary ---")
    print(f"  Wheels scanned: {total_wheels}")
    pass_count = len([r for r in results if not r["findings"] and not r.get("build_error")])
    print(f"  PASS:           {pass_count}")
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
    parser = parse_args_common(
        "Compile-audit: build each wheel's test with strict warnings "
        "and report findings.",
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

        output = stdout + "\n" + stderr
        findings = parse_warnings(output, wheel)
        failure = "" if success else build_failure(output, findings)

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
        if failure:
            entry["build_error"] = failure

        results.append(entry)

    # Output
    if args.json:
        print(json.dumps(build_json_output(results), indent=2))
    else:
        print_text_report(results, quiet=args.quiet)

    # Exit code: 1 if any wheel has warnings or did not build, 0 otherwise
    failed = any(r["findings"] or r.get("build_error") for r in results)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
