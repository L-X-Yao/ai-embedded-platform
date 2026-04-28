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


def has_project_origin(wheel_dir: Path) -> bool:
    """Check if README documents extraction from a real project."""
    readme = wheel_dir / "README.md"
    if not readme.exists():
        return False
    try:
        text = readme.read_text(encoding="utf-8")
    except OSError:
        return False
    return "Extracted from" in text and "projects/" in text


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
