#!/bin/bash
# find_all_refs.sh — find ALL references to a wheel across code + docs.
#
# Usage:
#   find_all_refs.sh crc16              # find all references to crc16
#   find_all_refs.sh crc16 --code-only  # skip documentation
#   find_all_refs.sh crc16 --docs-only  # skip code
#
# Unlike test_downstream.sh (which finds build/test consumers), this
# finds EVERY mention: #include, CMakeLists, README deps, CLAUDE.md
# wheel lists, arm-binary-sizes.md, audit reports, memory files, etc.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"

BOLD='\033[1m'
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

NAME=""
CODE_ONLY=false
DOCS_ONLY=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --code-only) CODE_ONLY=true; shift ;;
        --docs-only) DOCS_ONLY=true; shift ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0 ;;
        *) NAME="$1"; shift ;;
    esac
done

[[ -z "$NAME" ]] && { echo "Usage: find_all_refs.sh <wheel-name> [--code-only|--docs-only]"; exit 1; }

# Find the wheel's own directory (to exclude self-references)
SELF_DIR=""
for layer in primitives drivers protocols modules _draft; do
    if [[ -d "$REPO/wheels/$layer/$NAME" ]]; then
        SELF_DIR="wheels/$layer/$NAME"
        break
    fi
done

TOTAL=0

# --- Code references ---
if ! $DOCS_ONLY; then
    echo -e "${BOLD}=== Code references ===${NC}"

    echo -e "\n${CYAN}#include references:${NC}"
    COUNT=0
    while IFS= read -r f; do
        [[ -z "$f" ]] && continue
        rel="${f#$REPO/}"
        [[ -n "$SELF_DIR" && "$rel" == "$SELF_DIR"/* ]] && continue
        echo "  $rel"
        COUNT=$((COUNT + 1))
    done < <(grep -rl "#include.*[\"<]${NAME}\.h[>\"]" "$REPO" \
        --include="*.c" --include="*.h" \
        --exclude-dir=build --exclude-dir=build_* --exclude-dir=.git \
        --exclude-dir=__pycache__ --exclude-dir=node_modules \
        2>/dev/null || true)
    [[ $COUNT -eq 0 ]] && echo "  (none)"
    TOTAL=$((TOTAL + COUNT))

    echo -e "\n${CYAN}CMakeLists.txt references:${NC}"
    COUNT=0
    while IFS= read -r f; do
        [[ -z "$f" ]] && continue
        rel="${f#$REPO/}"
        [[ -n "$SELF_DIR" && "$rel" == "$SELF_DIR"/* ]] && continue
        echo "  $rel"
        COUNT=$((COUNT + 1))
    done < <(grep -rlF "$NAME" "$REPO" \
        --include="CMakeLists.txt" --include="*.cmake" \
        --exclude-dir=build --exclude-dir=build_* --exclude-dir=.git \
        2>/dev/null | grep -vF "${SELF_DIR:-__NOMATCH__}" || true)
    [[ $COUNT -eq 0 ]] && echo "  (none)"
    TOTAL=$((TOTAL + COUNT))
fi

# --- Documentation references ---
if ! $CODE_ONLY; then
    echo -e "\n${BOLD}=== Documentation references ===${NC}"

    echo -e "\n${CYAN}docs/ mentions:${NC}"
    COUNT=0
    while IFS= read -r f; do
        [[ -z "$f" ]] && continue
        echo "  ${f#$REPO/}"
        COUNT=$((COUNT + 1))
    done < <(grep -rlF "$NAME" "$REPO/docs" --include="*.md" 2>/dev/null || true)
    [[ $COUNT -eq 0 ]] && echo "  (none)"
    TOTAL=$((TOTAL + COUNT))

    echo -e "\n${CYAN}CLAUDE.md:${NC}"
    if grep -qF "$NAME" "$REPO/CLAUDE.md" 2>/dev/null; then
        grep -nF "$NAME" "$REPO/CLAUDE.md" | head -5 | sed 's/^/  /'
        TOTAL=$((TOTAL + 1))
    else
        echo "  (not mentioned)"
    fi

    echo -e "\n${CYAN}Other wheel READMEs (Dependencies/cross-refs):${NC}"
    COUNT=0
    while IFS= read -r f; do
        [[ -z "$f" ]] && continue
        rel="${f#$REPO/}"
        [[ -n "$SELF_DIR" && "$rel" == "$SELF_DIR"/* ]] && continue
        echo "  $rel"
        COUNT=$((COUNT + 1))
    done < <(grep -rlF "$NAME" "$REPO/wheels" --include="README.md" 2>/dev/null || true)
    [[ $COUNT -eq 0 ]] && echo "  (none)"
    TOTAL=$((TOTAL + COUNT))

    echo -e "\n${CYAN}CI yaml:${NC}"
    if grep -q "$NAME" "$REPO/.github/workflows/ci.yml" 2>/dev/null; then
        grep -n "$NAME" "$REPO/.github/workflows/ci.yml" | head -5 | sed 's/^/  /'
        TOTAL=$((TOTAL + 1))
    else
        echo "  (not mentioned)"
    fi
fi

echo -e "\n${BOLD}=== Total: $TOTAL reference(s) outside wheel's own directory ===${NC}"
