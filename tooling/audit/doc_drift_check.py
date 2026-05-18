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

import argparse
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
    targets = set()
    for line in text.splitlines():
        if line.startswith('.PHONY'):
            targets.update(line.split(':', 1)[1].split())
    return len(targets)


def count_audit_check_functions():
    audit = REPO / "tooling" / "audit" / "audit_wheels.py"
    if not audit.exists():
        return 0
    text = audit.read_text(encoding="utf-8")
    # Count entries in CHECK_TABLE only (between "CHECK_TABLE = [" and closing "]")
    m = re.search(r'^CHECK_TABLE\s*=\s*\[(.+?)^\]', text, re.MULTILINE | re.DOTALL)
    if not m:
        return 0
    table_text = m.group(1)
    table_entries = len(re.findall(r'^\s+\("[\w_]+"', table_text, re.MULTILINE))
    # Unique special checks not in table: consumer, wishlist_stale, project_hygiene
    special_names = set(re.findall(r'record\("([\w_]+)"', text))
    table_names = set(re.findall(r'^\s+\("([\w_]+)"', table_text, re.MULTILINE))
    special = len(special_names - table_names)
    return table_entries + special


def count_tooling_scripts():
    tooling = REPO / "tooling"
    count = 0
    for subdir in ("audit", "test", "wheel", "session", "metrics", "ci"):
        d = tooling / subdir
        if d.is_dir():
            count += len([f for f in d.iterdir()
                          if f.is_file() and f.suffix in (".sh", ".py") and f.name != "__init__.py"])
    return count


def count_test_suites():
    count = 0
    for cmake in WHEELS.rglob("test/CMakeLists.txt"):
        count += 1
    for pytest in WHEELS.rglob("test/test_*.py"):
        count += 1
    for proj_cmake in (REPO / "projects").rglob("test/*/CMakeLists.txt"):
        count += 1
    return count


def check_catalog_completeness():
    readme = WHEELS / "README.md"
    if not readme.exists():
        return []
    text = readme.read_text(encoding="utf-8")
    drifts = []
    for layer in ["primitives", "drivers", "protocols", "modules"]:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for d in sorted(layer_dir.iterdir()):
            if not d.is_dir() or d.name == "__pycache__":
                continue
            if f"[{d.name}](" not in text and f"`{d.name}`" not in text:
                drifts.append((
                    "wheels/README.md",
                    f"{layer}/{d.name} exists on disk but missing from catalog",
                    None, None,
                ))
    return drifts


def check_markdown_links():
    md_files = []
    for p in [REPO / "CLAUDE.md", REPO / "CONTRIBUTING.md", REPO / "README.md"]:
        if p.exists():
            md_files.append(p)
    docs = REPO / "docs"
    if docs.is_dir():
        md_files.extend(sorted(docs.glob("*.md")))
        adr = docs / "adr"
        if adr.is_dir():
            md_files.extend(sorted(adr.glob("*.md")))
    drifts = []
    link_re = re.compile(r'\[([^\]]*)\]\(([^)]+)\)')
    for md in md_files:
        text = md.read_text(encoding="utf-8")
        for m in link_re.finditer(text):
            target = m.group(2)
            if target.startswith(("http://", "https://", "#", "mailto:")):
                continue
            target_clean = target.split("#")[0]
            if not target_clean:
                continue
            resolved = (md.parent / target_clean).resolve()
            if not resolved.exists():
                rel = md.relative_to(REPO).as_posix()
                drifts.append((
                    rel,
                    f"broken link: [{m.group(1)}]({target}) -> {resolved.relative_to(REPO)}",
                    None, None,
                ))
    return drifts


def check_script_headers():
    drifts = []
    for pattern in ("**/*.sh", "**/*.py"):
        for script in sorted((REPO / "tooling").glob(pattern)):
            if ".vsf-upstream" in str(script) or "__pycache__" in str(script):
                continue
            try:
                lines = script.read_text(encoding="utf-8").splitlines()[:5]
            except (UnicodeDecodeError, PermissionError):
                continue
            actual_name = script.name
            for line in lines:
                m = re.match(r'^#\s+([\w._-]+\.(?:sh|py))\s+', line)
                if not m:
                    m = re.match(r'^["\']?\s*([\w._-]+\.(?:sh|py))\s+', line)
                if m:
                    claimed = m.group(1)
                    if claimed != actual_name:
                        rel = script.relative_to(REPO).as_posix()
                        drifts.append((rel,
                                       f"header says {claimed}, file is {actual_name}",
                                       claimed, actual_name))
                    break
    return drifts


