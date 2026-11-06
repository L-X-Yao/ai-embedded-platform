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
#   6. Test the wheel and its consumers, and run the audit
#   7. Print summary of changes for review
#
# The script does NOT commit — review the diff and commit manually.
# Exit 1 if a step 6 check fails; the move stays in the tree for review.

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

# Validate wheel name (safe for use in sed/grep patterns)
[[ "$WHEEL_NAME" =~ ^[a-z][a-z0-9_]*$ ]] || \
    { echo -e "${RED}Error: invalid wheel name '$WHEEL_NAME' (must match [a-z][a-z0-9_]*)${NC}"; exit 1; }

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
    if grep -qE "wheels/$FROM|$FROM_NAME" "$claude_file" 2>/dev/null; then
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
# A wheel is named four ways: wheels/<layer>/<name> from projects and examples,
# ${WHEELS}/<layer>/<name> or ${WHEELS_DIR}/... from examples,
# ../../../<layer>/<name> from any wheel, and ../../<name> from its own layer.
to_layer=$(echo "$TO" | cut -d'/' -f1)
REF_FILTER=(--include="*.txt" --include="*.c" --include="*.h" --include="*.cmake"
            --exclude-dir="build" --exclude-dir="build_*" --exclude-dir=".cmake")
END='([^a-z0-9_]|$)'    # so crc16 does not match crc16_extra
ref_count=0

# rewrite_refs <pattern> <sed replacement> <search roots...>
rewrite_refs() {
    local pattern="$1" replacement="$2" ref_file
    shift 2
    while IFS= read -r ref_file; do
        [[ -z "$ref_file" ]] && continue
        echo "  ${ref_file#$REPO/}"
        if ! $DRY_RUN; then
            sed -i -E "s#$pattern#$replacement#g" "$ref_file"
        fi
        ref_count=$((ref_count + 1))
    done < <(grep -rlE "$pattern" "$@" "${REF_FILTER[@]}" 2>/dev/null || true)
}

rewrite_refs "(wheels/|\.\./|\\\$\{WHEELS(_DIR)?\}/)$FROM$END" "\1$TO\3" \
    "$REPO/projects" "$REPO/wheels" "$REPO/examples"

if [[ "$from_layer" != "$to_layer" ]]; then
    rewrite_refs "\.\./\.\./$FROM_NAME$END" "../../../$TO\1" "$REPO/wheels/$from_layer"

    # The moved wheel's own ../../<sibling> now lands in the wrong layer.
    moved_dir="$TO_DIR"
    $DRY_RUN && moved_dir="$FROM_DIR"
    while IFS= read -r sibling; do
        [[ -d "$REPO/wheels/$from_layer/$sibling" ]] || continue
        rewrite_refs "\.\./\.\./$sibling$END" "../../../$from_layer/$sibling\1" "$moved_dir"
    done < <(grep -rhoE '\.\./\.\./[a-z0-9_]+/' "$moved_dir" "${REF_FILTER[@]}" 2>/dev/null \
                 | sed -E 's#^\.\./\.\./##; s#/$##' | sort -u || true)
fi

if [[ $ref_count -gt 0 ]]; then
    echo "  → updated"
    CHANGES=$((CHANGES + ref_count))
else
    echo "  No source references to $FROM"
fi

tool_refs=$(grep -rlE "wheels/$FROM$END" "$REPO/tooling" \
    --include="*.sh" --include="*.py" --include="*.c" 2>/dev/null || true)
if [[ -n "$tool_refs" ]]; then
    echo -e "  ${YELLOW}Update these by hand, they list wheels by path:${NC}"
    echo "$tool_refs" | sed "s|^$REPO/|    |"
fi

# --- Auto-verify (test + consumers + audit) -----------------------------------

VERIFY_OK=true
if ! $DRY_RUN; then
    echo ""
    echo -e "${BOLD}[Auto] Running tests for $WHEEL_NAME${NC}"
    if bash "$REPO/tooling/test/test_all.sh" "$WHEEL_NAME" 2>&1; then
        echo -e "  ${GREEN}Tests passed${NC}"
    else
        echo -e "  ${RED}Tests FAILED — review before committing${NC}"
        VERIFY_OK=false
    fi

    echo ""
    echo -e "${BOLD}[Auto] Testing wheels that use $WHEEL_NAME${NC}"
    downstream_rc=0
    bash "$REPO/tooling/test/test_downstream.sh" "$WHEEL_NAME" 2>&1 || downstream_rc=$?
    case "$downstream_rc" in
        0) echo -e "  ${GREEN}Downstream passed${NC}" ;;
        2) echo "  Downstream: no consumers" ;;
        *) echo -e "  ${RED}Downstream FAILED — review before committing${NC}"
           VERIFY_OK=false ;;
    esac

    echo ""
    echo -e "${BOLD}[Auto] Running wheel audit${NC}"
    if python3 "$REPO/tooling/audit/audit_wheels.py" --quiet 2>&1; then
        echo -e "  ${GREEN}Audit passed${NC}"
    else
        echo -e "  ${RED}Audit FAILED — review output above${NC}"
        VERIFY_OK=false
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
elif ! $VERIFY_OK; then
    echo -e "  ${RED}VERIFICATION FAILED.${NC} The move is left in the tree: review with git diff"
    exit 1
else
    echo -e "  ${GREEN}Done.${NC} Review with: git diff"
    echo ""
    echo "  Next steps:"
    echo "    1. git diff  — review all changes"
    echo "    2. git add + commit"
fi
