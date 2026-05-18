#!/usr/bin/env python3
"""sync_wheel_catalog.py -- keep wheels/README.md catalog in sync with disk.

Scans all wheel directories across L0-L3, compares against the catalog
table in wheels/README.md, and adds missing entries.  Also updates the
section-header counts and the top summary table.

Usage:
    python tooling/wheel/sync_wheel_catalog.py            # dry-run (print diff)
    python tooling/wheel/sync_wheel_catalog.py --apply     # rewrite in place
    python tooling/wheel/sync_wheel_catalog.py --check     # exit 1 if any missing (CI)

Idempotent: running twice with --apply produces the same file.
"""

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
README = WHEELS / "README.md"

# Layer directories in the order they appear in the README
LAYERS = [
    ("drivers",    "L1 Drivers"),
    ("primitives", "L0 Primitives"),
    ("protocols",  "L2 Protocols"),
    ("modules",    "L3 Modules"),
]

# Map from layer directory to the relative path prefix used in links
# e.g. [bmp280](drivers/bmp280/)
LAYER_LINK_PREFIX = {
    "drivers":    "drivers",
    "primitives": "primitives",
    "protocols":  "protocols",
    "modules":    "modules",
}

# Patterns for detecting interface type from header content
INTERFACE_PATTERNS = [
    (re.compile(r'_i2c_t\b'),      "I2C"),
    (re.compile(r'_spi_t\b'),      "SPI"),
    (re.compile(r'_gpio_t\b'),     "GPIO"),
    (re.compile(r'_uart_t\b'),     "UART"),
    (re.compile(r'_pwm_t\b'),      "PWM"),
    (re.compile(r'_onewire_t\b'),  "1-Wire"),
]

# Also detect from Deps line in header comment
DEPS_INTERFACE = [
    ("i2c_hal",    "I2C"),
    ("spi_hal",    "SPI"),
    ("gpio_hal",   "GPIO"),
    ("uart_hal",   "UART"),
    ("pwm_hal",    "PWM"),
    ("onewire",    "1-Wire"),
]


# ---------------------------------------------------------------------------
# Scanning
# ---------------------------------------------------------------------------

def scan_wheel_dirs():
    """Return {layer_dir: sorted list of wheel names} for all 4 layers."""
    result = {}
    for layer_dir, _ in LAYERS:
        d = WHEELS / layer_dir
        if not d.is_dir():
            result[layer_dir] = []
            continue
        names = sorted(
            e.name for e in d.iterdir()
            if e.is_dir() and e.name != "__pycache__"
        )
        result[layer_dir] = names
    return result


def extract_wheel_info(layer_dir, name):
    """Read the wheel header and extract description + interface type.

    Returns (description: str, interface: str).
    """
    wheel_dir = WHEELS / layer_dir / name
    header = wheel_dir / f"{name}.h"

    description = name  # fallback
    interface = ""

    if not header.is_file():
        return description, interface

    try:
        text = header.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return description, interface

    # Extract one-line description from the header comment block.
    # Pattern:  " * name -- description" or " * name — description"
    m = re.search(
        r'^\s*\*\s*' + re.escape(name) + r'\s*(?:--|—|–)\s*(.+)',
        text, re.MULTILINE,
    )
    if m:
        description = m.group(1).strip()

    # Detect interface from HAL struct typedef names
    for pat, iface in INTERFACE_PATTERNS:
        if pat.search(text):
            interface = iface
            break

    # Fallback: check the Deps line
    if not interface:
        deps_m = re.search(r'Deps:\s*(.+)', text)
        if deps_m:
            deps_text = deps_m.group(1)
            for dep_key, iface in DEPS_INTERFACE:
                if dep_key in deps_text:
                    interface = iface
                    break

    # Append interface in parens to description if no parens already present
    # at the end.  Many descriptions already end with "(I2C)", "(bit-bang)",
    # "(SPI, RGB565)" etc. — don't double up.
    if interface and not re.search(r'\([^)]+\)\s*$', description):
        description = f"{description} ({interface})"

    return description, interface


# ---------------------------------------------------------------------------
# README parsing
# ---------------------------------------------------------------------------

def find_cataloged_names(text):
    """Return set of wheel names found in catalog table rows.

    Matches both formats:
      | [`name`](...) | ... |
      | [name](...) | ... |
    """
    # Pattern: | [optional-backtick name optional-backtick](path/) |
    names = set()
    for m in re.finditer(r'\|\s*\[`?(\w+)`?\]\(', text):
        names.add(m.group(1))
    return names


def find_layer_section(text, layer_label):
    """Find the byte range of a layer section in the README.

    Returns (start, end) indices.  The section runs from the ## heading
    to the next --- or ## heading or end-of-file.
    """
    # Match ## L1 Drivers (115) or ## L3 Modules (20)
    pattern = re.compile(
        r'^## ' + re.escape(layer_label) + r'\s*\(\d+\)',
        re.MULTILINE,
    )
    m = pattern.search(text)
    if not m:
        return None, None

    start = m.start()

    # Find end: next "---" or "## " heading
    rest = text[m.end():]
    end_m = re.search(r'^(?:---|## )', rest, re.MULTILINE)
    if end_m:
        end = m.end() + end_m.start()
    else:
        end = len(text)

    return start, end


def find_last_table_row(section_text):
    """Return the index (within section_text) just after the last table row.

    Table rows match: | ... | ... |
    We want to insert new rows right after the last one in the section.
    """
    last_end = 0
    for m in re.finditer(r'^\|.+\|[ \t]*$', section_text, re.MULTILINE):
        last_end = m.end()
    return last_end


