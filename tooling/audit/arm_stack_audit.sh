#!/bin/bash
# arm_stack_audit.sh — cross-compile every wheel with -fstack-usage and report per-function stack.
#
# Usage:
#   arm_stack_audit.sh                    # table to stdout (threshold 256)
#   arm_stack_audit.sh --csv              # CSV to stdout
#   arm_stack_audit.sh --markdown         # Markdown table to stdout
#   arm_stack_audit.sh --threshold 128    # custom warning threshold (bytes)
#   arm_stack_audit.sh --csv --threshold 512
#
# Compiles each wheel's .c file(s) with:
#   arm-none-eabi-gcc -fstack-usage -Os -mcpu=cortex-m4 -mthumb -ffreestanding -std=c99 -c
# Then parses the generated .su files (format: file:line:col:function\tN\tstatic|dynamic).
#
# Vendor wheels (fal, kvs) compile vendor/src/*.c instead.
# Skips wheels that have no .c files at wheel root.
# Skips test files (test_*.c, prop_*.c, verify_*.c).

set -euo pipefail

# shellcheck source=_arm_audit_common.sh
source "$(dirname "$0")/_arm_audit_common.sh"

REPO="$ARM_AUDIT_REPO"
CC="$ARM_AUDIT_CC"
CFLAGS="-fstack-usage -Os -mcpu=cortex-m4 -mthumb -ffreestanding -std=c99 -c"
CFLAGS="$CFLAGS -Wall -Wextra"

FORMAT="table"
THRESHOLD=256

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --csv)       FORMAT="csv";      shift ;;
        --markdown)  FORMAT="markdown";  shift ;;
        --threshold) THRESHOLD="$2";     shift 2 ;;
        *)           echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

arm_audit_check_toolchain

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

# Collect all rows into arrays for sorting warnings to the end
declare -a ROW_WHEEL=()
declare -a ROW_FUNC=()
declare -a ROW_STACK=()
declare -a ROW_TYPE=()
declare -a ROW_WARN=()

WHEEL_COUNT=0
SKIP_COUNT=0
FAIL_COUNT=0
FUNC_COUNT=0
WARN_COUNT=0

