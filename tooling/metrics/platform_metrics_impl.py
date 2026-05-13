#!/usr/bin/env python3
"""Platform metrics collection and comparison — implementation."""

import argparse
import json
import os
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def collect(repo: Path) -> dict:
    wheels = repo / "wheels"
    counts = {}
    for layer in ["primitives", "drivers", "protocols", "modules"]:
        ld = wheels / layer
        if ld.is_dir():
            counts[layer] = len([d for d in ld.iterdir()
                                 if d.is_dir() and d.name != "__pycache__"])
        else:
            counts[layer] = 0

    test_suites = len(list(wheels.rglob("test/test_*.c")))
    assertions = 0
    for tc in wheels.rglob("test/test_*.c"):
        try:
            assertions += sum(1 for line in tc.read_text(encoding="utf-8", errors="ignore").splitlines()
                              if re.search(r'\bASSERT', line))
        except OSError:
            pass

    audit_fail = 0
    audit_warn = 0
    try:
        r = subprocess.run(
            [sys.executable, str(repo / "tooling/audit/audit_wheels.py"), "--quiet"],
            capture_output=True, text=True, cwd=str(repo), timeout=30
        )
        out = r.stdout + r.stderr
        audit_fail = out.count("[FAIL]")
        audit_warn = out.count("[WARN]")
    except (subprocess.TimeoutExpired, OSError):
        pass

    scripts_dir = repo / "tooling"
    tool_count = sum(
        1 for f in scripts_dir.rglob("*")
        if f.is_file()
        and f.suffix in (".sh", ".py")
        and not f.name.startswith("__")
        and "test" not in f.relative_to(scripts_dir).parts[:1]
        and "experiments" not in f.relative_to(scripts_dir).parts[:1]
        and "__pycache__" not in f.parts
    ) if scripts_dir.is_dir() else 0

    makefile = repo / "Makefile"
    make_targets = 0
    if makefile.exists():
        mk_text = makefile.read_text(encoding="utf-8")
        targets = set()
        for line in mk_text.splitlines():
            stripped = line.strip()
            if stripped.startswith('.PHONY'):
                parts = stripped.split(':', 1)
                if len(parts) > 1:
                    targets.update(parts[1].split())
        make_targets = len(targets)

    claude_lines = 0
    claude_md = repo / "CLAUDE.md"
    if claude_md.exists():
        claude_lines = len(claude_md.read_text(encoding="utf-8").splitlines())

    commits = 0
    try:
        r = subprocess.run(["git", "rev-list", "--count", "HEAD"],
                           capture_output=True, text=True, cwd=str(repo))
        commits = int(r.stdout.strip())
    except (OSError, ValueError):
        pass

    commit_hash = "unknown"
    try:
        r = subprocess.run(["git", "rev-parse", "--short", "HEAD"],
                           capture_output=True, text=True, cwd=str(repo))
        commit_hash = r.stdout.strip()
    except OSError:
        pass

    return {
        "timestamp": datetime.now().strftime("%Y-%m-%dT%H:%M:%S"),
        "commit": commit_hash,
        "wheels_l0": counts["primitives"],
        "wheels_l1": counts["drivers"],
        "wheels_l2": counts["protocols"],
        "wheels_l3": counts["modules"],
        "wheels_total": sum(counts.values()),
        "test_suites": test_suites,
        "assertions": assertions,
        "audit_fail": audit_fail,
        "audit_warn": audit_warn,
        "tool_scripts": tool_count,
        "makefile_targets": make_targets,
        "claude_md_lines": claude_lines,
        "commits": commits,
    }


def collect_quality(repo: Path) -> dict:
    """Collect code quality metrics: test/code ratio, assertion density, error coverage."""
    wheels = repo / "wheels"
    total_code_lines = 0
    total_test_lines = 0
    total_test_funcs = 0
    total_asserts = 0
    total_err_codes = 0
    covered_err_codes = 0

    err_pattern_cache: dict = {}
    for h in wheels.rglob("*.h"):
        if "vendor" in str(h) or "test" in str(h):
            continue
        name = h.stem
        try:
            text = h.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        pat = re.compile(r'\b' + re.escape(name.upper()) + r'_ERR_\w+\b')
        codes = set(pat.findall(text))
        if codes:
            # Merge codes across multiple .h files in the same wheel directory
            existing_name, existing_codes = err_pattern_cache.get(h.parent, (name, set()))
            err_pattern_cache[h.parent] = (existing_name, existing_codes | codes)

    for c_file in wheels.rglob("*.c"):
        rel = str(c_file).replace("\\", "/")
        if "vendor" in rel or "/build" in rel:
            continue
        try:
            text = c_file.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        if "/test/" in rel:
            total_test_lines += len(text.splitlines())
            total_test_funcs += text.count("TEST(")
            total_asserts += sum(1 for line in text.splitlines()
                                    if re.search(r'\bASSERT', line))
        else:
            total_code_lines += len(text.splitlines())

    for wheel_dir, (name, codes) in err_pattern_cache.items():
        total_err_codes += len(codes)
        test_dir = wheel_dir / "test"
        if test_dir.is_dir():
            test_text = ""
            for tf in test_dir.glob("*.c"):
                test_text += tf.read_text(encoding="utf-8", errors="ignore")
            covered_err_codes += sum(1 for c in codes if c in test_text)

    assert_density = (total_asserts / total_test_funcs) if total_test_funcs else 0
    test_ratio = (total_test_lines / total_code_lines) if total_code_lines else 0
    err_coverage = (covered_err_codes / total_err_codes * 100) if total_err_codes else 0

    return {
        "code_lines": total_code_lines,
        "test_lines": total_test_lines,
        "test_ratio": round(test_ratio, 2),
        "test_functions": total_test_funcs,
        "assertions": total_asserts,
        "assert_density": round(assert_density, 1),
        "error_codes_total": total_err_codes,
        "error_codes_covered": covered_err_codes,
        "error_coverage_pct": round(err_coverage, 1),
    }


