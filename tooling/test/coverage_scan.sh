#!/bin/bash
# coverage_scan.sh — batch coverage scan using coverage_wheel.sh
#
# Usage:
#   coverage_scan.sh                  # scan all, show sub-80%
#   coverage_scan.sh --threshold 90   # custom threshold
#   coverage_scan.sh --all            # show all wheels sorted
#   coverage_scan.sh --gate           # exit 1 if any wheel < threshold (for CI)
#   coverage_scan.sh --jobs 8         # parallel workers (default: 4)
#
# Depends on coverage_wheel.sh --machine (gcov-tool merge for accuracy).
# Runs wheels in parallel for ~4x speedup over serial scan.

set -uo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT="$REPO/tooling/test/coverage_wheel.sh"
THRESHOLD=80
SHOW_ALL=0
GATE=0
JOBS=4

while [[ $# -gt 0 ]]; do
    case "$1" in
        --threshold)
            [[ "$2" =~ ^[0-9]+$ ]] || { echo "ERROR: --threshold must be numeric"; exit 1; }
            THRESHOLD="$2"; shift 2 ;;
        --all)       SHOW_ALL=1; shift ;;
        --gate)      GATE=1; shift ;;
        --jobs)
            [[ "$2" =~ ^[0-9]+$ ]] || { echo "ERROR: --jobs must be numeric"; exit 1; }
            JOBS="$2"; shift 2 ;;
        *)           echo "Unknown arg: $1"; exit 1 ;;
    esac
done

SCAN_TMPDIR=$(mktemp -d)
trap 'rm -rf "$SCAN_TMPDIR"' EXIT

# Discover all measurable wheels
declare -a wheel_list=()
for layer in primitives drivers protocols modules; do
    layer_dir="$REPO/wheels/$layer"
    [ -d "$layer_dir" ] || continue
    for wheel_dir in "$layer_dir"/*/; do
        [ -d "$wheel_dir" ] || continue
        name=$(basename "$wheel_dir")
        [ -f "$wheel_dir/${name}.c" ] || continue
        [ -d "$wheel_dir/test" ] || continue
        wheel_list+=("$layer/$name")
    done
done

total=${#wheel_list[@]}

# Worker function: measure one wheel, write result to file
measure_wheel() {
    local wheel="$1"
    local idx="$2"
    local out
    out=$(bash "$SCRIPT" --machine "$wheel" 2>/dev/null)
    echo "$wheel $out" > "$SCAN_TMPDIR/result_${idx}"
}

# Launch workers with concurrency limit
running=0
for i in "${!wheel_list[@]}"; do
    measure_wheel "${wheel_list[$i]}" "$i" &
    running=$((running + 1))
    if [[ $running -ge $JOBS ]]; then
        wait -n 2>/dev/null || true
        running=$((running - 1))
    fi
done
wait

# Collect results
declare -a results=()
below=0
ok=0
errors=0

for i in "${!wheel_list[@]}"; do
    result_file="$SCAN_TMPDIR/result_${i}"
    [[ -f "$result_file" ]] || continue
    line=$(cat "$result_file")
    wheel=$(echo "$line" | awk '{print $1}')
    out=$(echo "$line" | cut -d' ' -f2-)

    case "$out" in
        BUILD_FAIL|NO_EXES|NO_GCDA)
            results+=("$(printf '%7s  %7s  %s' "$out" "-" "$wheel")")
            errors=$((errors + 1))
            below=$((below + 1))
            ;;
        *)
            line_pct=$(echo "$out" | awk '{print $1}')
            branch_pct=$(echo "$out" | awk '{print $2}')
            int_pct=${line_pct%%.*}
            if [[ $SHOW_ALL -eq 1 ]] || [[ "$int_pct" -lt "$THRESHOLD" ]] 2>/dev/null; then
                results+=("$(printf '%6s%%  %6s%%  %s' "$line_pct" "$branch_pct" "$wheel")")
            fi
            if [[ "$int_pct" -lt "$THRESHOLD" ]] 2>/dev/null; then
                below=$((below + 1))
            else
                ok=$((ok + 1))
            fi
            ;;
    esac
done

echo "=== Coverage Scan (${JOBS} workers) ==="
echo "  Scanned: $total wheels"
echo "  OK (>=${THRESHOLD}%): $ok | Below: $below | Errors: $errors"
echo ""

if [[ ${#results[@]} -gt 0 ]]; then
    printf "  %7s  %7s  %s\n" "Lines" "Branch" "Wheel"
    printf "  %7s  %7s  %s\n" "-------" "-------" "-----"
    printf '%s\n' "${results[@]}" | sort -n
else
    echo "  All wheels at or above ${THRESHOLD}% coverage."
fi

if [[ $GATE -eq 1 && $below -gt 0 ]]; then
    echo ""
    echo "FAIL: $below wheel(s) below ${THRESHOLD}% coverage threshold."
    exit 1
fi
