#!/usr/bin/env python3
"""
audit_wheels.py — flag orphaned wheels and stale WISHLIST entries.

Codifies feedback_wheel_commit_not_sunk.md as a CI guardrail. The orphan
problem we hit twice (w25qxx, qmi8658a both committed but never wired)
gets caught here automatically — no reliance on humans/AI remembering
the three-step sink checklist.

Rules:
  - Every wheel under wheels/<layer>/<name>/ MUST have a CI job entry
    mentioning its path in .github/workflows/ci.yml.
  - Wheels under drivers/, protocols/, modules/ SHOULD have at least one
    project consumer (CMakeLists.txt or #include reference in projects/).
    Missing consumer is WARN not FAIL — proactive building is encouraged
    (see feedback_no_human_cost_model.md). CI presence remains hard FAIL.
  - Wheels under primitives/, _draft/ MAY be unconsumed (warn, don't
    fail) — primitives are general building blocks, drafts are
    incubating.
  - Names listed in wheels/WISHLIST.md MUST NOT correspond to existing
    wheels (a stale candidate is finished work that wasn't pruned).
  - Every wheel README MUST have a "Why not VSF native?" section
    (ADR-006 / CLAUDE.md zero-rule).
  - Vendor wheels (those with vendor/ subdir) MUST have VENDOR.md
    documenting upstream origin (ADR-010).
  - Wheels with cross-wheel dependencies (detected via test CMakeLists)
    SHOULD document them in a "Dependencies" README section (ADR-009).
  - Non-vendor wheel .h/.c files SHOULD have the Layer/Status/Deps file
    header per api-conventions §8.
  - Non-draft wheel READMEs SHOULD have candidate-level sections:
    Supported platforms, Test status, Dependencies (wheel-contract §3.2).

Exit 0 if all checks pass; 1 if any failure.
"""

import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
CI_YAML = REPO / ".github" / "workflows" / "ci.yml"
WISHLIST = WHEELS / "WISHLIST.md"
PROJECTS = REPO / "projects"

LAYERS = ["primitives", "drivers", "protocols", "modules", "_draft"]
CONSUMER_REQUIRED = {"drivers", "protocols", "modules"}
SKIP_DIRS = {"test", "build", "__pycache__", ".pytest_cache"}


def find_wheels():
    """Return list of (layer, name, rel_path) for each wheel directory."""
    wheels = []
    for layer in LAYERS:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for entry in sorted(layer_dir.iterdir()):
            if not entry.is_dir():
                continue
            if entry.name in SKIP_DIRS or entry.name.startswith("."):
                continue
            wheels.append((layer, entry.name, f"wheels/{layer}/{entry.name}"))
    return wheels


def read_readme(wheel_dir: Path) -> str:
    """Return README.md text or empty string."""
    readme = wheel_dir / "README.md"
    if not readme.exists():
        return ""
    try:
        return readme.read_text(encoding="utf-8")
    except OSError:
        return ""


def has_project_origin(wheel_dir: Path) -> bool:
    """Check if README documents extraction from a real project."""
    text = read_readme(wheel_dir)
    return "Extracted from" in text and "projects/" in text


def has_vsf_native_section(wheel_dir: Path) -> bool:
    """Check README has 'Why not VSF native?' heading (ADR-006)."""
    text = read_readme(wheel_dir)
    return "## Why not VSF native?" in text


def has_api_review(wheel_dir: Path) -> bool:
    """Check README has an API review section (wheel-contract §7)."""
    text = read_readme(wheel_dir)
    return "## API Review" in text or "## API review" in text


def has_responsible_human(wheel_dir: Path) -> bool:
    """Check README has 'Responsible human' section (wheel-contract template)."""
    text = read_readme(wheel_dir)
    return "## Responsible human" in text


def has_standard_deviation_heading(wheel_dir: Path) -> bool:
    """Check that deviation heading is 'API convention deviations', not legacy."""
    text = read_readme(wheel_dir)
    if "## Convention Deviations" in text:
        return False
    return True


def is_vendor_wheel(wheel_dir: Path) -> bool:
    """A vendor wheel has a vendor/ subdirectory."""
    return (wheel_dir / "vendor").is_dir()


def has_vendor_md(wheel_dir: Path) -> bool:
    return (wheel_dir / "VENDOR.md").exists()


def has_dependencies_section(wheel_dir: Path) -> bool:
    text = read_readme(wheel_dir)
    return "## Dependencies" in text or "## 依赖" in text


