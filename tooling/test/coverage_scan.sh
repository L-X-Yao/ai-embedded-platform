#!/bin/bash
# coverage_scan.sh — scan all wheels and report coverage, sorted lowest-first.
# Usage: coverage_scan.sh [--top N]

set -uo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
TOP="${1:-0}"  # 0 = show all

results=()

for layer in primitives drivers protocols modules; do
    layer_dir="$REPO/wheels/$layer"
    [ -d "$layer_dir" ] || continue
    for wheel_dir in "$layer_dir"/*/; do
        [ -d "$wheel_dir" ] || continue
        name=$(basename "$wheel_dir")
        src="$wheel_dir/${name}.c"
        test_dir="$wheel_dir/test"
        [ -f "$src" ] && [ -d "$test_dir" ] || continue

        # Build with coverage
        build_dir="$test_dir/build_cov"
        rm -rf "$build_dir"
        mkdir -p "$build_dir"

        if ! cmake -GNinja -S "$test_dir" -B "$build_dir" \
            -DCMAKE_C_FLAGS="--coverage -O0 -g" \
            -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
            > "$build_dir/cmake.log" 2>&1; then
            results+=("  0.0  FAIL(cmake)  $layer/$name")
            continue
        fi

        if ! cmake --build "$build_dir" > "$build_dir/build.log" 2>&1; then
            results+=("  0.0  FAIL(build)  $layer/$name")
            continue
        fi

        # Run tests
        for exe in "$build_dir"/test_*; do
            [ -x "$exe" ] && "$exe" > /dev/null 2>&1 || true
        done

        # Get coverage
        gcda=$(find "$build_dir" -name "${name}.c.gcda" 2>/dev/null | head -1)
        if [ -z "$gcda" ]; then
            results+=("  0.0  NO_GCDA     $layer/$name")
            continue
        fi

        gcda_dir=$(dirname "$gcda")
        gcov_out=$(cd "$gcda_dir" && gcov -b "${name}.c.gcda" 2>&1)
        line_pct=$(echo "$gcov_out" | grep -A1 "File.*${name}.c'" | grep "Lines executed" | head -1 | grep -oP '[0-9.]+' | head -1)
        branch_pct=$(echo "$gcov_out" | grep -A2 "File.*${name}.c'" | grep "Branches executed" | head -1 | grep -oP '[0-9.]+' | head -1)

        [ -z "$line_pct" ] && line_pct="?"
        [ -z "$branch_pct" ] && branch_pct="?"

        results+=("$(printf '%6s  %6s  %s/%s' "${line_pct}%" "${branch_pct}%" "$layer" "$name")")

        # Cleanup
        rm -rf "$build_dir"
    done
done

echo "=== Coverage Scan: $(echo "${#results[@]}") wheels ==="
echo ""
printf "  %6s  %6s  %s\n" "Lines" "Branch" "Wheel"
printf "  %6s  %6s  %s\n" "-----" "------" "-----"

# Sort by first field (line coverage percentage)
printf '%s\n' "${results[@]}" | sort -t'%' -k1 -n

if [ "$TOP" -gt 0 ]; then
    echo ""
    echo "(showing all; use head to limit)"
fi
