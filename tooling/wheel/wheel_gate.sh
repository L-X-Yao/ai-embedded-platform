#!/bin/bash
# wheel_gate.sh — validate wheels are test+audit clean before commit
#
# Usage:
#   wheel_gate.sh <layer/name> [<layer/name> ...]
#   wheel_gate.sh primitives/ewma drivers/bmp388
#   wheel_gate.sh --staged   # auto-detect from git staged changes
#
# Runs for each wheel:
#   1. Auto-generate null tests if missing (gen_null_tests.py)
#   2. CMake build + test (all test_* executables)
#   2.5. ASan + UBSan build, run every test_* and prop_* target
#   2.7. -Wconversion strict compile (-Wconversion -Wsign-conversion)
#   2.8. SPDX header check (all .c/.h must have SPDX-License-Identifier)
#   2.9. README quality check (no TODO placeholders)
#   3. Audit (--wheel --quiet, 0 WARN/FAIL required)
#
# Exit 0 if all pass, 1 if any fail, 2 if no wheels given.

set -uo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m'

BUILD_DIR="build_gate"

WHEELS=()
AUTO_STAGED=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --staged)
            AUTO_STAGED=true
            shift
            ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0
            ;;
        *)
            WHEELS+=("$1")
            shift
            ;;
    esac
done

if $AUTO_STAGED; then
    while IFS= read -r f; do
        if [[ "$f" =~ ^wheels/([^/]+/[^/]+)/ ]]; then
            w="${BASH_REMATCH[1]}"
            found=false
            for existing in "${WHEELS[@]:-}"; do
                [[ "$existing" == "$w" ]] && found=true && break
            done
            $found || WHEELS+=("$w")
        fi
    done < <(git -C "$REPO" diff --cached --name-only 2>/dev/null)
fi