def has_ctest_registration(wheel_dir: Path) -> bool:
    """Check that test CMakeLists.txt has enable_testing() + add_test()."""
    cmake = wheel_dir / "test" / "CMakeLists.txt"
    if not cmake.exists():
        cmake = wheel_dir / "CMakeLists.txt"
    if not cmake.exists():
        return True
    try:
        text = cmake.read_text(encoding="utf-8")
    except OSError:
        return True
    return "enable_testing()" in text and "add_test(" in text


def has_c99_standard(wheel_dir: Path) -> bool:
    """Check that test CMakeLists.txt sets CMAKE_C_STANDARD 99."""
    cmake = wheel_dir / "test" / "CMakeLists.txt"
    if not cmake.exists():
        cmake = wheel_dir / "CMakeLists.txt"
    if not cmake.exists():
        return True  # no cmake = nothing to check
    try:
        text = cmake.read_text(encoding="utf-8")
    except OSError:
        return True
    if "vsf.cmake" in text:
        return True  # VSF cmake sets standard internally
    return "CMAKE_C_STANDARD" in text


def has_candidate_sections(wheel_dir: Path) -> list:
    """Check README has candidate-level sections (wheel-contract §3.2).

    Returns list of missing section names (empty = all present).
    """
    text = read_readme(wheel_dir)
    missing = []
    if "## Supported platforms" not in text and "## 支持的平台" not in text:
        missing.append("Supported platforms")
    if "## Test status" not in text and "## 测试状态" not in text:
        missing.append("Test status")
    if "## Dependencies" not in text and "## 依赖" not in text:
        missing.append("Dependencies")
    if "## How to test" not in text and "## 如何测试" not in text:
        missing.append("How to test")
    if "## Known issues" not in text and "## 已知坑" not in text and "## 已知问题" not in text:
        missing.append("Known issues")
    if "## Origin" not in text and "## 来源" not in text:
        missing.append("Origin")
    return missing


def has_file_header(wheel_dir: Path, name: str) -> bool:
    """Check .h and .c have api-conventions §8 file header in first 10 lines."""
    for ext in (".h", ".c"):
        src = wheel_dir / f"{name}{ext}"
        if not src.exists():
            continue
        try:
            lines = src.read_text(encoding="utf-8", errors="ignore").splitlines()[:10]
            top = "\n".join(lines)
        except OSError:
            continue
        if "Layer:" not in top or "Status:" not in top:
            return False
    return True


def has_extern_c_guard(wheel_dir: Path, name: str) -> bool:
    """Check .h has extern 'C' guards (api-conventions §9)."""
    header = wheel_dir / f"{name}.h"
    if not header.exists():
        return True
    try:
        text = header.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return True
    return 'extern "C"' in text or "extern \"C\"" in text


def has_thread_safe_annotations(wheel_dir: Path, name: str) -> bool:
    """Check .h has at least one @thread_safe annotation (api-conventions §4)."""
    header = wheel_dir / f"{name}.h"
    if not header.exists():
        return True
    try:
        text = header.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return True
    return "@thread_safe" in text


def has_werror(wheel_dir: Path) -> bool:
    """Check that test CMakeLists.txt includes -Werror (non-vendor only)."""
    cmake = wheel_dir / "test" / "CMakeLists.txt"
    if not cmake.exists():
        cmake = wheel_dir / "CMakeLists.txt"
    if not cmake.exists():
        return True
    try:
        text = cmake.read_text(encoding="utf-8")
    except OSError:
        return True
    if "-Wall" not in text:
        return True  # no warning flags at all — different problem
    return "-Werror" in text


def has_wconversion(wheel_dir: Path) -> bool:
    """Check that test CMakeLists.txt includes -Wconversion."""
    cmake = wheel_dir / "test" / "CMakeLists.txt"
    if not cmake.exists():
        cmake = wheel_dir / "CMakeLists.txt"
    if not cmake.exists():
        return True
    try:
        text = cmake.read_text(encoding="utf-8")
    except OSError:
        return True
    if "-Wall" not in text:
        return True
    return "-Wconversion" in text


def has_busy_wait_timeout(wheel_dir: Path, name: str) -> list:
    """Check that while-loops waiting on HW flags have timeout guards.
    Returns list of (file, line_num, line) for unguarded busy-waits."""
    patterns = re.compile(
        r'while\s*\(.*(?:flag|status|busy|ready|done|complete|measuring|drdy'
        r'|conversion|eoc|txe|rxne|bsy|tbe)\b',
        re.IGNORECASE
    )
    timeout_patterns = re.compile(
        r'timeout|timer|count|retry|break|MAX_WAIT|TIMEOUT|deadline',
        re.IGNORECASE
    )
    unguarded = []
    for ext in (".c",):
        src = wheel_dir / f"{name}{ext}"
        if not src.exists():
            continue
        try:
            lines = src.read_text(encoding="utf-8", errors="ignore").splitlines()
        except OSError:
            continue
        for i, line in enumerate(lines):
            if patterns.search(line):
                context = "\n".join(lines[max(0, i-3):i+5])
                if not timeout_patterns.search(context):
                    unguarded.append((src.name, i + 1, line.strip()))
    return unguarded


