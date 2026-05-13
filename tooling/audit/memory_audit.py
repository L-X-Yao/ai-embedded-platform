#!/usr/bin/env python3
"""
memory_audit.py — detect stale, orphaned, and bloated memory files.

Checks:
  1. Orphan detection: .md files not referenced in MEMORY.md index
  2. Staleness: files containing TODO/pending/进行中 keywords
  3. Bloat warning: total count > threshold
  4. Index quality: MEMORY.md entries pointing to nonexistent files

Exit 0 if clean, 1 if issues found.
"""

import argparse
import os
import re
import sys
from pathlib import Path


def find_memory_dir():
    """Find the memory directory matching the current repo.

    Derives the expected project slug from the repo root path (e.g.
    D:\\temp\\project\\ai-embedded-platform -> D--temp-project-ai-embedded-platform)
    and looks for that specific directory first.
    """
    repo_root = Path(__file__).resolve().parents[2]
    slug = str(repo_root).replace(":", "-").replace("\\", "-").replace("/", "-")
    candidates = [Path.home() / ".claude" / "projects"]
    win_user = os.environ.get("USERPROFILE") or os.environ.get("HOME_WINDOWS")
    if win_user:
        candidates.append(Path(win_user) / ".claude" / "projects")
    wsl_mnt = Path("/mnt/c/Users")
    if wsl_mnt.is_dir():
        for u in wsl_mnt.iterdir():
            candidates.append(u / ".claude" / "projects")
    # First pass: exact match by repo slug
    for base in candidates:
        if not base.is_dir():
            continue
        exact = base / slug / "memory"
        if exact.is_dir() and (exact / "MEMORY.md").exists():
            return exact
    # Fallback: first directory containing MEMORY.md
    for base in candidates:
        if not base.is_dir():
            continue
        for d in sorted(base.iterdir()):
            if not d.is_dir():
                continue
            candidate = d / "memory"
            if candidate.is_dir() and (candidate / "MEMORY.md").exists():
                return candidate
    return None


STALE_PATTERNS = re.compile(
    r'\bTODO\b|(?<![A-Z_])pending(?![A-Z_])|进行中|残留|(?<!已)未测(?!试)|待执行|未完成',
    re.IGNORECASE
)

BACKTICK_RE = re.compile(r'`[^`]+`')


def strip_code_spans(line: str) -> str:
    return BACKTICK_RE.sub('', line)

BLOAT_THRESHOLD = 80


def audit(mem_dir: Path) -> list:
    issues = []
    index_file = mem_dir / "MEMORY.md"
    index_text = index_file.read_text(encoding="utf-8", errors="replace")

    all_files = {f.name for f in mem_dir.glob("*.md") if f.name != "MEMORY.md"}

    referenced = set(re.findall(r'\(([^)]+\.md)\)', index_text))

    orphans = all_files - referenced
    for f in sorted(orphans):
        issues.append(("ORPHAN", f, "not referenced in MEMORY.md index"))

    missing = referenced - all_files
    for f in sorted(missing):
        issues.append(("BROKEN", f, "referenced in MEMORY.md but file missing"))

    for f in sorted(all_files):
        try:
            content = (mem_dir / f).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        lines = content.splitlines()
        in_frontmatter = False
        for i, line in enumerate(lines):
            if line.strip() == "---":
                in_frontmatter = not in_frontmatter
                continue
            if in_frontmatter:
                continue
            if STALE_PATTERNS.search(strip_code_spans(line)):
                issues.append(("STALE", f, f"L{i+1}: {line.strip()[:80]}"))
                break

    if len(all_files) > BLOAT_THRESHOLD:
        issues.append(("BLOAT", "MEMORY.md",
                       f"{len(all_files)} files (threshold {BLOAT_THRESHOLD})"))

    return issues


def main():
    parser = argparse.ArgumentParser(
        description="Detect stale, orphaned, and bloated memory files."
    )
    parser.add_argument("--dir", type=Path, default=None,
                        help="memory directory to audit (default: auto-detect)")
    args = parser.parse_args()
    mem_dir = args.dir
    if not mem_dir:
        mem_dir = find_memory_dir()
    if not mem_dir:
        print("memory_audit: memory directory not found (skipping)")
        return 0

    issues = audit(mem_dir)

    if not issues:
        print(f"memory_audit: clean ({len(list(mem_dir.glob('*.md')))-1} files)")
        return 0

    fails = [i for i in issues if i[0] in ("BROKEN", "BLOAT")]
    warns = [i for i in issues if i[0] not in ("BROKEN", "BLOAT")]

    print(f"memory_audit: {len(fails)} error(s), {len(warns)} warning(s)\n")
    for level, target, msg in issues:
        print(f"  [{level}]  {target}: {msg}")

    if warns:
        print(f"\nWarnings are non-blocking. Fix: delete orphans or update stale claims.")
    if fails:
        print(f"\nErrors block check. Fix: add missing files or reduce memory count.")

    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
