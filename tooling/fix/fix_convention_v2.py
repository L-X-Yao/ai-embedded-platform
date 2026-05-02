#!/usr/bin/env python3
"""Batch fix convention v2 violations across all wheels."""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules"]
SPDX_LINE = "// SPDX-License-Identifier: MIT\n"

stats = {"spdx": 0, "vsf_native": 0, "err_params": 0, "thread_safe": 0}


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


def fix_spdx(wheel_dir, name):
    changed = False
    for ext in (".h", ".c"):
        f = wheel_dir / f"{name}{ext}"
        if not f.exists():
            continue
        text = f.read_text(encoding="utf-8")
        if "SPDX-License-Identifier" in text:
            continue
        f.write_text(SPDX_LINE + text, encoding="utf-8")
        changed = True
    # Also fix test file
    test_c = wheel_dir / "test" / f"test_{name}.c"
    if test_c.exists():
        text = test_c.read_text(encoding="utf-8")
        if "SPDX-License-Identifier" not in text:
            test_c.write_text(SPDX_LINE + text, encoding="utf-8")
    return changed


def fix_vsf_native(wheel_dir):
    readme = wheel_dir / "README.md"
    if not readme.exists():
        return False
    text = readme.read_text(encoding="utf-8")
    if "why not vsf native" not in text.lower():
        return False

    lines = text.split("\n")
    out = []
    skip = False
    for line in lines:
        if re.match(r"^##\s+Why not VSF native", line, re.IGNORECASE):
            skip = True
            # Remove blank line before heading if present
            while out and out[-1].strip() == "":
                out.pop()
            continue
        if skip and line.startswith("## "):
            skip = False
        if skip:
            continue
        out.append(line)

    # Clean up trailing blank lines
    while len(out) > 1 and out[-1].strip() == "" and out[-2].strip() == "":
        out.pop()

    new_text = "\n".join(out)
    if new_text != text:
        readme.write_text(new_text, encoding="utf-8")
        return True
    return False


def fix_err_params(wheel_dir, name):
    changed = False
    for ext in (".h", ".c"):
        f = wheel_dir / f"{name}{ext}"
        if not f.exists():
            continue
        text = f.read_text(encoding="utf-8")
        if "ERR_PARAMS" not in text:
            continue
        new_text = text.replace("ERR_PARAMS", "ERR_PARAM")
        f.write_text(new_text, encoding="utf-8")
        changed = True
    # Also fix test file
    test_c = wheel_dir / "test" / f"test_{name}.c"
    if test_c.exists():
        text = test_c.read_text(encoding="utf-8")
        if "ERR_PARAMS" in text:
            test_c.write_text(text.replace("ERR_PARAMS", "ERR_PARAM"), encoding="utf-8")
    return changed


def fix_init_thread_safe(wheel_dir, name):
    h_file = wheel_dir / f"{name}.h"
    if not h_file.exists():
        return False
    text = h_file.read_text(encoding="utf-8")
    lines = text.split("\n")
    changed = False

    for i in range(len(lines)):
        if "caller_must_lock" not in lines[i]:
            continue
        # Check if next 1-3 lines contain init/deinit/reset
        for j in range(i, min(i + 3, len(lines))):
            fn = lines[j].lower()
            if (f"{name}_init" in fn or f"{name}_deinit" in fn or
                    f"{name}_reset" in fn or f"{name}_fini" in fn):
                lines[i] = lines[i].replace("caller_must_lock", "no")
                changed = True
                break

    if changed:
        h_file.write_text("\n".join(lines), encoding="utf-8")
    return changed


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "all"

    for layer, name, wheel_dir in find_wheels():
        path = f"wheels/{layer}/{name}"

        if mode in ("all", "spdx"):
            if fix_spdx(wheel_dir, name):
                stats["spdx"] += 1
                print(f"  SPDX    {path}")

        if mode in ("all", "vsf"):
            if fix_vsf_native(wheel_dir):
                stats["vsf_native"] += 1
                print(f"  VSF     {path}")

        if mode in ("all", "err"):
            if fix_err_params(wheel_dir, name):
                stats["err_params"] += 1
                print(f"  ERR     {path}")

        if mode in ("all", "thread"):
            if fix_init_thread_safe(wheel_dir, name):
                stats["thread_safe"] += 1
                print(f"  THREAD  {path}")

    print(f"\n  SPDX: {stats['spdx']}  VSF: {stats['vsf_native']}  "
          f"ERR: {stats['err_params']}  THREAD: {stats['thread_safe']}")


if __name__ == "__main__":
    main()
