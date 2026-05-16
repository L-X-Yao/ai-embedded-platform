#!/usr/bin/env bash
# Test that all examples/ compile on host.
# Usage: bash tooling/test/test_examples.sh
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

pass=0
fail=0
fails=""

for dir in examples/*/; do
    name=$(basename "$dir")
    if [ ! -f "$dir/CMakeLists.txt" ]; then
        echo "SKIP $name (no CMakeLists.txt)"
        continue
    fi

    rm -rf "$dir/build"
    mkdir -p "$dir/build"

    if cmake -S "$dir" -B "$dir/build" -G "Unix Makefiles" > /dev/null 2>&1 && \
       make -C "$dir/build" --no-print-directory > /dev/null 2>&1; then
        echo "PASS $name"
        pass=$((pass + 1))
    else
        echo "FAIL $name"
        fail=$((fail + 1))
        fails="$fails $name"
    fi

    rm -rf "$dir/build"
done

echo ""
echo "=== Results: $pass PASS, $fail FAIL ==="
if [ "$fail" -gt 0 ]; then
    echo "Failed:$fails"
    exit 1
fi
