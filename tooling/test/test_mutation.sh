#!/usr/bin/env bash
# test_mutation.sh — mutation testing for wheel host tests.
#
# Applies small source-level mutations one at a time, rebuilds, and checks
# whether the existing tests catch the change.  Survived mutations expose
# test gaps; killed mutations confirm coverage.
#
# Usage:
#   test_mutation.sh                     # all non-vendor wheels
#   test_mutation.sh --wheel crc16       # single wheel
#   test_mutation.sh --quick             # first 3 operators only
#   test_mutation.sh --verbose           # print each mutation attempt
#   test_mutation.sh --limit 30          # max 30 mutations per wheel
#   test_mutation.sh --wheel log --verbose --limit 20
#
# Exit 0 if every wheel achieves >= 85% kill rate; 1 otherwise.

set -euo pipefail

# Require GNU sed for \b word-boundary support
if ! sed --version 2>/dev/null | grep -q 'GNU'; then
    echo "WARNING: GNU sed required for mutation testing (\\b word boundary)." >&2
    echo "  On macOS: brew install gnu-sed && export PATH=\"\$(brew --prefix)/opt/gnu-sed/libexec/gnubin:\$PATH\"" >&2
fi

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
LAYERS=(primitives drivers protocols modules)
KILL_THRESHOLD=85   # minimum acceptable kill rate (%)
TEST_TIMEOUT=30     # seconds per test run
BUILD_DIR_NAME="build_mut"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
DIM='\033[2m'
NC='\033[0m'

# ---------------------------------------------------------------------------
# Mutation operators — each entry is a sed substitution
# ---------------------------------------------------------------------------

MUTATION_OPS=(
    # Boundary mutations
    's/< /<=/'
    's/<=/< /'
    's/> />=/'
    's/>=/> /'
    # Arithmetic
    's/+ /- /'
    's/- /+ /'
    's/\* /\/ /'
    # Logic
    's/&&/||/'
    's/||/&&/'
    's/!=/==/'
    's/==/!=/'
    # Return value
    's/return 0/return -1/'
    's/return -1/return 0/'
    # Constant — NOTE: \b is GNU sed word boundary (not POSIX)
    's/\b1\b/0/'
    's/\b0\b/1/'
)

MUTATION_LABELS=(
    "boundary: < -> <="
    "boundary: <= -> <"
    "boundary: > -> >="
    "boundary: >= -> >"
    "arith: + -> -"
    "arith: - -> +"
    "arith: * -> /"
    "logic: && -> ||"
    "logic: || -> &&"
    "logic: != -> =="
    "logic: == -> !="
    "return: 0 -> -1"
    "return: -1 -> 0"
    "const: 1 -> 0"
    "const: 0 -> 1"
)

# ---------------------------------------------------------------------------
# CLI parsing
# ---------------------------------------------------------------------------

WHEEL_FILTER=""
QUICK=false
VERBOSE=false
LIMIT=50

while [[ $# -gt 0 ]]; do
    case "$1" in
        --wheel)  shift; WHEEL_FILTER="$1"; shift ;;
        --quick)  QUICK=true; shift ;;
        --verbose) VERBOSE=true; shift ;;
        --limit)  shift; LIMIT="$1"; shift ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown argument: $1${NC}" >&2
            exit 1
            ;;
    esac
done

if $QUICK; then
    OP_COUNT=3
