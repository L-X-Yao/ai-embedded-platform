#!/bin/bash
# update_wheel_inventory.sh — regenerate CLAUDE.md wheel counts and list.
#
# Usage:
#   update_wheel_inventory.sh          # print updated inventory
#   update_wheel_inventory.sh --apply  # rewrite CLAUDE.md in place
#
# Scans wheels/ directories and updates the "58 个 wheels" section in CLAUDE.md.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
CLAUDE_MD="$REPO/CLAUDE.md"

APPLY=false
[[ "${1:-}" == "--apply" ]] && APPLY=true

count_l0=0; count_l1=0; count_l2=0; count_l3=0
names_l0=(); names_l1=(); names_l2=(); names_l3=()

for d in "$REPO"/wheels/primitives/*/; do
    [[ ! -d "$d" ]] && continue
    name=$(basename "$d")
    [[ "$name" == "__pycache__" ]] && continue
    names_l0+=("$name"); count_l0=$((count_l0 + 1))
done

for d in "$REPO"/wheels/drivers/*/; do
    [[ ! -d "$d" ]] && continue
    name=$(basename "$d")
    names_l1+=("$name"); count_l1=$((count_l1 + 1))
done

for d in "$REPO"/wheels/protocols/*/; do
    [[ ! -d "$d" ]] && continue
    name=$(basename "$d")
    names_l2+=("$name"); count_l2=$((count_l2 + 1))
done

for d in "$REPO"/wheels/modules/*/; do
    [[ ! -d "$d" ]] && continue
    name=$(basename "$d")
    names_l3+=("$name"); count_l3=$((count_l3 + 1))
done

total=$((count_l0 + count_l1 + count_l2 + count_l3))

join_names() {
    local IFS=" / "
    echo "$*"
}

l0_list=$(join_names "${names_l0[@]}")
l1_list=$(join_names "${names_l1[@]}")
l2_list=$(join_names "${names_l2[@]}")

echo "Wheel inventory:"
echo "  L0 (${count_l0}): ${l0_list}"
echo "  L1 (${count_l1}): ${l1_list}"
echo "  L2 (${count_l2}): ${l2_list}"
if [[ $count_l3 -gt 0 ]]; then
    l3_list=$(join_names "${names_l3[@]}")
    echo "  L3 (${count_l3}): ${l3_list}"
fi
echo "  Total: ${total}"

if ! $APPLY; then
    echo ""
    echo "(use --apply to update CLAUDE.md)"
    exit 0
fi

# Build replacement block
BLOCK="- ✅ **${total} 个 wheels**（全部 candidate，audit 0 non-consumer WARN）：
  - L0 (${count_l0}): ${l0_list}
  - L1 (${count_l1}): ${l1_list}
  - L2 (${count_l2}): ${l2_list}"

if [[ $count_l3 -gt 0 ]]; then
    l3_list=$(join_names "${names_l3[@]}")
    BLOCK="${BLOCK}
  - L3 (${count_l3}): ${l3_list}"
fi

# Replace in CLAUDE.md using sed: find the wheels block and replace
# Pattern: line starting with "- ✅ **N 个 wheels**" through the last "  - L" line
python3 -c "
import re, sys

text = open('$CLAUDE_MD', 'r', encoding='utf-8').read()

# Match from '- ✅ **N 个 wheels**' through consecutive '  - L' lines
pattern = r'- ✅ \*\*\d+ 个 wheels\*\*.*?\n(?:  - L\d.*\n)+'
replacement = '''$BLOCK\n'''

new_text, count = re.subn(pattern, replacement, text, count=1)
if count == 0:
    print('ERROR: could not find wheel inventory block in CLAUDE.md', file=sys.stderr)
    sys.exit(1)

open('$CLAUDE_MD', 'w', encoding='utf-8').write(new_text)
print(f'Updated CLAUDE.md: {$total} wheels')
"
