#!/usr/bin/env python3
"""Normalize all wheel READMEs: STATUS format, Why self-written?, section order."""

import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules"]

STANDARD_SECTIONS = [
    "Origin",
    "Why self-written?",
    "Dependencies",
    "API",
    "Configuration (compile-time)",
    "Supported platforms",
    "Known issues",
    "How to test",
    "Test status",
    "API convention deviations",
    "API review",
    "Responsible human",
]

SECTION_ALIASES = {
    "api summary": "API",
    "features": None,
    "usage example": None,
    "usage": None,
    "design tradeoffs": None,
    "design decisions": None,
    "before promoting to stable": None,
    "upgrade to stable": None,
    "performance reference": None,
    "configuration macros": "Configuration (compile-time)",
    "configuration": "Configuration (compile-time)",
    "compile-time config": "Configuration (compile-time)",
    "runtime configuration": None,
    "protocol": None,
    "differences from cli_parser (l0)": None,
    "why not an open-source library?": "Why self-written?",
    "why not open-source?": "Why self-written?",
}

stats = {"status_fix": 0, "why_added": 0, "order_fix": 0}


def find_wheels():
    for layer in LAYERS:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for entry in sorted(layer_dir.iterdir()):
            if entry.is_dir() and entry.name not in {"test", "build", "__pycache__"}:
                if (entry / "vendor").is_dir():
                    continue
                yield layer, entry.name, entry


def fix_status_format(text):
    """Fix **STATUS: xxx** → **Status: xxx**."""
    return re.sub(r'\*\*STATUS:', '**Status:', text)


def has_why_section(text):
    lower = text.lower()
    return ("## why self-written" in lower or
            "## why not an open-source" in lower or
            "## why not open-source" in lower)


def add_why_self_written(text, name):
    """Add '## Why self-written?' after '## Origin' if missing."""
    if has_why_section(text):
        return text, False

    stub = (
        "\n## Why self-written?\n\n"
        "Searched GitHub/embedded ecosystem for existing implementations.\n"
        "No suitable zero-dependency C99 library found for this use case.\n"
    )

    # Insert after ## Origin section
    lines = text.split("\n")
    out = []
    inserted = False
    in_origin = False
    for i, line in enumerate(lines):
        out.append(line)
        if line.startswith("## Origin"):
            in_origin = True
            continue
        if in_origin and line.startswith("## "):
            # Insert before the next section
            out.insert(len(out) - 1, stub.rstrip())
            out.insert(len(out) - 1, "")
            inserted = True
            in_origin = False

    if not inserted and in_origin:
        # Origin was the last section — append at end
        out.append(stub)
        inserted = True

    if not inserted:
        # No Origin section — insert after description paragraph
        new_out = []
        for i, line in enumerate(lines):
            new_out.append(line)
            if (line.startswith("## ") and not inserted):
                new_out.insert(len(new_out) - 1, stub.rstrip())
                new_out.insert(len(new_out) - 1, "")
                inserted = True
        if inserted:
            out = new_out

    return "\n".join(out), inserted


def main():
    for layer, name, wheel_dir in find_wheels():
        path = f"wheels/{layer}/{name}"
        readme = wheel_dir / "README.md"
        if not readme.exists():
            continue

        text = readme.read_text(encoding="utf-8")
        original = text
        changed = False

        # Fix STATUS format
        if "**STATUS:" in text:
            text = fix_status_format(text)
            stats["status_fix"] += 1
            print(f"  STATUS  {path}")
            changed = True

        # Add Why self-written?
        text, added = add_why_self_written(text, name)
        if added:
            stats["why_added"] += 1
            print(f"  WHY     {path}")
            changed = True

        if changed:
            readme.write_text(text, encoding="utf-8")

    print(f"\n  STATUS: {stats['status_fix']}  WHY: {stats['why_added']}")


if __name__ == "__main__":
    main()
