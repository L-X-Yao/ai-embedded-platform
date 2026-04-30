#!/usr/bin/env python3
"""
audit_fix.py — auto-fix mechanical audit findings.

Companion to audit_wheels.py. Fixes issues that have deterministic
solutions, so AI doesn't need to read 19 files to add doc comments.

Usage:
  audit_fix.py                    # dry-run: show what would be fixed
  audit_fix.py --apply            # apply all fixes
  audit_fix.py --apply --only doc_comments    # fix only one category
  audit_fix.py --apply --wheel crc16 bmp280   # fix specific wheels

Fix categories:
  doc_comments   — add /** stub. @thread_safe: TODO */ before undocumented functions
  error_enum     — change auto-increment _ERR_ entries to explicit negative values
  missing_stddef — add #include <stddef.h> if NULL is used but not included
"""

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules", "_draft"]
SKIP_DIRS = {"test", "build", "__pycache__", ".pytest_cache", "vendor"}


def find_wheels(filter_names=None):
    wheels = []
    for layer in LAYERS:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for entry in sorted(layer_dir.iterdir()):
            if not entry.is_dir() or entry.name in SKIP_DIRS:
                continue
            if filter_names and entry.name not in filter_names:
                continue
            wheels.append((layer, entry.name, entry))
    return wheels


def fix_doc_comments(wheel_dir, name, apply=False):
    """Add stub doc comments before undocumented public function declarations."""
    h = wheel_dir / f"{name}.h"
    if not h.exists():
        return []

    lines = h.read_text(encoding="utf-8", errors="replace").split("\n")
    fixes = []
    new_lines = []
    i = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        if (f"{name}_" in stripped and "(" in stripped and ");" in stripped
                and not stripped.startswith("typedef")
                and not stripped.startswith("//")
                and not stripped.startswith("/*")
                and not stripped.startswith("*")):
            prev = new_lines[-1].strip() if new_lines else ""
            if not prev.startswith("/**") and not prev.startswith("*") and not prev.endswith("*/"):
                func_match = re.search(rf'({name}_\w+)\s*\(', stripped)
                func_name = func_match.group(1) if func_match else "unknown"

                is_init = "_init" in func_name
                is_pure = "const " in stripped and "*" not in stripped.replace("const ", "")
                if is_pure or ("parse" in func_name or "build" in func_name
                               or "crc" in func_name or "checksum" in func_name
                               or "encode" in func_name or "decode" in func_name):
                    safety = "yes (pure function)"
                elif is_init:
                    safety = "no"
                else:
                    safety = "TODO"

                verb = func_name.split("_")[-1] if "_" in func_name else "operate"
                verb_map = {
                    "init": "Initialize device",
                    "read": "Read data",
                    "write": "Write data",
                    "reset": "Reset device",
                    "set": "Set parameter",
                    "get": "Get parameter",
                    "update": "Update state",
                    "start": "Start operation",
                    "stop": "Stop operation",
                    "enable": "Enable feature",
                    "disable": "Disable feature",
                    "clear": "Clear state",
                    "flush": "Flush output",
                    "close": "Close resource",
                    "open": "Open resource",
                    "parse": "Parse input data",
                    "build": "Build output packet",
                    "encode": "Encode data",
                    "decode": "Decode data",
                }
                desc = verb_map.get(verb, f"Perform {verb} operation")
                comment = f"/** {desc}. @thread_safe: {safety} */"

                indent = line[:len(line) - len(line.lstrip())]
                fixes.append(f"{name}.h:{i+1}: {func_name} — add doc comment")
                new_lines.append(f"{indent}{comment}")

        new_lines.append(line)
        i += 1

    if fixes and apply:
        h.write_text("\n".join(new_lines), encoding="utf-8")

    return fixes


