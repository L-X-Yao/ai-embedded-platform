#!/usr/bin/env python3
"""Reorder README sections to match wheel-contract §3.2 standard order."""

import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules"]

STANDARD_ORDER = [
    "origin",
    "why self-written?",
    "why not an open-source library?",
    "why not open-source?",
    "why this upstream?",
    "dependencies",
    "api",
    "api summary",
    "configuration (compile-time)",
    "configuration macros",
    "configuration",
    "compile-time config",
    "supported platforms",
    "known issues",
    "how to test",
    "test status",
    "api convention deviations",
    "convention deviations",
    "api review",
    "responsible human",
]

stats = {"reordered": 0, "already_ok": 0}


def find_wheels():
    for layer in LAYERS:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for entry in sorted(layer_dir.iterdir()):
            if entry.is_dir() and entry.name not in {"test", "build", "__pycache__"}:
                yield layer, entry.name, entry


def parse_sections(text):
    """Split README into preamble + list of (heading, content) tuples."""
    lines = text.split("\n")
    preamble_lines = []
    sections = []
    current_heading = None
    current_lines = []

    for line in lines:
        if line.startswith("## "):
            if current_heading is not None:
                sections.append((current_heading, "\n".join(current_lines)))
            current_heading = line[3:].strip()
            current_lines = []
        elif current_heading is None:
            preamble_lines.append(line)
        else:
            current_lines.append(line)

    if current_heading is not None:
        sections.append((current_heading, "\n".join(current_lines)))

    preamble = "\n".join(preamble_lines)
    return preamble, sections


def section_sort_key(heading):
    """Return sort key: standard sections get their index, extras get 999+."""
    lower = heading.lower()
    for i, std in enumerate(STANDARD_ORDER):
        if lower == std:
            return i
    return 999


def is_ordered(sections):
    """Check if sections are already in standard order."""
    keys = [section_sort_key(h) for h, _ in sections]
    return keys == sorted(keys)


def reorder_sections(sections):
    """Sort sections: standard order first, extras after Responsible human."""
    standard = []
    extras = []
    for heading, content in sections:
        if section_sort_key(heading) < 999:
            standard.append((heading, content))
        else:
            extras.append((heading, content))

    standard.sort(key=lambda x: section_sort_key(x[0]))
    return standard + extras


def rebuild_readme(preamble, sections):
    """Rebuild README from preamble and ordered sections."""
    parts = [preamble.rstrip()]
    for heading, content in sections:
        parts.append("")
        parts.append(f"## {heading}")
        # Ensure content starts with a blank line after heading
        stripped = content.lstrip("\n")
        if stripped:
            parts.append("")
            parts.append(stripped.rstrip())
        else:
            parts.append("")
    # Ensure single trailing newline
    result = "\n".join(parts)
    result = result.rstrip() + "\n"
    return result


def main():
    for layer, name, wheel_dir in find_wheels():
        path = f"wheels/{layer}/{name}"
        readme = wheel_dir / "README.md"
        if not readme.exists():
            continue

        text = readme.read_text(encoding="utf-8")
        preamble, sections = parse_sections(text)

        if not sections:
            continue

        if is_ordered(sections):
            stats["already_ok"] += 1
            continue

        reordered = reorder_sections(sections)
        new_text = rebuild_readme(preamble, reordered)

        if new_text != text:
            readme.write_text(new_text, encoding="utf-8")
            stats["reordered"] += 1

            old_order = [h for h, _ in sections]
            new_order = [h for h, _ in reordered]
            print(f"  REORDER {path}")
        else:
            stats["already_ok"] += 1

    print(f"\n  Reordered: {stats['reordered']}  Already OK: {stats['already_ok']}")


if __name__ == "__main__":
    main()
