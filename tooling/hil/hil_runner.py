#!/usr/bin/env python3
"""HIL soak-test runner — wraps build_flash_check.sh with profiles and result archival."""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CONF_DIR = REPO_ROOT / "tooling" / "hil" / "build_flash"
RESULTS_DIR = Path(__file__).resolve().parent / "results"
BFC_SCRIPT = CONF_DIR / "build_flash_check.sh"
HIL_TMP_DIR = Path(__file__).resolve().parent / ".tmp"

DEFAULT_TIERS = {"quick": 30, "standard": 600, "stress": 1800}


def discover_projects():
    projects = {}
    for f in sorted(CONF_DIR.glob("*.conf")):
        if f.name == "example.conf":
            continue
        projects[f.stem] = f
    return projects


def parse_conf_value(conf_path, key):
    pattern = re.compile(rf'^{key}="([^"]*)"', re.MULTILINE)
    text = conf_path.read_text(encoding="utf-8")
    m = pattern.search(text)
    return m.group(1) if m else None


def parse_soak_tiers(conf_path):
    tiers = {}
    for tier in ("quick", "standard", "stress"):
        val = parse_conf_value(conf_path, f"SOAK_{tier.upper()}")
        tiers[tier] = int(val) if val else DEFAULT_TIERS[tier]
    return tiers


def get_git_info():
    try:
        sha = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True, text=True, cwd=str(REPO_ROOT),
        ).stdout.strip()
        dirty = subprocess.run(
            ["git", "diff", "--quiet"],
            capture_output=True, cwd=str(REPO_ROOT),
        ).returncode != 0
        return sha, dirty
    except Exception:
        return "unknown", False


def resolve_result_dir(project, name):
    base = RESULTS_DIR / project
    if not base.exists():
        return None
    dirs = sorted([d for d in base.iterdir() if d.is_dir()])
    if not dirs:
        return None
    if name == "latest":
        return dirs[-1]
    if name == "previous":
        return dirs[-2] if len(dirs) >= 2 else None
    p = Path(name)
    if p.is_dir():
        return p
    candidate = base / name
    return candidate if candidate.is_dir() else None


def run_soak(project, conf_path, soak_tier, skip_build, skip_flash):
    tiers = parse_soak_tiers(conf_path)
    duration = tiers[soak_tier]
    serial_log = parse_conf_value(conf_path, "SERIAL_LOG")
    if not serial_log:
        print("[ERR] SERIAL_LOG not set in conf", file=sys.stderr)
        return None

    serial_log_path = Path(serial_log)
    verdict_json_path = Path(serial_log.replace(".log", "_verdict.json"))

    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    result_dir = RESULTS_DIR / project / ts
    result_dir.mkdir(parents=True, exist_ok=True)

    # temp conf that sources the real one then overrides CHECK_DURATION
    # Must live inside the repo so build_flash_check.sh accepts it.
    HIL_TMP_DIR.mkdir(parents=True, exist_ok=True)
    tmp = tempfile.NamedTemporaryFile(
        mode="w", suffix=".conf", delete=False, encoding="utf-8",
        dir=str(HIL_TMP_DIR),
    )
    try:
        conf_posix = conf_path.as_posix()
        tmp.write(f'. "{conf_posix}"\n')
        tmp.write(f'CHECK_DURATION="{duration}"\n')
        tmp.close()

        if skip_build and skip_flash:
            action = ["check"]
        elif skip_build:
            action = ["flash"]
        elif skip_flash:
            action = ["build"]
        else:
            action = ["all"]

        # for skip combos that need two steps, run check separately after
        need_separate_check = (skip_build != skip_flash) and action[0] != "check"

        print(f"=== HIL {soak_tier} soak: {project} @ {duration}s ===")
        print(f"    conf:   {conf_path}")
        print(f"    result: {result_dir}")
        print()

        cmd_base = ["bash", str(BFC_SCRIPT), "--conf", tmp.name]

        rc = subprocess.run(cmd_base + action).returncode
        if rc != 0 and action[0] in ("build", "flash"):
            print(f"[ERR] {action[0]} failed (rc={rc})")
            _save_meta(result_dir, project, soak_tier, duration, conf_path, "ERROR")
            return result_dir

        if need_separate_check:
            rc = subprocess.run(cmd_base + ["check"]).returncode

        # collect results
        if serial_log_path.exists():
            shutil.copy2(str(serial_log_path), str(result_dir / "serial.log"))
        if verdict_json_path.exists():
            shutil.copy2(str(verdict_json_path), str(result_dir / "verdict.json"))

        verdict_data = _load_json(result_dir / "verdict.json")
        verdict_str = verdict_data.get("verdict", "UNKNOWN") if verdict_data else "NO_VERDICT"
        _save_meta(result_dir, project, soak_tier, duration, conf_path, verdict_str)

        print()
        print(f"=== Result: {verdict_str} — saved to {result_dir} ===")
        return result_dir

    finally:
        Path(tmp.name).unlink(missing_ok=True)


def _save_meta(result_dir, project, soak_tier, duration, conf_path, verdict):
    sha, dirty = get_git_info()
    meta = {
        "timestamp": datetime.now().isoformat(),
        "project": project,
        "soak_tier": soak_tier,
        "duration_sec": duration,
        "git_commit": sha,
        "git_dirty": dirty,
        "conf": str(conf_path),
        "verdict": verdict,
    }
    (result_dir / "meta.json").write_text(
        json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8",
    )


