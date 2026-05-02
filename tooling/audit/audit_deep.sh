#!/bin/bash
# audit_deep.sh — 5-layer automated deep code audit
#
# Usage:
#   audit_deep.sh              # run all layers
#   audit_deep.sh --layer 1    # pattern grep only
#   audit_deep.sh --layer 2    # sanitizer only
#   audit_deep.sh --layer 3    # fuzz only
#   audit_deep.sh --layer 4    # stress only
#   audit_deep.sh --quick      # layers 1+2 only (fastest)

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO"

BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

LAYER_FILTER=""
QUICK=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --layer)  LAYER_FILTER="$2"; shift 2 ;;
        --quick)  QUICK=true; shift ;;
        -h|--help) echo "Usage: audit_deep.sh [--layer N] [--quick]"; exit 0 ;;
        *)        echo "Unknown arg: $1"; exit 1 ;;
    esac
done

TOTAL_ISSUES=0
LAYER_ISSUES=()

run_layer() {
    local num="$1" name="$2"
    if [[ -n "$LAYER_FILTER" && "$LAYER_FILTER" != "$num" ]]; then
        return 0
    fi
    if $QUICK && [[ "$num" -gt 2 ]]; then
        return 0
    fi
    echo -e "\n${BOLD}=== Layer $num: $name ===${NC}\n"
}

# ============================================================
# Layer 1: Pattern grep (static analysis via regex)
# ============================================================

layer1_issues=0

scan_pattern() {
    local label="$1" pattern="$2" glob="$3"
    local files
    files=$(grep -rlP "$pattern" --include="$glob" wheels/ 2>/dev/null | grep -v vendor/ | grep -v test/ || true)
    if [[ -n "$files" ]]; then
        local count
        count=$(echo "$files" | wc -l)
        echo -e "  ${YELLOW}WARN${NC}  $label ($count files)"
        echo "$files" | while read -r f; do
            grep -nP "$pattern" "$f" 2>/dev/null | head -3 | while read -r line; do
                echo "         $f:$line"
            done
        done
        layer1_issues=$((layer1_issues + count))
    else
        echo -e "  ${GREEN} ok ${NC}  $label"
    fi
}

if run_layer 1 "Pattern grep (static)"; then
    # NaN bypass: float < min || float > max (without negated-positive guard)
    # This catches the explicit < || > pattern but may have false positives
    scan_pattern "NaN bypass: < || > on float" \
        '\b\w+\s*<\s*[-\d.]+f?\s*\|\|\s*\w+\s*>\s*[-\d.]+f' "*.c"

    # INT_MIN negation: (uint32_t)(-var) is safe, but bare (-var) on int32_t is UB
    scan_pattern "INT_MIN negation: bare -(int var)" \
        '(?<!\(uint32_t\))\(-\w+\)' "*.c"

    # Signed left shift (C99 UB for negative values)
    scan_pattern "Signed left shift" \
        '\bint(?:8|16|32|64)_t\s+\w+.*<<' "*.c"

    # float-to-int cast without range check
    scan_pattern "float→int cast: (uint16_t)float" \
        '\(u?int(?:8|16|32)_t\)\s*(?:val|duty|temp|pct|factor|alpha|gain|scale|freq)' "*.c"

    # strlen truncation to narrow type
    scan_pattern "strlen truncation: (uint16_t)strlen" \
        '\(uint(?:8|16)_t\)\s*strlen' "*.c"

    # size_t addition overflow before memcpy/read
    scan_pattern "Potential size_t add overflow before memcpy" \
        'memcpy\([^,]+,\s*[^,]+,\s*\w+\s*\+\s*\w+\)' "*.c"

    echo ""
    if [[ $layer1_issues -gt 0 ]]; then
        echo -e "  ${YELLOW}Layer 1: $layer1_issues potential issues${NC}"
        echo "  (Review each to determine if false positive or real bug)"
    else
        echo -e "  ${GREEN}Layer 1: clean${NC}"
    fi
    TOTAL_ISSUES=$((TOTAL_ISSUES + layer1_issues))
fi

# ============================================================
# Layer 2: Sanitizer build + run
# ============================================================

