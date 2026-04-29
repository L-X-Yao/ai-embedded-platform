#!/bin/bash
# test_all.sh — build and run every host test in the repo in one shot.
#
# Usage:
#   test_all.sh                  # run all discoverable tests
#   test_all.sh w25qxx crc16    # run only named targets
#   test_all.sh --wheel          # wheels only (skip project tests)
#   test_all.sh --project        # project tests only
#   test_all.sh --list           # just list discoverable targets, don't run
#   test_all.sh --changed        # only run tests affected by uncommitted changes
#   test_all.sh --coverage       # build with gcov instrumentation, print coverage after tests
#   test_all.sh --fail-fast      # stop on first test failure
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
CHANGED_ONLY=false
COVERAGE=false
FAIL_FAST=false
declare -a NAME_FILTER=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --wheel)    FILTER_WHEEL=true; shift ;;
        --project)  FILTER_PROJECT=true; shift ;;
        --list)     LIST_ONLY=true; shift ;;
        --changed)  CHANGED_ONLY=true; shift ;;
        --coverage) COVERAGE=true; shift ;;
        --fail-fast) FAIL_FAST=true; shift ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0
            ;;
        *)          NAME_FILTER+=("$1"); shift ;;
    esac
done

# --changed: derive affected test targets from git diff
if $CHANGED_ONLY; then
    changed_files=$(git -C "$REPO" diff --name-only HEAD 2>/dev/null || true)
    # Also include staged + untracked
    changed_files+=$'\n'$(git -C "$REPO" diff --name-only --cached 2>/dev/null || true)
    changed_files+=$'\n'$(git -C "$REPO" ls-files --others --exclude-standard 2>/dev/null || true)

    declare -A affected_names=()
    while IFS= read -r f; do
        [[ -z "$f" ]] && continue
        # wheels/<layer>/<name>/... → name
        if [[ "$f" =~ ^wheels/([^/]+)/([^/]+)/ ]]; then
            layer="${BASH_REMATCH[1]}"
            wname="${BASH_REMATCH[2]}"
            [[ "$layer" == "_draft" ]] && continue
            affected_names["$wname"]=1
        fi
        # projects/<proj>/test/<name>/... → name
        if [[ "$f" =~ ^projects/([^/]+)/test/([^/]+)/ ]]; then
            affected_names["${BASH_REMATCH[2]}"]=1
        fi
        # projects/<proj>/src/... → find which project tests exist
        if [[ "$f" =~ ^projects/([^/]+)/src/ ]]; then
            proj="${BASH_REMATCH[1]}"
            while IFS= read -r tdir; do
                tname=$(basename "$tdir")
                affected_names["$tname"]=1
            done < <(find "$REPO/projects/$proj/test" -mindepth 1 -maxdepth 1 -type d 2>/dev/null)
        fi
    done <<< "$changed_files"

    if [[ ${#affected_names[@]} -eq 0 ]]; then
        echo -e "${YELLOW}No test-affecting changes detected in git diff.${NC}"
        exit 0
    fi

    for n in "${!affected_names[@]}"; do
        NAME_FILTER+=("$n")
    done
    echo -e "${CYAN}Changed targets: ${NAME_FILTER[*]}${NC}"
    echo ""
fi

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

is_linux() {
    [[ "$(uname -s)" == "Linux" ]]
}

run_c_test() {
    local name="$1" src_dir="$2" type="$3"
    TOTAL=$((TOTAL + 1))

    # Check VSF dependency — linux64 sim target requires Linux + pkg-config
    if is_vsf_dependent "$src_dir"; then
        if [[ -z "${VSF_PATH:-}" && ! -d "$REPO/vsf" ]]; then
            echo -e "  ${YELLOW}SKIP${NC}  $name  (VSF sim, VSF_PATH not set)"
            SKIPPED=$((SKIPPED + 1))
            return 0
        fi
        if ! is_linux; then
            echo -e "  ${YELLOW}SKIP${NC}  $name  (VSF linux64 sim requires Linux)"
            SKIPPED=$((SKIPPED + 1))
            return 0
        fi
    fi

    local build_dir="$src_dir/$BUILD_DIR_NAME"

    # Configure
    local cov_flags=""
    if $COVERAGE; then
        cov_flags="-DCMAKE_C_FLAGS=--coverage -DCMAKE_EXE_LINKER_FLAGS=--coverage"
    fi
    if ! cmake -GNinja -S "$src_dir" -B "$build_dir" $cov_flags > "$build_dir.cmake.log" 2>&1; then
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
        summary_line=$(echo "$output" | grep -E '[0-9]+ tests?,\s*[0-9]+ (passed|failures?)' | tail -1 || true)
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
        local count; count=$(echo "$output" | grep -oE '[0-9]+ passed' | grep -oE '[0-9]+' | tail -1 || true)
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
    $FAIL_FAST && [[ $FAILED -gt 0 ]] && break
done

# pytest
if ! { $FAIL_FAST && [[ $FAILED -gt 0 ]]; }; then
    for p in "${PYTEST[@]:-}"; do
        [[ -z "$p" ]] && continue
        IFS='|' read -r name pyf <<< "$p"
        run_pytest "$name" "$pyf"
        $FAIL_FAST && [[ $FAILED -gt 0 ]] && break
    done
fi

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

# --- Coverage report ----------------------------------------------------------

if $COVERAGE; then
    echo ""
    echo -e "${BOLD}--- Coverage ---${NC}"
    gcov_files=$(find "$REPO/wheels" -name "*.gcda" 2>/dev/null)
    if [[ -n "$gcov_files" ]]; then
        total_lines=0 hit_lines=0
        while IFS= read -r gcda; do
            dir=$(dirname "$gcda")
            gcov -n "$gcda" -o "$dir" 2>/dev/null | while IFS= read -r line; do
                if [[ "$line" =~ Lines\ executed:([0-9.]+)%\ of\ ([0-9]+) ]]; then
                    pct="${BASH_REMATCH[1]}"
                    lines="${BASH_REMATCH[2]}"
                    src=$(echo "$line" | grep -oE "File '.*'" || true)
                    printf "  %6s%%  (%3d lines)  %s\n" "$pct" "$lines" "$src"
                fi
            done
        done <<< "$gcov_files"
    else
        echo "  No .gcda files found. Ensure tests ran with --coverage."
    fi
fi

exit 0
