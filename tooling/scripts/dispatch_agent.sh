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
#   dispatch_agent.sh --status
#
# Options:
#   --name    <worktree-name>   worktree name (required for dispatch)
#   --budget  <usd>             max spend in USD (required for dispatch)
#   --prompt  <text>            task prompt (required for dispatch)
#   --scope   <paths>           restrict agent to these paths (comma-separated)
#   --wait                      block until agent completes
#   --merge-all                 squash-merge all worktree-* branches, run make check
#   --push                      push after merge-all (default: don't push)
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
SCOPE=""
WAIT=false
MERGE_ALL=false
PUSH=false
STATUS=false

usage() {
    sed -n '2,/^$/{ s/^# //; s/^#$//; p }' "$0"
    exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --name)      NAME="$2";   shift 2 ;;
        --budget)    BUDGET="$2"; shift 2 ;;
        --prompt)    PROMPT="$2"; shift 2 ;;
        --scope)     SCOPE="$2";  shift 2 ;;
        --wait)      WAIT=true;   shift   ;;
        --merge-all) MERGE_ALL=true; shift ;;
        --push)      PUSH=true;   shift   ;;
        --status)    STATUS=true; shift   ;;
        -h|--help)   usage 0 ;;
        *) echo -e "${RED}Unknown option: $1${NC}"; usage 1 ;;
    esac
done

REPO_ROOT="$(git rev-parse --show-toplevel)"

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
        if git merge --squash "$b" > /dev/null 2>&1; then
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

    # Clean up merged worktrees
    if [[ $MERGED -gt 0 ]]; then
        echo ""
        echo -e "${CYAN}Cleaning up merged worktrees...${NC}"
        for b in $branches; do
            wt_dir="$REPO_ROOT/.claude/worktrees/${b#worktree-}"
            if [[ -d "$wt_dir" ]]; then
                git worktree unlock "$wt_dir" 2>/dev/null
                git worktree remove --force "$wt_dir" 2>/dev/null
            fi
            git branch -D "$b" 2>/dev/null
        done
        git worktree prune 2>/dev/null
    fi

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
if [[ -z "$PROMPT" ]]; then
    echo -e "${RED}Error: --prompt is required${NC}"
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
    EXIT_CODE=${PIPESTATUS[0]}
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
