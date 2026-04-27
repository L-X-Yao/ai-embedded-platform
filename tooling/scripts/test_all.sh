#!/bin/bash
# test_all.sh — build and run every host test in the repo in one shot.
#
# Usage:
#   test_all.sh                  # run all discoverable tests
#   test_all.sh w25qxx crc16    # run only named targets
#   test_all.sh --wheel          # wheels only (skip project tests)
#   test_all.sh --project        # project tests only
#   test_all.sh --list           # just list discoverable targets, don't run
#
# Each test dir gets its own build_ta/ subdirectory (ignored via .gitignore).
# VSF-dependent tests (log wheel) are skipped unless VSF_PATH is set or
# the default ../../../vsf relative path exists.
#
# Exit 0 if all tests pass; 1 if any fail; 2 if no tests found.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR_NAME="build_ta"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

FILTER_WHEEL=false
FILTER_PROJECT=false
LIST_ONLY=false
declare -a NAME_FILTER=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --wheel)    FILTER_WHEEL=true; shift ;;
        --project)  FILTER_PROJECT=true; shift ;;
        --list)     LIST_ONLY=true; shift ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0
            ;;
        *)          NAME_FILTER+=("$1"); shift ;;
    esac
done

# --- Discover test targets ---------------------------------------------------

declare -a TARGETS=()    # "name|src_dir|type"  type = wheel|project
declare -a PYTEST=()     # "name|pytest_file"

discover_wheel_tests() {
    # L1+ driver wheels: wheels/<layer>/<name>/test/CMakeLists.txt
    while IFS= read -r cml; do
        local test_dir; test_dir="$(dirname "$cml")"
        local wheel_dir; wheel_dir="$(dirname "$test_dir")"
        local name; name="$(basename "$wheel_dir")"
        TARGETS+=("${name}|${test_dir}|wheel")
    done < <(find "$REPO/wheels" -path "*/test/CMakeLists.txt" 2>/dev/null | sort)

    # L0 primitives: wheels/primitives/<name>/CMakeLists.txt (test in same dir)
    while IFS= read -r cml; do
        local src_dir; src_dir="$(dirname "$cml")"
        local name; name="$(basename "$src_dir")"
        # skip if already found via test/ subdir
        local already=false
        for t in "${TARGETS[@]:-}"; do
            [[ "${t%%|*}" == "$name" ]] && already=true && break
        done
        $already && continue
        TARGETS+=("${name}|${src_dir}|wheel")
    done < <(find "$REPO/wheels/primitives" -maxdepth 2 -name "CMakeLists.txt" 2>/dev/null | sort)

    # pytest files
    while IFS= read -r pyf; do
        local dir; dir="$(dirname "$pyf")"
        local name; name="$(basename "$dir")"
        PYTEST+=("${name}|${pyf}")
    done < <(find "$REPO/wheels" -name "test_*.py" 2>/dev/null | sort)
}

discover_project_tests() {
    while IFS= read -r cml; do
        local test_dir; test_dir="$(dirname "$cml")"
        local name; name="$(basename "$test_dir")"
        TARGETS+=("${name}|${test_dir}|project")
    done < <(find "$REPO/projects" -path "*/test/*/CMakeLists.txt" 2>/dev/null | sort)
}

if ! $FILTER_PROJECT; then
    discover_wheel_tests
fi
if ! $FILTER_WHEEL; then
    discover_project_tests
fi