else
    OP_COUNT=${#MUTATION_OPS[@]}
fi

# ---------------------------------------------------------------------------
# Cleanup trap — always restore originals and remove all temp dirs
# ---------------------------------------------------------------------------

# Single parent temp dir for ALL mutation work (backups + build dirs).
# Cleaned up by the trap regardless of how the script exits.
MUT_TMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/mutation_XXXXXX")

BACKUP_FILE=""
ORIGINAL_FILE=""
TMP_BUILD_DIR=""

cleanup() {
    # Restore the source file if we were interrupted mid-mutation
    if [[ -n "$BACKUP_FILE" && -f "$BACKUP_FILE" && -n "$ORIGINAL_FILE" ]]; then
        cp "$BACKUP_FILE" "$ORIGINAL_FILE"
    fi
    # Remove the entire parent temp dir (covers all build dirs and backups)
    rm -rf "$MUT_TMPDIR"
}
trap cleanup EXIT INT TERM

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

log_verbose() {
    if $VERBOSE; then
        echo -e "    ${DIM}$*${NC}"
    fi
}

# Find the test directory for a wheel.  Returns empty string if not found.
find_test_dir() {
    local wheel_dir="$1"
    local name="$2"
    local test_dir="$wheel_dir/test"
    if [[ -d "$test_dir" && -f "$test_dir/CMakeLists.txt" ]]; then
        echo "$test_dir"
        return
    fi
    # Some L0 primitives have CMakeLists.txt at wheel root level
    if [[ -f "$wheel_dir/CMakeLists.txt" ]]; then
        echo "$wheel_dir"
        return
    fi
    echo ""
}

# Build the test.  Returns 0 on success, 1 on failure.
build_test() {
    local test_dir="$1"
    local build_dir="$2"

    mkdir -p "$build_dir"
    if ! cmake -GNinja -S "$test_dir" -B "$build_dir" > "$build_dir/cmake.log" 2>&1; then
        return 1
    fi
    if ! cmake --build "$build_dir" > "$build_dir/build.log" 2>&1; then
        return 1
    fi
    return 0
}

# Run the test executable.  Returns 0 if tests pass, 1 if fail/timeout.
run_test() {
    local build_dir="$1"
    local name="$2"

    # Locate the test executable
    local exe=""
    for candidate in \
        "$build_dir/test_${name}" \
        "$build_dir/${name}_test" \
        "$build_dir/test_main"; do
        if [[ -x "$candidate" ]]; then
            exe="$candidate"
            break
        fi
    done

    # Fallback: first executable in build dir
    if [[ -z "$exe" ]]; then
        exe="$(find "$build_dir" -maxdepth 1 -type f -executable 2>/dev/null | head -1 || true)"
    fi

    if [[ -z "$exe" ]]; then
        # No executable — treat as build failure
        return 1
    fi

    if timeout "$TEST_TIMEOUT" "$exe" > "$build_dir/test.log" 2>&1; then
        return 0
    else
        return 1
    fi
}

# Apply a sed mutation to a specific line of a file.
# $1 = file, $2 = line number, $3 = sed expression
apply_mutation() {
    local file="$1"
    local lineno="$2"
    local expr="$3"

    sed -i "${lineno}${expr}" "$file"
}

# ---------------------------------------------------------------------------
# Discover wheels
# ---------------------------------------------------------------------------

declare -a WHEELS=()   # "name|wheel_dir|layer"

for layer in "${LAYERS[@]}"; do
    layer_dir="$REPO/wheels/$layer"
    [[ -d "$layer_dir" ]] || continue

    for wheel_dir in "$layer_dir"/*/; do
        [[ -d "$wheel_dir" ]] || continue
        name="$(basename "$wheel_dir")"

        # Skip non-wheel entries
        [[ "$name" == "README.md" ]] && continue
        [[ "$name" == "__pycache__" ]] && continue

        # Skip vendor wheels (have vendor/ subdir, no <name>.c)
        if [[ -d "$wheel_dir/vendor" ]]; then
            log_verbose "skip vendor: $name"
            continue
        fi

        # Skip _draft
        [[ "$layer" == "_draft" ]] && continue

        # Must have a main .c file
        if [[ ! -f "$wheel_dir/${name}.c" ]]; then
            continue
        fi

        # Apply filter
        if [[ -n "$WHEEL_FILTER" && "$name" != "$WHEEL_FILTER" ]]; then
            continue
        fi

        WHEELS+=("${name}|${wheel_dir}|${layer}")
    done
done

if [[ ${#WHEELS[@]} -eq 0 ]]; then
    echo -e "${RED}No matching wheels found.${NC}"
    exit 1
fi

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------

echo -e "${BOLD}=== test_mutation.sh ===${NC}"
echo -e "  Operators: ${OP_COUNT}/${#MUTATION_OPS[@]}  Limit: ${LIMIT}/wheel  Threshold: ${KILL_THRESHOLD}%"
echo ""

GLOBAL_PASS=true
declare -a RESULT_LINES=()

for entry in "${WHEELS[@]}"; do
    IFS='|' read -r name wheel_dir layer <<< "$entry"
    src_file="$wheel_dir/${name}.c"
    test_dir="$(find_test_dir "$wheel_dir" "$name")"

    if [[ -z "$test_dir" ]]; then
        echo -e "  ${YELLOW}SKIP${NC}  ${layer}/${name}  (no test found)"
        RESULT_LINES+=("$(printf "  %-30s  %s" "${layer}/${name}" "SKIP (no test)")")
        continue
    fi

    # Verify baseline: the test must pass unmutated
    TMP_BUILD_DIR=$(mktemp -d "$MUT_TMPDIR/build_${name}_XXXXXX")
    if ! build_test "$test_dir" "$TMP_BUILD_DIR"; then
        echo -e "  ${YELLOW}SKIP${NC}  ${layer}/${name}  (baseline build failed)"
        RESULT_LINES+=("$(printf "  %-30s  %s" "${layer}/${name}" "SKIP (build fail)")")
        rm -rf "$TMP_BUILD_DIR"
        TMP_BUILD_DIR=""
        continue
    fi
    if ! run_test "$TMP_BUILD_DIR" "$name"; then
        echo -e "  ${YELLOW}SKIP${NC}  ${layer}/${name}  (baseline test fails)"
        RESULT_LINES+=("$(printf "  %-30s  %s" "${layer}/${name}" "SKIP (test fail)")")
        rm -rf "$TMP_BUILD_DIR"
        TMP_BUILD_DIR=""
        continue
    fi
    rm -rf "$TMP_BUILD_DIR"
    TMP_BUILD_DIR=""

    echo -e "  ${CYAN}TEST${NC}  ${layer}/${name}"

    # Back up the original source
    BACKUP_FILE=$(mktemp "$MUT_TMPDIR/${name}_backup_XXXXXX.c")
    ORIGINAL_FILE="$src_file"
    cp "$src_file" "$BACKUP_FILE"

    total=0
    killed=0
    survived=0
    build_fail=0
    declare -a survived_list=()

    for (( op_idx=0; op_idx < OP_COUNT; op_idx++ )); do
        op="${MUTATION_OPS[$op_idx]}"
        label="${MUTATION_LABELS[$op_idx]}"

        # Extract the search pattern from the sed expression to find matching lines.
        # The operator format is 's/PATTERN/REPLACEMENT/' — extract PATTERN.
        search_pattern=$(echo "$op" | sed -n 's|^s/\(.*\)/.*/$|\1|p')
        if [[ -z "$search_pattern" ]]; then
            # Fallback: try without trailing /
            search_pattern=$(echo "$op" | sed -n 's|^s/\(.*\)/.*$|\1|p')
        fi

        if [[ -z "$search_pattern" ]]; then
            continue
        fi

        # Find lines in the source that match (skip comments and strings)
        matching_lines=()
        while IFS= read -r lineno; do
            [[ -n "$lineno" ]] && matching_lines+=("$lineno")
        done < <(grep -n "$search_pattern" "$src_file" 2>/dev/null \
            | grep -v '^\s*//' \
            | grep -v '^\s*\*' \
            | grep -v '^\s*#' \
            | cut -d: -f1 || true)

        for lineno in "${matching_lines[@]}"; do
            if [[ $total -ge $LIMIT ]]; then
                break 2
            fi

            total=$((total + 1))

            # Apply the mutation to the specific line
            cp "$BACKUP_FILE" "$src_file"
            apply_mutation "$src_file" "$lineno" "$op"

            # Check if the mutation actually changed anything
            if diff -q "$BACKUP_FILE" "$src_file" > /dev/null 2>&1; then
                # No change — sed pattern did not match on this line after all
                total=$((total - 1))
                cp "$BACKUP_FILE" "$src_file"
                continue
            fi

            TMP_BUILD_DIR=$(mktemp -d "$MUT_TMPDIR/build_${name}_XXXXXX")

            if ! build_test "$test_dir" "$TMP_BUILD_DIR"; then
                build_fail=$((build_fail + 1))
                log_verbose "KILLED(build)  L${lineno}  ${label}"
            else
                if ! run_test "$TMP_BUILD_DIR" "$name"; then
                    killed=$((killed + 1))
                    log_verbose "KILLED(test)   L${lineno}  ${label}"
                else
                    survived=$((survived + 1))
                    survived_list+=("L${lineno}: ${label}")
                    log_verbose "${RED}SURVIVED${NC}       L${lineno}  ${label}"
                fi
            fi

            rm -rf "$TMP_BUILD_DIR"
            TMP_BUILD_DIR=""

            # Restore original
            cp "$BACKUP_FILE" "$src_file"
        done
    done

    # Restore original one final time (belt and suspenders)
    cp "$BACKUP_FILE" "$src_file"
    rm -f "$BACKUP_FILE"
    BACKUP_FILE=""
    ORIGINAL_FILE=""

    # Calculate kill rate
    if [[ $total -eq 0 ]]; then
        echo -e "    ${YELLOW}No applicable mutations found${NC}"
        RESULT_LINES+=("$(printf "  %-30s  %s" "${layer}/${name}" "0 mutations (n/a)")")
        continue
    fi

    killed_total=$((killed + build_fail))
    kill_rate=$(( (killed_total * 100) / total ))

    if [[ $kill_rate -ge $KILL_THRESHOLD ]]; then
        color="$GREEN"
    elif [[ $kill_rate -ge 60 ]]; then
        color="$YELLOW"
    else
        color="$RED"
    fi

    echo -e "    killed ${killed}  build_fail ${build_fail}  survived ${survived}  total ${total}  ${color}${kill_rate}%${NC}"

    if [[ $survived -gt 0 ]]; then
        echo -e "    ${RED}Survived mutations:${NC}"
        for s in "${survived_list[@]}"; do
            echo -e "      - ${s}"
        done
    fi

    RESULT_LINES+=("$(printf "  %-30s  %4d total  %4d killed  %4d build  %4d survived  %3d%%" \
        "${layer}/${name}" "$total" "$killed" "$build_fail" "$survived" "$kill_rate")")

    if [[ $kill_rate -lt $KILL_THRESHOLD ]]; then
        GLOBAL_PASS=false
    fi
done

# ---------------------------------------------------------------------------
# Summary table
# ---------------------------------------------------------------------------

echo ""
echo -e "${BOLD}--- Mutation Testing Summary ---${NC}"
echo ""
printf "  ${BOLD}%-30s  %5s  %6s  %5s  %8s  %4s${NC}\n" \
    "Wheel" "Total" "Killed" "Build" "Survived" "Rate"
echo "  $(printf '%.0s-' {1..80})"

for line in "${RESULT_LINES[@]}"; do
    echo -e "$line"
done

echo ""

if $GLOBAL_PASS; then
    echo -e "  ${GREEN}All wheels meet the ${KILL_THRESHOLD}% kill rate threshold.${NC}"
    exit 0
else
    echo -e "  ${RED}Some wheels are below the ${KILL_THRESHOLD}% kill rate threshold.${NC}"
    exit 1
fi
