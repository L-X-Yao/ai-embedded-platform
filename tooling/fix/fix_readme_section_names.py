#!/usr/bin/env python3
"""Normalize README section heading names to standard."""

from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules"]

RENAMES = {
    "## API summary": "## API",
    "## Configuration macros": "## Configuration (compile-time)",
    "## Compile-time config": "## Configuration (compile-time)",
    "## Convention Deviations": "## API convention deviations",
}

stats = {"renamed": 0}


def find_wheels():
    for layer in LAYERS:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for entry in sorted(layer_dir.iterdir()):
            if entry.is_dir() and entry.name not in {"test", "build", "__pycache__"}:
                yield layer, entry.name, entry


def fix_bare_configuration(text):
    """Fix '## Configuration' (bare) → '## Configuration (compile-time)'
    but NOT '## Configuration (compile-time)' which is already correct."""
    lines = text.split("\n")
    changed = False
    for i, line in enumerate(lines):
        if line.strip() == "## Configuration":
            lines[i] = "## Configuration (compile-time)"
            changed = True
    return "\n".join(lines), changed


def main():
    for layer, name, wheel_dir in find_wheels():
        path = f"wheels/{layer}/{name}"
        readme = wheel_dir / "README.md"
        if not readme.exists():
            continue

        text = readme.read_text(encoding="utf-8")
        new_text = text
        changed = False

        for old, new in RENAMES.items():
            if old in new_text:
                new_text = new_text.replace(old, new)
                changed = True

        new_text, bare_changed = fix_bare_configuration(new_text)
        if bare_changed:
            changed = True

        if changed:
            readme.write_text(new_text, encoding="utf-8")
            stats["renamed"] += 1
            print(f"  FIX  {path}")

    print(f"\n  Renamed: {stats['renamed']}")


if __name__ == "__main__":
    main()