def has_test_exit_code(wheel_dir: Path) -> bool:
    """Check test main() returns non-zero on failure."""
    patterns = ["tests_fail", "g_failures", "failures", "TEST_MAIN_END"]
    for glob_pat in ["test/test_*.c", "test_*.c"]:
        for src in wheel_dir.glob(glob_pat):
            try:
                text = src.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            if 'test_harness.h' in text:
                return True
            if "return" in text and any(p in text for p in patterns):
                return True
    if list(wheel_dir.glob("test_*.py")) or list(wheel_dir.glob("test/test_*.py")):
        return True
    return False


def has_balanced_test_run(wheel_dir: Path) -> tuple:
    """Check that every TEST() function has a matching RUN() call.

    Returns (ok: bool, test_count: int, run_count: int).
    """
    import re
    for glob_pat in ["test/test_*.c", "test_*.c"]:
        for src in wheel_dir.glob(glob_pat):
            try:
                text = src.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            if "test_harness.h" not in text:
                continue
            tests = set(re.findall(r'^TEST\((\w+)\)', text, re.MULTILINE))
            runs = set(re.findall(r'RUN\((\w+)\)', text))
            unrun = tests - runs
            if unrun:
                return (False, len(tests), len(runs))
    return (True, 0, 0)


def detect_cross_wheel_deps(wheel_dir: Path) -> list:
    """Detect cross-wheel deps from test/CMakeLists.txt paths like ../../<layer>/<name>/."""
    cmake = wheel_dir / "test" / "CMakeLists.txt"
    if not cmake.exists():
        return []
    try:
        text = cmake.read_text(encoding="utf-8")
    except OSError:
        return []
    pattern = re.compile(r"\.\./\.\./\.\./(primitives|drivers|protocols|modules)/([a-z0-9_]+)/")
    return list({m[1] for m in pattern.findall(text)})


def has_ci_job(wheel_path: str, ci_text: str, wheel_dir: Path) -> bool:
    if wheel_path in ci_text:
        return True
    if "wheels/*/*/test/CMakeLists.txt" in ci_text and (wheel_dir / "test" / "CMakeLists.txt").exists():
        return True
    return False


def find_consumers(wheel_path: str, wheel_name: str) -> list:
    """Find project files referencing this wheel (CMakeLists.txt or #include)."""
    consumers = []
    if not PROJECTS.is_dir():
        return consumers
    for cm in PROJECTS.rglob("CMakeLists.txt"):
        try:
            text = cm.read_text(encoding="utf-8")
        except OSError:
            continue
        if wheel_path in text:
            consumers.append(str(cm.relative_to(REPO)).replace("\\", "/"))
    if not consumers:
        header = f"{wheel_name}.h"
        for src in PROJECTS.rglob("*.[ch]"):
            try:
                text = src.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            if f'#include "{header}"' in text or f"#include <{header}>" in text:
                consumers.append(str(src.relative_to(REPO)).replace("\\", "/"))
    return consumers


def parse_wishlist_names() -> list:
    """Extract candidate wheel names from WISHLIST.md (backticked first cell)."""
    if not WISHLIST.exists():
        return []
    text = WISHLIST.read_text(encoding="utf-8")
    pattern = re.compile(r"^\|\s*`([a-zA-Z0-9_]+)`\s*\|", re.MULTILINE)
    return pattern.findall(text)


STRAY_EXTENSIONS = {".log", ".o", ".obj", ".a", ".lib", ".elf", ".hex", ".bin",
                    ".map", ".d", ".gcda", ".gcno", ".su", ".pyc"}
ROOT_CONFIGS = {".clang-format", ".editorconfig"}


def _git_tracked_files(directory: Path) -> set:
    """Return set of git-tracked file paths (relative to repo) under directory."""
    import subprocess
    rel = str(directory.relative_to(REPO)).replace("\\", "/")
    try:
        result = subprocess.run(
            ["git", "ls-files", "--", f"{rel}/"],
            capture_output=True, text=True, cwd=str(REPO)
        )
        return {line.strip() for line in result.stdout.splitlines() if line.strip()}
    except (OSError, subprocess.SubprocessError):
        return set()


