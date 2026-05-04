#!/bin/bash
# merge_agents.sh — merge all worktree-agent-* branches into main
#
# Usage:
#   merge_agents.sh              # merge + verify + test
#   merge_agents.sh --push       # merge + verify + test + push
#   merge_agents.sh --dry-run    # show what would be merged
#
# Handles the known scaffold-residual problem: if a merged wheel's .c
# file is < 50 lines, restores from the agent's original commit.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO"

BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

PUSH=false
DRY_RUN=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --push)    PUSH=true; shift ;;
        --dry-run) DRY_RUN=true; shift ;;
        -h|--help) sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"; exit 0 ;;
        *)         echo "Unknown arg: $1"; exit 1 ;;
    esac
done

# --- Clean worktrees ---
for wt_dir in .claude/worktrees/*/; do
    [[ -d "$wt_dir" ]] || continue
    name=$(basename "$wt_dir")
    git worktree unlock "$wt_dir" 2>/dev/null || true
    git worktree remove --force "$wt_dir" 2>/dev/null || true
done
git worktree prune 2>/dev/null

# --- Ensure on main ---
git checkout main 2>/dev/null || true
git pull origin main 2>/dev/null || true

# --- Find branches to merge ---
BRANCHES=$(git branch --list "worktree-agent-*" --format="%(refname:short)" 2>/dev/null)
if [[ -z "$BRANCHES" ]]; then
    echo -e "${YELLOW}No worktree-agent-* branches found.${NC}"
    exit 0
fi

MERGED=0
SKIPPED=0
RESTORED=0

for b in $BRANCHES; do
    ahead=$(git log --oneline "$b" --not main 2>/dev/null || true)
    if [[ -z "$ahead" ]]; then
        SKIPPED=$((SKIPPED + 1))
        $DRY_RUN && echo -e "  SKIP  $b"
        continue
    fi

    msg=$(git log --oneline -1 "$b" | cut -c9-)

    if $DRY_RUN; then
        echo -e "  MERGE $b → $msg"
        continue
    fi

    if git merge --squash "$b" >/dev/null 2>&1; then
        if git diff --cached --quiet 2>/dev/null; then
            echo -e "  ${YELLOW}SKIP${NC}  $b (no changes after squash)"
            SKIPPED=$((SKIPPED + 1))
            continue
        fi
        git commit -m "$msg" >/dev/null 2>&1
        echo -e "  ${GREEN}OK${NC}    $b"
        MERGED=$((MERGED + 1))
    else
        # Merge failed — likely untracked files (scaffold residual)
        git reset HEAD >/dev/null 2>&1 || true

        # Find new wheel directories — collect all dirs to add after loop
        commit=$(git log --oneline -1 "$b" --format="%H")
        restored_dirs=()
        for new_dir in $(git diff-tree --no-commit-id --name-only -r "$commit" 2>/dev/null | sed 's|/[^/]*$||' | sort -u | grep "^wheels/"); do
            [[ -d "$new_dir" ]] || continue
            main_c=$(find "$new_dir" -maxdepth 1 -name "*.c" ! -path "*/test/*" 2>/dev/null | head -1)
            if [[ -n "$main_c" ]]; then
                lines=$(wc -l < "$main_c")
                if [[ $lines -lt 50 ]]; then
                    echo -e "  ${YELLOW}RESTORE${NC} $new_dir (${lines} lines → scaffold detected)"
                    # Restore from agent commit
                    for f in $(git diff-tree --no-commit-id --name-only -r "$commit" 2>/dev/null | grep "^${new_dir}/"); do
                        git show "$commit:$f" > "$f" 2>/dev/null || true
                    done
                    RESTORED=$((RESTORED + 1))
                fi
            fi
            restored_dirs+=("$new_dir")
        done

        for d in "${restored_dirs[@]}"; do
            git add "$d" 2>/dev/null || true
        done
        git commit -m "$msg" >/dev/null 2>&1 || true
        echo -e "  ${GREEN}OK${NC}    $b (with restore)"
        MERGED=$((MERGED + 1))
    fi
done

# --- Verify file sizes ---
echo ""
echo -e "${BOLD}--- File size verification ---${NC}"
SUSPECT=0
for c_file in $(git diff --name-only "HEAD~${MERGED}" HEAD 2>/dev/null | grep "\.c$" | grep -v "test/" || true); do
    [[ -f "$c_file" ]] || continue
    lines=$(wc -l < "$c_file")
    if [[ $lines -lt 50 ]]; then
        echo -e "  ${RED}WARN${NC}  $c_file: ${lines} lines (possible scaffold stub)"
        SUSPECT=$((SUSPECT + 1))
    fi
done
[[ $SUSPECT -eq 0 ]] && echo -e "  ${GREEN}All files OK${NC}"

# --- Clean branches ---
for b in $BRANCHES; do
    git branch -D "$b" 2>/dev/null || true
done
git worktree prune 2>/dev/null

# --- Summary ---
echo ""
echo -e "${BOLD}--- Summary ---${NC}"
echo -e "  Merged: $MERGED  Skipped: $SKIPPED  Restored: $RESTORED  Suspect: $SUSPECT"

# --- Test ---
if [[ $MERGED -gt 0 ]] && ! $DRY_RUN; then
    echo ""
    echo -e "${BOLD}--- Running tests ---${NC}"
    bash "$REPO/tooling/test/test_all.sh" --parallel
fi

# --- Push ---
if $PUSH && [[ $MERGED -gt 0 ]] && ! $DRY_RUN; then
    echo ""
    git push
    echo -e "${GREEN}Pushed.${NC}"
fi
