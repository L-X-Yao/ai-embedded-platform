#!/bin/bash
# arm_size_audit.sh — cross-compile every wheel with arm-none-eabi-gcc and report sizes.
#
# Usage:
#   arm_size_audit.sh                # table to stdout
#   arm_size_audit.sh --csv          # CSV to stdout
#   arm_size_audit.sh --markdown     # Markdown table to stdout
#
# Compiles each wheel's .c file(s) with:
#   arm-none-eabi-gcc -Os -mcpu=cortex-m4 -mthumb -ffreestanding -c
# Then runs arm-none-eabi-size on each .o to get .text/.data/.bss.
#
# Vendor wheels (fal, kvs) compile vendor/src/*.c instead.
# Skips wheels that have no .c files at wheel root.
#
# Exit 0 if every wheel compiled; 1 if any failed or none compiled.

set -euo pipefail

# shellcheck source=_arm_audit_common.sh
source "$(dirname "$0")/_arm_audit_common.sh"

REPO="$ARM_AUDIT_REPO"
CC="$ARM_AUDIT_CC"
SIZE="arm-none-eabi-size"
CFLAGS="-Os -mcpu=cortex-m4 -mthumb -ffreestanding -std=c99 -c"
CFLAGS="$CFLAGS -Wall -Wextra -Werror"
CFLAGS="$CFLAGS -ffunction-sections -fdata-sections"

FORMAT="table"
[[ "${1:-}" == "--csv" ]] && FORMAT="csv"
[[ "${1:-}" == "--markdown" ]] && FORMAT="markdown"

arm_audit_check_toolchain

AUDIT_TMPDIR=$(mktemp -d)
trap 'rm -rf "$AUDIT_TMPDIR"' EXIT

# Generate config stubs for wheels that need compile-time config headers
mkdir -p "$AUDIT_TMPDIR/stubs"
cat > "$AUDIT_TMPDIR/stubs/fal_cfg.h" <<'STUB'
#ifndef FAL_CFG_H
#define FAL_CFG_H
#include "fal_def.h"
extern const struct fal_flash_dev _stub_flash;
#define FAL_FLASH_DEV_TABLE { &_stub_flash }
#define FAL_PART_TABLE { {"stub", "stub", 0, 4096} }
#endif
STUB
cat > "$AUDIT_TMPDIR/stubs/fdb_cfg.h" <<'STUB'
#ifndef _FDB_CFG_H_
#define _FDB_CFG_H_
#define FDB_USING_FAL_MODE
#define FDB_USING_KVDB
#define FDB_PRINT(...)
#endif
STUB

# Header
case "$FORMAT" in
    csv)      echo "wheel,layer,text,data,bss,total" ;;
    markdown) echo "| Wheel | Layer | .text | .data | .bss | Total |"
              echo "|-------|-------|------:|------:|-----:|------:|" ;;
    table)    printf "%-25s %-12s %8s %8s %8s %8s\n" "WHEEL" "LAYER" ".text" ".data" ".bss" "TOTAL" ;;
esac

TOTAL_TEXT=0
TOTAL_DATA=0
TOTAL_BSS=0
WHEEL_COUNT=0
SKIP_COUNT=0
FAIL_COUNT=0

for wheel_dir in "$REPO"/wheels/*/*/; do
    [[ ! -d "$wheel_dir" ]] && continue
    name=$(basename "$wheel_dir")
    layer=$(basename "$(dirname "$wheel_dir")")

    [[ "$name" == "__pycache__" ]] && continue
    [[ "$name" == "README.md" ]] && continue

    # Find source files (include all — size audit measures everything)
    sources=()
    if ! discover_wheel_sources "$wheel_dir" "all"; then
        SKIP_COUNT=$((SKIP_COUNT + 1))
        continue
    fi

    # Determine include paths (extra: config stubs dir)
    INCLUDES=$(resolve_includes "$wheel_dir" "$AUDIT_TMPDIR/stubs")

    # Compile each source (vendor sources get -Wno-error)
    obj_files=()
    build_ok=true
    for src in "${sources[@]}"; do
        obj="$AUDIT_TMPDIR/$(basename "$src" .c).o"
        src_flags="$CFLAGS"
        if [[ "$src" == *"/vendor/"* ]]; then
            src_flags="${src_flags/-Werror/}"
        fi
        if $CC $src_flags $INCLUDES -o "$obj" "$src" 2>/dev/null; then
            obj_files+=("$obj")
        else
            build_ok=false
            break
        fi
    done

    if ! $build_ok || [[ ${#obj_files[@]} -eq 0 ]]; then
        FAIL_COUNT=$((FAIL_COUNT + 1))
        case "$FORMAT" in
            csv)      echo "$name,$layer,FAIL,FAIL,FAIL,FAIL" ;;
            markdown) echo "| $name | $layer | FAIL | FAIL | FAIL | FAIL |" ;;
            table)    printf "%-25s %-12s %8s %8s %8s %8s\n" "$name" "$layer" "FAIL" "" "" "" ;;
        esac
        continue
    fi

    # Sum sizes across all .o files for this wheel
    w_text=0; w_data=0; w_bss=0
    for obj in "${obj_files[@]}"; do
        read -r t d b _ _ <<< "$($SIZE -d "$obj" | tail -1)"
        w_text=$((w_text + t))
        w_data=$((w_data + d))
        w_bss=$((w_bss + b))
    done
    w_total=$((w_text + w_data + w_bss))

    TOTAL_TEXT=$((TOTAL_TEXT + w_text))
    TOTAL_DATA=$((TOTAL_DATA + w_data))
    TOTAL_BSS=$((TOTAL_BSS + w_bss))
    WHEEL_COUNT=$((WHEEL_COUNT + 1))

    case "$FORMAT" in
        csv)      echo "$name,$layer,$w_text,$w_data,$w_bss,$w_total" ;;
        markdown) printf "| %-20s | %-12s | %5d | %5d | %4d | %5d |\n" "$name" "$layer" "$w_text" "$w_data" "$w_bss" "$w_total" ;;
        table)    printf "%-25s %-12s %8d %8d %8d %8d\n" "$name" "$layer" "$w_text" "$w_data" "$w_bss" "$w_total" ;;
    esac

    # Clean up for next wheel
    rm -f "$AUDIT_TMPDIR"/*.o
done

# Footer
GRAND=$((TOTAL_TEXT + TOTAL_DATA + TOTAL_BSS))
echo ""
case "$FORMAT" in
    csv)      echo "TOTAL,,$TOTAL_TEXT,$TOTAL_DATA,$TOTAL_BSS,$GRAND" ;;
    markdown) printf "| **TOTAL** | | **%d** | **%d** | **%d** | **%d** |\n" "$TOTAL_TEXT" "$TOTAL_DATA" "$TOTAL_BSS" "$GRAND" ;;
    table)    printf "%-25s %-12s %8d %8d %8d %8d\n" "TOTAL" "" "$TOTAL_TEXT" "$TOTAL_DATA" "$TOTAL_BSS" "$GRAND" ;;
esac
echo ""
echo "Compiled: $WHEEL_COUNT  Skipped: $SKIP_COUNT  Failed: $FAIL_COUNT"

if [[ $FAIL_COUNT -gt 0 || $WHEEL_COUNT -eq 0 ]]; then
    exit 1
fi
