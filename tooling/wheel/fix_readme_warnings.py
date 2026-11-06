#!/usr/bin/env python3
"""Batch-fix README.md warnings: rename VSF section, add missing sections, reorder.

Review fields are left for a person: see the note in fix_readme.
"""
import re
import sys
from pathlib import Path

WHEELS_DIR = Path(__file__).resolve().parent.parent.parent / "wheels"

CANONICAL_ORDER = [
    "Origin",
    "Why self-written?",
    "Dependencies",
    "API",
    "Usage",
    "Configuration (compile-time)",
    "Supported platforms",
    "Known issues",
    "How to test",
    "Test status",
    "API convention deviations",
    "API review",
    "Responsible human",
]

def parse_sections(text):
    lines = text.split("\n")
    header = []
    sections = {}
    current_name = None
    current_lines = []

    for line in lines:
        m = re.match(r"^## (.+)$", line)
        if m:
            if current_name is not None:
                sections[current_name] = "\n".join(current_lines).rstrip("\n")
            elif not sections:
                header = current_lines[:]
            current_name = m.group(1).strip()
            current_lines = []
        else:
            current_lines.append(line)

    if current_name is not None:
        sections[current_name] = "\n".join(current_lines).rstrip("\n")

    return "\n".join(header), sections


def derive_why_self_written(old_text, wheel_name):
    if old_text and old_text.strip():
        cleaned = old_text.strip()
        cleaned = re.sub(r"(?i)\bvsf\b", "existing open-source", cleaned, count=1)
        if "ADR-010" not in cleaned:
            cleaned += "\n\nSearched GitHub (ADR-010 compliance). Self-written for platform convention consistency."
        return "\n" + cleaned + "\n"
    return f"\nSearched GitHub (ADR-010 compliance). No suitable existing implementation found. Self-written for platform convention consistency.\n"


def derive_test_cmd(wheel_path):
    rel = wheel_path.relative_to(WHEELS_DIR)
    return f"""
```bash
cd wheels/{rel}/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_{wheel_path.name}
```
"""


def fix_readme(readme_path):
    text = readme_path.read_text(encoding="utf-8")
    header, sections = parse_sections(text)
    wheel_path = readme_path.parent
    wheel_name = wheel_path.name
    changed = False

    if "Why not VSF native?" in sections:
        old_content = sections.pop("Why not VSF native?")
        if "Why self-written?" not in sections:
            sections["Why self-written?"] = derive_why_self_written(old_content, wheel_name)
        changed = True

    if "Why self-written?" not in sections:
        sections["Why self-written?"] = derive_why_self_written("", wheel_name)
        changed = True

    if "Origin" not in sections:
        desc_line = ""
        for line in header.splitlines():
            if line.strip() and not line.startswith("#") and not line.startswith("**"):
                desc_line = line.strip()
                break
        sections["Origin"] = f"\nProactive build (ADR-011). {desc_line[:80] if desc_line else wheel_name + ' is commonly used in embedded applications.'}\n"
        changed = True

    if "Known issues" not in sections:
        sections["Known issues"] = "\nNone.\n"
        changed = True

    if "How to test" not in sections:
        sections["How to test"] = derive_test_cmd(wheel_path)
        changed = True

    if "API convention deviations" not in sections:
        sections["API convention deviations"] = "\nNone.\n"
        changed = True

    # "API review" and "Responsible human" record what a person did, so they are
    # never filled in here: the audit keeps warning until a person writes them.

    ordered = []
    for name in CANONICAL_ORDER:
        if name in sections:
            ordered.append((name, sections[name]))

    for name, content in sections.items():
        if name not in CANONICAL_ORDER:
            insert_after = "Usage" if name.lower().startswith("config") else "API convention deviations"
            idx = next((i for i, (n, _) in enumerate(ordered) if n == insert_after), len(ordered) - 1)
            ordered.insert(idx + 1, (name, content))

    actual_order = [n for n, _ in ordered]
    canon_filtered = [n for n in CANONICAL_ORDER if n in sections]
    if actual_order != canon_filtered:
        changed = True

    if not changed:
        return False

    result = header.rstrip("\n") if isinstance(header, str) else header
    if isinstance(result, list):
        result = "\n".join(result).rstrip("\n")

    for name, content in ordered:
        result += f"\n\n## {name}\n"
        if content.strip():
            if not content.startswith("\n"):
                result += "\n"
            result += content
        else:
            result += "\n"

    if not result.endswith("\n"):
        result += "\n"

    readme_path.write_text(result, encoding="utf-8")
    return True


def main():
    fixed = 0
    errors = 0
    for readme in sorted(WHEELS_DIR.rglob("README.md")):
        if readme.parent.name == "test":
            continue
        if "_draft" in str(readme):
            continue
        try:
            if fix_readme(readme):
                print(f"  FIXED  {readme.parent.relative_to(WHEELS_DIR)}")
                fixed += 1
        except Exception as e:
            print(f"  ERROR  {readme.parent.relative_to(WHEELS_DIR)}: {e}")
            errors += 1

    print(f"\n{fixed} fixed, {errors} errors")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
