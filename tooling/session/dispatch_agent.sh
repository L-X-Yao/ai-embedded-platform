#!/bin/bash
# dispatch_agent.sh — dispatch and merge Claude Code agents in isolated worktrees.
#
# Wraps `claude -p -w` with --dangerously-skip-permissions, launches in
# background, and reports PID + worktree branch name.
#
# Usage:
#   dispatch_agent.sh --name audit --budget 1.00 --prompt 'run full audit'
#   dispatch_agent.sh --name fix  --budget 0.50 --prompt 'fix L0 findings' --wait
#   dispatch_agent.sh --merge-all [--push]
#   dispatch_agent.sh --cleanup
#   dispatch_agent.sh --status
#
# Options:
#   --name    <worktree-name>   worktree name (required for dispatch)
#   --budget  <usd>             max spend in USD (required for dispatch)
#   --prompt  <text>            task prompt (required for dispatch)
#   --prompt-file <path>       read prompt from file (use for complex prompts)
#   --scope   <paths>           restrict agent to these paths (comma-separated)
#   --wait                      block until agent completes
#   --merge-all                 squash-merge all worktree-* branches, run make check
#   --push                      push after merge-all (default: don't push)
#   --cleanup                   remove all worktree-* branches and directories
#   --status                    show all active worktree branches and their commits
#   -h|--help                   show this help
#
set -euo pipefail

BOLD='\033[1m'
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'

NAME=""
BUDGET=""
PROMPT=""
PROMPT_FILE=""
SCOPE=""
WAIT=false
MERGE_ALL=false
PUSH=false
STATUS=false
CLEANUP=false

usage() {
    sed -n '2,/^$/{ s/^# //; s/^#$//; p }' "$0"
    exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --name)      NAME="$2";   shift 2 ;;
        --budget)    BUDGET="$2"; shift 2 ;;
        --prompt)    PROMPT="$2"; shift 2 ;;
        --prompt-file) PROMPT_FILE="$2"; shift 2 ;;
        --scope)     SCOPE="$2";  shift 2 ;;
        --wait)      WAIT=true;   shift   ;;
        --merge-all) MERGE_ALL=true; shift ;;
        --push)      PUSH=true;   shift   ;;
        --cleanup)   CLEANUP=true; shift   ;;
        --status)    STATUS=true; shift   ;;
        -h|--help)   usage 0 ;;
        *) echo -e "${RED}Unknown option: $1${NC}"; usage 1 ;;
    esac
done

REPO_ROOT="$(git rev-parse --show-toplevel)"

# --- robust worktree directory removal (handles Windows long paths) ---
remove_worktree_dir() {
    local dir="$1"
    [[ -d "$dir" ]] || return 0

    git worktree unlock "$dir" 2>/dev/null || true
    if git worktree remove --force "$dir" 2>/dev/null; then
        if [[ ! -d "$dir" ]]; then
            return 0
        fi
    fi

    # git worktree remove failed (Windows long path) — force delete
    if command -v cmd.exe &>/dev/null; then
        local win_path
        win_path=$(cd "$dir" 2>/dev/null && pwd -W 2>/dev/null || echo "$dir" | sed 's|^/mnt/\([a-z]\)/|\1:/|; s|/|\\|g')
        cmd.exe /c "rd /s /q \"$win_path\"" 2>/dev/null || true
    fi

    if [[ -d "$dir" ]]; then
        rm -rf "$dir" 2>/dev/null || true
    fi

    if [[ -d "$dir" ]]; then
        echo -e "  ${RED}WARN${NC}  could not remove $dir — delete manually"
        return 1
    fi
    return 0
}