if run_layer 2 "Sanitizer (-fsanitize=address,undefined -O2)"; then
    layer2_pass=0
    layer2_fail=0

    for cmake_file in wheels/*/*/test/CMakeLists.txt; do
        wheel_dir="$(dirname "$(dirname "$cmake_file")")"
        name="$(basename "$wheel_dir")"
        test_dir="$wheel_dir/test"

        # Skip vendor wheels
        [[ -d "$wheel_dir/vendor" ]] && continue

        build_dir="$test_dir/build_san"
        rm -rf "$build_dir"

        # Build with sanitizers + O2
        if cmake -GNinja -S "$test_dir" -B "$build_dir" \
            -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -O2" \
            -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
            > /dev/null 2>&1 && \
           cmake --build "$build_dir" > /dev/null 2>&1; then

            # Run with timeout
            if timeout 30 "$build_dir/test_$name" > /dev/null 2>&1; then
                layer2_pass=$((layer2_pass + 1))
            else
                echo -e "  ${RED}FAIL${NC}  $name (sanitizer error or timeout)"
                layer2_fail=$((layer2_fail + 1))
            fi
        else
            echo -e "  ${YELLOW}SKIP${NC}  $name (build failed with sanitizers)"
        fi

        rm -rf "$build_dir"
    done

    echo ""
    echo -e "  Passed: $layer2_pass  Failed: $layer2_fail"
    if [[ $layer2_fail -gt 0 ]]; then
        echo -e "  ${RED}Layer 2: $layer2_fail failures${NC}"
    else
        echo -e "  ${GREEN}Layer 2: all clean${NC}"
    fi
    TOTAL_ISSUES=$((TOTAL_ISSUES + layer2_fail))
fi

# ============================================================
# Layer 3: Fuzz (L2 protocols)
# ============================================================

if run_layer 3 "Fuzz (L2 protocol parsers)"; then
    if [[ -x "$REPO/tooling/test/run_fuzz_protocols.sh" ]]; then
        bash "$REPO/tooling/test/run_fuzz_protocols.sh" 2>&1 | tail -5
    else
        echo -e "  ${YELLOW}SKIP${NC}  run_fuzz_protocols.sh not found"
    fi
fi

# ============================================================
# Layer 4: Stress tests (long-running edge cases)
# ============================================================

if run_layer 4 "Stress tests (accumulation, overflow, saturation)"; then
    layer4_pass=0
    layer4_fail=0

    for stress_file in wheels/*/*/test/stress_*.c; do
        [[ -f "$stress_file" ]] || continue
        wheel_dir="$(dirname "$(dirname "$stress_file")")"
        name="$(basename "$stress_file" .c)"
        test_dir="$wheel_dir/test"

        # Build stress test standalone
        out="/tmp/stress_$$_$name"
        if gcc -std=c99 -O2 -Wall -Wextra \
            -I"$wheel_dir" -I"$REPO/tooling/test" \
            "$stress_file" "$wheel_dir/$(basename "$wheel_dir").c" \
            -lm -o "$out" 2>/dev/null; then
            if timeout 60 "$out" > /dev/null 2>&1; then
                layer4_pass=$((layer4_pass + 1))
                echo -e "  ${GREEN} ok ${NC}  $name"
            else
                echo -e "  ${RED}FAIL${NC}  $name"
                layer4_fail=$((layer4_fail + 1))
            fi
            rm -f "$out"
        else
            echo -e "  ${YELLOW}SKIP${NC}  $name (build failed)"
        fi
    done

    if [[ $layer4_pass -eq 0 && $layer4_fail -eq 0 ]]; then
        echo -e "  ${YELLOW}No stress tests found (wheels/*/test/stress_*.c)${NC}"
    else
        echo ""
        echo -e "  Passed: $layer4_pass  Failed: $layer4_fail"
    fi
    TOTAL_ISSUES=$((TOTAL_ISSUES + layer4_fail))
fi

# ============================================================
# Summary
# ============================================================

echo -e "\n${BOLD}=== Deep Audit Summary ===${NC}"
echo -e "  Total issues: $TOTAL_ISSUES"
if [[ $TOTAL_ISSUES -eq 0 ]]; then
    echo -e "  ${GREEN}All layers clean.${NC}"
else
    echo -e "  ${RED}Review issues above.${NC}"
fi
exit $TOTAL_ISSUES
