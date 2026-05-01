#!/bin/bash
# platform_metrics.sh — capture and compare platform health metrics.
#
# Usage:
#   platform_metrics.sh                 # capture current snapshot + show
#   platform_metrics.sh --save          # capture and save to .metrics/
#   platform_metrics.sh --compare       # compare current vs last saved
#   platform_metrics.sh --history       # show all saved snapshots
#
# Tracked metrics: wheel counts, test counts, audit FAIL/WARN,
# ARM .text total, tool count, CLAUDE.md lines, commit count.
#
# Snapshots saved to .metrics/<timestamp>.json (gitignored).

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
METRICS_DIR="$REPO/.metrics"

mkdir -p "$METRICS_DIR"
grep -q '^\.metrics/' "$REPO/.gitignore" 2>/dev/null || echo '.metrics/' >> "$REPO/.gitignore"

exec python3 "$REPO/tooling/metrics/platform_metrics_impl.py" "$REPO" "$METRICS_DIR" "$@"