# --- clean up all worktree-* branches and directories ---
cleanup_worktrees() {
    local branches
    branches=$(git branch --list 'worktree-*' | sed 's/^[+* ]*//' | tr -d ' ')
    local wt_base="$REPO_ROOT/.claude/worktrees"
    local cleaned=0

    # Remove worktree directories found via git worktree list
    while IFS= read -r line; do
        local wt_path
        wt_path=$(echo "$line" | awk '{print $1}')
        [[ "$wt_path" == "$REPO_ROOT" ]] && continue
        [[ "$wt_path" == */.claude/worktrees/* ]] || continue
        echo -e "  ${CYAN}REMOVE${NC}  $wt_path"
        remove_worktree_dir "$wt_path"
        cleaned=$((cleaned + 1))
    done < <(git worktree list 2>/dev/null)

    # Also scan for orphaned directories not tracked by git worktree
    if [[ -d "$wt_base" ]]; then
        for d in "$wt_base"/*/; do
            [[ -d "$d" ]] || continue
            echo -e "  ${CYAN}REMOVE${NC}  $d (orphaned)"
            remove_worktree_dir "${d%/}"
            cleaned=$((cleaned + 1))
        done
    fi

    # Delete branches
    for b in $branches; do
        git branch -D "$b" 2>/dev/null && echo -e "  ${GREEN}DELETE${NC}  branch $b"
    done

    git worktree prune 2>/dev/null

    # Verify
    local remaining
    remaining=$(git branch --list 'worktree-*' 2>/dev/null | wc -l)
    local remaining_dirs=0
    [[ -d "$wt_base" ]] && remaining_dirs=$(find "$wt_base" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l)

    if [[ $remaining -eq 0 && $remaining_dirs -eq 0 ]]; then
        echo -e "${GREEN}All worktrees cleaned ($cleaned removed).${NC}"
    else
        echo -e "${YELLOW}Warning: $remaining branch(es), $remaining_dirs dir(s) remain.${NC}"
    fi
}

# --- --status mode ---
if $STATUS; then
    branches=$(git branch --list 'worktree-*' | sed 's/^[+* ]*//' | tr -d ' ')
    if [[ -z "$branches" ]]; then
        echo -e "${GREEN}No active worktree branches.${NC}"
        exit 0
    fi
    echo -e "${BOLD}Active worktree branches:${NC}"
    for b in $branches; do
        commits=$(git log main.."$b" --oneline 2>/dev/null | wc -l)
        echo -e "  ${CYAN}$b${NC}  ($commits commits ahead of main)"
        git log main.."$b" --oneline 2>/dev/null | sed 's/^/    /'
    done
    exit 0
fi

# --- --cleanup mode ---
if $CLEANUP; then
    echo -e "${BOLD}=== Cleaning up all worktrees ===${NC}"
    cleanup_worktrees
    exit 0
fi

# --- --merge-all mode ---
if $MERGE_ALL; then
    branches=$(git branch --list 'worktree-*' | sed 's/^[+* ]*//' | tr -d ' ')
    if [[ -z "$branches" ]]; then
        echo -e "${GREEN}No worktree branches to merge.${NC}"
        exit 0
    fi

    current=$(git branch --show-current)
    if [[ "$current" != "main" ]]; then
        echo -e "${RED}Error: must be on main branch (currently on $current)${NC}"
        exit 1
    fi

    echo -e "${BOLD}=== Merging $(echo "$branches" | wc -w) worktree branches ===${NC}"
    echo ""

    MERGED=0
    FAILED=0
    for b in $branches; do
        commits=$(git log main.."$b" --oneline 2>/dev/null | wc -l)
        if [[ "$commits" -eq 0 ]]; then
            echo -e "  ${YELLOW}SKIP${NC}  $b  (no commits ahead of main)"
            continue
        fi

        echo -e "  ${CYAN}MERGE${NC} $b  ($commits commits)"
        merge_output=$(git merge --squash "$b" 2>&1)
        merge_rc=$?
        if [[ $merge_rc -ne 0 ]] && echo "$merge_output" | grep -q "untracked working tree files would be overwritten"; then
            # Scaffold files on main block the merge — remove them and retry
            untracked=$(echo "$merge_output" | grep $'^\t' | sed 's/^\t//')
            for f in $untracked; do
                [[ -f "$f" ]] && rm -f "$f"
            done
            echo -e "  ${YELLOW}  RETRY${NC}  removed untracked scaffold files blocking merge"
            merge_output=$(git merge --squash "$b" 2>&1)
            merge_rc=$?
        fi
        if [[ $merge_rc -eq 0 ]]; then
            summary=$(git log main.."$b" --oneline | head -1 | sed 's/^[a-f0-9]* //')
            git commit -m "$summary" > /dev/null 2>&1
            echo -e "  ${GREEN}  OK${NC}  → $summary"
            MERGED=$((MERGED + 1))
        else
            echo -e "  ${RED}FAIL${NC}  $b  (merge conflict — resolve manually)"
            git merge --abort 2>/dev/null || git reset --hard HEAD 2>/dev/null
            FAILED=$((FAILED + 1))
        fi
    done

    echo ""
    echo -e "${BOLD}--- Summary ---${NC}"
    echo -e "  Merged: $MERGED  Failed: $FAILED"

    if [[ $FAILED -gt 0 ]]; then
        echo -e "${YELLOW}Some branches failed to merge. Resolve manually.${NC}"
    fi

    # Clean up all worktrees (merged + skipped + orphaned)
    echo ""
    echo -e "${CYAN}Cleaning up worktrees...${NC}"
    cleanup_worktrees

    if [[ $MERGED -gt 0 ]]; then
        echo ""
        echo -e "${BOLD}=== Running make check ===${NC}"
        if make -C "$REPO_ROOT" check; then
            echo -e "  ${GREEN}make check passed${NC}"
        else
            echo -e "  ${RED}make check FAILED — review before pushing${NC}"
            FAILED=$((FAILED + 1))
        fi
    fi

    if $PUSH && [[ $MERGED -gt 0 && $FAILED -eq 0 ]]; then
        echo -e "${CYAN}Pushing...${NC}"
        git push
    elif $PUSH && [[ $FAILED -gt 0 ]]; then
        echo -e "${YELLOW}Skipping push due to failures.${NC}"
    fi

    exit $FAILED
fi

if [[ -z "$NAME" ]]; then
    echo -e "${RED}Error: --name is required${NC}"
    usage 1
fi
if [[ -z "$BUDGET" ]]; then
    echo -e "${RED}Error: --budget is required${NC}"
    usage 1
fi
if [[ -n "$PROMPT_FILE" ]]; then
    if [[ ! -f "$PROMPT_FILE" ]]; then
        echo -e "${RED}Error: prompt file not found: $PROMPT_FILE${NC}"
        exit 1
    fi
    PROMPT="$(cat "$PROMPT_FILE")"
fi
if [[ -z "$PROMPT" ]]; then
    echo -e "${RED}Error: --prompt or --prompt-file is required${NC}"
    usage 1
fi

WORKTREE_BRANCH="worktree-${NAME}"
WORKTREE_DIR="${REPO_ROOT}/.claude/worktrees/${NAME}"
LOG_DIR="${REPO_ROOT}/tooling/logs"
mkdir -p "$LOG_DIR"
LOG_FILE="${LOG_DIR}/dispatch-${NAME}-$(date +%Y%m%d-%H%M%S).log"

SCOPE_SUFFIX=""
if [[ -n "$SCOPE" ]]; then
    SCOPE_SUFFIX="

SCOPE RESTRICTION: You may ONLY modify files under these paths: ${SCOPE}
Do NOT modify files outside these paths. If you need to change files outside scope, note them but do not edit."
fi

COMMIT_SUFFIX="

When you are done with ALL changes, you MUST commit your work:
1. git add the files you changed
2. git commit with a message following the repo's commit conventions
Do NOT skip the commit step."

FULL_PROMPT="${PROMPT}${SCOPE_SUFFIX}${COMMIT_SUFFIX}"

echo -e "${CYAN}${BOLD}Dispatching agent:${NC} ${NAME}"
echo -e "  worktree branch: ${GREEN}${WORKTREE_BRANCH}${NC}"
echo -e "  budget:          \$${BUDGET}"
echo -e "  log:             ${LOG_FILE}"

if $WAIT; then
    echo -e "${YELLOW}Running in foreground (--wait)...${NC}"
    claude -p "$FULL_PROMPT" \
        -w "$NAME" \
        --dangerously-skip-permissions \
        --max-budget-usd "$BUDGET" \
        2>&1 | tee "$LOG_FILE"
    EXIT_CODE=${PIPESTATUS[0]}  # PIPESTATUS requires bash (not POSIX sh)
    echo -e "${GREEN}Agent ${NAME} finished (exit ${EXIT_CODE}).${NC}"
    exit "$EXIT_CODE"
else
    claude -p "$FULL_PROMPT" \
        -w "$NAME" \
        --dangerously-skip-permissions \
        --max-budget-usd "$BUDGET" \
        > "$LOG_FILE" 2>&1 &
    PID=$!
    echo -e "  PID:             ${GREEN}${PID}${NC}"
    echo -e "${GREEN}Agent launched in background.${NC}"
    echo -e "  tail -f ${LOG_FILE}"
    echo "$PID"
fi
