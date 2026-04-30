#!/bin/bash
# wheel_promote.sh — automate wheel stage promotion.
#
# Usage:
#   wheel_promote.sh _draft/mywheel drivers/mywheel
#   wheel_promote.sh --dry-run _draft/mywheel primitives/mywheel
#
# Steps performed:
#   1. Move wheel directory
#   2. Update CI yaml paths (if existing job references old path)
#   3. Remove matching entry from WISHLIST.md
#   4. Update all projects/*/CLAUDE.md wheel table references
#   5. Update all CMakeLists.txt / .c / .h that reference the old path
#   6. Print summary of changes for review
#
# The script does NOT commit — review the diff and commit manually.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
CI_YAML="$REPO/.github/workflows/ci.yml"
WISHLIST="$REPO/wheels/WISHLIST.md"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m'

DRY_RUN=false

# --- Args ---------------------------------------------------------------------

FROM=""
TO=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) DRY_RUN=true; shift ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0
            ;;
        *)
            if [[ -z "$FROM" ]]; then
                FROM="$1"
            elif [[ -z "$TO" ]]; then
                TO="$1"
            else
                echo "Error: too many args" >&2; exit 1
            fi
            shift
            ;;
    esac
done

[[ -z "$FROM" || -z "$TO" ]] && {
    echo "Usage: wheel_promote.sh [--dry-run] <from> <to>"
    echo "  e.g.: wheel_promote.sh _draft/mywheel drivers/mywheel"
    exit 1
}

FROM_DIR="$REPO/wheels/$FROM"
TO_DIR="$REPO/wheels/$TO"
WHEEL_NAME=$(basename "$TO")
FROM_NAME=$(basename "$FROM")

# Validate
[[ -d "$FROM_DIR" ]] || { echo -e "${RED}Error: $FROM_DIR does not exist${NC}"; exit 1; }
[[ -e "$TO_DIR" ]]   && { echo -e "${RED}Error: $TO_DIR already exists${NC}"; exit 1; }

# Ensure target layer dir exists
TO_LAYER_DIR=$(dirname "$TO_DIR")
[[ -d "$TO_LAYER_DIR" ]] || { echo -e "${RED}Error: layer dir $TO_LAYER_DIR does not exist${NC}"; exit 1; }

echo -e "${BOLD}=== Wheel Promote ===${NC}"
echo -e "  From: wheels/$FROM"
echo -e "  To:   wheels/$TO"
echo -e "  Name: $WHEEL_NAME"
$DRY_RUN && echo -e "  ${YELLOW}(dry run — no changes will be made)${NC}"
echo ""

# --- Pre-flight checks -------------------------------------------------------

echo -e "${BOLD}[Pre-flight] Validating wheel readiness${NC}"
PREFLIGHT_OK=true

[[ -f "$FROM_DIR/README.md" ]] || { echo -e "  ${RED}FAIL: $FROM_DIR/README.md missing${NC}"; PREFLIGHT_OK=false; }

test_dir=""
if [[ -d "$FROM_DIR/test" ]]; then
    test_dir="$FROM_DIR/test"
elif ls "$FROM_DIR"/test_*.c > /dev/null 2>&1; then
    test_dir="$FROM_DIR"
else
    echo -e "  ${RED}FAIL: no test directory or test_*.c found${NC}"
    PREFLIGHT_OK=false
fi

if [[ -n "$test_dir" ]] && ! ls "$test_dir"/CMakeLists.txt > /dev/null 2>&1 && ! ls "$FROM_DIR"/CMakeLists.txt > /dev/null 2>&1; then
    echo -e "  ${RED}FAIL: no CMakeLists.txt found for test build${NC}"
    PREFLIGHT_OK=false
fi

$PREFLIGHT_OK && echo -e "  ${GREEN}All pre-flight checks passed${NC}" || { echo -e "  ${RED}Fix pre-flight issues before promoting${NC}"; exit 1; }

from_layer=$(echo "$FROM" | cut -d'/' -f1)
if [[ "$from_layer" == "_draft" ]] && [[ -f "$FROM_DIR/README.md" ]]; then
    if grep -q "STATUS:.*draft" "$FROM_DIR/README.md" 2>/dev/null; then
        echo -e "  ${YELLOW}REMIND: Update README STATUS from 'draft' to 'candidate' after move${NC}"
    fi
fi
echo ""

CHANGES=0

# --- Step 1: Move directory ---------------------------------------------------

echo -e "${BOLD}[1/5] Move directory${NC}"
echo "  git mv wheels/$FROM → wheels/$TO"
if ! $DRY_RUN; then
    git mv "$FROM_DIR" "$TO_DIR"
fi
CHANGES=$((CHANGES + 1))

# --- Step 2: Update CI yaml --------------------------------------------------

echo -e "${BOLD}[2/5] Update CI yaml${NC}"
if grep -q "wheels/$FROM" "$CI_YAML" 2>/dev/null; then
    count=$(grep -c "wheels/$FROM" "$CI_YAML")
    echo "  Found $count reference(s) to wheels/$FROM"
    if ! $DRY_RUN; then
        sed -i "s|wheels/$FROM|wheels/$TO|g" "$CI_YAML"
    fi
    echo "  → replaced with wheels/$TO"
    CHANGES=$((CHANGES + 1))
