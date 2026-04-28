#!/bin/bash
# test_downstream.sh — find and test all consumers of a given wheel.
#
# Usage:
#   test_downstream.sh crc16              # test everything that depends on crc16
#   test_downstream.sh crc16 fal          # multiple wheels
#   test_downstream.sh --list crc16       # just list consumers, don't build
#   test_downstream.sh --all              # test all cross-wheel dependency chains
#
# Finds two kinds of downstream consumers:
#   - Wheel-to-wheel: other wheels whose test/CMakeLists.txt references the
#     source wheel via relative path (e.g. ../../../primitives/crc16/)
#   - Project consumers: project CMakeLists.txt or #include references
#
# Wheel consumers are built and tested automatically.  Project consumers are
# reported only (cross-compile toolchains vary per project).
#
# Exit 0 if all downstream tests pass; 1 if any fail; 2 if no consumers found.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR_NAME="build_td"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

LIST_ONLY=false
ALL_MODE=false
WHEELS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --list) LIST_ONLY=true; shift ;;
        --all)  ALL_MODE=true; shift ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0
            ;;
        *)
            WHEELS+=("$1"); shift ;;
    esac
done

if $ALL_MODE; then
    # Discover all upstream wheels referenced via cross-wheel paths
    while IFS= read -r match; do
        name=$(echo "$match" | sed -nE 's#.*\.\./\.\./\.\./(primitives|drivers|protocols|modules)/([a-z0-9_]+)/.*#\2#p' | head -1)
        [[ -n "$name" ]] && WHEELS+=("$name")
    done < <(grep -rh '\.\./\.\./\.\.\/' "$REPO/wheels" --include="CMakeLists.txt" 2>/dev/null || true)
    # Deduplicate
    mapfile -t WHEELS < <(printf '%s\n' "${WHEELS[@]}" | sort -u)
fi