for wheel_dir in "$REPO"/wheels/*/*/; do
    [[ ! -d "$wheel_dir" ]] && continue
    name=$(basename "$wheel_dir")
    layer=$(basename "$(dirname "$wheel_dir")")

    [[ "$name" == "__pycache__" ]] && continue
    [[ "$name" == "README.md" ]] && continue

    # Find source files (skip test/prop/verify files — stack audit measures production code only)
    sources=()
    if ! discover_wheel_sources "$wheel_dir" "skip_tests"; then
        SKIP_COUNT=$((SKIP_COUNT + 1))
        continue
    fi

    # Determine include paths
    INCLUDES=$(resolve_includes "$wheel_dir")

    # Compile each source (generates .su files alongside .o)
    build_ok=true
    su_files=()
    for src in "${sources[@]}"; do
        stem=$(basename "$src" .c)
        obj="$TMPDIR/${stem}.o"
        if $CC $CFLAGS $INCLUDES -o "$obj" "$src" 2>/dev/null; then
            su="$TMPDIR/${stem}.su"
            if [[ -f "$su" ]]; then
                su_files+=("$su")
            fi
        else
            build_ok=false
            break
        fi
    done

    if ! $build_ok; then
        FAIL_COUNT=$((FAIL_COUNT + 1))
        # Clean up for next wheel
        rm -f "$TMPDIR"/*.o "$TMPDIR"/*.su
        continue
    fi

    if [[ ${#su_files[@]} -eq 0 ]]; then
        SKIP_COUNT=$((SKIP_COUNT + 1))
        rm -f "$TMPDIR"/*.o "$TMPDIR"/*.su
        continue
    fi

    WHEEL_COUNT=$((WHEEL_COUNT + 1))

    # Parse .su files
    # Format: file:line:col:function\tN\tstatic|dynamic
    for su in "${su_files[@]}"; do
        while IFS=$'\t' read -r loc stack_size stack_type; do
            [[ -z "$loc" ]] && continue
            # Extract function name from loc (file:line:col:function)
            func="${loc##*:}"
            [[ -z "$func" ]] && continue

            warn=""
            if [[ "$stack_size" -gt "$THRESHOLD" ]] 2>/dev/null; then
                warn="WARNING"
                WARN_COUNT=$((WARN_COUNT + 1))
            fi

            ROW_WHEEL+=("$name")
            ROW_FUNC+=("$func")
            ROW_STACK+=("$stack_size")
            ROW_TYPE+=("$stack_type")
            ROW_WARN+=("$warn")
            FUNC_COUNT=$((FUNC_COUNT + 1))
        done < "$su"
    done

    # Clean up for next wheel
    rm -f "$TMPDIR"/*.o "$TMPDIR"/*.su
done

# Print header
case "$FORMAT" in
    csv)      echo "wheel,function,stack_bytes,type,warning" ;;
    markdown) echo "| Wheel | Function | Stack (bytes) | Type | Warning |"
              echo "|-------|----------|-------------:|------|---------|" ;;
    table)    printf "%-25s %-35s %14s  %-8s %s\n" "WHEEL" "FUNCTION" "STACK (BYTES)" "TYPE" ""
              printf "%-25s %-35s %14s  %-8s %s\n" "-----" "--------" "-------------" "----" "" ;;
esac

# Print rows: normal entries first, then warnings
for pass in normal warn; do
    for i in "${!ROW_WHEEL[@]}"; do
        if [[ "$pass" == "normal" && -n "${ROW_WARN[$i]}" ]]; then
            continue
        fi
        if [[ "$pass" == "warn" && -z "${ROW_WARN[$i]}" ]]; then
            continue
        fi

        case "$FORMAT" in
            csv)
                echo "${ROW_WHEEL[$i]},${ROW_FUNC[$i]},${ROW_STACK[$i]},${ROW_TYPE[$i]},${ROW_WARN[$i]}"
                ;;
            markdown)
                warn_marker=""
                [[ -n "${ROW_WARN[$i]}" ]] && warn_marker="**WARNING**"
                printf "| %-23s | %-33s | %12s | %-8s | %s |\n" \
                    "${ROW_WHEEL[$i]}" "${ROW_FUNC[$i]}" "${ROW_STACK[$i]}" "${ROW_TYPE[$i]}" "$warn_marker"
                ;;
            table)
                warn_marker=""
                [[ -n "${ROW_WARN[$i]}" ]] && warn_marker="<-- WARNING"
                printf "%-25s %-35s %14s  %-8s %s\n" \
                    "${ROW_WHEEL[$i]}" "${ROW_FUNC[$i]}" "${ROW_STACK[$i]}" "${ROW_TYPE[$i]}" "$warn_marker"
                ;;
        esac
    done
done

# Footer / summary
echo ""
case "$FORMAT" in
    csv)
        echo "# Wheels: $WHEEL_COUNT  Functions: $FUNC_COUNT  Skipped: $SKIP_COUNT  Failed: $FAIL_COUNT"
        echo "# Threshold: $THRESHOLD bytes  Warnings: $WARN_COUNT"
        ;;
    markdown)
        echo ""
        echo "**Summary:** $WHEEL_COUNT wheels, $FUNC_COUNT functions | Skipped: $SKIP_COUNT | Failed: $FAIL_COUNT"
        echo ""
        echo "**Threshold:** $THRESHOLD bytes | **Warnings:** $WARN_COUNT"
        ;;
    table)
        echo "Wheels: $WHEEL_COUNT  Functions: $FUNC_COUNT  Skipped: $SKIP_COUNT  Failed: $FAIL_COUNT"
        echo "Threshold: $THRESHOLD bytes  Warnings: $WARN_COUNT"
        ;;
esac

if [[ "$WARN_COUNT" -gt 0 ]]; then
    echo ""
    echo "Functions exceeding $THRESHOLD-byte threshold:"
    for i in "${!ROW_WHEEL[@]}"; do
        [[ -z "${ROW_WARN[$i]}" ]] && continue
        echo "  ${ROW_WHEEL[$i]}/${ROW_FUNC[$i]}: ${ROW_STACK[$i]} bytes (${ROW_TYPE[$i]})"
    done
fi
