#!/usr/bin/env python3
"""Aggregate -fstack-usage output and walk the call graph from objdump.

Usage:
  analyze_stack.py [BUILD_DIR] [--elf PATH] [--objdump PATH] [--top N]

For each task entrypoint declared in TASK_ENTRIES below, walks the static
call graph from the disassembly and reports the worst-case static stack
usage compared to the configured stack size.

Limitations:
  - Static analysis only. Doesn't account for VLAs/alloca (marked `dynamic`
    in .su) or function pointers (printed as warnings).
  - Recursion is detected and broken at the cycle (worst-case 1 frame).
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path


# Task entrypoint -> configured stack size in bytes.
# Mirror of compat task.h logic: max(2048, words * 4 * VSF_STACK_SCALE).
# VSF_STACK_SCALE compensates for the ~13-frame POSIX-shim overhead on
# every blocking call (sleep/sem). Keep in sync with compat/task.h.
VSF_STACK_SCALE = 2

def _stk(words: int) -> int:
    return max(2048, words * 4 * VSF_STACK_SCALE)

TASK_ENTRIES = {
    "air780eg_task_entry":    _stk(128 * 6),
    "gnss_task_entry":        _stk(128 * 4),
    "imu_task_entry":         _stk(128 * 4),
    "baro_task_entry":        _stk(128 * 2),
    "data_fusion_task_entry": _stk(128 * 4),
    "mqtt_task_entry":        _stk(128 * 4),
    "ota_task_entry":         _stk(128 * 4),
    "TaskWatchdog":           _stk(128 * 1),
    "TaskLED":                _stk(128 * 1),
}


def parse_su_files(build_dir: Path) -> dict[str, tuple[int, str]]:
    """Return {function_name: (bytes, kind)} from all .su files.

    GCC .su line format (tab separated):
      <path>:<line>:<col>:<bare_function_name>\t<bytes>\t<kind>
    """
    sizes: dict[str, tuple[int, str]] = {}
    for su in build_dir.rglob("*.su"):
        for raw in su.read_text(errors="replace").splitlines():
            parts = raw.split("\t")
            if len(parts) < 3:
                continue
            location_sig, byte_str, kind = parts[0], parts[1], parts[2]
            try:
                bytes_used = int(byte_str)
            except ValueError:
                continue
            # location_sig = <path>:<line>:<col>:<name>. Splitting from the
            # right takes the function name even if the path contains colons
            # (Windows drive letters under WSL).
            m = re.match(r"^(.*):(\d+):(\d+):(.+)$", location_sig)
            if not m:
                continue
            name = m.group(4).strip()
            # Some entries may have parens (function-style decl); strip if so.
            paren = name.find("(")
            if paren > 0:
                name = name[:paren].strip().split()[-1]
            else:
                name = name.split()[-1]
            prev = sizes.get(name)
            if prev is None or bytes_used > prev[0]:
                sizes[name] = (bytes_used, kind)
    return sizes


def parse_callgraph(elf: Path, objdump: str) -> dict[str, set[str]]:
    """Return {caller: {callees}} from disassembly. Misses indirect calls."""
    out = subprocess.run(
        [objdump, "-d", "--no-show-raw-insn", str(elf)],
        capture_output=True, text=True, check=True,
    ).stdout

    callgraph: dict[str, set[str]] = defaultdict(set)
    current: str | None = None
    func_hdr = re.compile(r"^[0-9a-fA-F]+\s+<([^>]+)>:\s*$")
    # Match `bl <fn>` and `b <fn>` for tail calls. Filter out PLT/got noise.
    call_re = re.compile(r"\b(?:bl|b)\s+[0-9a-fA-F]+\s+<([^>+]+)(?:\+0x[0-9a-fA-F]+)?>")

    for line in out.splitlines():
        m = func_hdr.match(line)
        if m:
            current = m.group(1)
            continue
        if current is None:
            continue
        for callee in call_re.findall(line):
            if callee != current:
                callgraph[current].add(callee)
    return callgraph


def deepest_chain(
    fn: str,
    sizes: dict[str, tuple[int, str]],
    callgraph: dict[str, set[str]],
    visiting: set[str],
    cache: dict[str, tuple[int, list[str]]],
) -> tuple[int, list[str]]:
    """Return (max_stack_bytes, chain_of_function_names)."""
    if fn in cache:
        return cache[fn]
    if fn in visiting:
        # Cycle — break here, charge nothing further.
        return 0, [fn + " <recursion>"]
    own = sizes.get(fn, (0, "missing"))[0]

    visiting.add(fn)
    best_extra = 0
    best_chain: list[str] = []
    for callee in callgraph.get(fn, ()):
        sub_bytes, sub_chain = deepest_chain(
            callee, sizes, callgraph, visiting, cache,
        )
        if sub_bytes > best_extra:
            best_extra = sub_bytes
            best_chain = sub_chain
    visiting.discard(fn)

    total = own + best_extra
    chain = [f"{fn} ({own}B)"] + best_chain
    cache[fn] = (total, chain)
    return total, chain


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("build_dir", nargs="?", default=os.path.expanduser("~/flybox_build"),
                    help="CMake build dir containing .su files (default: ~/flybox_build)")
    ap.add_argument("--elf", default=None,
                    help="ELF for objdump callgraph (default: <build_dir>/flybox_vsf.elf)")
    ap.add_argument("--objdump", default="arm-none-eabi-objdump",
                    help="objdump binary (default: arm-none-eabi-objdump)")
    ap.add_argument("--top", type=int, default=20, help="show top N largest single functions")
    ap.add_argument("--threshold", type=int, default=512, help="warn on functions >= this many bytes")
    args = ap.parse_args()

    build_dir = Path(args.build_dir)
    if not build_dir.is_dir():
        print(f"[ERR] build_dir not found: {build_dir}", file=sys.stderr)
        return 1
    elf = Path(args.elf or build_dir / "flybox_vsf.elf")

    print(f"=== Reading .su files under {build_dir} ===")
    sizes = parse_su_files(build_dir)
    print(f"  {len(sizes)} unique function frames")

    big = sorted(sizes.items(), key=lambda kv: -kv[1][0])

    print(f"\n=== Top {args.top} single-frame stack users ===")
    for name, (bytes_used, kind) in big[:args.top]:
        marker = "  " if bytes_used < args.threshold else "!!"
        print(f"  {marker} {bytes_used:>5}B  {kind:<8}  {name}")

    if not elf.exists():
        print(f"\n[WARN] ELF not found: {elf} — skipping callgraph walk")
        print("       (per-task cumulative analysis disabled)")
        return 0

    print(f"\n=== Building callgraph from {elf} ===")
    try:
        callgraph = parse_callgraph(elf, args.objdump)
    except FileNotFoundError:
        print(f"[ERR] objdump not found: {args.objdump}", file=sys.stderr)
        return 1
    print(f"  {len(callgraph)} functions in callgraph")

    print("\n=== Per-task cumulative stack (static, depth-first) ===")
    cache: dict[str, tuple[int, list[str]]] = {}
    for entry, allocated in TASK_ENTRIES.items():
        if entry not in sizes and entry not in callgraph:
            print(f"  [skip] {entry}: not found in build")
            continue
        total, chain = deepest_chain(entry, sizes, callgraph, set(), cache)
        margin = allocated - total
        flag = "OK"
        if margin < 256:
            flag = "!!" if margin < 0 else "warn"
        print(f"\n  Task {entry}: {total}B used / {allocated}B allocated "
              f"(margin {margin:+d}B) [{flag}]")
        if margin < 512:
            print("    deepest chain:")
            for step in chain[:15]:
                print(f"      -> {step}")
            if len(chain) > 15:
                print(f"      -> ... ({len(chain) - 15} more)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
