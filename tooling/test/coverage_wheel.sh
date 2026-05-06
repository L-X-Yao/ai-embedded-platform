#!/bin/bash
# coverage_wheel.sh — per-wheel gcov coverage report with branch coverage
#
# Usage:
#   coverage_wheel.sh <wheel-name>         # e.g. soft_i2c, flash_ring
#   coverage_wheel.sh drivers/air780eg     # layer/name also accepted
#
# Output: line coverage %, branch coverage %, list of uncovered lines.
# Exit code: 0 always (informational tool, not a gate).

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"

if [[ $# -lt 1 ]]; then
    echo "Usage: coverage_wheel.sh <wheel-name|layer/name>"
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

BUILD_DIR="$TEST_DIR/build_cov"

echo "=== Coverage: $NAME ==="
echo ""

# Clean previous coverage data
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Configure with coverage flags
if ! cmake -GNinja -S "$TEST_DIR" -B "$BUILD_DIR" \
    -DCMAKE_C_FLAGS="--coverage -O0 -g" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
    > "$BUILD_DIR/cmake.log" 2>&1; then
    echo "Error: cmake configure failed"
    cat "$BUILD_DIR/cmake.log"
    exit 1
fi

# Build
if ! cmake --build "$BUILD_DIR" > "$BUILD_DIR/build.log" 2>&1; then
    echo "Error: build failed"
    cat "$BUILD_DIR/build.log"
    exit 1
fi

# Find and run all test executables
EXES=$(find "$BUILD_DIR" -maxdepth 1 -type f -executable 2>/dev/null || true)
if [[ -z "$EXES" ]]; then
    echo "Error: no test executables found"
    exit 1
fi

echo "Running tests..."
for exe in $EXES; do
    "$exe" > /dev/null 2>&1 || true
done

# Collect coverage for the wheel source file only (not test files)
SRC_FILE="$WHEEL_DIR/${NAME}.c"
if [[ ! -f "$SRC_FILE" ]]; then
    echo "Warning: $SRC_FILE not found, scanning all .gcda"
    SRC_FILE=""
fi

echo ""

# Find .gcda for the wheel source
GCDA_FILE=$(find "$BUILD_DIR" -name "${NAME}.c.gcda" 2>/dev/null | head -1)
if [[ -z "$GCDA_FILE" ]]; then
    echo "  No .gcda file found for ${NAME}.c"
    exit 0
fi

GCDA_DIR=$(dirname "$GCDA_FILE")

# Run gcov with branch coverage from the .gcda directory
cd "$GCDA_DIR"
GCOV_OUT=$(gcov -b "${NAME}.c.gcda" 2>&1 || true)
cd "$BUILD_DIR"

# Extract summary from gcov stdout
LINE_INFO=$(echo "$GCOV_OUT" | grep -A1 "File.*${NAME}.c'" | grep "Lines executed" | head -1)
BRANCH_INFO=$(echo "$GCOV_OUT" | grep -A2 "File.*${NAME}.c'" | grep "Branches executed" | head -1)

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
