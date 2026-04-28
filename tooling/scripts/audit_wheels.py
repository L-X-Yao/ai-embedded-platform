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
    return missing


def has_file_header(wheel_dir: Path, name: str) -> bool:
    """Check .h and .c have api-conventions §8 file header (Layer/Status/Deps)."""
    for ext in (".h", ".c"):
        src = wheel_dir / f"{name}{ext}"
        if not src.exists():
            continue
        try:
            text = src.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        if "Layer:" not in text or "Status:" not in text:
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


def has_ci_job(wheel_path: str, ci_text: str) -> bool:
    return wheel_path in ci_text


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
        ci_ok = has_ci_job(path, ci_text)
        consumers = find_consumers(path, name)
        consumer_required = layer in CONSUMER_REQUIRED

        wheel_dir = WHEELS / layer / name
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


if __name__ == "__main__":
    sys.exit(main())