if [[ ${#WHEELS[@]} -eq 0 ]]; then
    echo "Usage: test_downstream.sh [--list] [--all] <wheel_name> [wheel_name ...]"
    exit 1
fi

# --- Find consumers -----------------------------------------------------------

declare -A WHEEL_CONSUMERS=()   # "consumer_name|test_dir" keyed by unique id
declare -A PROJECT_CONSUMERS=() # "project|file" keyed by unique id

find_consumers() {
    local target_name="$1"

    # Find the wheel's layer and path
    local wheel_path=""
    for layer in primitives drivers protocols modules _draft; do
        if [[ -d "$REPO/wheels/$layer/$target_name" ]]; then
            wheel_path="wheels/$layer/$target_name"
            break
        fi
    done

    if [[ -z "$wheel_path" ]]; then
        echo -e "${RED}Warning: wheel '$target_name' not found in wheels/${NC}" >&2
        return
    fi

    # 1. Wheel-to-wheel: other wheel test/CMakeLists.txt referencing this wheel
    while IFS= read -r cml; do
        local test_dir; test_dir="$(dirname "$cml")"
        local consumer_wheel_dir; consumer_wheel_dir="$(dirname "$test_dir")"
        local consumer_name; consumer_name="$(basename "$consumer_wheel_dir")"
        # Don't list a wheel as its own consumer
        [[ "$consumer_name" == "$target_name" ]] && continue
        local key="${consumer_name}|${test_dir}"
        WHEEL_CONSUMERS["$key"]="$target_name"
    done < <(grep -rl "$wheel_path\|/$target_name/" "$REPO/wheels" \
        --include="CMakeLists.txt" 2>/dev/null || true)

    # 2. Project consumers: CMakeLists.txt references
    while IFS= read -r ref_file; do
        local proj_dir
        proj_dir=$(echo "$ref_file" | sed -n "s|$REPO/projects/\([^/]*\)/.*|\1|p")
        [[ -z "$proj_dir" ]] && continue
        local rel="${ref_file#$REPO/}"
        local key="${proj_dir}|${rel}"
        PROJECT_CONSUMERS["$key"]="$target_name"
    done < <(grep -rl "$wheel_path" "$REPO/projects" \
        --include="*.txt" --include="*.cmake" \
        --exclude-dir="build" --exclude-dir="build_*" \
        2>/dev/null || true)

    # 3. Project consumers: #include references
    local header="${target_name}.h"
    while IFS= read -r ref_file; do
        local proj_dir
        proj_dir=$(echo "$ref_file" | sed -n "s|$REPO/projects/\([^/]*\)/.*|\1|p")
        [[ -z "$proj_dir" ]] && continue
        local rel="${ref_file#$REPO/}"
        local key="${proj_dir}|${rel}"
        PROJECT_CONSUMERS["$key"]="$target_name"
    done < <(grep -rl "#include.*[\"<]${header}[>\"]" "$REPO/projects" \
        --include="*.c" --include="*.h" \
        --exclude-dir="build" --exclude-dir="build_*" \
        2>/dev/null || true)
}

echo -e "${BOLD}=== Downstream consumer scan ===${NC}"
echo ""

for w in "${WHEELS[@]}"; do
    echo -e "  Scanning consumers of ${CYAN}$w${NC}..."
    find_consumers "$w"
done

# --- Report -------------------------------------------------------------------

echo ""
total_wheel=${#WHEEL_CONSUMERS[@]}
total_project=${#PROJECT_CONSUMERS[@]}

if [[ $total_wheel -eq 0 && $total_project -eq 0 ]]; then
    echo -e "${YELLOW}No downstream consumers found for: ${WHEELS[*]}${NC}"
    exit 2
fi

echo -e "${BOLD}Wheel consumers (will build+test):${NC}"
if [[ $total_wheel -eq 0 ]]; then
    echo "  (none)"
else
    for key in "${!WHEEL_CONSUMERS[@]}"; do
        IFS='|' read -r name test_dir <<< "$key"
        via="${WHEEL_CONSUMERS[$key]}"
        echo -e "  ${CYAN}$name${NC} (via $via) — $test_dir"
    done
fi

echo ""
declare -A seen_projects=()
echo -e "${BOLD}Project consumers (report only):${NC}"
if [[ $total_project -eq 0 ]]; then
    echo "  (none)"
else
    for key in "${!PROJECT_CONSUMERS[@]}"; do
        IFS='|' read -r proj file <<< "$key"
        via="${PROJECT_CONSUMERS[$key]}"
        if [[ -z "${seen_projects[$proj]:-}" ]]; then
            echo -e "  ${CYAN}$proj${NC} (via $via)"
            seen_projects["$proj"]=1
        fi
    done
    echo -e "  ${YELLOW}(project builds require specific toolchains — run manually or via CI)${NC}"
fi

if $LIST_ONLY; then
    exit 0
fi

# --- Build and test wheel consumers ------------------------------------------

if [[ $total_wheel -eq 0 ]]; then
    echo ""
    echo -e "${GREEN}No wheel consumers to test.${NC}"
    exit 0
fi

echo ""
echo -e "${BOLD}=== Building downstream wheel tests ===${NC}"

PASS=0
FAIL=0
SKIP=0

for key in "${!WHEEL_CONSUMERS[@]}"; do
    IFS='|' read -r name test_dir <<< "$key"
    echo ""
    echo -e "${BOLD}--- $name ---${NC}"

    # Check for VSF dependency
    if grep -q "vsf\.cmake" "$test_dir/CMakeLists.txt" 2>/dev/null; then
        vsf_path="${VSF_PATH:-$REPO/vsf}"
        if [[ ! -d "$vsf_path" ]]; then
            echo -e "  ${YELLOW}SKIP: requires VSF (VSF_PATH not set)${NC}"
            SKIP=$((SKIP + 1))
            continue
        fi
        export VSF_PATH="$vsf_path"
    fi

    build_dir="$test_dir/$BUILD_DIR_NAME"
    rm -rf "$build_dir"

    if cmake -GNinja -S "$test_dir" -B "$build_dir" > /dev/null 2>&1 && \
       cmake --build "$build_dir" > /dev/null 2>&1; then
        # Find test executable
        test_exe="$build_dir/test_${name}"
        if [[ -x "$test_exe" ]]; then
            if "$test_exe"; then
                echo -e "  ${GREEN}PASS${NC}"
                PASS=$((PASS + 1))
            else
                echo -e "  ${RED}FAIL (test returned non-zero)${NC}"
                FAIL=$((FAIL + 1))
            fi
        else
            echo -e "  ${YELLOW}SKIP: test_${name} not found in build${NC}"
            SKIP=$((SKIP + 1))
        fi
    else
        echo -e "  ${RED}FAIL (build error)${NC}"
        FAIL=$((FAIL + 1))
    fi
done

# --- Summary ------------------------------------------------------------------

echo ""
echo -e "${BOLD}=== Summary ===${NC}"
echo -e "  Wheel consumers tested: $((PASS + FAIL + SKIP))"
echo -e "  ${GREEN}Pass: $PASS${NC}  ${RED}Fail: $FAIL${NC}  ${YELLOW}Skip: $SKIP${NC}"
echo -e "  Project consumers (manual): ${#seen_projects[@]}"

[[ $FAIL -gt 0 ]] && exit 1
exit 0
