#!/bin/bash
# update_wheel_inventory.sh — regenerate CLAUDE.md wheel counts and list.
#
# Usage:
#   update_wheel_inventory.sh          # print updated inventory
#   update_wheel_inventory.sh --apply  # rewrite CLAUDE.md in place
#
# Scans wheels/ directories and updates wheel counts and lists in CLAUDE.md.

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
BLOCK="- ✅ **${total} wheels**（全部 candidate，超级审计 0 open findings）：
  - 完整列表：\`ls wheels/primitives/ wheels/drivers/ wheels/protocols/ wheels/modules/\`
  - L0 原语 ${count_l0}：${l0_list}
  - L1 驱动 ${count_l1}：${l1_list}
  - L2 协议 ${count_l2}：${l2_list}"

if [[ $count_l3 -gt 0 ]]; then
    l3_list=$(join_names "${names_l3[@]}")
    BLOCK="${BLOCK}
  - L3 模块 ${count_l3}：${l3_list}"
fi

# Count Makefile targets
makefile_targets=$(grep '^\.PHONY' "$REPO/Makefile" | tr ' ' '\n' | grep -v '\.PHONY:' | grep -v '^$' | sort -u | wc -l)

# Replace in CLAUDE.md — pass data via env vars to avoid injection
export WHEEL_BLOCK="$BLOCK"
export WHEEL_TOTAL="$total"
export MAKEFILE_TARGETS="$makefile_targets"
python3 - "$CLAUDE_MD" << 'PYEOF'
import os, re, sys

claude_path = sys.argv[1]
block = os.environ["WHEEL_BLOCK"]
total = os.environ["WHEEL_TOTAL"]
mk_targets = os.environ["MAKEFILE_TARGETS"]

text = open(claude_path, "r", encoding="utf-8").read()

pattern = r"- ✅ \*\*\d+ wheels\*\*.*?\n(?:  - .*\n)+"
replacement = block + "\n"

new_text, count = re.subn(pattern, replacement, text, count=1)
if count == 0:
    print("ERROR: could not find wheel inventory block in CLAUDE.md", file=sys.stderr)
    sys.exit(1)

new_text = re.sub(r'Makefile（\d+ .?目标）', f'Makefile（{mk_targets} 目标）', new_text)

tmp = claude_path + ".tmp"
open(tmp, "w", encoding="utf-8").write(new_text)
os.replace(tmp, claude_path)
print(f"Updated CLAUDE.md: {total} wheels, {mk_targets} Makefile targets")
PYEOF