def audit_projects() -> list:
    """Lightweight scan of projects/ for stray or duplicate tracked files."""
    issues = []
    if not PROJECTS.is_dir():
        return issues

    tracked = _git_tracked_files(PROJECTS)

    for proj_dir in sorted(PROJECTS.iterdir()):
        if not proj_dir.is_dir() or proj_dir.name.startswith("."):
            continue
        proj_name = proj_dir.name

        for cfg_name in ROOT_CONFIGS:
            rel_path = f"projects/{proj_name}/{cfg_name}"
            if rel_path not in tracked:
                continue
            cfg = proj_dir / cfg_name
            root_cfg = REPO / cfg_name
            if root_cfg.exists():
                try:
                    if cfg.read_bytes() == root_cfg.read_bytes():
                        issues.append(
                            f"{rel_path}: duplicate of repo root "
                            f"(clang-format inherits upward, safe to delete)"
                        )
                except OSError:
                    pass

        for f in proj_dir.iterdir():
            if not f.is_file():
                continue
            rel_path = f"projects/{proj_name}/{f.name}"
            if rel_path not in tracked:
                continue
            if f.suffix in STRAY_EXTENSIONS:
                issues.append(
                    f"{rel_path}: build artifact tracked in git"
                )
            if f.suffix == ".py" and f.name != "__init__.py":
                try:
                    text = f.read_text(encoding="utf-8", errors="ignore")
                    if re.search(r'[A-Z]:\\', text) or re.search(r'filepath\s*=\s*r"', text):
                        issues.append(
                            f"{rel_path}: "
                            f"looks like a one-time script with hardcoded paths"
                        )
                except OSError:
                    pass

    return issues


def has_negative_error_codes(wheel_dir: Path, name: str) -> bool:
    """Check that error enum uses explicit negative values (Q4 audit)."""
    h = wheel_dir / f"{name}.h"
    if not h.exists():
        return True
    text = h.read_text(encoding="utf-8", errors="replace")
    if f"{name}_err_t" not in text:
        return True
    lines = text.split("\n")
    in_enum = False
    for line in lines:
        stripped = line.strip()
        if f"_OK" in stripped and "= 0" in stripped:
            in_enum = True
            continue
        if in_enum:
            if "}" in stripped:
                break
            if "_ERR_" in stripped and "=" not in stripped:
                return False
    return True


def has_error_code_test_coverage(wheel_dir: Path, name: str) -> list:
    """Check that every error enum value has at least one test triggering it.
    Returns list of uncovered error code names (empty = all covered)."""
    h = wheel_dir / f"{name}.h"
    if not h.exists():
        return []
    text = h.read_text(encoding="utf-8", errors="replace")
    err_pattern = re.compile(r'\b(' + re.escape(name.upper()) + r'_ERR_\w+)\b')
    err_codes = set(err_pattern.findall(text))
    if not err_codes:
        return []
    test_dir = wheel_dir / "test"
    if not test_dir.is_dir():
        return sorted(err_codes)
    test_text = ""
    for tf in test_dir.glob("*.c"):
        test_text += tf.read_text(encoding="utf-8", errors="replace")
    uncovered = [e for e in sorted(err_codes) if e not in test_text]
    return uncovered


def has_naming_convention(wheel_dir: Path, name: str) -> bool:
    """Check that public function declarations follow <wheel_name>_<verb> pattern.

    Scans the wheel's .h file for function declarations and verifies they are
    prefixed with the wheel name (api-conventions S1.1).
    """
    h = wheel_dir / f"{name}.h"
    if not h.exists():
        return True
    try:
        text = h.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return True
    # Match function declarations: return_type wheel_name_verb(...);
    # Skip typedefs, comments, macros, and lines inside struct/enum bodies
    prefix = name + "_"
    violations = 0
    for line in text.split("\n"):
        stripped = line.strip()
        # Skip non-declaration lines
        if not ("(" in stripped and ");" in stripped):
            continue
        if stripped.startswith("typedef"):
            continue
        if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
            continue
        if stripped.startswith("#"):
            continue
        # Skip callback field declarations inside structs (e.g. bool (*write_reg)(...))
        if stripped.startswith("(") or "(*" in stripped:
            continue
        # Look for a function name token before the opening paren
        m = re.search(r'\b([a-z_][a-z0-9_]*)\s*\(', stripped)
        if not m:
            continue
        func_name = m.group(1)
        # Skip common non-function identifiers
        if func_name in ("if", "for", "while", "switch", "return", "sizeof"):
            continue
        if not func_name.startswith(prefix):
            violations += 1
    return violations == 0