def check_layer_readmes():
    """Check wheels/<layer>/README.md headline counts match actual."""
    drifts = []
    layer_map = {
        "drivers": "L1",
        "primitives": "L0",
        "protocols": "L2",
        "modules": "L3",
    }
    wheels = count_wheels()
    for layer, label in layer_map.items():
        readme = WHEELS / layer / "README.md"
        if not readme.exists():
            continue
        text = readme.read_text(encoding="utf-8")
        m = re.search(r'目录清单（(\d+)\s*wheels?）', text)
        if not m:
            m = re.search(r'清单.*?(\d+)\s*wheels?', text)
        if m:
            claimed = int(m.group(1))
            actual = wheels[layer]
            if claimed != actual:
                drifts.append((
                    f"wheels/{layer}/README.md",
                    f"{label} count: says {claimed}, actual {actual}",
                    f"{claimed} wheels", f"{actual} wheels",
                ))
    return drifts


def check_claude_md():
    """Check CLAUDE.md numeric assertions."""
    claude = REPO / "CLAUDE.md"
    if not claude.exists():
        return []
    text = claude.read_text(encoding="utf-8")
    drifts = []
    wheels = count_wheels()

    # Wheel total count: "**150 wheels**" or "**150 个 wheels**"
    m = re.search(r'\*\*(\d+)\s*(?:个\s*)?wheels\*\*', text)
    if m:
        claimed = int(m.group(1))
        actual = wheels["total"]
        if claimed != actual:
            fixed_match = re.sub(r'\b' + str(claimed) + r'\b', str(actual), m.group(0))
            drifts.append(("CLAUDE.md", f"wheel count: says {claimed}, actual {actual}",
                           m.group(0), fixed_match))

    # Per-layer counts: "L0 原语 55：" or "L0 (55):" — Chinese and English formats
    layer_map = [("L0", "primitives"), ("L1", "drivers"), ("L2", "protocols"), ("L3", "modules")]
    for layer, key in layer_map:
        # Match "L0 原语 55" or "L0 (55)"
        m = re.search(rf'{layer}\s+\S+\s+(\d+)[：:]', text)
        if not m:
            m = re.search(rf'{layer}\s*\((\d+)\)', text)
        if m:
            claimed = int(m.group(1))
            actual = wheels[key]
            if claimed != actual:
                fixed_match = re.sub(r'\b' + str(claimed) + r'\b', str(actual), m.group(0))
                drifts.append(("CLAUDE.md", f"{layer} count: says {claimed}, actual {actual}",
                               m.group(0), fixed_match))

    # Makefile target count: "Makefile（N 目标）"
    m = re.search(r'Makefile（(\d+) .?目标）', text)
    if m:
        claimed = int(m.group(1))
        actual = count_makefile_targets()
        if claimed != actual:
            drifts.append(("CLAUDE.md", f"Makefile targets: says {claimed}, actual {actual}",
                           f"Makefile（{claimed}", f"Makefile（{actual}"))

    # Audit check count: handles both "52 项 CHECK_TABLE + 3 特殊检查" breakdown
    # and simple "N 项" total. Validate the SUM matches actual.
    actual = count_audit_check_functions()
    m_detail = re.search(r'(\d+)\s*项?\s*CHECK_TABLE\s*\+\s*(\d+)\s*特殊', text)
    m_simple = re.search(r'audit_wheels\.py（(\d+) 项', text)
    if m_detail:
        table_claimed = int(m_detail.group(1))
        special_claimed = int(m_detail.group(2))
        if table_claimed + special_claimed != actual:
            drifts.append(("CLAUDE.md",
                           f"audit check count: says {table_claimed}+{special_claimed}={table_claimed+special_claimed}, actual {actual}",
                           m_detail.group(0),
                           f"{actual} 项"))
    elif m_simple:
        claimed = int(m_simple.group(1))
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
    parser = argparse.ArgumentParser(
        description="Verify documentation assertions match reality "
                    "(CLAUDE.md, ai-workflow.md, debugging-playbook.md)."
    )
    parser.add_argument("--fix", action="store_true",
                        help="auto-fix simple numeric drifts in CLAUDE.md")
    args = parser.parse_args()
    fix_mode = args.fix

    all_drifts = []
    all_drifts.extend(check_catalog_completeness())
    all_drifts.extend(check_markdown_links())
    all_drifts.extend(check_script_headers())
    all_drifts.extend(check_layer_readmes())
    all_drifts.extend(check_claude_md())
    all_drifts.extend(check_workflow_md())
    all_drifts.extend(check_playbook_md())

    if not all_drifts:
        print("doc_drift_check: all assertions match reality")
        return 0

    print(f"doc_drift_check: {len(all_drifts)} drift(s) found\n")
    fixed = 0
    unfixable = 0
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
                raw = filepath.read_bytes()
                text = raw.decode("utf-8")
                text = text.replace(old, new, 1)
                filepath.write_bytes(text.encode("utf-8"))
                print(f"    → fixed: {old!r} → {new!r}")
                fixed += 1
        elif fix_mode and (not old or not new):
            unfixable += 1

    if fix_mode:
        if unfixable:
            print(f"\n{fixed} fixed, {unfixable} unfixable (manual intervention needed)")
            return 1
        print(f"\nAll {fixed} drift(s) auto-fixed")
        return 0

    print(f"\nRe-run with --fix to auto-correct simple drifts.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