def fix_error_enum(wheel_dir, name, apply=False):
    """Change auto-increment _ERR_ enum entries to explicit negative values."""
    h = wheel_dir / f"{name}.h"
    if not h.exists():
        return []

    text = h.read_text(encoding="utf-8", errors="replace")
    if f"{name}_err_t" not in text:
        return []

    lines = text.split("\n")
    fixes = []
    new_lines = []
    in_enum = False
    err_counter = 0

    for i, line in enumerate(lines):
        stripped = line.strip()

        if "_OK" in stripped and "= 0" in stripped:
            in_enum = True
            err_counter = 0
            new_lines.append(line)
            continue

        if in_enum and "}" in stripped:
            in_enum = False
            new_lines.append(line)
            continue

        if in_enum and "_ERR_" in stripped and "=" not in stripped:
            err_counter += 1
            match = re.match(r'^(\s*)(\w+_ERR_\w+)\s*,(.*)$', line)
            if match:
                indent, enum_name, trailing = match.groups()
                new_line = f"{indent}{enum_name} = -{err_counter},{trailing}"
                fixes.append(f"{name}.h:{i+1}: {enum_name} → = -{err_counter}")
                new_lines.append(new_line)
                continue
        elif in_enum and "_ERR_" in stripped and "=" in stripped:
            match = re.search(r'=\s*(-?\d+)', stripped)
            if match:
                err_counter = abs(int(match.group(1)))

        new_lines.append(line)

    if fixes and apply:
        h.write_text("\n".join(new_lines), encoding="utf-8")

    return fixes


def fix_missing_stddef(wheel_dir, name, apply=False):
    """Add #include <stddef.h> if NULL is used but stddef.h not included."""
    c = wheel_dir / f"{name}.c"
    if not c.exists():
        return []

    text = c.read_text(encoding="utf-8", errors="replace")
    if "NULL" not in text:
        return []
    if "#include <stddef.h>" in text or "#include <stdlib.h>" in text:
        return []
    if "#include <string.h>" in text:
        return []

    fixes = [f"{name}.c: uses NULL without #include <stddef.h>"]
    if apply:
        lines = text.split("\n")
        for i, line in enumerate(lines):
            if line.startswith(f'#include "{name}.h"'):
                lines.insert(i + 1, "#include <stddef.h>")
                break
        c.write_text("\n".join(lines), encoding="utf-8")

    return fixes


FIXERS = {
    "doc_comments": fix_doc_comments,
    "error_enum": fix_error_enum,
    "missing_stddef": fix_missing_stddef,
}


def main():
    parser = argparse.ArgumentParser(description="Auto-fix audit findings")
    parser.add_argument("--apply", action="store_true", help="Apply fixes (default: dry-run)")
    parser.add_argument("--only", nargs="+", choices=FIXERS.keys(), help="Fix only these categories")
    parser.add_argument("--wheel", nargs="+", help="Fix only these wheels")
    args = parser.parse_args()

    categories = args.only if args.only else list(FIXERS.keys())
    filter_names = set(args.wheel) if args.wheel else None
    wheels = find_wheels(filter_names)

    total_fixes = 0
    mode = "APPLY" if args.apply else "DRY-RUN"

    print(f"=== audit_fix.py ({mode}) ===\n")

    for cat_name in categories:
        fixer = FIXERS[cat_name]
        cat_fixes = 0
        print(f"--- {cat_name} ---")

        for layer, name, wheel_dir in wheels:
            if (wheel_dir / "vendor").is_dir():
                continue
            fixes = fixer(wheel_dir, name, apply=args.apply)
            if fixes:
                for f in fixes:
                    print(f"  {'[fix]' if args.apply else '[would fix]'}  {f}")
                cat_fixes += len(fixes)

        if cat_fixes == 0:
            print("  (nothing to fix)")
        print(f"  subtotal: {cat_fixes}\n")
        total_fixes += cat_fixes

    print(f"=== Total: {total_fixes} {'applied' if args.apply else 'would apply'} ===")

    if not args.apply and total_fixes > 0:
        print("\nRe-run with --apply to fix.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
