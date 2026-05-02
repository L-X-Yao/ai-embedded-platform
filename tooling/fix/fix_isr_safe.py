#!/usr/bin/env python3
"""
fix_isr_safe.py — batch-add @isr_safe annotations to L0/L1 wheel headers.

Rules:
  - init/deinit/reset/config → @isr_safe: no
  - Functions calling HAL callbacks → @isr_safe: no
  - Pure functions → @isr_safe: yes

Usage:
  fix_isr_safe.py           # dry-run
  fix_isr_safe.py --apply   # apply changes
"""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
APPLY = "--apply" in sys.argv

SETUP_SUFFIXES = ("_init", "_deinit", "_reset", "_config", "_configure",
                  "_set_callback", "_register", "_create", "_destroy")
HAL_TOKENS = {"i2c", "spi", "gpio", "uart", "hal", "flash", "write_reg",
              "read_reg", "transfer", "send", "recv", "transmit", "delay"}


def get_func_body(c_text, func_name):
    """Extract function body text for classification."""
    pat = re.compile(rf'\b{re.escape(func_name)}\s*\([^)]*\)\s*\{{', re.DOTALL)
    m = pat.search(c_text)
    if not m:
        return ""
    depth, pos = 1, m.end()
    while pos < len(c_text) and depth > 0:
        if c_text[pos] == '{':
            depth += 1
        elif c_text[pos] == '}':
            depth -= 1
        pos += 1
    return c_text[m.end():pos].lower()


def classify(func_name, c_text):
    """Return 'yes' or 'no' for @isr_safe."""
    fl = func_name.lower()
    if any(fl.endswith(s) for s in SETUP_SUFFIXES):
        return "no"
    if c_text:
        body = get_func_body(c_text, func_name)
        if any(t in body for t in HAL_TOKENS):
            return "no"
    return "yes"


def fix_header(layer, name, h_path, dry_run):
    """Add @isr_safe to every @thread_safe doc comment that lacks it."""
    text = h_path.read_text(encoding="utf-8", errors="replace")
    if "@isr_safe" in text:
        return 0

    c_path = h_path.parent / f"{name}.c"
    c_text = c_path.read_text(encoding="utf-8", errors="replace") if c_path.exists() else ""

    ts_pattern = re.compile(r'(@thread_safe:\s*\w+)')
    lines = text.split("\n")
    new_lines = []
    changes = 0
    i = 0

    while i < len(lines):
        line = lines[i]
        m = ts_pattern.search(line)
        if m and "@isr_safe" not in line:
            func_name = ""
            for j in range(i + 1, min(i + 5, len(lines))):
                fm = re.search(r'\b([a-z_][a-z0-9_]*)\s*\(', lines[j])
                if fm and fm.group(1) not in ("if", "for", "while", "return"):
                    func_name = fm.group(1)
                    break

            cls = classify(func_name, c_text) if func_name else "no"

            if line.rstrip().endswith("*/"):
                line = line.rstrip()[:-2].rstrip() + f" @isr_safe: {cls} */"
            else:
                line = line.rstrip() + f" @isr_safe: {cls}"
            changes += 1

        new_lines.append(line)
        i += 1

    if changes > 0:
        result = "\n".join(new_lines)
        if dry_run:
            print(f"  [DRY] {layer}/{name}: {changes} annotations")
        else:
            h_path.write_text(result, encoding="utf-8")
            print(f"  [FIX] {layer}/{name}: {changes} annotations")
    return changes


def main():
    total = 0
    for layer in ("primitives", "drivers"):
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for wdir in sorted(layer_dir.iterdir()):
            if not wdir.is_dir() or (wdir / "vendor").is_dir():
                continue
            h = wdir / f"{wdir.name}.h"
            if not h.exists():
                continue
            total += fix_header(layer, wdir.name, h, dry_run=not APPLY)

    mode = "applied" if APPLY else "dry-run"
    print(f"\nTotal: {total} annotations ({mode})")
    if not APPLY and total > 0:
        print("Re-run with --apply to apply changes.")


if __name__ == "__main__":
    main()