if [[ ${#WHEELS[@]} -eq 0 ]]; then
    echo "No wheels specified. Use: wheel_gate.sh <layer/name> ... or --staged"
    exit 2
fi

FAIL=0
PASS=0
TOTAL=${#WHEELS[@]}
FAILED_WHEELS=()

for wheel in "${WHEELS[@]}"; do
    wheel_dir="$REPO/wheels/${wheel}"
    wheel_name=$(basename "$wheel")

    if [[ ! -d "$wheel_dir" ]]; then
        echo -e "${RED}FAIL${NC} ${wheel}: directory not found"
        FAIL=$((FAIL + 1))
        FAILED_WHEELS+=("$wheel")
        continue
    fi

    echo -e "${BOLD}━━━━ ${wheel} ━━━━${NC}"

    # --- 1. Auto-generate null tests if missing ---
    header_file="${wheel_dir}/${wheel_name}.h"
    null_test="${wheel_dir}/test/test_null_${wheel_name}.c"
    if [[ -f "$header_file" ]] && [[ ! -f "$null_test" ]]; then
        if python3 "$REPO/tooling/codegen/gen_null_tests.py" "$header_file" > /dev/null 2>&1; then
            echo -e "  ${GREEN}AUTO${NC} generated test_null_${wheel_name}.c"
        else
            echo -e "  ${YELLOW}SKIP${NC} gen_null_tests failed (non-blocking)"
        fi
    fi

    # --- 2. Build + test ---
    test_dir="${wheel_dir}/test"
    if [[ -d "$test_dir" ]] && [[ -f "$test_dir/CMakeLists.txt" ]]; then
        build_dir="${test_dir}/${BUILD_DIR}"
        rm -rf "$build_dir"

        build_log=$(mktemp)
        if cmake -S "$test_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug > "$build_log" 2>&1 && \
           cmake --build "$build_dir" >> "$build_log" 2>&1; then

            test_ok=true
            test_count=0
            for exe in "$build_dir"/test_*; do
                [[ -x "$exe" ]] || continue
                test_count=$((test_count + 1))
                exe_name=$(basename "$exe")
                if ! "$exe" > /dev/null 2>&1; then
                    echo -e "  ${RED}FAIL${NC} test: $exe_name"
                    test_ok=false
                fi
            done

            if [[ $test_count -eq 0 ]]; then
                echo -e "  ${YELLOW}WARN${NC} no test executables found"
            elif $test_ok; then
                echo -e "  ${GREEN}PASS${NC} tests ($test_count executables)"
            else
                FAIL=$((FAIL + 1))
                FAILED_WHEELS+=("$wheel")
                rm -rf "$build_dir" "$build_log"
                continue
            fi
        else
            echo -e "  ${RED}FAIL${NC} build"
            tail -20 "$build_log" | sed 's/^/    /'
            FAIL=$((FAIL + 1))
            FAILED_WHEELS+=("$wheel")
            rm -rf "$build_dir" "$build_log"
            continue
        fi
        rm -rf "$build_dir" "$build_log"
    else
        echo -e "  ${YELLOW}SKIP${NC} no test/CMakeLists.txt"
    fi

    # --- 2.5. ASan + UBSan build + run ---
    if [[ -d "$test_dir" ]] && [[ -f "$test_dir/CMakeLists.txt" ]]; then
        san_dir="${test_dir}/build_san_gate"
        rm -rf "$san_dir"
        san_log=$(mktemp)
        san_cflags="-fsanitize=address,undefined -fno-sanitize-recover=all -w"
        san_ldflags="-fsanitize=address,undefined"
        if cmake -S "$test_dir" -B "$san_dir" -DCMAKE_BUILD_TYPE=Debug \
                 -DCMAKE_C_FLAGS_DEBUG="$san_cflags" \
                 -DCMAKE_EXE_LINKER_FLAGS="$san_ldflags" > "$san_log" 2>&1 && \
           cmake --build "$san_dir" >> "$san_log" 2>&1; then

            ubsan_ok=true
            for exe in "$san_dir"/test_* "$san_dir"/prop_*; do
                [[ -f "$exe" && -x "$exe" ]] || continue
                if ! "$exe" > /dev/null 2>&1; then
                    echo -e "  ${RED}FAIL${NC} ASan/UBSan: $(basename "$exe")"
                    ubsan_ok=false
                fi
            done

            if $ubsan_ok; then
                echo -e "  ${GREEN}PASS${NC} ASan/UBSan"
            else
                FAIL=$((FAIL + 1))
                FAILED_WHEELS+=("$wheel")
                rm -rf "$san_dir" "$san_log"
                continue
            fi
        else
            echo -e "  ${RED}FAIL${NC} ASan/UBSan build"
            tail -n 20 "$san_log"
            FAIL=$((FAIL + 1))
            FAILED_WHEELS+=("$wheel")
            rm -rf "$san_dir" "$san_log"
            continue
        fi
        rm -rf "$san_dir" "$san_log"
    fi

    # --- 2.7. -Wconversion strict compile ---
    if [[ -d "$test_dir" ]] && [[ -f "$test_dir/CMakeLists.txt" ]]; then
        conv_dir="${test_dir}/build_conv_gate"
        rm -rf "$conv_dir"
        conv_log=$(mktemp)
        conv_cflags="-Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion"
        conv_gen=()
        if [[ "$(uname -o 2>/dev/null)" == "Msys" ]] || [[ "$(uname -s)" == MINGW* ]]; then
            conv_gen=(-G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc)
        fi
        if cmake -S "$test_dir" -B "$conv_dir" "${conv_gen[@]}" \
                 -DCMAKE_C_FLAGS="-O0 $conv_cflags" > "$conv_log" 2>&1 && \
           cmake --build "$conv_dir" >> "$conv_log" 2>&1; then
            echo -e "  ${GREEN}PASS${NC} -Wconversion"
        else
            echo -e "  ${RED}FAIL${NC} -Wconversion (strict integer promotion)"
            grep "error:" "$conv_log" | head -5 | sed 's/^/    /'
            FAIL=$((FAIL + 1))
            FAILED_WHEELS+=("$wheel")
            rm -rf "$conv_dir" "$conv_log"
            continue
        fi
        rm -rf "$conv_dir" "$conv_log"
    fi

    # --- 2.75. Property test ---
    # Most prop_*.c files are not CMake targets, so the build above skips them.
    if ls "${wheel_dir}"/test/prop_*.c >/dev/null 2>&1; then
        if bash "$REPO/tooling/test/run_generated_tests.sh" "prop_*.c" "property"                 --extra-cflags="-Wextra" --wheel="$wheel" >/dev/null 2>&1; then
            echo -e "  ${GREEN}PASS${NC} property test"
        else
            echo -e "  ${RED}FAIL${NC} property test"
            FAIL=$((FAIL + 1))
            FAILED_WHEELS+=("$wheel")
            continue
        fi
    fi

    # --- 2.8. SPDX header check ---
    spdx_missing=0
    while IFS= read -r f; do
        head -5 "$f" | grep -q "SPDX-License-Identifier" || spdx_missing=$((spdx_missing + 1))
    done < <(find "$wheel_dir" \( -name "*.c" -o -name "*.h" \) -not -path "*/build_*" -not -path "*/build/*" -not -path "*/vendor/*" 2>/dev/null)
    if [[ "$spdx_missing" -gt 0 ]]; then
        echo -e "  ${RED}FAIL${NC} SPDX: $spdx_missing files missing header"
        FAIL=$((FAIL + 1))
        FAILED_WHEELS+=("$wheel")
        continue
    else
        echo -e "  ${GREEN}PASS${NC} SPDX"
    fi

    # --- 2.9. README quality check ---
    readme_file="${wheel_dir}/README.md"
    if [[ -f "$readme_file" ]] && grep -q "TODO" "$readme_file"; then
        echo -e "  ${RED}FAIL${NC} README: contains TODO placeholders"
        FAIL=$((FAIL + 1))
        FAILED_WHEELS+=("$wheel")
        continue
    else
        echo -e "  ${GREEN}PASS${NC} README"
    fi

    # --- 3. Audit ---
    audit_output=$(cd "$REPO" && python3 tooling/audit/audit_wheels.py --wheel "$wheel" --quiet 2>&1)
    warn_count=$(echo "$audit_output" | grep -cE "\[(WARN|FAIL)\]" || true)
    if [[ "$warn_count" -gt 0 ]]; then
        echo -e "  ${RED}FAIL${NC} audit: $warn_count findings"
        echo "$audit_output" | grep -E "\[(WARN|FAIL)\]" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
        FAILED_WHEELS+=("$wheel")
        continue
    else
        echo -e "  ${GREEN}PASS${NC} audit (0 findings)"
    fi

    PASS=$((PASS + 1))
done

echo ""
echo -e "${BOLD}━━━━ Gate Summary ━━━━${NC}"
echo -e "  ${GREEN}PASS${NC}: $PASS / $TOTAL"
if [[ $FAIL -gt 0 ]]; then
    echo -e "  ${RED}FAIL${NC}: $FAIL / $TOTAL"
    for w in "${FAILED_WHEELS[@]}"; do
        echo -e "    ${RED}✗${NC} $w"
    done
    echo ""
    echo -e "${RED}Gate blocked. Fix failures before committing.${NC}"
    exit 1
fi
echo -e "${GREEN}All wheels ready to commit.${NC}"
exit 0