def has_consistent_hal_pattern(wheel_dir: Path, name: str) -> bool:
    """Check that L1 driver wheels use HAL injection (struct with function pointers
    passed to init).

    Verifies:
    - Header defines a HAL struct (e.g. <name>_i2c_t, <name>_spi_t, <name>_hal_t)
      containing at least one function pointer field.
    - The init function accepts a pointer to this HAL struct.
    """
    h = wheel_dir / f"{name}.h"
    if not h.exists():
        return True
    try:
        text = h.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return True
    # Look for a HAL struct typedef: <name>_i2c_t, <name>_spi_t, <name>_hal_t,
    # <name>_gpio_t, <name>_onewire_t, <name>_pwm_t
    hal_pattern = re.compile(
        r'\b' + re.escape(name) + r'_(i2c|spi|hal|gpio|onewire|ow|pwm)_t\b'
    )
    has_hal_struct = bool(hal_pattern.search(text))
    if not has_hal_struct:
        return False
    # Verify at least one function pointer in the HAL struct
    # Pattern: something like  (*func_name)(...)
    if "(*" not in text:
        return False
    return True


def has_function_doc_all(wheel_dir: Path, name: str) -> bool:
    """Check that every function declaration in .h has a doc comment (Q29)."""
    h = wheel_dir / f"{name}.h"
    if not h.exists():
        return True
    lines = h.read_text(encoding="utf-8", errors="replace").split("\n")
    undoc = 0
    for i, line in enumerate(lines):
        stripped = line.strip()
        if (f"{name}_" in stripped and "(" in stripped and ");" in stripped
                and not stripped.startswith("typedef")
                and not stripped.startswith("//")
                and not stripped.startswith("/*")
                and not stripped.startswith("*")):
            prev = lines[i - 1].strip() if i > 0 else ""
            if not prev.startswith("/**") and not prev.startswith("*") and not prev.endswith("*/"):
                undoc += 1
    return undoc == 0


