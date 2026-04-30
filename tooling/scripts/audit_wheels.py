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


def main() -> int:
    if not WHEELS.is_dir():
        print(f"ERROR: wheels/ not found at {WHEELS}", file=sys.stderr)
        return 2

    ci_text = CI_YAML.read_text(encoding="utf-8") if CI_YAML.exists() else ""
    wheels = find_wheels()

    failures = []
    warnings = []

    print(f"=== Wheel audit ({len(wheels)} wheels) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        ci_ok = has_ci_job(path, ci_text, wheel_dir)
        consumers = find_consumers(path, name)
        consumer_required = layer in CONSUMER_REQUIRED
        origin = has_project_origin(wheel_dir)

        if not ci_ok:
            icon = "FAIL"
            failures.append(
                f"{path}: missing CI job — no mention of '{path}' in "
                f"{CI_YAML.relative_to(REPO)}"
            )
        elif consumer_required and not consumers:
            icon = "WARN"
            reason = "extracted from project" if origin else "proactively built"
            warnings.append(
                f"{path}: no consumer yet ({reason}, acceptable)"
            )
        elif not consumers:
            icon = "WARN"
            warnings.append(f"{path}: no consumer yet (acceptable for {layer})")
        else:
            icon = " ok "

        ci_label = "yes" if ci_ok else "MISSING"
        cons_label = ", ".join(consumers) if consumers else "(none)"
        print(f"  [{icon}]  {path:36s}  CI: {ci_label:8s}  Consumer: {cons_label}")

    # --- "Why not VSF native?" section check (ADR-006) ---
    print("\n=== README: 'Why not VSF native?' (ADR-006) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if not has_vsf_native_section(wheel_dir):
            failures.append(f"{path}: README missing '## Why not VSF native?' section")
            print(f"  [FAIL]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- Vendor wheel VENDOR.md check (ADR-010) ---
    print("\n=== Vendor wheels: VENDOR.md (ADR-010) ===\n")
    vendor_found = False
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if not is_vendor_wheel(wheel_dir):
            continue
        vendor_found = True
        if not has_vendor_md(wheel_dir):
            failures.append(f"{path}: has vendor/ but no VENDOR.md")
            print(f"  [FAIL]  {path}: vendor/ present, VENDOR.md missing")
        else:
            print(f"  [ ok ]  {path}")
    if not vendor_found:
        print("  (no vendor wheels found)")

    # --- Cross-wheel dependency documentation (ADR-009) ---
    print("\n=== Cross-wheel deps documented (ADR-009) ===\n")
    deps_found = False
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        deps = detect_cross_wheel_deps(wheel_dir)
        if not deps:
            continue
        deps_found = True
        if not has_dependencies_section(wheel_dir):
            warnings.append(
                f"{path}: depends on {', '.join(deps)} but README has no Dependencies section"
            )
            print(f"  [WARN]  {path} → {', '.join(deps)} (undocumented)")
        else:
            print(f"  [ ok ]  {path} → {', '.join(deps)}")
    if not deps_found:
        print("  (no cross-wheel dependencies detected)")

    # --- API review section (wheel-contract §7) ---
    print("\n=== API review metadata (wheel-contract §7) ===\n")
    for layer, name, path in wheels:
        if layer == "_draft":
            continue
        wheel_dir = WHEELS / layer / name
        if not has_api_review(wheel_dir):
            warnings.append(f"{path}: README missing API review section")
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- ctest registration check ---
    print("\n=== ctest registration (enable_testing + add_test) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if not has_ctest_registration(wheel_dir):
            warnings.append(f"{path}: test CMakeLists.txt missing enable_testing()/add_test()")
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- CMAKE_C_STANDARD 99 check ---
    print("\n=== CMAKE_C_STANDARD 99 ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if not has_c99_standard(wheel_dir):
            warnings.append(f"{path}: test CMakeLists.txt missing CMAKE_C_STANDARD 99")
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- Candidate README sections (wheel-contract §3.2) ---
    print("\n=== Candidate README sections (wheel-contract §3.2) ===\n")
    for layer, name, path in wheels:
        if layer == "_draft":
            continue
        wheel_dir = WHEELS / layer / name
        missing = has_candidate_sections(wheel_dir)
        if missing:
            warnings.append(
                f"{path}: README missing candidate sections: {', '.join(missing)}"
            )
            print(f"  [WARN]  {path}: missing {', '.join(missing)}")
        else:
            print(f"  [ ok ]  {path}")

    # --- File header (Layer/Status/Deps) check (api-conventions §8) ---
    print("\n=== File header (api-conventions §8) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            print(f"  [skip]  {path} (vendor)")
            continue
        if not has_file_header(wheel_dir, name):
            warnings.append(f"{path}: .h/.c missing Layer/Status/Deps file header")
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- extern "C" guard check (api-conventions §9) ---
    print("\n=== extern \"C\" guard (api-conventions §9) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            print(f"  [skip]  {path} (vendor)")
            continue
        if not has_extern_c_guard(wheel_dir, name):
            warnings.append(f"{path}: .h missing extern \"C\" guard")
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- @thread_safe annotation check (api-conventions §4) ---
    print("\n=== @thread_safe annotations (api-conventions §4) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            print(f"  [skip]  {path} (vendor)")
            continue
        if not has_thread_safe_annotations(wheel_dir, name):
            warnings.append(f"{path}: .h missing @thread_safe annotations")
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- Test exit code check ---
    print("\n=== Test exit code (return tests_fail) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            print(f"  [skip]  {path} (vendor)")
            continue
        if not has_test_exit_code(wheel_dir):
            warnings.append(f"{path}: test harness may not return non-zero on failure")
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- TEST/RUN balance check ---
    print("\n=== TEST/RUN balance (no orphan tests) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            print(f"  [skip]  {path} (vendor)")
            continue
        ok, tc, rc = has_balanced_test_run(wheel_dir)
        if not ok:
            warnings.append(f"{path}: {tc} TEST() but only {rc} RUN() — orphan test functions")
            print(f"  [WARN]  {path}: {tc} TEST() vs {rc} RUN()")
        else:
            print(f"  [ ok ]  {path}")

    # --- -Werror check ---
    print("\n=== -Werror flag (warnings as errors) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            print(f"  [skip]  {path} (vendor)")
            continue
        if not has_werror(wheel_dir):
            warnings.append(f"{path}: test CMakeLists.txt missing -Werror")
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- Responsible human section (wheel-contract template) ---
    print("\n=== Responsible human (wheel-contract template) ===\n")
    for layer, name, path in wheels:
        if layer == "_draft":
            continue
        wheel_dir = WHEELS / layer / name
        if not has_responsible_human(wheel_dir):
            warnings.append(f"{path}: README missing '## Responsible human' section")
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- API convention deviations heading consistency ---
    print("\n=== API convention deviations heading ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if not has_standard_deviation_heading(wheel_dir):
            warnings.append(
                f"{path}: use '## API convention deviations' not '## Convention Deviations'"
            )
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    print("\n=== WISHLIST staleness ===\n")
    wishlist_names = parse_wishlist_names()
    wheel_names = {name for _, name, _ in wheels}
    stale = [n for n in wishlist_names if n in wheel_names]
    if stale:
        for n in stale:
            failures.append(
                f"WISHLIST: '{n}' is listed but already exists as a wheel — "
                f"remove the row from {WISHLIST.relative_to(REPO)}"
            )
            print(f"  [FAIL]  {n} listed in WISHLIST but already exists")
    else:
        print("  [ ok ]  no stale entries")

    # --- Projects directory hygiene ---
    print("\n=== Projects directory hygiene ===\n")
    proj_issues = audit_projects()
    for msg in proj_issues:
        warnings.append(msg)
        print(f"  [WARN]  {msg}")
    if not proj_issues:
        print("  [ ok ]  no stray files detected")

    # --- Error code sign convention (Q4 audit) ---
    print("\n=== Error code sign convention (Q4) ===\n")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            print(f"  [skip]  {path} (vendor)")
            continue
        if not has_negative_error_codes(wheel_dir, name):
            warnings.append(f"{path}: error enum uses auto-increment (should be explicit negative)")
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- Function documentation completeness (Q29 audit) ---
    print("\n=== Function doc completeness (Q29) ===\n")
    for layer, name, path in wheels:
        if layer == "_draft":
            continue
        wheel_dir = WHEELS / layer / name
        if is_vendor_wheel(wheel_dir):
            print(f"  [skip]  {path} (vendor)")
            continue
        if not has_function_doc_all(wheel_dir, name):
            warnings.append(f"{path}: some functions lack doc comment before declaration")
            print(f"  [WARN]  {path}")
        else:
            print(f"  [ ok ]  {path}")

    # --- Consumer mapping summary ---
    print("\n=== Consumer mapping ===\n")
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
            print(f"  {wpath:36s} ← {', '.join(sorted(projs))}")
    print(f"\n  consumed: {len(consumed)}/{len(wheels)}  |  unconsumed: {len(unconsumed)}")

    print("\n=== Summary ===")
    print(f"  wheels:   {len(wheels)}")
    print(f"  failures: {len(failures)}")
    print(f"  warnings: {len(warnings)}")

    if failures:
        print("\nFailures:")
        for f in failures:
            print(f"  - {f}")
        return 1

    if warnings:
        print("\nWarnings (non-blocking):")
        for w in warnings:
            print(f"  - {w}")

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
    if "--graph" in sys.argv:
        sys.argv.remove("--graph")
        wheels = find_wheels()
        print_dep_graph(wheels)
        sys.exit(0)
    sys.exit(main())
