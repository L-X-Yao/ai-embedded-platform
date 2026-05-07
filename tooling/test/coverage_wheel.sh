#!/bin/bash
# coverage_wheel.sh — per-wheel gcov coverage report with branch coverage
#
# Usage:
#   coverage_wheel.sh <wheel-name>         # e.g. soft_i2c, flash_ring
#   coverage_wheel.sh drivers/air780eg     # layer/name also accepted
#   coverage_wheel.sh --machine <name>     # machine-readable: prints "LINE_PCT BRANCH_PCT"
#
# Output: line coverage %, branch coverage %, list of uncovered lines.
# Exit code: 0 always (informational tool, not a gate).
#
# Performance: on WSL with /mnt/ sources, builds in /tmp/ (native ext4)
# for ~5-10x speedup over cross-filesystem builds.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
MACHINE=0

if [[ "${1:-}" == "--machine" ]]; then
    MACHINE=1
    shift
fi

if [[ $# -lt 1 ]]; then
    echo "Usage: coverage_wheel.sh [--machine] <wheel-name|layer/name>"
    exit 1
fi

INPUT="$1"

# Resolve wheel directory
if [[ "$INPUT" == */* ]]; then
    WHEEL_DIR="$REPO/wheels/$INPUT"
    NAME=$(basename "$INPUT")
else
    NAME="$INPUT"
    WHEEL_DIR=$(find "$REPO/wheels" -maxdepth 2 -type d -name "$NAME" | head -1)
fi

if [[ -z "$WHEEL_DIR" || ! -d "$WHEEL_DIR" ]]; then
    echo "Error: wheel '$INPUT' not found"
    exit 1
fi

TEST_DIR="$WHEEL_DIR/test"
if [[ ! -d "$TEST_DIR" ]]; then
    echo "Error: no test/ directory in $WHEEL_DIR"
    exit 1
fi

# On WSL with /mnt/ paths, put build dir on native ext4 for ~5x speedup.
# Source stays on /mnt/ (read once); build artifacts go to /tmp/ (heavy I/O).
if [[ "$REPO" == /mnt/* ]] && [[ -d /tmp ]]; then
    BUILD_DIR="/tmp/cov_$$_${NAME}"
    trap 'rm -rf "$BUILD_DIR" 2>/dev/null || true' EXIT
else
    BUILD_DIR="$TEST_DIR/build_cov"
fi

[[ $MACHINE -eq 0 ]] && echo "=== Coverage: $NAME ===" && echo ""

# Clean previous coverage data
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Configure with coverage flags
if ! cmake -GNinja -S "$TEST_DIR" -B "$BUILD_DIR" \
    -DCMAKE_C_FLAGS="--coverage -O0 -g" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
    > "$BUILD_DIR/cmake.log" 2>&1; then
    [[ $MACHINE -eq 1 ]] && echo "BUILD_FAIL" && exit 0
    echo "Error: cmake configure failed"
    cat "$BUILD_DIR/cmake.log"
    exit 1
fi

# Build
if ! cmake --build "$BUILD_DIR" > "$BUILD_DIR/build.log" 2>&1; then
    [[ $MACHINE -eq 1 ]] && echo "BUILD_FAIL" && exit 0
    echo "Error: build failed"
    cat "$BUILD_DIR/build.log"
    exit 1
fi

# Find and run all test executables
EXES=$(find "$BUILD_DIR" -maxdepth 1 -type f -executable 2>/dev/null || true)
if [[ -z "$EXES" ]]; then
    [[ $MACHINE -eq 1 ]] && echo "NO_EXES" && exit 0
    echo "Error: no test executables found"
    exit 1
fi

[[ $MACHINE -eq 0 ]] && echo "Running tests..."
for exe in $EXES; do
    "$exe" > /dev/null 2>&1 || true
done

# Find ALL .gcda files for the wheel source (one per cmake target)
mapfile -t GCDA_FILES < <(find "$BUILD_DIR" -name "${NAME}.c.gcda" 2>/dev/null)

if [[ ${#GCDA_FILES[@]} -eq 0 ]]; then
    [[ $MACHINE -eq 1 ]] && echo "NO_GCDA" && exit 0
    echo "  No .gcda file found for ${NAME}.c"
    exit 0
fi

# Merge all gcda directories if multiple targets exist
MERGED_DIR="$BUILD_DIR/_merged"
if [[ ${#GCDA_FILES[@]} -gt 1 ]]; then
    # Start with the first gcda's directory (has .gcno + .gcda)
    FIRST_DIR=$(dirname "${GCDA_FILES[0]}")
    cp -r "$FIRST_DIR" "$MERGED_DIR"

    # Merge remaining gcda directories into it
    for ((i=1; i<${#GCDA_FILES[@]}; i++)); do
        OTHER_DIR=$(dirname "${GCDA_FILES[$i]}")
        TEMP_MERGED="$BUILD_DIR/_merge_tmp"
        if gcov-tool merge "$MERGED_DIR" "$OTHER_DIR" -o "$TEMP_MERGED" 2>/dev/null; then
            # gcov-tool merge only outputs .gcda; copy .gcno from original
            cp "$MERGED_DIR"/*.gcno "$TEMP_MERGED/" 2>/dev/null || true
            rm -rf "$MERGED_DIR"
            mv "$TEMP_MERGED" "$MERGED_DIR"
        else
            rm -rf "$TEMP_MERGED" 2>/dev/null || true
        fi
    done
    GCDA_DIR="$MERGED_DIR"
else
    GCDA_DIR=$(dirname "${GCDA_FILES[0]}")
fi

# Run gcov with branch coverage
cd "$GCDA_DIR"
GCOV_OUT=$(gcov -b "${NAME}.c.gcda" 2>&1 || true)
cd "$BUILD_DIR"

# Extract summary
LINE_INFO=$(echo "$GCOV_OUT" | grep -A1 "File.*${NAME}.c'" | grep "Lines executed" | head -1)
BRANCH_INFO=$(echo "$GCOV_OUT" | grep -A2 "File.*${NAME}.c'" | grep "Branches executed" | head -1)

if [[ $MACHINE -eq 1 ]]; then
    LINE_PCT=$(echo "$LINE_INFO" | sed -n 's/.*Lines executed:\([0-9.]*\)%.*/\1/p')
    BRANCH_PCT=$(echo "$BRANCH_INFO" | sed -n 's/.*Branches executed:\([0-9.]*\)%.*/\1/p')
    echo "${LINE_PCT:-0} ${BRANCH_PCT:-0}"
    exit 0
fi

echo ""
echo "--- Results ---"
if [[ -n "$LINE_INFO" ]]; then
    echo "  $LINE_INFO"
else
    echo "  Lines: no data"
fi
if [[ -n "$BRANCH_INFO" ]]; then
    echo "  $BRANCH_INFO"
else
    echo "  Branches: no data"
fi

# Show uncovered lines from .gcov file
GCOV_FILE=$(find "$GCDA_DIR" -name "*${NAME}.c.gcov" 2>/dev/null | head -1 || true)
if [[ -n "$GCOV_FILE" && -f "$GCOV_FILE" ]]; then
    MISSED=$(grep -c '^\s*#####:' "$GCOV_FILE" 2>/dev/null || echo 0)
    if [[ $MISSED -gt 0 ]]; then
        echo ""
        echo "--- Uncovered Lines ($MISSED) ---"
        grep '^\s*#####:' "$GCOV_FILE" | head -20 | while IFS=: read -r _ lnum src; do
            printf "  L%-4s %s\n" "$(echo "$lnum" | tr -d ' ')" "$src"
        done
        if [[ $MISSED -gt 20 ]]; then
            echo "  ... and $((MISSED - 20)) more"
        fi
        echo ""
        echo "  Detail: $GCOV_FILE"
    else
        echo ""
        echo "  All lines covered!"
    fi
fi
