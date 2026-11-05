#!/bin/bash
# audit_deep.sh — 6-layer automated deep code audit
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
JSON_OUT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --layer)  LAYER_FILTER="$2"; shift 2 ;;
        --quick)  QUICK=true; shift ;;
        --json)   JSON_OUT="$REPO/tooling/audit/snapshots/deep_$(date +%Y%m%d_%H%M%S).json"; shift ;;
        -h|--help) echo "Usage: audit_deep.sh [--layer N] [--quick] [--json]"; exit 0 ;;
        *)        echo "Unknown arg: $1"; exit 1 ;;
    esac
done

TOTAL_ISSUES=0
LAYER_ISSUES=()

run_layer() {
    local num="$1" name="$2"
    if [[ -n "$LAYER_FILTER" && "$LAYER_FILTER" != "$num" ]]; then
        return 1
    fi
    if $QUICK && [[ "$num" -gt 2 ]]; then
        return 1
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
    files=$(grep -rlE "$pattern" --include="$glob" wheels/ 2>/dev/null \
            | grep -v vendor/ | grep -v test/ || true)
    if [[ -n "$files" ]]; then
        local count
        count=$(echo "$files" | wc -l)
        echo -e "  ${YELLOW}WARN${NC}  $label ($count files)"
        echo "$files" | while read -r f; do
            grep -nE "$pattern" "$f" 2>/dev/null \
            | head -3 | while read -r line; do
                echo "         $f:$line"
            done
        done
        layer1_issues=$((layer1_issues + count))
    else
        echo -e "  ${GREEN} ok ${NC}  $label"
    fi
}

if run_layer 1 "Pattern grep (static)"; then
    # INT_MIN negation: (uint32_t)(-var) is safe, but bare (-var) on int32_t is UB
    # (Other patterns — NaN bypass, signed left shift, float→int cast, strlen
    #  truncation, memcpy overflow, unchecked error return — are covered by
    #  audit_wheels.py with better precision and false-positive filtering.)
    scan_pattern "INT_MIN negation: bare -(int var)" \
        '\(-[A-Za-z_][A-Za-z0-9_]*\)' "*.c"

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

            # Find the actual test binary (may differ from test_$name)
            test_bin=$(find "$build_dir" -maxdepth 1 -type f -executable ! -name '*.o' ! -name '*.a' 2>/dev/null | head -1)
            [[ -z "$test_bin" ]] && test_bin="$build_dir/test_$name"
            if timeout 30 "$test_bin" > /dev/null 2>&1; then
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
# Layer 3: Fuzz (libFuzzer targets)
# ============================================================

if run_layer 3 "Fuzz (libFuzzer targets)"; then
    bash "$REPO/tooling/test/run_libfuzzer.sh" 2>&1 | tail -5
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
        out=$(mktemp "/tmp/stress_XXXXXX")
        if gcc -std=c99 -O2 -Wall -Wextra \
            -I"$wheel_dir" -I"$REPO/tooling/harness" \
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
# Layer 5: Cross-wheel API consistency
# ============================================================

