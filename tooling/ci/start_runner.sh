#!/bin/bash
# start_runner.sh — start self-hosted GitHub Actions runner in background
#
# Usage:
#   bash tooling/ci/start_runner.sh          # start in background
#   bash tooling/ci/start_runner.sh --fg     # start in foreground (ctrl-c to stop)
#   bash tooling/ci/start_runner.sh --stop   # stop runner
#   bash tooling/ci/start_runner.sh --status # check status
#
# Prerequisites: ~/actions-runner must be configured.

set -euo pipefail

RUNNER_DIR="$HOME/actions-runner"
PID_FILE="$RUNNER_DIR/.runner_pid"

if [[ ! -f "$RUNNER_DIR/run.sh" ]]; then
    echo "ERROR: runner not configured at $RUNNER_DIR"
    echo "Run the setup first (see CLAUDE.md CI section)."
    exit 1
fi

case "${1:-}" in
    --stop)
        if [[ -f "$PID_FILE" ]]; then
            PID=$(cat "$PID_FILE")
            if kill -0 "$PID" 2>/dev/null; then
                kill "$PID"
                echo "Runner stopped (PID $PID)"
            else
                echo "Runner not running (stale PID $PID)"
            fi
            rm -f "$PID_FILE"
        else
            echo "No PID file found"
        fi
        ;;
    --status)
        if [[ -f "$PID_FILE" ]] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
            echo "Runner is running (PID $(cat "$PID_FILE"))"
        else
            echo "Runner is not running"
        fi
        ;;
    --fg)
        cd "$RUNNER_DIR" && exec ./run.sh
        ;;
    *)
        cd "$RUNNER_DIR"
        nohup ./run.sh > /tmp/actions-runner.log 2>&1 &
        echo $! > "$PID_FILE"
        echo "Runner started (PID $!) — log at /tmp/actions-runner.log"
        ;;
esac
