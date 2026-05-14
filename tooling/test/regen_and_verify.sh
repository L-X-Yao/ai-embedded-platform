#!/usr/bin/env bash
# regen_and_verify.sh — regenerate codegen output and re-run the matching
# test suite, printing only the FAIL summary.
#
# Closes the "edit codegen detection -> regenerate -> hunt FAILs" loop
# that took ~5 manual iterations during the 2026-05-14 boundary work.
#
# Usage:
#   regen_and_verify.sh boundary   # gen_boundary_tests.py + test_boundary_*
#   regen_and_verify.sh null       # gen_null_tests.py + test_null_*
#   regen_and_verify.sh sentinel   # gen_sentinel_tests.py + test_sentinel_*
#
# Exit 0 if regeneration + all tests pass, 1 otherwise.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

KIND="${1:?Usage: $0 <boundary|null|sentinel>}"

case "$KIND" in
    boundary)
        GEN="tooling/codegen/gen_boundary_tests.py"
        PATTERN="test_boundary_*.c"
        LABEL="type-boundary"
        ;;
    null)
        GEN="tooling/codegen/gen_null_tests.py"
        PATTERN="test_null_*.c"
        LABEL="NULL parameter"
        ;;
    sentinel)
        GEN="tooling/codegen/gen_sentinel_tests.py"
        PATTERN="test_sentinel_*.c"
        LABEL="sentinel"
        ;;
    *)
        echo "Unknown kind: $KIND (use boundary|null|sentinel)" >&2
        exit 2
        ;;
esac

if [ ! -f "$GEN" ]; then
    echo "ERR: generator missing: $GEN" >&2
    exit 2
fi

echo "=== Regenerate $KIND ==="
if ! python3 "$GEN" 2>&1 | tail -8; then
    echo "ERR: regeneration failed" >&2
    exit 1
fi

echo ""
echo "=== Run $LABEL tests ==="
# Funnel output through grep so only FAIL/SUMMARY lines reach stdout —
# the per-wheel PASS noise is irrelevant for the iterate-on-failure loop.
bash tooling/test/run_generated_tests.sh "$PATTERN" "$LABEL" \
    2>&1 | grep -E '^(  FAIL|  BUILD_FAIL|  RUN_FAIL|=== |[0-9]+ wheels)'
rc=${PIPESTATUS[0]}
exit "$rc"
