#!/usr/bin/env bash
# run_generated_tests.sh — compile and run generated test files (test_null_*, test_sentinel_*, etc.)
#
# Usage:
#   run_generated_tests.sh <pattern> <label> [options...]
#
# Options:
#   --extra-cflags=FLAGS    additional compiler flags (e.g. "-Wextra")
#   --define=MACRO          add -D<MACRO> to compilation
#   --filter-contract       only test wheels whose .c includes contract.h
#   --use-main-test         use test_<name>.c (not generated) as test source
#
# Examples:
#   run_generated_tests.sh "test_null_*.c" "NULL parameter"
#   run_generated_tests.sh "prop_*.c" "property" --extra-cflags="-Wextra"
#   run_generated_tests.sh "test_*.c" "contract" --define=CONTRACT_ENABLED --filter-contract --use-main-test

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

PATTERN="${1:?Usage: $0 <pattern> <label> [options...]}"
LABEL="${2:?Usage: $0 <pattern> <label> [options...]}"
shift 2

EXTRA_CFLAGS=""
DEFINES=""
FILTER_CONTRACT=0
USE_MAIN_TEST=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --extra-cflags=*) EXTRA_CFLAGS="${1#*=}"; shift ;;
        --define=*)       DEFINES="$DEFINES -D${1#*=}"; shift ;;
        --filter-contract) FILTER_CONTRACT=1; shift ;;
        --use-main-test)  USE_MAIN_TEST=1; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Exclude patterns for contract mode
EXCLUDE_NAMES="test_null_|test_sentinel_|test_boundary_|test_hw_model_"

echo "Running $LABEL tests ($PATTERN)..."

fail=0
total=0

# Cleanup temp executables on exit
trap 'rm -f /tmp/gentest_*_$$' EXIT

while IFS= read -r f; do
    dir=$(dirname "$f")
    wheel_dir=$(dirname "$dir")
    name=$(basename "$wheel_dir")
    src="$wheel_dir/$name.c"

    # Skip if source doesn't exist
    [ -f "$src" ] || continue

    # In use-main-test mode, skip generated tests
    if [[ $USE_MAIN_TEST -eq 1 ]]; then
        basename_f=$(basename "$f")
        if echo "$basename_f" | grep -qE "$EXCLUDE_NAMES"; then
            continue
        fi
    fi

    # Contract filter: only wheels that include contract.h
    if [[ $FILTER_CONTRACT -eq 1 ]]; then
        grep -q "contract.h" "$src" 2>/dev/null || continue
    fi

    total=$((total + 1))

    # Resolve cross-wheel dependencies
    dep_srcs=""
    dep_incs=""
    for inc in $(grep -oE '#include "[^"]+\.h"' "$src" 2>/dev/null | sed 's/^#include "//;s/\.h"$//' \
                 | grep -vw "contract\|string\|stdint\|stdbool\|stddef\|stdlib\|stdio\|math\|limits" | grep -v "^$name$"); do
        dep_c=$(find wheels/ -name "$inc.c" -not -path "*/test/*" 2>/dev/null | head -1)
        if [ -n "$dep_c" ]; then
            dep_srcs="$dep_srcs $dep_c"
            dep_incs="$dep_incs -I$(dirname "$dep_c")"
        fi
    done

    # Include test-local mock/helper files (mock_*.c)
    mock_srcs=""
    for mc in "$dir"/mock_*.c; do
        [ -f "$mc" ] && mock_srcs="$mock_srcs $mc"
    done

    # Compile
    out="/tmp/gentest_${name}_$$"
    # shellcheck disable=SC2086
    if gcc -std=c99 -Wall $EXTRA_CFLAGS $DEFINES \
           -I"$wheel_dir" -I"$dir" -I"tooling/harness" $dep_incs \
           "$f" "$src" $dep_srcs $mock_srcs \
           -o "$out" -lm 2>/dev/null; then
        # Run
        if "$out" >/dev/null 2>&1; then
            printf "  PASS %s\n" "$name"
        else
            printf "  FAIL %s\n" "$name"
            fail=$((fail + 1))
        fi
        rm -f "$out"
    else
        printf "  BUILD_FAIL %s\n" "$name"
        fail=$((fail + 1))
    fi
done < <(find wheels/ -name "$PATTERN" 2>/dev/null | sort)

echo ""
echo "$total wheels, $fail failures"
[ $fail -eq 0 ]