def _load_json(path):
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return None


def cmd_list(project):
    base = RESULTS_DIR / project
    if not base.exists():
        print(f"No results for '{project}'.")
        return
    dirs = sorted([d for d in base.iterdir() if d.is_dir()])
    if not dirs:
        print(f"No results for '{project}'.")
        return

    print(f"{'Timestamp':<20} {'Verdict':<10} {'Tier':<10} {'Duration':>8}  {'Commit':<10}")
    print("-" * 65)
    for d in dirs:
        meta = _load_json(d / "meta.json") or {}
        verdict = _load_json(d / "verdict.json") or {}
        print(
            f"{d.name:<20} "
            f"{meta.get('verdict', '?'):<10} "
            f"{meta.get('soak_tier', '?'):<10} "
            f"{meta.get('duration_sec', '?'):>8}s "
            f"{meta.get('git_commit', '?'):<10}"
        )


def cmd_latest(project):
    d = resolve_result_dir(project, "latest")
    if not d:
        print(f"No results for '{project}'.")
        return
    print(f"=== Latest: {d.name} ===\n")
    meta = _load_json(d / "meta.json")
    if meta:
        print("meta.json:")
        print(json.dumps(meta, indent=2, ensure_ascii=False))
    verdict = _load_json(d / "verdict.json")
    if verdict:
        print("\nverdict.json:")
        print(json.dumps(verdict, indent=2, ensure_ascii=False))


def cmd_compare(project, ref_a, ref_b):
    dir_a = resolve_result_dir(project, ref_a)
    dir_b = resolve_result_dir(project, ref_b)
    if not dir_a or not dir_b:
        missing = ref_a if not dir_a else ref_b
        print(f"[ERR] Cannot resolve '{missing}' for project '{project}'.", file=sys.stderr)
        sys.exit(1)

    va = _load_json(dir_a / "verdict.json") or {}
    vb = _load_json(dir_b / "verdict.json") or {}
    ma = _load_json(dir_a / "meta.json") or {}
    mb = _load_json(dir_b / "meta.json") or {}

    name_a = dir_a.name
    name_b = dir_b.name

    print(f"=== Compare: {name_a}  vs  {name_b} ===\n")
    print(f"{'':>20} {name_a:>16} {name_b:>16}  {'delta':>8}")
    print("-" * 65)

    rows = [
        ("verdict", ma.get("verdict", "?"), mb.get("verdict", "?"), None),
        ("soak_tier", ma.get("soak_tier", "?"), mb.get("soak_tier", "?"), None),
        ("duration_sec", va.get("duration_sec", "?"), vb.get("duration_sec", "?"), None),
        ("total_lines", va.get("total_lines", 0), vb.get("total_lines", 0), True),
        ("pass_matched", va.get("pass_matched", 0), vb.get("pass_matched", 0), True),
    ]
    ca = va.get("counters", {})
    cb = vb.get("counters", {})
    bad_counters = {"watchdog_reset", "hardfault"}
    all_keys = sorted(set(list(ca.keys()) + list(cb.keys())))
    for k in all_keys:
        rows.append((k, ca.get(k, 0), cb.get(k, 0), True))

    for label, a, b, show_delta in rows:
        delta_str = ""
        flag = ""
        if show_delta and isinstance(a, (int, float)) and isinstance(b, (int, float)):
            d = b - a
            if d != 0:
                delta_str = f"{d:+d}" if isinstance(d, int) else f"{d:+.1f}"
                if label in bad_counters and d > 0:
                    flag = " [!]"
        print(f"{label:>20} {str(a):>16} {str(b):>16}  {delta_str:>8}{flag}")


def main():
    projects = discover_projects()

    parser = argparse.ArgumentParser(
        description="HIL soak-test runner",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"Discovered projects: {', '.join(projects) or '(none)'}",
    )
    parser.add_argument("--project", "-p", help="Project name (from build_flash/*.conf)")
    parser.add_argument("--conf", type=Path, help="Explicit conf path (overrides --project)")
    parser.add_argument(
        "--soak", choices=["quick", "standard", "stress"], default="quick",
        help="Soak tier (default: quick)",
    )
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--skip-flash", action="store_true")
    parser.add_argument("--list", action="store_true", help="List past results")
    parser.add_argument("--latest", action="store_true", help="Show latest result")
    parser.add_argument("--compare", nargs=2, metavar=("A", "B"),
                        help="Compare two runs (timestamp dir, 'latest', or 'previous')")

    args = parser.parse_args()

    if args.conf:
        if not args.conf.exists():
            parser.error(f"Conf not found: {args.conf}")
        project = args.conf.stem
        conf_path = args.conf
    elif args.project:
        if args.project not in projects:
            parser.error(f"Unknown project '{args.project}'. Available: {', '.join(projects)}")
        project = args.project
        conf_path = projects[project]
    else:
        parser.error("--project or --conf is required")

    if args.list:
        cmd_list(project)
    elif args.latest:
        cmd_latest(project)
    elif args.compare:
        cmd_compare(project, args.compare[0], args.compare[1])
    else:
        result = run_soak(project, conf_path, args.soak, args.skip_build, args.skip_flash)
        sys.exit(0 if result else 1)


if __name__ == "__main__":
    main()