# Apply name filter
if [[ ${#NAME_FILTER[@]} -gt 0 ]]; then
    declare -a FILTERED=()
    for t in "${TARGETS[@]:-}"; do
        tname="${t%%|*}"
        for f in "${NAME_FILTER[@]}"; do
            [[ "$tname" == "$f" ]] && FILTERED+=("$t") && break
        done
    done
    TARGETS=("${FILTERED[@]:-}")

    declare -a FILTERED_PY=()
    for p in "${PYTEST[@]:-}"; do
        pname="${p%%|*}"
        for f in "${NAME_FILTER[@]}"; do
            [[ "$pname" == "$f" ]] && FILTERED_PY+=("$p") && break
        done
    done
    PYTEST=("${FILTERED_PY[@]:-}")
fi

if [[ ${#TARGETS[@]} -eq 0 && ${#PYTEST[@]} -eq 0 ]]; then
    echo -e "${RED}No test targets found.${NC}"
    exit 2
fi

# --- List mode ----------------------------------------------------------------

if $LIST_ONLY; then
    echo -e "${BOLD}Discoverable test targets:${NC}"
    for t in "${TARGETS[@]:-}"; do
        IFS='|' read -r name src_dir type <<< "$t"
        printf "  %-20s %-8s %s\n" "$name" "[$type]" "$src_dir"
    done
    for p in "${PYTEST[@]:-}"; do
        IFS='|' read -r name pyf <<< "$p"
        printf "  %-20s %-8s %s\n" "${name}(py)" "[pytest]" "$pyf"
    done
    exit 0
fi

# --- Build & run --------------------------------------------------------------

TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0
declare -a FAIL_NAMES=()

is_vsf_dependent() {
    grep -q "vsf\.cmake" "$1/CMakeLists.txt" 2>/dev/null
}

run_c_test() {
    local name="$1" src_dir="$2" type="$3"
    TOTAL=$((TOTAL + 1))

    # Check VSF dependency
    if is_vsf_dependent "$src_dir"; then
        if [[ -z "${VSF_PATH:-}" && ! -d "$REPO/vsf" ]]; then
            echo -e "  ${YELLOW}SKIP${NC}  $name  (VSF sim, VSF_PATH not set)"
            SKIPPED=$((SKIPPED + 1))
            return 0
        fi
    fi

    local build_dir="$src_dir/$BUILD_DIR_NAME"

    # Configure
    if ! cmake -GNinja -S "$src_dir" -B "$build_dir" > "$build_dir.cmake.log" 2>&1; then
        echo -e "  ${RED}FAIL${NC}  $name  (cmake configure failed)"
        FAILED=$((FAILED + 1))
        FAIL_NAMES+=("$name")
        return 0
    fi

    # Build
    if ! cmake --build "$build_dir" > "$build_dir.build.log" 2>&1; then
        echo -e "  ${RED}FAIL${NC}  $name  (build failed)"
        cat "$build_dir.build.log" | tail -20
        FAILED=$((FAILED + 1))
        FAIL_NAMES+=("$name")
        return 0
    fi

    # Find test executable
    local exe=""
    for candidate in "$build_dir/test_${name}" "$build_dir/${name}_test" "$build_dir/test_main"; do
        [[ -x "$candidate" ]] && exe="$candidate" && break
    done
    # Fallback: first executable in build dir
    if [[ -z "$exe" ]]; then
        exe="$(find "$build_dir" -maxdepth 1 -type f -executable 2>/dev/null | head -1 || true)"
    fi

    if [[ -z "$exe" ]]; then
        echo -e "  ${RED}FAIL${NC}  $name  (no test executable found in $build_dir)"
        FAILED=$((FAILED + 1))
        FAIL_NAMES+=("$name")
        return 0
    fi

    # Run
    local output
    if output=$("$exe" 2>&1); then
        local summary_line
        summary_line=$(echo "$output" | grep -E '[0-9]+ tests?,\s*[0-9]+ passed' | tail -1 || true)
        if [[ -n "$summary_line" ]]; then
            echo -e "  ${GREEN}PASS${NC}  $name  (${summary_line})"
        else
            echo -e "  ${GREEN}PASS${NC}  $name"
        fi
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}FAIL${NC}  $name  (test execution failed)"
        echo "$output" | tail -10
        FAILED=$((FAILED + 1))
        FAIL_NAMES+=("$name")
    fi
}

run_pytest() {
    local name="$1" pyf="$2"
    TOTAL=$((TOTAL + 1))

    local dir; dir="$(dirname "$pyf")"

    if ! command -v python3 &>/dev/null && ! command -v python &>/dev/null; then
        echo -e "  ${YELLOW}SKIP${NC}  ${name}(py)  (python not found)"
        SKIPPED=$((SKIPPED + 1))
        return 0
    fi

    local py; py="$(command -v python3 2>/dev/null || command -v python 2>/dev/null)"

    if ! "$py" -m pytest --version &>/dev/null 2>&1; then
        echo -e "  ${YELLOW}SKIP${NC}  ${name}(py)  (pytest not installed)"
        SKIPPED=$((SKIPPED + 1))
        return 0
    fi

    # pytest harnesses build to their own build/ dir; stale cross-platform
    # caches (e.g. Windows cmake cache when running in WSL) cause failures.
    local wheel_build="$dir/build"
    if [[ -d "$wheel_build" ]] && grep -q 'CMAKE_C_COMPILER.*\.exe' "$wheel_build/CMakeCache.txt" 2>/dev/null; then
        rm -rf "$wheel_build"
    fi

    local output
    if output=$("$py" -m pytest "$pyf" -v --tb=short 2>&1); then
        local count; count=$(echo "$output" | grep -oP '\d+(?= passed)' | tail -1 || true)
        echo -e "  ${GREEN}PASS${NC}  ${name}(py)  (${count:-?} pytest cases)"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}FAIL${NC}  ${name}(py)"
        echo "$output" | tail -15
        FAILED=$((FAILED + 1))
        FAIL_NAMES+=("${name}(py)")
    fi
}

echo -e "${BOLD}=== test_all.sh ===${NC}"
echo ""

# C tests
for t in "${TARGETS[@]:-}"; do
    [[ -z "$t" ]] && continue
    IFS='|' read -r name src_dir type <<< "$t"
    run_c_test "$name" "$src_dir" "$type"
done

# pytest
for p in "${PYTEST[@]:-}"; do
    [[ -z "$p" ]] && continue
    IFS='|' read -r name pyf <<< "$p"
    run_pytest "$name" "$pyf"
done

# --- Summary ------------------------------------------------------------------

echo ""
echo -e "${BOLD}--- Summary ---${NC}"
echo -e "  Total: $TOTAL  ${GREEN}Passed: $PASSED${NC}  ${RED}Failed: $FAILED${NC}  ${YELLOW}Skipped: $SKIPPED${NC}"

if [[ $FAILED -gt 0 ]]; then
    echo -e "  ${RED}Failed targets: ${FAIL_NAMES[*]}${NC}"
    exit 1
fi

if [[ $PASSED -eq 0 && $SKIPPED -gt 0 ]]; then
    echo -e "  ${YELLOW}All tests were skipped.${NC}"
    exit 2
fi

echo -e "  ${GREEN}All tests passed.${NC}"
exit 0