else
    echo -e "  ${YELLOW}No CI references to wheels/$FROM (may need manual job addition)${NC}"
fi

# --- Step 3: Remove from WISHLIST --------------------------------------------

echo -e "${BOLD}[3/5] Update WISHLIST.md${NC}"
if [[ -f "$WISHLIST" ]]; then
    # Match lines containing the wheel name as a markdown code span
    if grep -qiE "\`${WHEEL_NAME}\`|\`${FROM_NAME}\`" "$WISHLIST"; then
        echo "  Found WISHLIST entry matching '$WHEEL_NAME'"
        if ! $DRY_RUN; then
            # Remove the matching table row (line containing the name)
            sed -i "/\`${WHEEL_NAME}\`/Id" "$WISHLIST"
            # Also try the from-name if different
            if [[ "$FROM_NAME" != "$WHEEL_NAME" ]]; then
                sed -i "/\`${FROM_NAME}\`/Id" "$WISHLIST"
            fi
        fi
        echo "  → removed from WISHLIST"
        CHANGES=$((CHANGES + 1))
    else
        echo "  No WISHLIST entry for '$WHEEL_NAME'"
    fi
else
    echo "  WISHLIST.md not found"
fi

# --- Step 4: Update project CLAUDE.md files -----------------------------------

echo -e "${BOLD}[4/5] Update project CLAUDE.md wheel tables${NC}"
while IFS= read -r claude_file; do
    if grep -q "wheels/$FROM\|$FROM_NAME" "$claude_file" 2>/dev/null; then
        proj=$(basename "$(dirname "$claude_file")")
        echo "  $proj/CLAUDE.md: found reference"
        if ! $DRY_RUN; then
            sed -i "s|wheels/$FROM|wheels/$TO|g" "$claude_file"
            # Update layer references in wheel tables (e.g. "_draft" → "drivers")
            from_layer=$(echo "$FROM" | cut -d'/' -f1)
            to_layer=$(echo "$TO" | cut -d'/' -f1)
            if [[ "$from_layer" != "$to_layer" ]]; then
                sed -i "s|\`wheels/${from_layer}/${WHEEL_NAME}/\`|\`wheels/${to_layer}/${WHEEL_NAME}/\`|g" "$claude_file"
            fi
        fi
        echo "  → updated paths"
        CHANGES=$((CHANGES + 1))
    fi
done < <(find "$REPO/projects" -name "CLAUDE.md" 2>/dev/null)

# --- Step 5: Update source references -----------------------------------------

echo -e "${BOLD}[5/5] Update source references (CMakeLists.txt, .c, .h)${NC}"
ref_files=$(grep -rl "wheels/$FROM" "$REPO/projects" "$REPO/wheels" \
    --include="*.txt" --include="*.c" --include="*.h" --include="*.cmake" \
    --exclude-dir="build" --exclude-dir="build_*" --exclude-dir=".cmake" \
    2>/dev/null || true)
if [[ -n "$ref_files" ]]; then
    while IFS= read -r ref_file; do
        rel=${ref_file#$REPO/}
        echo "  $rel"
        if ! $DRY_RUN; then
            sed -i "s|wheels/$FROM|wheels/$TO|g" "$ref_file"
        fi
        CHANGES=$((CHANGES + 1))
    done <<< "$ref_files"
    echo "  → updated"
else
    echo "  No source references to wheels/$FROM"
fi

# --- Auto-verify (test + audit) -----------------------------------------------

if ! $DRY_RUN; then
    echo ""
    echo -e "${BOLD}[Auto] Running tests for $WHEEL_NAME${NC}"
    if bash "$REPO/tooling/scripts/test_all.sh" "$WHEEL_NAME" 2>&1; then
        echo -e "  ${GREEN}Tests passed${NC}"
    else
        echo -e "  ${RED}Tests FAILED — review before committing${NC}"
    fi

    echo ""
    echo -e "${BOLD}[Auto] Running wheel audit${NC}"
    if python3 "$REPO/tooling/scripts/audit_wheels.py" --quiet 2>&1; then
        echo -e "  ${GREEN}Audit passed${NC}"
    else
        echo -e "  ${YELLOW}Audit warnings — review output above${NC}"
    fi
fi

# --- Summary ------------------------------------------------------------------

echo ""
echo -e "${BOLD}=== Summary ===${NC}"
echo -e "  Changes: $CHANGES"

# --- Vendor wheel check (ADR-010) ---
if [[ -d "$TO_DIR/vendor" ]] && [[ ! -f "$TO_DIR/VENDOR.md" ]]; then
    echo -e "  ${RED}WARNING: vendor/ dir found but VENDOR.md missing — ADR-010 requires it${NC}"
fi

if $DRY_RUN; then
    echo -e "  ${YELLOW}Dry run — no files were modified. Remove --dry-run to execute.${NC}"
else
    echo -e "  ${GREEN}Done.${NC} Review with: git diff"
    echo ""
    echo "  Next steps:"
    echo "    1. git diff  — review all changes"
    echo "    2. git add + commit"
fi
