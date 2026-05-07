#!/bin/bash
# coverage_scan.sh — batch coverage scan using coverage_wheel.sh
#
# Usage:
#   coverage_scan.sh                  # scan all, show sub-80%
#   coverage_scan.sh --threshold 90   # custom threshold
#   coverage_scan.sh --all            # show all wheels sorted
#
# Depends on coverage_wheel.sh --machine (gcov-tool merge for accuracy).

set -uo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT="$REPO/tooling/test/coverage_wheel.sh"
THRESHOLD=80
SHOW_ALL=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --threshold) THRESHOLD="$2"; shift 2 ;;
        --all)       SHOW_ALL=1; shift ;;
        *)           echo "Unknown arg: $1"; exit 1 ;;
    esac
done

declare -a results=()
total=0
below=0

for layer in primitives drivers protocols modules; do
    layer_dir="$REPO/wheels/$layer"
    [ -d "$layer_dir" ] || continue
    for wheel_dir in "$layer_dir"/*/; do
        [ -d "$wheel_dir" ] || continue
        name=$(basename "$wheel_dir")
        [ -f "$wheel_dir/${name}.c" ] || continue
        [ -d "$wheel_dir/test" ] || continue

        total=$((total + 1))
        out=$(bash "$SCRIPT" --machine "$layer/$name" 2>/dev/null)

        case "$out" in
            BUILD_FAIL|NO_EXES|NO_GCDA)
                results+=("$(printf '%7s  %7s  %s/%s' "$out" "-" "$layer" "$name")")
                below=$((below + 1))
                ;;
            *)
                line_pct=$(echo "$out" | awk '{print $1}')
                branch_pct=$(echo "$out" | awk '{print $2}')
                int_pct=${line_pct%%.*}
                if [[ $SHOW_ALL -eq 1 ]] || [[ "$int_pct" -lt "$THRESHOLD" ]] 2>/dev/null; then
                    results+=("$(printf '%6s%%  %6s%%  %s/%s' "$line_pct" "$branch_pct" "$layer" "$name")")
                fi
                if [[ "$int_pct" -lt "$THRESHOLD" ]] 2>/dev/null; then
                    below=$((below + 1))
                fi
                ;;
        esac
    done
done

echo "=== Coverage Scan ==="
echo "  Scanned: $total wheels"
echo "  Below ${THRESHOLD}%: $below"
echo ""

if [[ ${#results[@]} -gt 0 ]]; then
    printf "  %7s  %7s  %s\n" "Lines" "Branch" "Wheel"
    printf "  %7s  %7s  %s\n" "-------" "-------" "-----"
    printf '%s\n' "${results[@]}" | sort -n
else
    echo "  All wheels at or above ${THRESHOLD}% coverage."
fi
