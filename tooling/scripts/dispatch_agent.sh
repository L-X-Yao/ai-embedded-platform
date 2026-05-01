#!/bin/bash
# dispatch_agent.sh — dispatch a Claude Code agent in an isolated worktree.
#
# Wraps `claude -p -w` with --dangerously-skip-permissions, launches in
# background, and reports PID + worktree branch name.
#
# Usage:
#   dispatch_agent.sh --name audit --budget 1.00 --prompt 'run full audit'
#   dispatch_agent.sh --name fix  --budget 0.50 --prompt 'fix L0 findings' --wait
#
# Options:
#   --name    <worktree-name>   worktree name (required)
#   --budget  <usd>             max spend in USD (required)
#   --prompt  <text>            task prompt (required)
#   --wait                      block until agent completes
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
WAIT=false

usage() {
    sed -n '2,/^$/{ s/^# //; s/^#$//; p }' "$0"
    exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --name)   NAME="$2";   shift 2 ;;
        --budget) BUDGET="$2"; shift 2 ;;
        --prompt) PROMPT="$2"; shift 2 ;;
        --wait)   WAIT=true;   shift   ;;
        -h|--help) usage 0 ;;
        *) echo -e "${RED}Unknown option: $1${NC}"; usage 1 ;;
    esac
done

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

REPO_ROOT="$(git rev-parse --show-toplevel)"
WORKTREE_BRANCH="worktree-${NAME}"
WORKTREE_DIR="${REPO_ROOT}/.claude/worktrees/${NAME}"
LOG_DIR="${REPO_ROOT}/tooling/logs"
mkdir -p "$LOG_DIR"
LOG_FILE="${LOG_DIR}/dispatch-${NAME}-$(date +%Y%m%d-%H%M%S).log"

echo -e "${CYAN}${BOLD}Dispatching agent:${NC} ${NAME}"
echo -e "  worktree branch: ${GREEN}${WORKTREE_BRANCH}${NC}"
echo -e "  budget:          \$${BUDGET}"
echo -e "  log:             ${LOG_FILE}"

if $WAIT; then
    echo -e "${YELLOW}Running in foreground (--wait)...${NC}"
    claude -p "$PROMPT" \
        -w "$NAME" \
        --dangerously-skip-permissions \
        --max-cost "$BUDGET" \
        2>&1 | tee "$LOG_FILE"
    EXIT_CODE=${PIPESTATUS[0]}
    echo -e "${GREEN}Agent ${NAME} finished (exit ${EXIT_CODE}).${NC}"
    exit "$EXIT_CODE"
else
    claude -p "$PROMPT" \
        -w "$NAME" \
        --dangerously-skip-permissions \
        --max-cost "$BUDGET" \
        > "$LOG_FILE" 2>&1 &
    PID=$!
    echo -e "  PID:             ${GREEN}${PID}${NC}"
    echo -e "${GREEN}Agent launched in background.${NC}"
    echo -e "  tail -f ${LOG_FILE}"
    echo "$PID"
fi
