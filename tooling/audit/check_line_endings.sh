#!/usr/bin/env bash
# check_line_endings.sh — detect CRLF line endings in tracked files that
# .gitattributes mandates as eol=lf.
#
# Purpose: guard against CRLF creeping back after a renormalize pass.
# Reads .gitattributes to discover which extensions/filenames require LF,
# then checks every matching tracked file in the working tree for \r\n bytes.
#
# Usage:
#   check_line_endings.sh            # check all tracked eol=lf files
#   check_line_endings.sh --quiet    # one-line summary only
#
# Exit codes:
#   0 — all files have LF line endings
#   1 — one or more files contain CRLF

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"

BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

QUIET=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --quiet|-q) QUIET=true; shift ;;
        -h|--help)
            echo "Usage: check_line_endings.sh [--quiet]"
            exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

# -----------------------------------------------------------------------
# Parse .gitattributes for patterns that require eol=lf
# -----------------------------------------------------------------------
GITATTR="$REPO/.gitattributes"
if [[ ! -f "$GITATTR" ]]; then
    echo "check-eol: .gitattributes not found — nothing to check"
    exit 0
fi

patterns=()
while IFS= read -r line; do
    # Skip blank lines and comments
    [[ -z "$line" || "$line" =~ ^# ]] && continue
    # Only care about lines with eol=lf
    echo "$line" | grep -q 'eol=lf' || continue
    # Extract the pattern (first whitespace-delimited token)
    pat="$(echo "$line" | awk '{print $1}')"
    [[ -n "$pat" ]] && patterns+=("$pat")
done < "$GITATTR"

if [[ ${#patterns[@]} -eq 0 ]]; then
    echo "check-eol: no eol=lf patterns found in .gitattributes"
    exit 0
fi

if ! $QUIET; then
    echo -e "${BOLD}=== Line Ending Check (CRLF guard) ===${NC}"
    echo "  Patterns from .gitattributes: ${patterns[*]}"
    echo ""
fi

# -----------------------------------------------------------------------
# Collect tracked files matching the patterns via git ls-files
# -----------------------------------------------------------------------
file_list="$(mktemp)"
trap 'rm -f "$file_list"' EXIT

for pat in "${patterns[@]}"; do
    # git ls-files supports glob patterns directly
    git -C "$REPO" ls-files -- "$pat" >> "$file_list" 2>/dev/null || true
done

# Deduplicate
sort -u -o "$file_list" "$file_list"

total=0
crlf_count=0
crlf_files=()

while IFS= read -r relpath; do
    [[ -z "$relpath" ]] && continue
    # Skip vendored / upstream paths we do not normalize
    [[ "$relpath" == vsf/* ]] && continue
    filepath="$REPO/$relpath"
    # Skip files that no longer exist in the working tree (deleted but tracked)
    [[ -f "$filepath" ]] || continue

    total=$((total + 1))

    # Check for \r bytes — any \r in a text file indicates CRLF
    if grep -qP '\r' "$filepath" 2>/dev/null; then
        crlf_count=$((crlf_count + 1))
        crlf_files+=("$relpath")
        if ! $QUIET; then
            echo -e "  ${RED}CRLF${NC}  $relpath"
        fi
    fi
done < "$file_list"

# -----------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------
echo ""
if [[ $crlf_count -eq 0 ]]; then
    echo -e "${GREEN}check-eol: $total files checked, all LF${NC}"
    exit 0
else
    if $QUIET; then
        for f in "${crlf_files[@]}"; do
            echo -e "  ${RED}CRLF${NC}  $f"
        done
    fi
    echo -e "${RED}check-eol: $crlf_count/$total file(s) contain CRLF line endings${NC}"
    echo ""
    echo "Fix with:  git add --renormalize . && git commit -m \"fix: renormalize line endings\""
    exit 1
fi
