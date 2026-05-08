#!/bin/bash
# audit_report.sh — unified audit report combining all audit tools.
#
# Usage:
#   audit_report.sh             # run all tools, print combined report
#   audit_report.sh --save      # also save JSON snapshot

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO"

BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

SAVE=false
[[ "${1:-}" == "--save" ]] && SAVE=true

echo -e "${BOLD}=== Unified Audit Report ===${NC}"
echo -e "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo -e "  Repo: $REPO"
echo ""

TOTAL_PASS=0
TOTAL_WARN=0
TOTAL_FAIL=0

run_tool() {
    local name="$1" cmd="$2"
    echo -e "${BOLD}--- $name ---${NC}"
    local rc=0
    $cmd 2>&1 || rc=$?
    echo ""
    return $rc
}

# 1. Wheel audit
echo -e "${BOLD}[1/5] Wheel audit (audit_wheels.py)${NC}"
SAVE_FLAG=""
$SAVE && SAVE_FLAG="--save"
wheel_out=$(python3 tooling/audit/audit_wheels.py --fast --quiet $SAVE_FLAG 2>&1) || true
wheel_f=$(echo "$wheel_out" | sed -n 's/.*failures:[[:space:]]*\([0-9][0-9]*\).*/\1/p' | head -1 || echo 0)
wheel_w=$(echo "$wheel_out" | sed -n 's/.*warnings:[[:space:]]*\([0-9][0-9]*\).*/\1/p' | head -1 || echo 0)
wheel_i=$(echo "$wheel_out" | sed -n 's/.*info:[[:space:]]*\([0-9][0-9]*\).*/\1/p' | head -1 || echo 0)
echo -e "  Failures: $wheel_f  Warnings: $wheel_w  Info: $wheel_i"
TOTAL_FAIL=$((TOTAL_FAIL + wheel_f))
TOTAL_WARN=$((TOTAL_WARN + wheel_w))
echo ""

# 2. Doc drift
echo -e "${BOLD}[2/5] Doc drift (doc_drift_check.py)${NC}"
drift_out=$(python3 tooling/audit/doc_drift_check.py 2>&1) || true
if echo "$drift_out" | grep -q "all assertions match"; then
    echo -e "  ${GREEN}Clean${NC}"
    TOTAL_PASS=$((TOTAL_PASS + 1))
else
    drift_count=$(echo "$drift_out" | grep -oE '[0-9]+ drift' | grep -oE '[0-9]+' | head -1 || echo 0)
    echo -e "  ${YELLOW}$drift_count drift(s)${NC}"
    TOTAL_WARN=$((TOTAL_WARN + drift_count))
fi
echo ""

# 3. Memory audit
echo -e "${BOLD}[3/5] Memory audit (memory_audit.py)${NC}"
mem_out=$(python3 tooling/audit/memory_audit.py 2>&1) || true
if echo "$mem_out" | grep -q "clean"; then
    echo -e "  ${GREEN}Clean${NC}"
    TOTAL_PASS=$((TOTAL_PASS + 1))
else
    mem_errs=$(echo "$mem_out" | grep -oE '[0-9]+ error' | grep -oE '[0-9]+' | head -1 || echo 0)
    mem_warns=$(echo "$mem_out" | grep -oE '[0-9]+ warning' | grep -oE '[0-9]+' | head -1 || echo 0)
    echo -e "  Errors: $mem_errs  Warnings: $mem_warns"
    TOTAL_FAIL=$((TOTAL_FAIL + mem_errs))
    TOTAL_WARN=$((TOTAL_WARN + mem_warns))
fi
echo ""

# 4. Tests
echo -e "${BOLD}[4/5] Test suite (test_all.sh)${NC}"
test_out=$(bash tooling/test/test_all.sh --parallel 2>&1) || true
test_pass=$(echo "$test_out" | sed -n 's/.*Passed:[[:space:]]*\([0-9][0-9]*\).*/\1/p' | head -1 || echo 0)
test_fail=$(echo "$test_out" | sed -n 's/.*Failed:[[:space:]]*\([0-9][0-9]*\).*/\1/p' | head -1 || echo 0)
echo -e "  Passed: $test_pass  Failed: $test_fail"
TOTAL_FAIL=$((TOTAL_FAIL + test_fail))
echo ""

# 5. Audit tool self-test
echo -e "${BOLD}[5/5] Audit self-test (test_audit_wheels.py)${NC}"
selftest_out=$(python3 tooling/audit/test_audit_wheels.py 2>&1) || true
selftest_pass=$(echo "$selftest_out" | grep -oE '[0-9]+ passed' | grep -oE '[0-9]+' | head -1 || echo 0)
selftest_fail=$(echo "$selftest_out" | grep -oE '[0-9]+ failed' | grep -oE '[0-9]+' | head -1 || echo 0)
echo -e "  Passed: $selftest_pass  Failed: $selftest_fail"
TOTAL_FAIL=$((TOTAL_FAIL + selftest_fail))
echo ""

# Summary
echo -e "${BOLD}=== Combined Summary ===${NC}"
echo -e "  Total failures: $TOTAL_FAIL"
echo -e "  Total warnings: $TOTAL_WARN"
if [[ $TOTAL_FAIL -eq 0 && $TOTAL_WARN -eq 0 ]]; then
    echo -e "  ${GREEN}All systems clean.${NC}"
elif [[ $TOTAL_FAIL -eq 0 ]]; then
    echo -e "  ${YELLOW}Warnings only — no blockers.${NC}"
else
    echo -e "  ${RED}Failures found — review above.${NC}"
fi

exit $TOTAL_FAIL