def main(quiet: bool = False, json_mode: bool = False) -> int:
    def qprint(msg: str):
        if not quiet and not json_mode:
            print(msg)

    def tprint(msg: str):
        if not json_mode:
            print(msg)

    if not WHEELS.is_dir():
        print(f"ERROR: wheels/ not found at {WHEELS}", file=sys.stderr)
        return 2

    ci_text = CI_YAML.read_text(encoding="utf-8") if CI_YAML.exists() else ""
    wheels = find_wheels()

    failures = []
    warnings = []
    results = []

    def record(check, wheel_path, level, msg, fix_cmd=None):
        entry = {"check": check, "wheel": wheel_path, "level": level, "message": msg}
        if fix_cmd:
            entry["fix"] = fix_cmd
        results.append(entry)
        if level == "FAIL":
            failures.append(f"{wheel_path}: {msg}")
        elif level == "WARN":
            warnings.append(f"{wheel_path}: {msg}")

    qprint(f"=== Wheel audit ({len(wheels)} wheels) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        ci_ok = has_ci_job(path, ci_text, wheel_dir)
        consumers = find_consumers(path, name)
        consumer_required = layer in CONSUMER_REQUIRED
        origin = has_project_origin(wheel_dir)

        if not ci_ok:
            icon = "FAIL"
            record("ci_job", path, "FAIL",
                   f"missing CI job — no mention of '{path}' in {CI_YAML.relative_to(REPO)}",
                   fix_cmd=f"add '{path}' to .github/workflows/ci.yml wheel-test matrix")
        elif consumer_required and not consumers:
            icon = "WARN"
            reason = "extracted from project" if origin else "proactively built"
            record("consumer", path, "WARN", f"no consumer yet ({reason}, acceptable)",
                   fix_cmd=f"#include <{name}.h> in a project or add {path} to project CMakeLists.txt")
        elif not consumers:
            icon = "WARN"
            record("consumer", path, "WARN", f"no consumer yet (acceptable for {layer})",
                   fix_cmd=f"#include <{name}.h> in a project or add {path} to project CMakeLists.txt")
        else:
            icon = " ok "

        ci_label = "yes" if ci_ok else "MISSING"
        cons_label = ", ".join(consumers) if consumers else "(none)"
        tprint(f"  [{icon}]  {path:36s}  CI: {ci_label:8s}  Consumer: {cons_label}")

    # --- "Why not VSF native?" section check (ADR-006) ---
    qprint("\n=== README: 'Why not VSF native?' (ADR-006) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if not has_vsf_native_section(wheel_dir):
            record("vsf_native_section", path, "FAIL",
                   "README missing '## Why not VSF native?' section",
                   fix_cmd=f"add '## Why not VSF native?' section to {path}/README.md")
            tprint(f"  [FAIL]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- Vendor wheel VENDOR.md check (ADR-010) ---
    qprint("\n=== Vendor wheels: VENDOR.md (ADR-010) ===\n")
    vendor_found = False
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if not is_vendor_wheel(wheel_dir):
            continue
        vendor_found = True
        if not has_vendor_md(wheel_dir):
            record("vendor_md", path, "FAIL", "has vendor/ but no VENDOR.md",
                   fix_cmd=f"create {path}/VENDOR.md documenting upstream origin (ADR-010)")
            tprint(f"  [FAIL]  {path}: vendor/ present, VENDOR.md missing")
        else:
            qprint(f"  [ ok ]  {path}")
    if not vendor_found:
        qprint("  (no vendor wheels found)")

    # --- Cross-wheel dependency documentation (ADR-009) ---
    qprint("\n=== Cross-wheel deps documented (ADR-009) ===\n")
    deps_found = False
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        deps = detect_cross_wheel_deps(wheel_dir)
        if not deps:
            continue
        deps_found = True
        if not has_dependencies_section(wheel_dir):
            record("cross_wheel_deps", path, "WARN",
                   f"depends on {', '.join(deps)} but README has no Dependencies section",
                   fix_cmd=f"add '## Dependencies' section listing {', '.join(deps)} to {path}/README.md")
            tprint(f"  [WARN]  {path} → {', '.join(deps)} (undocumented)")
        else:
            qprint(f"  [ ok ]  {path} → {', '.join(deps)}")
    if not deps_found:
        qprint("  (no cross-wheel dependencies detected)")

    # --- API review section (wheel-contract §7) ---
    qprint("\n=== API review metadata (wheel-contract §7) ===\n")
    for layer, name, path in wheels:
        if layer == "_draft":
            continue
        wheel_dir = WHEELS / layer / name
        if not has_api_review(wheel_dir):
            record("api_review", path, "WARN", "README missing API review section",
                   fix_cmd=f"add '## API Review' section to {path}/README.md (see wheel-contract §7)")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- ctest registration check ---
    qprint("\n=== ctest registration (enable_testing + add_test) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if not has_ctest_registration(wheel_dir):
            record("ctest_registration", path, "WARN",
                   "test CMakeLists.txt missing enable_testing()/add_test()",
                   fix_cmd=f"add enable_testing() + add_test(NAME ... COMMAND ...) to {path}/test/CMakeLists.txt")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- CMAKE_C_STANDARD 99 check ---
    qprint("\n=== CMAKE_C_STANDARD 99 ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if not has_c99_standard(wheel_dir):
            record("c99_standard", path, "WARN",
                   "test CMakeLists.txt missing CMAKE_C_STANDARD 99",
                   fix_cmd=f"add set(CMAKE_C_STANDARD 99) to {path}/test/CMakeLists.txt")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- Candidate README sections (wheel-contract §3.2) ---
    qprint("\n=== Candidate README sections (wheel-contract §3.2) ===\n")
    for layer, name, path in wheels:
        if layer == "_draft":
            continue
        wheel_dir = WHEELS / layer / name
        missing = has_candidate_sections(wheel_dir)
        if missing:
            record("candidate_sections", path, "WARN",
                   f"README missing candidate sections: {', '.join(missing)}",
                   fix_cmd=f"add sections to {path}/README.md: {', '.join(missing)}")
            tprint(f"  [WARN]  {path}: missing {', '.join(missing)}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- Busy-wait timeout check ---
    qprint("\n=== Busy-wait timeout guards ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        unguarded = has_busy_wait_timeout(wheel_dir, name)
        if unguarded:
            locs = "; ".join(f"{f}:{ln}" for f, ln, _ in unguarded)
            record("busy_wait_timeout", path, "WARN",
                   f"while-loop waiting on HW flag without timeout: {locs}",
                   fix_cmd=f"add timeout counter or deadline check to busy-wait loops in {path}/{name}.c")
            tprint(f"  [WARN]  {path}: {locs}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- File header (Layer/Status/Deps) check (api-conventions §8) ---
    qprint("\n=== File header (api-conventions §8) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        if not has_file_header(wheel_dir, name):
            record("file_header", path, "WARN",
                   ".h/.c missing Layer/Status/Deps file header",
                   fix_cmd=f"add Layer:/Status:/Deps: header to {path}/{name}.h/.c first lines (api-conventions §8)")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- extern "C" guard check (api-conventions §9) ---
    qprint("\n=== extern \"C\" guard (api-conventions §9) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        if not has_extern_c_guard(wheel_dir, name):
            record("extern_c_guard", path, "WARN", ".h missing extern \"C\" guard",
                   fix_cmd=f"add extern \"C\" {{ }} guard to {path}/{name}.h")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- @thread_safe annotation check (api-conventions §4) ---
    qprint("\n=== @thread_safe annotations (api-conventions §4) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        if not has_thread_safe_annotations(wheel_dir, name):
            record("thread_safe", path, "WARN", ".h missing @thread_safe annotations",
                   fix_cmd="make audit-fix-apply --only doc_comments")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- Test exit code check ---
    qprint("\n=== Test exit code (return tests_fail) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        if not has_test_exit_code(wheel_dir):
            record("test_exit_code", path, "WARN",
                   "test harness may not return non-zero on failure",
                   fix_cmd=f"#include \"test_harness.h\" and use TEST_MAIN_BEGIN/TEST_MAIN_END in {path}/test/")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- TEST/RUN balance check ---
    qprint("\n=== TEST/RUN balance (no orphan tests) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        ok, tc, rc = has_balanced_test_run(wheel_dir)
        if not ok:
            record("test_run_balance", path, "WARN",
                   f"{tc} TEST() but only {rc} RUN() — orphan test functions",
                   fix_cmd=f"add missing RUN() calls inside TEST_MAIN_BEGIN/END in {path}/test/")
            tprint(f"  [WARN]  {path}: {tc} TEST() vs {rc} RUN()")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- -Werror check ---
    qprint("\n=== -Werror flag (warnings as errors) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        if not has_werror(wheel_dir):
            record("werror", path, "WARN", "test CMakeLists.txt missing -Werror",
                   fix_cmd=f"add -Werror to target_compile_options in {path}/test/CMakeLists.txt")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- -Wconversion check ---
    qprint("\n=== -Wconversion (implicit conversion warnings) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        if not has_wconversion(wheel_dir):
            record("wconversion", path, "WARN",
                   "test CMakeLists.txt missing -Wconversion",
                   fix_cmd=f"add -Wconversion to target_compile_options in {path}/test/CMakeLists.txt")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- Responsible human section (wheel-contract template) ---
    qprint("\n=== Responsible human (wheel-contract template) ===\n")
    for layer, name, path in wheels:
        if layer == "_draft":
            continue
        wheel_dir = WHEELS / layer / name
        if not has_responsible_human(wheel_dir):
            record("responsible_human", path, "WARN",
                   "README missing '## Responsible human' section",
                   fix_cmd=f"add '## Responsible human' section to {path}/README.md")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- API convention deviations heading consistency ---
    qprint("\n=== API convention deviations heading ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if not has_standard_deviation_heading(wheel_dir):
            record("deviation_heading", path, "WARN",
                   "use '## API convention deviations' not '## Convention Deviations'",
                   fix_cmd=f"rename heading in {path}/README.md to '## API convention deviations'")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    qprint("\n=== WISHLIST staleness ===\n")
    wishlist_names = parse_wishlist_names()
    wheel_names = {name for _, name, _ in wheels}
    stale = [n for n in wishlist_names if n in wheel_names]
    if stale:
        for n in stale:
            record("wishlist_stale", f"WISHLIST/{n}", "FAIL",
                   f"'{n}' listed but already exists as a wheel",
                   fix_cmd=f"remove '{n}' row from wheels/WISHLIST.md")
            tprint(f"  [FAIL]  {n} listed in WISHLIST but already exists")
    else:
        qprint("  [ ok ]  no stale entries")

    # --- Projects directory hygiene ---
    qprint("\n=== Projects directory hygiene ===\n")
    proj_issues = audit_projects()
    for msg in proj_issues:
        record("project_hygiene", "projects/", "WARN", msg)
    if not proj_issues:
        qprint("  [ ok ]  no stray files detected")
    else:
        for msg in proj_issues:
            tprint(f"  [WARN]  {msg}")

    # --- Error code sign convention (Q4 audit) ---
    qprint("\n=== Error code sign convention (Q4) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        if not has_negative_error_codes(wheel_dir, name):
            record("error_code_sign", path, "WARN",
                   "error enum uses auto-increment (should be explicit negative)",
                   fix_cmd="python3 tooling/audit/audit_fix.py --apply")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- Function documentation completeness (Q29 audit) ---
    qprint("\n=== Function doc completeness (Q29) ===\n")
    for layer, name, path in wheels:
        if layer == "_draft":
            continue
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        if not has_function_doc_all(wheel_dir, name):
            record("function_doc", path, "WARN",
                   "some functions lack doc comment before declaration",
                   fix_cmd="python3 tooling/audit/audit_fix.py --apply")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- Error code test coverage (R8 audit insight) ---
    qprint("\n=== Error code test coverage ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        uncovered = has_error_code_test_coverage(wheel_dir, name)
        if uncovered:
            record("error_code_coverage", path, "WARN",
                   f"untested error codes: {', '.join(uncovered)}",
                   fix_cmd=f"add test cases covering {', '.join(uncovered)} in {path}/test/")
            tprint(f"  [WARN]  {path}: {', '.join(uncovered)}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- Naming convention check (api-conventions S1.1) ---
    qprint("\n=== Naming convention (api-conventions S1.1) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        if not has_naming_convention(wheel_dir, name):
            record("naming_convention", path, "WARN",
                   f"public functions not following {name}_<verb> naming pattern",
                   fix_cmd=f"rename public functions in {path}/{name}.h to {name}_<verb>(...)")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- HAL injection pattern check (api-conventions S5.3, L1 only) ---
    qprint("\n=== HAL injection pattern (L1 drivers) ===\n")
    for layer, name, path in wheels:
        if layer != "drivers":
            continue
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            qprint(f"  [skip]  {path} (vendor)")
            continue
        if not has_consistent_hal_pattern(wheel_dir, name):
            record("hal_injection", path, "WARN",
                   "L1 driver missing HAL struct with function pointers",
                   fix_cmd=f"add {name}_i2c_t/{name}_spi_t/{name}_hal_t with callback fields to {path}/{name}.h")
            tprint(f"  [WARN]  {path}")
        else:
            qprint(f"  [ ok ]  {path}")

    # --- Consumer mapping summary ---
    qprint("\n=== Consumer mapping ===\n")
    consumed = []
    unconsumed = []
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        cons = find_consumers(path, name)
        if cons:
            consumed.append((path, cons))
        else:
            unconsumed.append(path)
    if consumed:
        for wpath, cons in consumed:
            projs = set()
            for c in cons:
                parts = c.split("/")
                if len(parts) > 1 and parts[0] == "projects":
                    projs.add(parts[1])
            qprint(f"  {wpath:36s} ← {', '.join(sorted(projs))}")
    qprint(f"\n  consumed: {len(consumed)}/{len(wheels)}  |  unconsumed: {len(unconsumed)}")

    if json_mode:
        output = {
            "wheels": len(wheels),
            "failures": len(failures),
            "warnings": len(warnings),
            "results": [r for r in results if r["level"] != "OK"],
            "consumed": len(consumed),
            "unconsumed": len(unconsumed),
        }
        print(json.dumps(output, indent=2, ensure_ascii=False))
        return 1 if failures else 0

    tprint("\n=== Summary ===")
    tprint(f"  wheels:   {len(wheels)}")
    tprint(f"  failures: {len(failures)}")
    tprint(f"  warnings: {len(warnings)}")

    if failures:
        tprint("\nFailures:")
        for r in results:
            if r["level"] == "FAIL":
                tprint(f"  - {r['wheel']}: {r['message']}")
                if "fix" in r:
                    tprint(f"    → {r['fix']}")
        return 1

    if warnings:
        tprint("\nWarnings (non-blocking):")
        for r in results:
            if r["level"] == "WARN":
                tprint(f"  - {r['wheel']}: {r['message']}")
                if "fix" in r:
                    tprint(f"    → {r['fix']}")

    return 0


def print_dep_graph(wheels):
    """Print wheel dependency graph in Mermaid format."""
    print("```mermaid")
    print("graph TD")
    layer_style = {"primitives": ":::prim", "drivers": ":::drv",
                   "protocols": ":::proto", "modules": ":::mod", "_draft": ":::draft"}
    for layer, name, path in wheels:
        label = f"{name}"
        style = layer_style.get(layer, "")
        print(f"    {name}[\"{label} ({layer[0].upper()}{layer[1:3]})\"]")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        deps = detect_cross_wheel_deps(wheel_dir)
        for dep in deps:
            print(f"    {name} --> {dep}")
    consumers = {}
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        rel = str(wheel_dir.relative_to(REPO)).replace("\\", "/")
        found = find_consumers(rel, name)
        for c in found:
            proj = c.split("/")[1] if c.startswith("projects/") else None
            if proj:
                consumers.setdefault(proj, set()).add(name)
    for proj, wset in consumers.items():
        print("    " + proj + '{{"' + proj + '"}}')
        for w in sorted(wset):
            print(f"    {proj} -.-> {w}")
    print("```")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Wheel audit")
    parser.add_argument("--graph", action="store_true", help="Print Mermaid dependency graph")
    parser.add_argument("--quiet", "-q", action="store_true", help="Only show WARN/FAIL and summary")
    parser.add_argument("--json", action="store_true", help="Output results as JSON (machine-readable)")
    args = parser.parse_args()
    if args.graph:
        wheels = find_wheels()
        print_dep_graph(wheels)
        sys.exit(0)
    sys.exit(main(quiet=args.quiet, json_mode=args.json))
