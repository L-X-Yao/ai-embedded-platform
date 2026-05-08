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

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
TOOLING_TEST="$REPO/tooling/test"

CC="arm-none-eabi-gcc"
SIZE="arm-none-eabi-size"
CFLAGS="-Os -mcpu=cortex-m4 -mthumb -ffreestanding -std=c99 -c"
CFLAGS="$CFLAGS -Wall -Wextra -Werror"
CFLAGS="$CFLAGS -ffunction-sections -fdata-sections"

FORMAT="table"
[[ "${1:-}" == "--csv" ]] && FORMAT="csv"
[[ "${1:-}" == "--markdown" ]] && FORMAT="markdown"

if ! command -v "$CC" &>/dev/null; then
    echo "ERROR: $CC not found in PATH" >&2
    exit 1
fi

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

# Generate config stubs for wheels that need compile-time config headers
mkdir -p "$TMPDIR/stubs"
cat > "$TMPDIR/stubs/fal_cfg.h" <<'STUB'
#ifndef FAL_CFG_H
#define FAL_CFG_H
#include "fal_def.h"
extern const struct fal_flash_dev _stub_flash;
#define FAL_FLASH_DEV_TABLE { &_stub_flash }
#define FAL_PART_TABLE { {"stub", "stub", 0, 4096} }
#endif
STUB
cat > "$TMPDIR/stubs/fdb_cfg.h" <<'STUB'
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

    # Find source files
    sources=()
    if [[ -d "$wheel_dir/vendor/src" ]]; then
        while IFS= read -r f; do sources+=("$f"); done < <(find "$wheel_dir/vendor/src" -name '*.c' 2>/dev/null)
    fi
    while IFS= read -r f; do
        sources+=("$f")
    done < <(find "$wheel_dir" -maxdepth 1 -name '*.c' 2>/dev/null)

    if [[ ${#sources[@]} -eq 0 ]]; then
        SKIP_COUNT=$((SKIP_COUNT + 1))
        continue
    fi

    # Determine include paths
    INCLUDES="-I$wheel_dir -I$TOOLING_TEST -I$TMPDIR/stubs"
    if [[ -d "$wheel_dir/vendor/inc" ]]; then
        INCLUDES="$INCLUDES -I$wheel_dir/vendor/inc"
    fi
    # Cross-wheel dependencies: auto-discover from CMakeLists.txt
    if [[ -f "$wheel_dir/CMakeLists.txt" ]]; then
        while IFS= read -r dep_name; do
            [[ -z "$dep_name" ]] && continue
            dep_dir=$(find "$REPO/wheels" -maxdepth 3 -type d -name "$dep_name" 2>/dev/null | head -1)
            [[ -n "$dep_dir" ]] && INCLUDES="$INCLUDES -I$dep_dir"
        done < <(grep -oE '\.\./\.\./[a-z_]+/[a-z_]+' "$wheel_dir/CMakeLists.txt" 2>/dev/null \
            | sed 's|.*/||' | sort -u || true)
    fi
    # Vendor sub-deps: kvs needs fal headers
    if [[ -d "$wheel_dir/vendor/inc" ]] && grep -rq '#include.*fal\.h' "$wheel_dir/vendor" 2>/dev/null; then
        INCLUDES="$INCLUDES -I$REPO/wheels/primitives/fal"
    fi

    # Compile each source (vendor sources get -Wno-error)
    obj_files=()
    build_ok=true
    for src in "${sources[@]}"; do
        obj="$TMPDIR/$(basename "$src" .c).o"
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
    rm -f "$TMPDIR"/*.o
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
