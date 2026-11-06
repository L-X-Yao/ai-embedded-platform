#!/usr/bin/env python3
"""Aggregate -fstack-usage output and walk the call graph from objdump.

Generic ARM GCC stack analyzer. Reads task entrypoint budgets from a JSON
config file (--task-config), so it's not tied to any specific RTOS or
project. See stack_tasks.example.json for the format.

Usage:
  stack_analyzer.py BUILD_DIR --task-config tasks.json [--elf PATH]
                    [--objdump PATH] [--top N] [--threshold BYTES]

Build with `-fstack-usage` (GCC) — each .o gets a .su sibling. The script
walks all .su under BUILD_DIR plus the disassembly callgraph from --elf,
then for each task entry in the JSON config reports cumulative depth-first
stack usage vs the configured allocation.

Limitations:
  - Static analysis only. Doesn't account for VLAs/alloca (marked `dynamic`
    in .su) or function pointers (silently miss).
  - Recursion is detected and broken at the cycle (worst-case 1 frame).
  - Functions with no .su, such as libc, count as 0 bytes; the run lists them.

Exit 1 if a task overflows its budget or is not in the build, or if the ELF
is missing or yields no callgraph.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path


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
            # Splitting from the right takes the function name even if the
            # path contains colons (Windows drive letters under WSL).
            m = re.match(r"^(.*):(\d+):(\d+):(.+)$", location_sig)
            if not m:
                continue
            name = m.group(4).strip()
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
    try:
        out = subprocess.run(
            [objdump, "-d", "--no-show-raw-insn", str(elf)],
            capture_output=True, text=True, check=True, timeout=120,
        ).stdout
    except subprocess.TimeoutExpired:
        print(f"ERROR: objdump timed out on {elf} (>120s)", file=sys.stderr)
        return {}
    except subprocess.CalledProcessError as e:
        print(f"ERROR: objdump failed on {elf}: {e}", file=sys.stderr)
        return {}

    callgraph: dict[str, set[str]] = defaultdict(set)
    current: str | None = None
    func_hdr = re.compile(r"^[0-9a-fA-F]+\s+<([^>]+)>:\s*$")
    # A tail call is a plain or conditional branch, b.w or bne.w at -Os, to
    # another function. Branches inside a function are dropped below.
    call_re = re.compile(
        r"\b(?:blx?|bx|b(?:eq|ne|cs|cc|hs|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)?)(?:\.[nw])?"
        r"\s+[0-9a-fA-F]+\s+<([^>+]+)(?:\+0x[0-9a-fA-F]+)?>"
    )

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
    no_frame: set[str],
) -> tuple[int, list[str]]:
    """Return (max_stack_bytes, chain_of_function_names).

    Functions with no .su entry count as 0 bytes and are added to no_frame.
    """
    if fn in cache:
        return cache[fn]
    if fn in visiting:
        return 0, [fn + " <recursion>"]
    if fn not in sizes:
        no_frame.add(fn)
    own = sizes.get(fn, (0, "missing"))[0]

    visiting.add(fn)
    best_extra = 0
    best_chain: list[str] = []
    for callee in callgraph.get(fn, ()):
        sub_bytes, sub_chain = deepest_chain(
            callee, sizes, callgraph, visiting, cache, no_frame,
        )
        if sub_bytes > best_extra:
            best_extra = sub_bytes
            best_chain = sub_chain
    visiting.discard(fn)

    total = own + best_extra
    chain = [f"{fn} ({own}B)"] + best_chain
    cache[fn] = (total, chain)
    return total, chain


def load_task_config(path: Path) -> tuple[dict[str, int], int, int]:
    """Load task budgets from JSON. Returns (task_entries, scale, min_bytes).

    JSON format:
      {
        "stack_scale": 2,        // optional, default 1
        "min_bytes":   2048,     // optional, default 0
        "tasks": [
          { "entry": "fn_name", "words": 512 },   // bytes = words*4*scale
          { "entry": "fn_name", "bytes": 4096 }   // bytes = bytes*scale
        ]
      }
    """
    cfg = json.loads(path.read_text())
    scale = int(cfg.get("stack_scale", 1))
    min_bytes = int(cfg.get("min_bytes", 0))
    tasks: dict[str, int] = {}
    for t in cfg.get("tasks", []):
        entry = t["entry"]
        if "words" in t:
            allocated = int(t["words"]) * 4 * scale
        elif "bytes" in t:
            allocated = int(t["bytes"]) * scale
        else:
            raise ValueError(f"task '{entry}': must specify 'words' or 'bytes'")
        tasks[entry] = max(min_bytes, allocated)
    return tasks, scale, min_bytes


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("build_dir", help="CMake/make build dir containing .su files")
    ap.add_argument("--task-config", required=True,
                    help="JSON file describing task entrypoints + stack budgets")
    ap.add_argument("--elf", default=None,
                    help="ELF for objdump callgraph (default: auto-detect first *.elf in build_dir)")
    ap.add_argument("--objdump", default="arm-none-eabi-objdump",
                    help="objdump binary (default: arm-none-eabi-objdump)")
    ap.add_argument("--top", type=int, default=20, help="show top N largest single functions")
    ap.add_argument("--threshold", type=int, default=512, help="warn on functions >= this many bytes")
    args = ap.parse_args()

    build_dir = Path(args.build_dir)
    if not build_dir.is_dir():
        print(f"[ERR] build_dir not found: {build_dir}", file=sys.stderr)
        return 1

    if args.elf:
        elf = Path(args.elf)
    else:
        elfs = sorted(build_dir.glob("*.elf"))
        if not elfs:
            print(f"[ERR] no *.elf in {build_dir} — pass --elf explicitly", file=sys.stderr)
            return 1
        elf = elfs[0]

    task_entries, scale, min_bytes = load_task_config(Path(args.task_config))
    print(f"=== Task config: {len(task_entries)} entries, "
          f"scale={scale}x, min_bytes={min_bytes} ===")

    print(f"\n=== Reading .su files under {build_dir} ===")
    sizes = parse_su_files(build_dir)
    print(f"  {len(sizes)} unique function frames")

    big = sorted(sizes.items(), key=lambda kv: -kv[1][0])

    print(f"\n=== Top {args.top} single-frame stack users ===")
    for name, (bytes_used, kind) in big[:args.top]:
        marker = "  " if bytes_used < args.threshold else "!!"
        print(f"  {marker} {bytes_used:>5}B  {kind:<8}  {name}")

    if not elf.exists():
        print(f"\n[ERR] ELF not found: {elf} — no callgraph, no task totals", file=sys.stderr)
        return 1

    print(f"\n=== Building callgraph from {elf} ===")
    try:
        callgraph = parse_callgraph(elf, args.objdump)
    except FileNotFoundError:
        print(f"[ERR] objdump not found: {args.objdump}", file=sys.stderr)
        return 1
    print(f"  {len(callgraph)} functions in callgraph")
    if not callgraph:
        print("[ERR] empty callgraph: per-task totals would count one frame only",
              file=sys.stderr)
        return 1

    print("\n=== Per-task cumulative stack (static, depth-first) ===")
    cache: dict[str, tuple[int, list[str]]] = {}
    no_frame: set[str] = set()
    overflows = 0
    not_found = 0
    for entry, allocated in task_entries.items():
        if entry not in sizes and entry not in callgraph:
            print(f"  [ERR] {entry}: not found in build")
            not_found += 1
            continue
        total, chain = deepest_chain(entry, sizes, callgraph, set(), cache, no_frame)
        margin = allocated - total
        flag = "OK"
        if margin < 256:
            flag = "!!" if margin < 0 else "warn"
        if margin < 0:
            overflows += 1
        print(f"\n  Task {entry}: {total}B used / {allocated}B allocated "
              f"(margin {margin:+d}B) [{flag}]")
        if margin < 512:
            print("    deepest chain:")
            for step in chain[:15]:
                print(f"      -> {step}")
            if len(chain) > 15:
                print(f"      -> ... ({len(chain) - 15} more)")

    if no_frame:
        names = ", ".join(sorted(no_frame)[:10])
        print(f"\n[WARN] {len(no_frame)} called function(s) have no .su entry and "
              f"count as 0B: {names}{' ...' if len(no_frame) > 10 else ''}")

    return 1 if overflows > 0 or not_found > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
