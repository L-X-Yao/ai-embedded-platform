#!/bin/bash
# release.sh — create a platform release tag with auto-generated changelog.
#
# Usage:
#   release.sh v0.1.0                 # create release
#   release.sh v0.2.0 --from v0.1.0   # changelog since v0.1.0
#   release.sh --list                 # list existing releases
#   release.sh --current              # show latest release tag
#   release.sh --dry-run v0.1.0       # preview without tagging
#
# The tag is a lightweight git tag on HEAD. The changelog is appended
# to CHANGELOG.md at repo root.
#
# Version format: vMAJOR.MINOR.PATCH
#   MAJOR: breaking wheel API change or architecture shift
#   MINOR: new wheels, features, significant fixes
#   PATCH: bug fixes, doc updates, tooling improvements

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
CHANGELOG="$REPO/CHANGELOG.md"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m'

DRY_RUN=false
LIST=false
CURRENT=false
VERSION=""
FROM=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --list)    LIST=true; shift ;;
        --current) CURRENT=true; shift ;;
        --dry-run) DRY_RUN=true; shift ;;
        --from)
            [[ $# -lt 2 ]] && { echo "Error: --from requires a version argument"; exit 1; }
            FROM="$2"
            if ! echo "$FROM" | grep -qE '^v[0-9]+\.[0-9]+\.[0-9]+$'; then
                echo "Error: --from must be vMAJOR.MINOR.PATCH (got: $FROM)"
                exit 1
            fi
            shift 2 ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0 ;;
        v*)        VERSION="$1"; shift ;;
        *)         echo "Unknown arg: $1"; exit 1 ;;
    esac
done

# --- List mode ---
if $LIST; then
    echo -e "${BOLD}Platform releases:${NC}"
    git -C "$REPO" tag -l 'v*' --sort=-version:refname | while read -r t; do
        date=$(git -C "$REPO" log -1 --format="%ci" "$t" 2>/dev/null | cut -d' ' -f1)
        commit=$(git -C "$REPO" rev-parse --short "$t" 2>/dev/null)
        echo "  $t  ($date, $commit)"
    done
    exit 0
fi

if $CURRENT; then
    latest=$(git -C "$REPO" tag -l 'v*' --sort=-version:refname | head -1)
    if [[ -z "$latest" ]]; then
        echo "(no releases yet)"
    else
        echo "$latest"
    fi
    exit 0
fi

# --- Release mode ---
[[ -z "$VERSION" ]] && { echo "Usage: release.sh <version> [--from <prev>] [--dry-run]"; exit 1; }

if ! echo "$VERSION" | grep -qE '^v[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "ERROR: version must match vMAJOR.MINOR.PATCH (got: $VERSION)"
    exit 1
fi

if git -C "$REPO" tag -l "$VERSION" | grep -q "$VERSION"; then
    echo "ERROR: tag $VERSION already exists"
    exit 1
fi

# Determine base for changelog
if [[ -z "$FROM" ]]; then
    FROM=$(git -C "$REPO" tag -l 'v*' --sort=-version:refname | head -1)
fi

echo -e "${BOLD}=== Release: $VERSION ===${NC}"
echo "  From: ${FROM:-<initial>}"
echo "  To:   HEAD ($(git -C "$REPO" rev-parse --short HEAD))"
$DRY_RUN && echo -e "  ${YELLOW}(dry run)${NC}"
echo ""

# --- Collect stats ---
if [[ -n "$FROM" ]]; then
    COMMITS=$(git -C "$REPO" rev-list --count "$FROM"..HEAD 2>/dev/null || echo "?")
    LOG=$(git -C "$REPO" log --oneline "$FROM"..HEAD 2>/dev/null)
else
    COMMITS=$(git -C "$REPO" rev-list --count HEAD 2>/dev/null || echo "?")
    LOG=$(git -C "$REPO" log --oneline 2>/dev/null)
fi

# Count wheels
L0=$(find "$REPO/wheels/primitives" -mindepth 1 -maxdepth 1 -type d ! -name __pycache__ 2>/dev/null | wc -l)
L1=$(find "$REPO/wheels/drivers" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l)
L2=$(find "$REPO/wheels/protocols" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l)
L3=$(find "$REPO/wheels/modules" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l)
TOTAL_WHEELS=$((L0 + L1 + L2 + L3))

# Count tests
TEST_COUNT=$(find "$REPO/wheels" -name 'test_*.c' -o -name 'test_*.py' 2>/dev/null | wc -l)

# Categorize commits
WHEEL_COMMITS=$(echo "$LOG" | grep -c '^[a-f0-9]* wheel' || true)
TOOL_COMMITS=$(echo "$LOG" | grep -c '^[a-f0-9]* tool' || true)
DOC_COMMITS=$(echo "$LOG" | grep -c '^[a-f0-9]* docs' || true)
CI_COMMITS=$(echo "$LOG" | grep -c '^[a-f0-9]* ci' || true)
PROJECT_COMMITS=$(echo "$LOG" | grep -c '^[a-f0-9]* project' || true)
PORT_COMMITS=$(echo "$LOG" | grep -c '^[a-f0-9]* port' || true)

DATE=$(date +%Y-%m-%d)

# --- Build changelog entry ---
ENTRY="## $VERSION ($DATE)

**Wheels:** $TOTAL_WHEELS (L0:$L0 L1:$L1 L2:$L2 L3:$L3) | **Tests:** $TEST_COUNT | **Commits since ${FROM:-initial}:** $COMMITS

### Changes by type
| Type | Count |
|------|------:|
| wheel | $WHEEL_COMMITS |
| tool | $TOOL_COMMITS |
| docs | $DOC_COMMITS |
| ci | $CI_COMMITS |
| project | $PROJECT_COMMITS |
| port | $PORT_COMMITS |

### Commit log
\`\`\`
$LOG
\`\`\`
"

echo -e "${BOLD}Changelog entry:${NC}"
echo "$ENTRY"

if $DRY_RUN; then
    echo -e "${YELLOW}Dry run — no tag created, CHANGELOG not updated.${NC}"
    exit 0
fi

# --- Write changelog ---
if [[ -f "$CHANGELOG" ]]; then
    # Insert after the first heading line
    HEADER=$(head -2 "$CHANGELOG")
    BODY=$(tail -n +3 "$CHANGELOG")
    echo "$HEADER" > "$CHANGELOG"
    echo "" >> "$CHANGELOG"
    echo "$ENTRY" >> "$CHANGELOG"
    echo "$BODY" >> "$CHANGELOG"
else
    cat > "$CHANGELOG" << HEADER
# Changelog

$ENTRY
HEADER
fi

# --- Create tag ---
git -C "$REPO" tag "$VERSION"

echo ""
echo -e "${GREEN}✓ Tagged: $VERSION${NC}"
echo -e "${GREEN}✓ Updated: CHANGELOG.md${NC}"
echo ""
echo "Next steps:"
echo "  1. Review: git diff CHANGELOG.md"
echo "  2. Commit: git add CHANGELOG.md && git commit -m 'docs: add $VERSION release notes'"
echo "  3. Push:   git push && git push --tags"
