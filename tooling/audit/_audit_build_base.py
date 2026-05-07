#!/usr/bin/env python3
"""
_audit_build_base.py -- shared infrastructure for build-based audit tools.

Private module (underscore prefix) consumed by:
  - audit_compile.py   (strict-warning compilation)
  - audit_sanitizer.py (ASan + UBSan)
  - audit_coverage.py  (gcov line coverage)

NOT used by audit_wheels.py (structural audit -- independent, no build step).
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Repository layout constants
# ---------------------------------------------------------------------------

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules", "_draft"]
SKIP_DIRS = {"test", "build", "build_audit", "__pycache__", ".pytest_cache"}

# ---------------------------------------------------------------------------
# ANSI color helpers (tty-aware)
# ---------------------------------------------------------------------------

GREEN = "\033[32m"
RED = "\033[31m"
YELLOW = "\033[33m"
BOLD = "\033[1m"
RESET = "\033[0m"


def color(text: str, code: str) -> str:
    """Wrap *text* in ANSI *code*, suppressed when stdout is not a tty."""
    if not sys.stdout.isatty():
        return text
    return f"{code}{text}{RESET}"


# ---------------------------------------------------------------------------
# Wheel discovery
# ---------------------------------------------------------------------------

def find_wheels(skip_vendor: bool = True) -> list[tuple[str, str, Path]]:
    """Return ``[(layer, name, wheel_dir), ...]`` for every wheel that has
    ``test/CMakeLists.txt``.

    When *skip_vendor* is True (default), wheels containing a ``vendor/``
    subdirectory are excluded.
    """
    wheels: list[tuple[str, str, Path]] = []
    for layer in LAYERS:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for entry in sorted(layer_dir.iterdir()):
            if not entry.is_dir() or entry.name in SKIP_DIRS or entry.name.startswith("."):
                continue
            if skip_vendor and (entry / "vendor").is_dir():
                continue
            cmake = entry / "test" / "CMakeLists.txt"
            if cmake.is_file():
                wheels.append((layer, entry.name, entry))
    return wheels


# ---------------------------------------------------------------------------
# Changed-wheel detection (git diff)
# ---------------------------------------------------------------------------

def get_changed_wheels() -> set[tuple[str, str]]:
    """Return set of ``(layer, name)`` for wheels changed vs origin/main.

    Falls back to ``HEAD~1`` when ``origin/main`` is unavailable.
    Returns an empty set on git failure (callers treat that as "compile all").
    """
    try:
        result = subprocess.run(
            ["git", "diff", "--name-only", "origin/main", "HEAD"],
            capture_output=True, text=True, cwd=str(REPO),
        )
        if result.returncode != 0:
            print("  NOTE: origin/main not available, using HEAD~1 (only last commit scanned)")
            result = subprocess.run(
                ["git", "diff", "--name-only", "HEAD~1", "HEAD"],
                capture_output=True, text=True, cwd=str(REPO),
            )
    except (OSError, subprocess.SubprocessError):
        return set()

    changed: set[tuple[str, str]] = set()
    pat = re.compile(r'^wheels/([^/]+)/([^/]+)/')
    for line in result.stdout.splitlines():
        m = pat.match(line.strip())
        if m:
            changed.add((m.group(1), m.group(2)))
    return changed


# ---------------------------------------------------------------------------
# CMakeLists.txt parsing
# ---------------------------------------------------------------------------

def extract_test_target(wheel_dir: Path) -> str:
    """Read ``test/CMakeLists.txt`` inside *wheel_dir* and return the first
    ``add_executable(...)`` target name, or ``""`` on failure."""
    cmake_path = wheel_dir / "test" / "CMakeLists.txt"
    try:
        text = cmake_path.read_text(encoding="utf-8")
    except OSError:
        return ""
    m = re.search(r'add_executable\(\s*(\w+)', text)
    return m.group(1) if m else ""


# ---------------------------------------------------------------------------
# cmake configure + build
# ---------------------------------------------------------------------------

def cmake_build(
    wheel_dir: Path,
    target: str,
    build_dir_name: str,
    extra_cmake_flags: str,
    extra_linker_flags: str = "",
) -> tuple[bool, str]:
    """Configure and build a wheel's test.

    Creates ``<wheel_dir>/test/<build_dir_name>/``, runs cmake + build.

    Returns ``(success, error_message)``.  On failure the build directory
    is cleaned up automatically.
    """
    test_dir = wheel_dir / "test"
    build_dir = test_dir / build_dir_name

    # Clean previous build
    if build_dir.exists():
        shutil.rmtree(build_dir, ignore_errors=True)
    build_dir.mkdir(parents=True, exist_ok=True)

    # Configure -- resolve() gives absolute paths so no cwd needed
    cmake_cmd = [
        "cmake",
        "-G", "Ninja",
        f"-DCMAKE_C_FLAGS={extra_cmake_flags}",
    ]
    if extra_linker_flags:
        cmake_cmd.append(f"-DCMAKE_EXE_LINKER_FLAGS={extra_linker_flags}")
    cmake_cmd += [
        "-S", str(test_dir.resolve()),
        "-B", str(build_dir.resolve()),
    ]

    try:
        result = subprocess.run(
            cmake_cmd,
            capture_output=True, text=True, timeout=60,
        )
        if result.returncode != 0:
            cleanup_build(build_dir)
            return False, result.stderr
    except (subprocess.TimeoutExpired, OSError) as e:
        cleanup_build(build_dir)
        return False, str(e)

    # Build
    build_cmd = ["cmake", "--build", str(build_dir.resolve())]
    try:
        result = subprocess.run(
            build_cmd,
            capture_output=True, text=True, timeout=120,
        )
        if result.returncode != 0:
            cleanup_build(build_dir)
            return False, result.stderr
    except (subprocess.TimeoutExpired, OSError) as e:
        cleanup_build(build_dir)
        return False, str(e)

    return True, ""


# ---------------------------------------------------------------------------
# Binary location
# ---------------------------------------------------------------------------

def find_test_binary(build_dir: Path, target_name: str) -> Path | None:
    """Locate the built test binary under *build_dir*."""
    candidates = [
        build_dir / target_name,
        build_dir / f"{target_name}.exe",
    ]
    for c in candidates:
        if c.is_file():
            return c
    # Search recursively as a fallback
    for f in build_dir.rglob(target_name + "*"):
        if f.is_file() and f.suffix in ("", ".exe"):
            return f
    return None


# ---------------------------------------------------------------------------
# Cleanup
# ---------------------------------------------------------------------------

def cleanup_build(build_dir: Path) -> None:
    """Remove *build_dir* tree, ignoring errors."""
    if build_dir.exists():
        shutil.rmtree(build_dir, ignore_errors=True)


# ---------------------------------------------------------------------------
# Common argument parser
# ---------------------------------------------------------------------------

def parse_args_common(description: str) -> argparse.ArgumentParser:
    """Return an ``ArgumentParser`` pre-loaded with ``--quiet``, ``--json``,
    and ``--changed`` flags.  Callers can add tool-specific arguments before
    calling ``parse_args()``."""
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "--quiet", "-q", action="store_true",
        help="Suppress per-wheel details, show only summary / failures",
    )
    parser.add_argument(
        "--json", action="store_true",
        help="Machine-readable JSON output",
    )
    parser.add_argument(
        "--changed", action="store_true",
        help="Only process wheels changed vs main",
    )
    return parser
