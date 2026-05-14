#!/usr/bin/env python3
"""audit_check_perf.py -- benchmark every CHECK_TABLE entry on a
representative wheel.

audit_wheels.py runs 60+ checks across ~260 wheels in `make check`.
A single check that takes 1s per wheel adds 4 minutes to the loop.
This script catches such regressions before they ship: it loads a
representative wheel context, runs each check N times, and flags
anything over WARN_THRESHOLD or FAIL_THRESHOLD seconds.

Reason this exists: `chk_boundary_skip_coverage` once took 57 minutes
in `make check` (parse_functions per wheel + per-check serial scan)
and the slowness was only caught by killing the runaway process.

Usage:
  python tooling/audit/audit_check_perf.py
  python tooling/audit/audit_check_perf.py --runs 10
  python tooling/audit/audit_check_perf.py --wheel wheels/drivers/bme280
"""

import argparse
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tooling" / "audit"))

import audit_wheels as aw

WARN_THRESHOLD = 0.05   # 50 ms per check per wheel
FAIL_THRESHOLD = 0.50   # 500 ms per check per wheel


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=5,
                    help="Iterations per check for averaging")
    ap.add_argument("--wheel", default="wheels/drivers/bme280",
                    help="Representative wheel path to benchmark against")
    ap.add_argument("--warn-threshold", type=float, default=WARN_THRESHOLD)
    ap.add_argument("--fail-threshold", type=float, default=FAIL_THRESHOLD)
    args = ap.parse_args()

    wheel_path = REPO / args.wheel
    if not wheel_path.is_dir():
        print(f"ERR: wheel dir not found: {wheel_path}", file=sys.stderr)
        return 2

    parts = args.wheel.split("/")
    if len(parts) < 3 or parts[0] != "wheels":
        print(f"ERR: --wheel must be wheels/<layer>/<name>", file=sys.stderr)
        return 2
    layer, name = parts[1], parts[2]

    ctx = aw.WheelCtx(layer, name, wheel_path)
    ci_text = (REPO / ".github" / "workflows" / "ci.yml").read_text(
        encoding="utf-8", errors="replace")

    results = []  # (name, mean_seconds)
    for chk_name, heading, chk_fn, level, layers, skip_vendor, skip_draft in aw.CHECK_TABLE:
        # Warm-up (catches first-call imports / lazy caches)
        try:
            chk_fn(ctx, ci_text=ci_text)
        except Exception as e:
            results.append((chk_name, -1.0, f"ERR: {e}"))
            continue

        timings = []
        for _ in range(args.runs):
            t0 = time.perf_counter()
            try:
                chk_fn(ctx, ci_text=ci_text)
            except Exception:
                pass
            timings.append(time.perf_counter() - t0)
        timings.sort()
        # drop fastest+slowest, take mean of middle
        if len(timings) >= 3:
            timings = timings[1:-1]
        mean = sum(timings) / len(timings)
        results.append((chk_name, mean, ""))

    # Report
    results.sort(key=lambda r: r[1], reverse=True)
    print(f"=== Per-check timing ({args.runs} runs, wheel={args.wheel}) ===\n")
    print(f"{'check':<28} {'mean(ms)':>10}  status")
    print("-" * 60)
    warns = 0
    fails = 0
    for chk_name, mean, err in results:
        if err:
            print(f"{chk_name:<28} {'ERR':>10}  {err}")
            continue
        ms = mean * 1000
        if mean >= args.fail_threshold:
            tag = "FAIL"
            fails += 1
        elif mean >= args.warn_threshold:
            tag = "WARN"
            warns += 1
        else:
            tag = "ok"
        print(f"{chk_name:<28} {ms:>10.2f}  {tag}")

    total = sum(r[1] for r in results if r[1] > 0)
    print("-" * 60)
    print(f"{'TOTAL per wheel':<28} {total * 1000:>10.2f}  ms")
    print(f"\nThresholds: WARN >= {args.warn_threshold * 1000:.0f}ms, "
          f"FAIL >= {args.fail_threshold * 1000:.0f}ms per check per wheel.")
    print(f"WARN: {warns}  FAIL: {fails}")

    return 1 if fails else 0


if __name__ == "__main__":
    raise SystemExit(main())
