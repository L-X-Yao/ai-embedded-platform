#!/bin/bash
# test_scaffold.sh — verify all new_wheel.sh templates compile clean.
#
# Usage:
#   test_scaffold.sh           # test all 7 templates
#
# Creates temporary wheels, builds them, verifies zero errors,
# then cleans up. Catches scaffold regressions before they propagate.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

PASS=0
FAIL=0
CLEANUP=()

test_template() {
    local layer="$1" name="$2" extra="${3:-}"
    local dir="$REPO/wheels/$layer/$name"
    CLEANUP+=("$dir")

    bash "$REPO/tooling/wheel/new_wheel.sh" --layer "$layer" --name "$name" $extra --no-audit > /dev/null 2>&1

    local test_dir="$dir/test"
    local build_dir="$test_dir/build_scaffold_test"

    if cmake -GNinja -S "$test_dir" -B "$build_dir" > /dev/null 2>&1 \
       && cmake --build "$build_dir" > /dev/null 2>&1; then
        echo -e "  ${GREEN}PASS${NC}  $layer/$name ($extra)"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC}  $layer/$name ($extra)"
        cmake --build "$build_dir" 2>&1 | grep "error:" | head -3 | sed 's/^/    /'
        FAIL=$((FAIL + 1))
    fi
}

echo -e "${BOLD}=== Scaffold Template Test ===${NC}"
echo ""

test_template drivers  sc_i2c    "--hal i2c"
test_template drivers  sc_spi    "--hal spi"
test_template drivers  sc_gpio   "--hal gpio"
test_template drivers  sc_ow     "--hal onewire"
test_template drivers  sc_pwm    "--hal pwm"
test_template protocols sc_proto ""
test_template primitives sc_prim ""

echo ""
echo -e "${BOLD}--- Summary ---${NC}"
echo -e "  Total: $((PASS + FAIL))  ${GREEN}Passed: $PASS${NC}  ${RED}Failed: $FAIL${NC}"

# Cleanup
for d in "${CLEANUP[@]}"; do
    rm -rf "$d" 2>/dev/null
done

[[ $FAIL -gt 0 ]] && exit 1
exit 0
