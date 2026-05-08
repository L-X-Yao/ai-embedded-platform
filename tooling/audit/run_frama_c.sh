#!/usr/bin/env bash
# run_frama_c.sh — Run Frama-C WP formal verification on annotated L0 wheels
#
# Usage:
#   wsl bash tooling/audit/run_frama_c.sh [--verbose] [--wheel <name>]
#
# Requires Frama-C + Alt-Ergo installed in WSL (opam).
# See CLAUDE.md for installation path.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Frama-C settings
TIMEOUT=10          # seconds per proof goal
PROVER="alt-ergo"
CPP_FLAGS="-Itooling/test/ -UCONTRACT_ENABLED"

VERBOSE=0
SINGLE_WHEEL=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --verbose|-v) VERBOSE=1; shift ;;
        --wheel|-w)   SINGLE_WHEEL="$2"; shift 2 ;;
        *)            echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Annotated wheels (wheel_name : directory)
declare -A WHEELS=(
    [ring_buffer]="wheels/primitives/ring_buffer"
    [crc16]="wheels/primitives/crc16"
    [base64]="wheels/primitives/base64"
    [checksum]="wheels/primitives/checksum"
    [bitfield]="wheels/primitives/bitfield"
)

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

TOTAL_PROVED=0
TOTAL_GOALS=0
PASS_COUNT=0
FAIL_COUNT=0

cd "$REPO_ROOT"

echo "========================================"
echo "  Frama-C WP Formal Verification Report"
echo "========================================"
echo ""
echo "Prover:  $PROVER"
echo "Timeout: ${TIMEOUT}s per goal"
echo "Date:    $(date -Iseconds)"
echo ""

run_wheel() {
    local name="$1"
    local dir="$2"
    local hfile="$dir/${name}.h"
    local cfile="$dir/${name}.c"

    if [[ ! -f "$hfile" || ! -f "$cfile" ]]; then
        echo -e "  ${RED}SKIP${NC} $name — missing .h or .c"
        return
    fi

    local cpp_inc="-I${dir}/ ${CPP_FLAGS}"

    local output
    output=$(frama-c -wp -wp-rte \
        -wp-prover="$PROVER" \
        -wp-timeout="$TIMEOUT" \
        "$cfile" "$hfile" \
        -cpp-extra-args="$cpp_inc" 2>&1) || true

    # Parse "Proved goals: X / Y" line
    local proved goals
    proved=$(echo "$output" | sed -n 's/.*Proved goals:[[:space:]]*\([0-9][0-9]*\).*/\1/p' | head -1 || echo "0")
    proved=${proved:-0}
    goals=$(echo "$output" | sed -n 's/.*Proved goals:[[:space:]]*[0-9][0-9]* \/ \([0-9][0-9]*\).*/\1/p' | head -1 || echo "0")
    goals=${goals:-0}
    local timeouts
    timeouts=$(echo "$output" | grep -c '\[Timeout\]' || true)

    local pct=0
    if [[ "$goals" -gt 0 ]]; then
        pct=$(( proved * 100 / goals ))
    fi

    TOTAL_PROVED=$((TOTAL_PROVED + proved))
    TOTAL_GOALS=$((TOTAL_GOALS + goals))

    # Status color
    local status_color="$RED"
    local status_text="FAIL"
    if [[ "$pct" -ge 80 ]]; then
        status_color="$GREEN"
        status_text="GOOD"
        PASS_COUNT=$((PASS_COUNT + 1))
    elif [[ "$pct" -ge 50 ]]; then
        status_color="$YELLOW"
        status_text="FAIR"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi

    printf "  %-14s  %s%s%s  %3d / %3d goals  (%d%%)  [%d timeouts]\n" \
        "$name" "$status_color" "$status_text" "$NC" \
        "$proved" "$goals" "$pct" "$timeouts"

    if [[ "$VERBOSE" -eq 1 ]]; then
        echo ""
        echo "$output" | grep -E '^\[wp\]' | head -30
        echo ""
    fi
}

echo "Results per wheel:"
echo "-------------------------------------------"

if [[ -n "$SINGLE_WHEEL" ]]; then
    if [[ -v "WHEELS[$SINGLE_WHEEL]" ]]; then
        run_wheel "$SINGLE_WHEEL" "${WHEELS[$SINGLE_WHEEL]}"
    else
        echo "Unknown wheel: $SINGLE_WHEEL"
        echo "Available: ${!WHEELS[*]}"
        exit 1
    fi
else
    for name in ring_buffer crc16 base64 checksum bitfield; do
        run_wheel "$name" "${WHEELS[$name]}"
    done
fi

echo "-------------------------------------------"
echo ""

# Summary
TOTAL_PCT=0
if [[ "$TOTAL_GOALS" -gt 0 ]]; then
    TOTAL_PCT=$(( TOTAL_PROVED * 100 / TOTAL_GOALS ))
fi

echo "Summary:"
echo "  Total proved:  $TOTAL_PROVED / $TOTAL_GOALS ($TOTAL_PCT%)"
echo "  Wheels passed: $PASS_COUNT"
echo "  Wheels failed: $FAIL_COUNT"
echo ""

if [[ "$TOTAL_PCT" -ge 50 ]]; then
    echo -e "  ${GREEN}PASS${NC} — formal verification proof of concept established"
else
    echo -e "  ${RED}FAIL${NC} — below 50% proof rate"
fi

echo ""
echo "Notes:"
echo "  - Timeouts are typically on loop-related goals (termination,"
echo "    loop invariants, in-loop memory access). Adding loop annotations"
echo "    (/*@ loop invariant ... */) in .c files would resolve these."
echo "  - Non-loop function specs (init, count, free, is_empty, is_full,"
echo "    reset, pure helpers) generally verify fully."
echo "  - RTE checks (division by zero, overflow, mem access) are included"
echo "    via -wp-rte for maximum coverage."