def print_quality(q: dict):
    print("\033[1m=== Quality Metrics ===\033[0m")
    print(f"  Code:       {q['code_lines']} lines (non-vendor, non-test)")
    print(f"  Tests:      {q['test_lines']} lines ({q['test_functions']} functions)")
    print(f"  Test/Code:  {q['test_ratio']}x")
    print(f"  Asserts:    {q['assertions']} total, {q['assert_density']} per test function")
    print(f"  Error codes: {q['error_codes_covered']}/{q['error_codes_total']} covered ({q['error_coverage_pct']}%)")


def print_metrics(d: dict, label: str = "Current"):
    print(f"\033[1m=== {label} ===\033[0m")
    print(f"  Wheels:     {d['wheels_total']} (L0:{d['wheels_l0']} L1:{d['wheels_l1']} L2:{d['wheels_l2']} L3:{d['wheels_l3']})")
    print(f"  Tests:      {d['test_suites']} suites, {d['assertions']} assertions")
    print(f"  Audit:      {d['audit_fail']} FAIL, {d['audit_warn']} WARN")
    print(f"  Tools:      {d['tool_scripts']} scripts, {d['makefile_targets']} make targets")
    print(f"  CLAUDE.md:  {d['claude_md_lines']} lines")
    print(f"  Commits:    {d['commits']}")
    print(f"  As of:      {d['timestamp']} ({d['commit']})")


def print_delta(prev: dict, curr: dict):
    print("\033[1m=== Delta ===\033[0m")
    keys = [
        ("wheels_total", "Wheels"),
        ("test_suites", "Test suites"),
        ("assertions", "Assertions"),
        ("audit_fail", "Audit FAIL"),
        ("audit_warn", "Audit WARN"),
        ("tool_scripts", "Tool scripts"),
        ("makefile_targets", "Make targets"),
        ("claude_md_lines", "CLAUDE.md lines"),
        ("commits", "Commits"),
    ]
    decrease_is_good = {"audit_fail", "audit_warn"}
    for key, label in keys:
        p = prev.get(key, 0)
        c = curr.get(key, 0)
        delta = c - p
        if delta == 0:
            sym = "  "
            color = "\033[0m"
        elif delta > 0:
            if key in decrease_is_good:
                color = "\033[0;33m"  # yellow (bad: more failures)
            else:
                color = "\033[0;32m"  # green (good: more wheels/tests)
            sym = "▲"
        else:
            if key in decrease_is_good:
                color = "\033[0;32m"  # green (good: fewer failures)
            else:
                color = "\033[0;33m"  # yellow (shrinkage)
            sym = "▼"
        sign = "+" if delta > 0 else ""
        print(f"  {color}{sym} {label:20s} {p:>8} → {c:>8}  ({sign}{delta})\033[0m")


def main():
    parser = argparse.ArgumentParser(
        description="Platform metrics collection and comparison."
    )
    parser.add_argument("repo", type=Path, help="repository root")
    parser.add_argument("metrics_dir", type=Path, help="metrics output directory")
    parser.add_argument("--history", action="store_true",
                        help="list snapshot history with key metrics")
    parser.add_argument("--quality", action="store_true",
                        help="show code-quality metrics (test/code ratio, density)")
    parser.add_argument("--compare", action="store_true",
                        help="compare current metrics against most recent saved snapshot")
    parser.add_argument("--save", action="store_true",
                        help="save current metrics as a timestamped snapshot")
    parsed = parser.parse_args()
    repo = parsed.repo
    metrics_dir = parsed.metrics_dir
    # Build flag list to keep downstream logic minimal-diff
    args = [f"--{flag}" for flag in ("history", "quality", "compare", "save")
            if getattr(parsed, flag)]

    if "--history" in args:
        print("\033[1m=== Metric Snapshots ===\033[0m")
        for f in sorted(metrics_dir.glob("*.json")):
            try:
                d = json.loads(f.read_text(encoding="utf-8"))
            except (json.JSONDecodeError, OSError):
                print(f"  {f.stem}  [corrupt or unreadable]")
                continue
            print(f"  {f.stem}  wheels={d.get('wheels_total','-')}  tests={d.get('test_suites','-')}  assertions={d.get('assertions','-')}")
        return

    if "--quality" in args:
        q = collect_quality(repo)
        print_quality(q)
        return

    current = collect(repo)

    if "--compare" in args:
        snapshots = sorted(metrics_dir.glob("*.json"))
        if not snapshots:
            print("No saved snapshots. Run with --save first.")
            sys.exit(1)
        try:
            prev = json.loads(snapshots[-1].read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError) as e:
            print(f"[ERR] Cannot read snapshot {snapshots[-1]}: {e}", file=sys.stderr)
            sys.exit(1)
        print_metrics(prev, f"Previous ({snapshots[-1].stem})")
        print()
        print_metrics(current, "Current")
        print()
        print_delta(prev, current)
        return

    print_metrics(current)

    if "--save" in args:
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        out = metrics_dir / f"{ts}.json"
        out.write_text(json.dumps(current, indent=2), encoding="utf-8")
        print(f"\n\033[0;32mSaved: {out}\033[0m")


if __name__ == "__main__":
    main()
