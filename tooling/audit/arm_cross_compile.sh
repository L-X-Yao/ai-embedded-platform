#!/usr/bin/env bash
# ARM Cortex-M cross-compilation verification for all self-written wheels
# Usage: bash tooling/audit/arm_cross_compile.sh
# Skips: fal, kvs (vendor wheels), test_*.c files, _draft/ wheels

set -euo pipefail

REPO="/mnt/d/temp/project/ai-embedded-platform"
CC="arm-none-eabi-gcc"
CFLAGS="-c -mcpu=cortex-m4 -mthumb -std=c99 -Wall -Wextra -Wconversion -Werror -ffreestanding -nostdlib"
SKIP_WHEELS="fal kvs"
LAYERS="primitives drivers protocols modules"

# Output file for results
RESULTS_FILE="/tmp/arm_cross_compile_results.txt"
> "$RESULTS_FILE"

PASS=0
FAIL=0
TOTAL=0
FAIL_LIST=""

# --- Phase 1: Build dependency map ---
# For each wheel, find which other wheels it includes
declare -A WHEEL_DIR_MAP  # header_name -> wheel_dir
declare -A WHEEL_DEPS     # wheel_name -> space-separated list of dep dirs

# First, build a map of header_name -> wheel_dir
for layer in $LAYERS; do
    wheel_base="$REPO/wheels/$layer"
    [ -d "$wheel_base" ] || continue
    for wheel_dir in "$wheel_base"/*/; do
        [ -d "$wheel_dir" ] || continue
        name=$(basename "$wheel_dir")
        # Skip non-wheel dirs
        [[ "$name" == "README.md" ]] && continue
        # Map each .h file in this wheel
        for hdr in "$wheel_dir"*.h; do
            [ -f "$hdr" ] || continue
            hdr_name=$(basename "$hdr")
            WHEEL_DIR_MAP["$hdr_name"]="$wheel_dir"
        done
    done
done

# Now, for each wheel, scan its .c and .h files for cross-wheel includes
for layer in $LAYERS; do
    wheel_base="$REPO/wheels/$layer"
    [ -d "$wheel_base" ] || continue
    for wheel_dir in "$wheel_base"/*/; do
        [ -d "$wheel_dir" ] || continue
        name=$(basename "$wheel_dir")
        [[ "$name" == "README.md" ]] && continue

        deps=""
        for src in "$wheel_dir"*.c "$wheel_dir"*.h; do
            [ -f "$src" ] || continue
            src_base=$(basename "$src")
            # Skip test files
            [[ "$src_base" == test_* ]] && continue

            # Find #include "xxx.h" lines
            while IFS= read -r inc_hdr; do
                # Skip self-includes
                [[ "$inc_hdr" == "${name}."* ]] && continue
                # Skip test/mock headers
                [[ "$inc_hdr" == test_* ]] && continue
                [[ "$inc_hdr" == mock_* ]] && continue

                # Look up the wheel dir for this header
                if [[ -n "${WHEEL_DIR_MAP[$inc_hdr]+x}" ]]; then
                    dep_dir="${WHEEL_DIR_MAP[$inc_hdr]}"
                    # Avoid duplicates
                    if [[ "$deps" != *"$dep_dir"* ]]; then
                        deps="$deps -I$dep_dir"
                    fi
                fi
            done < <(grep -oh '#include "[^"]*"' "$src" 2>/dev/null | sed 's/#include "//;s/"//' || true)
        done
        WHEEL_DEPS["$layer/$name"]="$deps"
    done
done

echo "=== ARM Cortex-M4 Cross-Compilation Verification ==="
echo "Compiler: $($CC --version | head -1)"
echo "Flags: $CFLAGS"
echo "=================================================="
echo ""

# --- Phase 2: Compile each wheel ---
for layer in $LAYERS; do
    wheel_base="$REPO/wheels/$layer"
    [ -d "$wheel_base" ] || continue

    echo "--- Layer: $layer ---"

    for wheel_dir in "$wheel_base"/*/; do
        [ -d "$wheel_dir" ] || continue
        name=$(basename "$wheel_dir")

        # Skip non-wheel dirs and vendor wheels
        [[ "$name" == "README.md" ]] && continue
        skip=0
        for s in $SKIP_WHEELS; do
            [[ "$name" == "$s" ]] && skip=1
        done
        [[ $skip -eq 1 ]] && { echo "  SKIP  $layer/$name (vendor)"; continue; }

        # Find non-test .c files
        c_files=()
        for f in "$wheel_dir"*.c; do
            [ -f "$f" ] || continue
            fb=$(basename "$f")
            [[ "$fb" == test_* ]] && continue
            c_files+=("$f")
        done

        if [ ${#c_files[@]} -eq 0 ]; then
            echo "  SKIP  $layer/$name (no .c files)"
            continue
        fi

        # Get dependency include paths
        dep_flags="${WHEEL_DEPS[$layer/$name]:-}"

        # Compile each .c file
        for c_file in "${c_files[@]}"; do
            c_base=$(basename "$c_file")
            TOTAL=$((TOTAL + 1))

            # Build the command
            cmd="$CC $CFLAGS -I$wheel_dir -I$REPO/tooling/test $dep_flags $c_file -o /dev/null"

            # Run compilation
            output=$($cmd 2>&1) && rc=0 || rc=$?

            if [ $rc -eq 0 ]; then
                PASS=$((PASS + 1))
                echo "  PASS  $layer/$name/$c_base"
            else
                FAIL=$((FAIL + 1))
                echo "  FAIL  $layer/$name/$c_base"
                echo "$output" | head -30 | sed 's/^/        /'
                FAIL_LIST="$FAIL_LIST
=== FAIL: $layer/$name/$c_base ===
CMD: $cmd
$output
"
            fi
        done
    done
    echo ""
done

# --- Summary ---
echo "=================================================="
echo "SUMMARY: $TOTAL compiled, $PASS passed, $FAIL failed"
echo "=================================================="

if [ $FAIL -gt 0 ]; then
    echo ""
    echo "=== FAILURE DETAILS ==="
    echo "$FAIL_LIST"
fi

# Write results to file for later processing
cat > "$RESULTS_FILE" << ENDRESULTS
TOTAL=$TOTAL
PASS=$PASS
FAIL=$FAIL
$FAIL_LIST
ENDRESULTS

exit $FAIL