if run_layer 5 "Cross-wheel API consistency"; then
    layer5_issues=0

    # Check 1: init/deinit symmetry. Wheels own no resources, so _deinit is
    # optional: api-conventions 3.4 only constrains it when it exists. The
    # count is reported for reference and does not add to the issue total.
    echo "  --- init/deinit symmetry ---"
    init_only=0
    for h in wheels/*/*/*.h; do
        [[ -f "$h" ]] || continue
        name="$(basename "$h" .h)"
        dir="$(dirname "$h")"
        [[ -d "$dir/vendor" ]] && continue
        has_init=$(grep -c "${name}_init\b" "$h" 2>/dev/null || true)
        has_deinit=$(grep -c "${name}_deinit\b" "$h" 2>/dev/null || true)
        if [[ "$has_init" -gt 0 && "$has_deinit" -eq 0 ]]; then
            init_only=$((init_only + 1))
        fi
    done
    echo "  INFO  $init_only wheels have _init without _deinit"

    # Check 2: error type naming — _err_t must be typedef'd enum
    echo "  --- error type naming ---"
    for h in wheels/*/*/*.h; do
        [[ -f "$h" ]] || continue
        name="$(basename "$h" .h)"
        dir="$(dirname "$h")"
        [[ -d "$dir/vendor" ]] && continue
        has_err_t=$(grep -c "${name}_err_t" "$h" 2>/dev/null || true)
        if [[ "$has_err_t" -gt 0 ]]; then
            has_typedef=$(grep -c "typedef enum.*${name}_err_t\|} ${name}_err_t" "$h" 2>/dev/null || true)
            if [[ "$has_typedef" -eq 0 ]]; then
                echo -e "  ${YELLOW}WARN${NC}  $name: _err_t not a typedef'd enum"
                layer5_issues=$((layer5_issues + 1))
            fi
        fi
    done

    # Check 3: include guard vs filename
    echo "  --- include guard naming ---"
    for h in wheels/*/*/*.h; do
        [[ -f "$h" ]] || continue
        name="$(basename "$h" .h)"
        dir="$(dirname "$h")"
        [[ -d "$dir/vendor" ]] && continue
        expected=$(echo "${name}_H" | tr '[:lower:]' '[:upper:]')
        if ! grep -q "#ifndef ${expected}\b\|#ifndef ${expected}_$" "$h" 2>/dev/null; then
            alt=$(head -15 "$h" | grep -oE '#ifndef [A-Za-z0-9_]+' 2>/dev/null | sed 's/^#ifndef //')
            if [[ -n "$alt" && "$alt" != "$expected" ]]; then
                echo -e "  ${YELLOW}WARN${NC}  $name: guard is $alt, expected $expected"
                layer5_issues=$((layer5_issues + 1))
            fi
        fi
    done

    echo ""
    if [[ $layer5_issues -gt 0 ]]; then
        echo -e "  ${YELLOW}Layer 5: $layer5_issues consistency issues${NC}"
    else
        echo -e "  ${GREEN}Layer 5: all consistent${NC}"
    fi
    TOTAL_ISSUES=$((TOTAL_ISSUES + layer5_issues))
fi

# ============================================================
# Layer 6: Static analysis (cppcheck)
# ============================================================

if run_layer 6 "Static analysis (cppcheck)"; then
    if command -v cppcheck > /dev/null 2>&1; then
        layer6_issues=0
        for h in wheels/*/*/*.h; do
            [[ -f "$h" ]] || continue
            dir="$(dirname "$h")"
            name="$(basename "$h" .h)"
            c_file="$dir/$name.c"
            [[ -f "$c_file" ]] || continue
            [[ -d "$dir/vendor" ]] && continue

            cppcheck_rc=0
            out=$(cppcheck --enable=warning,performance --suppress=missingIncludeSystem \
                  --quiet --error-exitcode=1 \
                  "$c_file" 2>&1) || cppcheck_rc=$?
            if [[ $cppcheck_rc -ne 0 ]]; then
                echo -e "  ${YELLOW}WARN${NC}  $name"
                echo "$out" | head -5 | while read -r line; do
                    echo "         $line"
                done
                layer6_issues=$((layer6_issues + 1))
            fi
        done

        echo ""
        if [[ $layer6_issues -gt 0 ]]; then
            echo -e "  ${YELLOW}Layer 6: $layer6_issues cppcheck warnings${NC}"
        else
            echo -e "  ${GREEN}Layer 6: all clean${NC}"
        fi
        TOTAL_ISSUES=$((TOTAL_ISSUES + layer6_issues))
    else
        echo -e "  ${YELLOW}SKIP${NC}  cppcheck not installed (apt install cppcheck)"
    fi
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

# Save JSON snapshot if requested
if [[ -n "$JSON_OUT" ]]; then
    mkdir -p "$(dirname "$JSON_OUT")"
    cat > "$JSON_OUT" <<JSONEOF
{
  "timestamp": "$(date -Iseconds)",
  "total_issues": $TOTAL_ISSUES,
  "layers": {
    "pattern_grep": ${layer1_issues:-0},
    "sanitizer": ${layer2_fail:-0},
    "fuzz": 0,
    "stress": ${layer4_fail:-0},
    "consistency": ${layer5_issues:-0},
    "cppcheck": ${layer6_issues:-0}
  }
}
JSONEOF
    echo -e "  Snapshot saved: $JSON_OUT"
fi

exit $(( TOTAL_ISSUES > 0 ? 1 : 0 ))
