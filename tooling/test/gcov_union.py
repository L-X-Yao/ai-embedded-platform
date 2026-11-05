#!/usr/bin/env python3
"""Union several gcov text reports of the same source file.

A wheel's test/CMakeLists.txt builds several executables (main, null,
boundary, hw_model ...) that each compile the wheel source with their own
defines.  gcov-tool merge needs identical instrumentation and, on a
mismatch, prints "mismatched profiles" yet exits 0 with empty data, which is
how a 95%-covered wheel ends up reported as 0%.  This script merges at the
line level instead: a line counts as executed if any target executed it, a
branch counts as executed if any target took it at least once.

Usage:
    gcov_union.py [--out MERGED.gcov] FILE.gcov [FILE.gcov ...]

Prints "LINE_PCT BRANCH_PCT" (two decimals, gcov style) on stdout.  With
--out, also writes a merged .gcov file that keeps the gcov line format so
`grep '^\\s*#####:'` still lists uncovered lines.
"""
import argparse
import re
import sys

LINE_RE = re.compile(r"^\s*([-#=0-9*]+):\s*(\d+):(.*)$")
BRANCH_RE = re.compile(r"^branch\s+(\d+)\s+(taken\s+(\d+)|never executed)")


def parse(path):
    counts = {}      # lineno -> int count (executable lines only)
    source = {}      # lineno -> source text
    branches = {}    # (lineno, idx) -> bool executed
    last_line = 0
    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            m = LINE_RE.match(raw)
            if m:
                tag, lineno, text = m.group(1), int(m.group(2)), m.group(3)
                if lineno == 0:
                    continue  # header: Source:, Graph:, Data:, Runs:
                last_line = lineno
                source.setdefault(lineno, text.rstrip("\n"))
                if tag == "-":
                    continue
                if tag.startswith("#") or tag.startswith("="):
                    counts[lineno] = counts.get(lineno, 0)
                else:
                    counts[lineno] = counts.get(lineno, 0) + int(tag.rstrip("*"))
                continue
            b = BRANCH_RE.match(raw)
            if b and last_line:
                key = (last_line, int(b.group(1)))
                taken = b.group(3) is not None and int(b.group(3)) > 0
                branches[key] = branches.get(key, False) or taken
    return counts, source, branches


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", help="write merged .gcov here")
    ap.add_argument("files", nargs="+")
    args = ap.parse_args()

    counts, source, branches = {}, {}, {}
    for path in args.files:
        c, s, b = parse(path)
        for k, v in c.items():
            counts[k] = counts.get(k, 0) + v
        for k, v in s.items():
            source.setdefault(k, v)
        for k, v in b.items():
            branches[k] = branches.get(k, False) or v

    total_lines = len(counts)
    hit_lines = sum(1 for v in counts.values() if v > 0)
    total_br = len(branches)
    hit_br = sum(1 for v in branches.values() if v)
    line_pct = (100.0 * hit_lines / total_lines) if total_lines else 0.0
    br_pct = (100.0 * hit_br / total_br) if total_br else 0.0
    print(f"{line_pct:.2f} {br_pct:.2f}")

    if args.out:
        with open(args.out, "w", encoding="utf-8") as out:
            out.write(f"        -:    0:Source:{args.files[0]}\n")
            out.write(f"        -:    0:Merged:{len(args.files)} gcov reports (line-level union)\n")
            for lineno in sorted(source):
                if lineno in counts:
                    tag = str(counts[lineno]) if counts[lineno] > 0 else "#####"
                else:
                    tag = "-"
                out.write(f"{tag:>9}:{lineno:>5}:{source[lineno]}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
