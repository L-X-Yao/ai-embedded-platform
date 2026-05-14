#!/usr/bin/env python3
"""scan_boundary_skips.py -- list functions the boundary codegen skips
and check whether each is exercised by a hand-written test file.

Boundary codegen (gen_boundary_tests.py) skips public functions whose
struct pointer params point to callback-bearing or unsafe structs
(see struct_has_callback_field). Those functions are supposed to be
covered by hw_model or functional tests instead. This script verifies
that promise: for every skipped function, is there at least one
test_*.c (excluding auto-generated test_null_/sentinel_/boundary_)
that references it?

Standalone tool, not run in `make check` — it walks every wheel and
takes ~6.5 minutes. Run before releases or after large codegen
detection changes.

  python tooling/audit/scan_boundary_skips.py

Output: per-wheel list of (function, covered?).
Exit 0 if every skip has functional coverage, 1 otherwise.

Current status (2026-05-14, 259 wheels): 1118 skipped functions,
194 wheels touched, 0 GAPs.
"""

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tooling" / "codegen"))

from _wheel_parser import (
    WHEELS_DIR, read_header, parse_functions, discover_wheels,
)
from gen_boundary_tests import function_has_callback_struct_arg


def test_calls(wheel_dir: Path, func_name: str) -> list[Path]:
    """Return any test_*.c file under wheel_dir/test that references
    func_name. Excludes the auto-generated null/sentinel/boundary
    files because those run codegen-shaped tests, not the
    integration calls that this scan is auditing. Empty list means
    no functional/hw_model test exercises this function."""
    test_dir = wheel_dir / "test"
    if not test_dir.is_dir():
        return []
    hits = []
    for p in test_dir.glob("test_*.c"):
        # Skip auto-generated codegen outputs
        n = p.name
        if (n.startswith("test_null_") or n.startswith("test_sentinel_")
                or n.startswith("test_boundary_")):
            continue
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if f"{func_name}(" in text:
            hits.append(p)
    return hits


def main() -> int:
    rows = []
    total_skipped = 0
    uncovered = []

    for wheel in discover_wheels():
        header = read_header(wheel.header_path)
        if header is None:
            continue
        funcs = parse_functions(header, wheel.name, compute_extra=True)
        for f in funcs:
            if not function_has_callback_struct_arg(f, header):
                continue
            total_skipped += 1
            covered = test_calls(wheel.path, f.name)
            rows.append((wheel.layer, wheel.name, f.name, bool(covered)))
            if not covered:
                uncovered.append((wheel.layer, wheel.name, f.name))

    # Print report grouped by wheel
    by_wheel: dict[tuple[str, str], list[tuple[str, bool]]] = {}
    for layer, wheel, fn, covered in rows:
        by_wheel.setdefault((layer, wheel), []).append((fn, covered))

    print(f"=== Boundary-skipped functions: {total_skipped} ===")
    print(f"Wheels touched: {len(by_wheel)}")
    print(f"Uncovered by hw_model: {len(uncovered)}\n")

    for (layer, wheel), funcs in sorted(by_wheel.items()):
        any_uncovered = any(not c for _, c in funcs)
        marker = "X" if any_uncovered else "."
        print(f"[{marker}] {layer}/{wheel}")
        for fn, covered in funcs:
            tag = "OK " if covered else "GAP"
            print(f"    {tag}  {fn}")

    if uncovered:
        print(f"\n{len(uncovered)} function(s) skipped by boundary AND "
              f"absent from hw_model — coverage GAP.")
        return 1
    print("\nAll boundary-skipped functions covered by hw_model tests.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
