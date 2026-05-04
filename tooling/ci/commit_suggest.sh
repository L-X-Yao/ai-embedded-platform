#!/bin/bash
# commit_suggest.sh — generate a draft commit message from staged changes.
#
# Usage:
#   commit_suggest.sh              # print suggested message
#   commit_suggest.sh --commit     # commit with the suggested message
#
# Analyzes git diff --cached to determine:
#   - type (wheel/tool/docs/ci/project/port) from file paths
#   - scope from directory structure
#   - file count and line stats
#
# The suggestion follows CLAUDE.md commit conventions exactly.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
DO_COMMIT=false
[[ "${1:-}" == "--commit" ]] && DO_COMMIT=true

# Check for staged changes
STAT=$(git -C "$REPO" diff --cached --stat 2>/dev/null)
if [[ -z "$STAT" ]]; then
    echo "No staged changes."
    exit 1
fi

FILES=$(git -C "$REPO" diff --cached --name-only 2>/dev/null)
FILE_COUNT=$(echo "$FILES" | wc -l)
INSERTIONS=$(git -C "$REPO" diff --cached --numstat 2>/dev/null | awk '{s+=$1} END {print s+0}')
DELETIONS=$(git -C "$REPO" diff --cached --numstat 2>/dev/null | awk '{s+=$2} END {print s+0}')

# Detect primary type from file paths
declare -A TYPE_COUNTS
while IFS= read -r f; do
    case "$f" in
        wheels/*)          TYPE_COUNTS[wheel]=$(( ${TYPE_COUNTS[wheel]:-0} + 1 )) ;;
        tooling/audit/*|tooling/test/*|tooling/wheel/*|tooling/session/*|tooling/metrics/*|tooling/ci/*|tooling/fix/*|tooling/debug/*|tooling/hil/*|Makefile)
                           TYPE_COUNTS[tool]=$(( ${TYPE_COUNTS[tool]:-0} + 1 )) ;;
        docs/*|*/README.md|CLAUDE.md)
                           TYPE_COUNTS[docs]=$(( ${TYPE_COUNTS[docs]:-0} + 1 )) ;;
        .github/*)         TYPE_COUNTS[ci]=$(( ${TYPE_COUNTS[ci]:-0} + 1 )) ;;
        projects/*)        TYPE_COUNTS[project]=$(( ${TYPE_COUNTS[project]:-0} + 1 )) ;;
        vsf-addon/*)       TYPE_COUNTS[port]=$(( ${TYPE_COUNTS[port]:-0} + 1 )) ;;
        .githooks/*)       TYPE_COUNTS[tool]=$(( ${TYPE_COUNTS[tool]:-0} + 1 )) ;;
        tooling/experiments/*) TYPE_COUNTS[exp]=$(( ${TYPE_COUNTS[exp]:-0} + 1 )) ;;
    esac
done <<< "$FILES"

# Find dominant type
MAX_TYPE="docs"
MAX_COUNT=0
for t in "${!TYPE_COUNTS[@]}"; do
    if [[ ${TYPE_COUNTS[$t]} -gt $MAX_COUNT ]]; then
        MAX_COUNT=${TYPE_COUNTS[$t]}
        MAX_TYPE=$t
    fi
done

# Detect scope
SCOPE=""
case "$MAX_TYPE" in
    wheel)
        # Find which wheels are affected
        WHEEL_NAMES=$(echo "$FILES" | grep '^wheels/' | sed -n 's|^wheels/[^/]*/\([^/]*\)/.*|\1|p' | sort -u)
        WHEEL_COUNT=$(echo "$WHEEL_NAMES" | wc -l)
        if [[ $WHEEL_COUNT -eq 1 ]]; then
            WNAME=$(echo "$WHEEL_NAMES" | head -1)
            WLAYER=$(echo "$FILES" | grep "^wheels/" | head -1 | sed -n 's|^wheels/\([^/]*\)/.*|\1|p')
            SCOPE="${WLAYER}/${WNAME}"
        fi
        ;;
    project)
        PROJ=$(echo "$FILES" | grep '^projects/' | head -1 | sed -n 's|^projects/\([^/]*\)/.*|\1|p')
        [[ -n "$PROJ" ]] && SCOPE="$PROJ"
        ;;
    ci)
        JOB=$(echo "$FILES" | grep '\.github/' | head -1 | sed -n 's|.*workflows/\([^.]*\).*|\1|p')
        [[ -n "$JOB" && "$JOB" != "ci" ]] && SCOPE="$JOB"
        ;;
    port)
        CHIP=$(echo "$FILES" | grep 'vsf-addon/' | head -1 | sed -n 's|.*driver/.*/\([^/]*\)/.*|\1|p' | tr '[:upper:]' '[:lower:]')
        [[ -n "$CHIP" ]] && SCOPE="$CHIP"
        ;;
esac

# Build summary
if [[ -n "$SCOPE" ]]; then
    HEADER="${MAX_TYPE}(${SCOPE}): "
else
    HEADER="${MAX_TYPE}: "
fi

# Generate verb + description
REMAINING=$((68 - ${#HEADER}))
if [[ $FILE_COUNT -eq 1 ]]; then
    FNAME=$(echo "$FILES" | head -1 | xargs basename)
    DESC="update ${FNAME}"
else
    case "$MAX_TYPE" in
        wheel)
            if [[ $WHEEL_COUNT -gt 1 ]]; then
                DESC="update ${WHEEL_COUNT} wheels (+${INSERTIONS}/-${DELETIONS})"
            else
                DESC="update implementation (+${INSERTIONS}/-${DELETIONS})"
            fi
            ;;
        tool)   DESC="update tooling scripts" ;;
        docs)   DESC="update documentation" ;;
        ci)     DESC="update CI pipeline" ;;
        *)      DESC="update ${FILE_COUNT} files" ;;
    esac
fi

# Truncate if too long
if [[ ${#DESC} -gt $REMAINING ]]; then
    DESC="${DESC:0:$((REMAINING - 3))}..."
fi

MESSAGE="${HEADER}${DESC}"

echo "=== Suggested commit message ==="
echo ""
echo "  $MESSAGE"
echo ""
echo "  Files: $FILE_COUNT  +$INSERTIONS/-$DELETIONS"
echo "  Type breakdown: $(for t in "${!TYPE_COUNTS[@]}"; do echo -n "$t=${TYPE_COUNTS[$t]} "; done)"
echo ""

if [[ ${#TYPE_COUNTS[@]} -gt 1 ]]; then
    echo "  WARNING: changes span multiple types. Consider splitting:"
    for t in "${!TYPE_COUNTS[@]}"; do
        echo "    $t: ${TYPE_COUNTS[$t]} files"
    done
    echo ""
fi

if $DO_COMMIT; then
    echo "(auto-commit disabled — review message first, then commit manually)"
fi

echo "To use: git commit -m \"$MESSAGE\""