def update_section_count(text, layer_label, new_count):
    """Update the count in a section header like '## L1 Drivers (115)'."""
    pattern = re.compile(
        r'(## ' + re.escape(layer_label) + r'\s*\()\d+(\))',
    )
    return pattern.sub(rf'\g<1>{new_count}\2', text)


def update_summary_table(text, layer_counts):
    """Update the top summary table counts."""
    label_map = {
        "drivers":    "L1 Drivers",
        "primitives": "L0 Primitives",
        "protocols":  "L2 Protocols",
        "modules":    "L3 Modules",
    }
    for layer_dir, count in layer_counts.items():
        label = label_map.get(layer_dir)
        if not label:
            continue
        # Match: | L1 Drivers | `drivers/` | 115 |
        pattern = re.compile(
            r'(\|\s*' + re.escape(label) + r'\s*\|[^|]+\|\s*)\d+(\s*\|)',
        )
        text = pattern.sub(rf'\g<1>{count}\2', text)

    total = sum(layer_counts.values())
    # Update bold total at top: **259**
    text = re.sub(r'\*\*\d+\*\*\s*reusable', f'**{total}** reusable', text)

    return text


def format_table_row(layer_dir, name, description):
    """Format a single catalog table row."""
    prefix = LAYER_LINK_PREFIX[layer_dir]
    return f"| [{name}]({prefix}/{name}/) | {description} |"


# ---------------------------------------------------------------------------
# Main logic
# ---------------------------------------------------------------------------

def sync_catalog(apply=False, check=False):
    """Synchronize wheels/README.md with wheel directories on disk.

    Returns the number of missing entries found.
    """
    if not README.is_file():
        print(f"ERROR: {README} not found", file=sys.stderr)
        return 1

    text = README.read_text(encoding="utf-8")
    original_text = text

    disk_wheels = scan_wheel_dirs()
    cataloged = find_cataloged_names(text)

    missing_by_layer = {}
    layer_counts = {}

    for layer_dir, layer_label in LAYERS:
        names_on_disk = disk_wheels.get(layer_dir, [])
        layer_counts[layer_dir] = len(names_on_disk)
        missing = [n for n in names_on_disk if n not in cataloged]
        if missing:
            missing_by_layer[layer_dir] = missing

    total_missing = sum(len(v) for v in missing_by_layer.values())

    if total_missing == 0 and not check:
        # Still update counts in case they drifted
        for _, layer_label in LAYERS:
            layer_dir = [ld for ld, ll in LAYERS if ll == layer_label][0]
            text = update_section_count(text, layer_label, layer_counts[layer_dir])
        text = update_summary_table(text, layer_counts)

        if text != original_text and apply:
            tmp = str(README) + ".tmp"
            Path(tmp).write_text(text, encoding="utf-8")
            Path(tmp).replace(README)
            print("Updated counts (no missing wheels).")
        elif text != original_text:
            print("Counts need updating. Run with --apply to fix.")
        else:
            print("Catalog is up to date.")
        return 0

    # Report missing
    if total_missing > 0:
        print(f"Found {total_missing} wheel(s) missing from catalog:")
        for layer_dir, missing in missing_by_layer.items():
            layer_label = dict(LAYERS)[layer_dir]
            for name in missing:
                print(f"  {layer_label}: {name}")

    if check:
        return total_missing

    # Insert missing entries into their layer sections
    for layer_dir, layer_label in LAYERS:
        missing = missing_by_layer.get(layer_dir, [])
        if not missing:
            continue

        sec_start, sec_end = find_layer_section(text, layer_label)
        if sec_start is None:
            print(f"WARNING: section '{layer_label}' not found in README",
                  file=sys.stderr)
            continue

        section = text[sec_start:sec_end]

        # Build new rows
        new_rows = []
        for name in missing:
            desc, _iface = extract_wheel_info(layer_dir, name)
            row = format_table_row(layer_dir, name, desc)
            new_rows.append(row)

        # Find insertion point: after last table row in the section
        insert_offset = find_last_table_row(section)
        if insert_offset == 0:
            # No table rows found (shouldn't happen), append at end
            insert_offset = len(section.rstrip())

        new_block = "\n" + "\n".join(new_rows)
        section = section[:insert_offset] + new_block + section[insert_offset:]
        text = text[:sec_start] + section + text[sec_end:]

    # Update counts
    for _, layer_label in LAYERS:
        layer_dir = [ld for ld, ll in LAYERS if ll == layer_label][0]
        text = update_section_count(text, layer_label, layer_counts[layer_dir])
    text = update_summary_table(text, layer_counts)

    if apply:
        tmp = str(README) + ".tmp"
        Path(tmp).write_text(text, encoding="utf-8")
        Path(tmp).replace(README)
        print(f"Updated {README.name}: added {total_missing} entries, "
              f"counts updated.")
    else:
        print(f"\n{total_missing} entries to add. Run with --apply to write.")
        if text != original_text:
            print("(counts also need updating)")

    return total_missing


def main():
    ap = argparse.ArgumentParser(
        description="Sync wheels/README.md catalog with wheel directories on disk.",
    )
    ap.add_argument("--apply", action="store_true",
                    help="write changes to wheels/README.md")
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if any wheels missing from catalog (CI mode)")
    args = ap.parse_args()

    if args.check and args.apply:
        print("ERROR: --check and --apply are mutually exclusive",
              file=sys.stderr)
        sys.exit(2)

    n_missing = sync_catalog(apply=args.apply, check=args.check)

    if args.check and n_missing > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
