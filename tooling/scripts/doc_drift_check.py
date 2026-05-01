#!/usr/bin/env python3
"""
doc_drift_check.py — verify documentation assertions match reality.

Checks numeric claims in CLAUDE.md, ai-workflow.md, debugging-playbook.md
against actual file counts, directory listings, and script contents.

Usage:
  doc_drift_check.py           # report all drift
  doc_drift_check.py --fix     # auto-fix simple numeric drifts in CLAUDE.md

Exit 0 if clean, 1 if drift found.
"""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"


def count_wheels():
    counts = {}
    for layer in ["primitives", "drivers", "protocols", "modules"]:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            counts[layer] = 0
            continue
        n = 0
        for d in layer_dir.iterdir():
            if d.is_dir() and d.name != "__pycache__":
                n += 1
        counts[layer] = n
    counts["total"] = sum(counts.values())
    return counts


def count_makefile_targets():
    mk = REPO / "Makefile"
    if not mk.exists():
        return 0
    text = mk.read_text(encoding="utf-8")
    phony = re.search(r'^\.PHONY:\s*(.+)$', text, re.MULTILINE)
    if not phony:
        return 0
    return len(phony.group(1).split())


def count_audit_checks():
    audit = REPO / "tooling" / "scripts" / "audit_wheels.py"
    if not audit.exists():
        return 0
    text = audit.read_text(encoding="utf-8")
    return len(re.findall(r'^\s+# ---.*check|^\s+# ---.*section|^def has_', text, re.MULTILINE))


def count_audit_check_functions():
    audit = REPO / "tooling" / "scripts" / "audit_wheels.py"
    if not audit.exists():
        return 0
    text = audit.read_text(encoding="utf-8")
    # Count section headings that represent actual checks, excluding
    # summary/info sections (Wheel audit count, Consumer mapping)
    exclude = {"Wheel audit", "Consumer mapping"}
    matches = re.findall(r'qprint.*=== (.+?) ===', text)
    return len([m for m in matches if not any(e in m for e in exclude)])


def count_tooling_scripts():
    scripts = REPO / "tooling" / "scripts"
    if not scripts.is_dir():
        return 0
    return len([f for f in scripts.iterdir()
                if f.is_file() and f.suffix in (".sh", ".py") and f.name != "__init__.py"])


def count_test_suites():
    count = 0
    for cmake in WHEELS.rglob("test/CMakeLists.txt"):
        count += 1
    for pytest in WHEELS.rglob("test/test_*.py"):
        count += 1
    for proj_cmake in (REPO / "projects").rglob("test/*/CMakeLists.txt"):
        count += 1
    return count


def check_claude_md():
    """Check CLAUDE.md numeric assertions."""
    claude = REPO / "CLAUDE.md"
    if not claude.exists():
        return []
    text = claude.read_text(encoding="utf-8")
    drifts = []
    wheels = count_wheels()

    # Wheel total count: "58 个 wheels" or "**58 个 wheels**"
    m = re.search(r'\*\*(\d+) 个 wheels\*\*', text)
    if m:
        claimed = int(m.group(1))
        actual = wheels["total"]
        if claimed != actual:
            drifts.append(("CLAUDE.md", f"wheel count: says {claimed}, actual {actual}",
                           f"**{claimed} 个 wheels**", f"**{actual} 个 wheels**"))

    # Per-layer counts: "L0 (30):"
    for layer, key in [("L0", "primitives"), ("L1", "drivers"), ("L2", "protocols"), ("L3", "modules")]:
        m = re.search(rf'{layer} \((\d+)\)', text)
        if m:
            claimed = int(m.group(1))
            actual = wheels[key]
            if claimed != actual:
                drifts.append(("CLAUDE.md", f"{layer} count: says {claimed}, actual {actual}",
                               f"{layer} ({claimed})", f"{layer} ({actual})"))

    # Makefile target count: "Makefile（N 目标）"
    m = re.search(r'Makefile（(\d+) .?目标）', text)
    if m:
        claimed = int(m.group(1))
        actual = count_makefile_targets()
        if claimed != actual:
            drifts.append(("CLAUDE.md", f"Makefile targets: says {claimed}, actual {actual}",
                           f"Makefile（{claimed}", f"Makefile（{actual}"))

    # Audit check count: "audit_wheels.py（N 项"
    m = re.search(r'audit_wheels\.py（(\d+) 项', text)
    if m:
        claimed = int(m.group(1))
        actual = count_audit_check_functions()
        if claimed != actual:
            drifts.append(("CLAUDE.md", f"audit check count: says {claimed}, actual {actual}",
                           f"audit_wheels.py（{claimed} 项", f"audit_wheels.py（{actual} 项"))

    return drifts


def check_workflow_md():
    """Check ai-workflow.md assertions."""
    wf = REPO / "docs" / "ai-workflow.md"
    if not wf.exists():
        return []
    text = wf.read_text(encoding="utf-8")
    drifts = []

    # Audit check count in tool table
    m = re.search(r'audit_wheels\.py.*?(\d+) 项检查', text)
    if m:
        claimed = int(m.group(1))
        actual = count_audit_check_functions()
        if claimed != actual:
            drifts.append(("ai-workflow.md", f"audit check count: says {claimed}, actual {actual}",
                           f"{claimed} 项检查", f"{actual} 项检查"))

    # Script count in tool table
    table_scripts = len(re.findall(r'^\| `\w+\.\w+`', text, re.MULTILINE))
    actual_scripts = count_tooling_scripts()
    if table_scripts > 0 and table_scripts < actual_scripts - 2:
        drifts.append(("ai-workflow.md",
                       f"tool table: lists {table_scripts} scripts, repo has {actual_scripts}",
                       None, None))

    return drifts


def check_playbook_md():
    """Check debugging-playbook.md assertions."""
    pb = REPO / "docs" / "debugging-playbook.md"
    if not pb.exists():
        return []
    text = pb.read_text(encoding="utf-8")
    drifts = []

    m = re.search(r'make audit\s+#\s*(\d+) 项', text)
    if m:
        claimed = int(m.group(1))
        actual = count_audit_check_functions()
        if claimed != actual:
            drifts.append(("debugging-playbook.md", f"audit count: says {claimed}, actual {actual}",
                           f"# {claimed} 项", f"# {actual} 项"))

    return drifts


def main():
    fix_mode = "--fix" in sys.argv

    all_drifts = []
    all_drifts.extend(check_claude_md())
    all_drifts.extend(check_workflow_md())
    all_drifts.extend(check_playbook_md())

    if not all_drifts:
        print("doc_drift_check: all assertions match reality")
        return 0

    print(f"doc_drift_check: {len(all_drifts)} drift(s) found\n")
    for filename, desc, old, new in all_drifts:
        print(f"  [{filename}] {desc}")
        if fix_mode and old and new:
            if filename == "CLAUDE.md":
                filepath = REPO / "CLAUDE.md"
            elif "/" not in filename:
                filepath = REPO / "docs" / filename
            else:
                filepath = REPO / filename
            if filepath.exists():
                text = filepath.read_text(encoding="utf-8")
                text = text.replace(old, new, 1)
                filepath.write_text(text, encoding="utf-8")
                print(f"    → fixed: {old!r} → {new!r}")

    if not fix_mode:
        print(f"\nRe-run with --fix to auto-correct simple drifts.")

    return 1


if __name__ == "__main__":
    sys.exit(main())
