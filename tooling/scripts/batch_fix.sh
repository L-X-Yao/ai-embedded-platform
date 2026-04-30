#!/bin/bash
# batch_fix.sh — apply a transformation across multiple wheels with auto-verify.
#
# Usage:
#   batch_fix.sh --sed 's/old/new/g' --file '*.c' crc16 bmp280 mpu6050
#   batch_fix.sh --py 'fix_script.py' --all
#   batch_fix.sh --sed 's/old/new/g' --file '*.h' --all --dry-run
#
# Modes:
#   --sed <expr>   Apply sed expression to matching files in each wheel
#   --py <script>  Run python script with wheel dir as argument
#
# Scope:
#   <wheel names>  Specific wheels
#   --all          All non-vendor wheels
#   --layer <L>    All wheels in a layer (primitives/drivers/protocols/modules)
#
# Options:
#   --file <glob>  File pattern within wheel dir (default: *.c *.h)
#   --dry-run      Show what would change without modifying
#   --verify       Run test_all.sh for each modified wheel (default: on)
#   --no-verify    Skip verification

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"

SED_EXPR=""
PY_SCRIPT=""
FILE_GLOB=""
DRY_RUN=false
VERIFY=true
ALL_MODE=false
LAYER_FILTER=""
WHEELS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --sed)       SED_EXPR="$2"; shift 2 ;;
        --py)        PY_SCRIPT="$2"; shift 2 ;;
        --file)      FILE_GLOB="$2"; shift 2 ;;
        --dry-run)   DRY_RUN=true; shift ;;
        --verify)    VERIFY=true; shift ;;
        --no-verify) VERIFY=false; shift ;;
        --all)       ALL_MODE=true; shift ;;
        --layer)     LAYER_FILTER="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0 ;;
        *)           WHEELS+=("$1"); shift ;;
    esac
done

[[ -z "$SED_EXPR" && -z "$PY_SCRIPT" ]] && { echo "Error: --sed or --py required"; exit 1; }
[[ ${#WHEELS[@]} -eq 0 && "$ALL_MODE" = false && -z "$LAYER_FILTER" ]] && {
    echo "Error: specify wheel names, --all, or --layer"
    exit 1
}

# Discover wheel directories
WHEEL_DIRS=()
for layer in primitives drivers protocols modules; do
    [[ -n "$LAYER_FILTER" && "$layer" != "$LAYER_FILTER" ]] && continue
    for d in "$REPO"/wheels/"$layer"/*/; do
        [[ ! -d "$d" ]] && continue
        name=$(basename "$d")
        [[ -d "$d/vendor" ]] && continue
        if $ALL_MODE || [[ -n "$LAYER_FILTER" ]]; then
            WHEEL_DIRS+=("$d")
        elif printf '%s\n' "${WHEELS[@]}" | grep -qx "$name"; then
            WHEEL_DIRS+=("$d")
        fi
    done
done

echo "=== batch_fix.sh ($($DRY_RUN && echo DRY-RUN || echo APPLY)) ==="
echo "  Wheels: ${#WHEEL_DIRS[@]}"
echo ""

MODIFIED=()
FAILED=()

for wd in "${WHEEL_DIRS[@]}"; do
    name=$(basename "$wd")

    if [[ -n "$SED_EXPR" ]]; then
        pattern="${FILE_GLOB:-*.[ch]}"
        files=$(find "$wd" -maxdepth 1 -name "$pattern" 2>/dev/null)
        [[ -z "$files" ]] && continue

        changed=false
        for f in $files; do
            if $DRY_RUN; then
                if sed -n "${SED_EXPR}p" "$f" | grep -q .; then
                    echo "  [would fix] $(basename "$f") in $name"
                    changed=true
                fi
            else
                before=$(md5sum "$f" | cut -d' ' -f1)
                sed -i "$SED_EXPR" "$f"
                after=$(md5sum "$f" | cut -d' ' -f1)
                if [[ "$before" != "$after" ]]; then
                    echo "  [fixed] $(basename "$f") in $name"
                    changed=true
                fi
            fi
        done
        $changed && MODIFIED+=("$name")
    fi

    if [[ -n "$PY_SCRIPT" ]]; then
        if $DRY_RUN; then
            echo "  [would run] python3 $PY_SCRIPT $wd"
            MODIFIED+=("$name")
        else
            if python3 "$PY_SCRIPT" "$wd" 2>&1; then
                echo "  [fixed] $name"
                MODIFIED+=("$name")
            else
                echo "  [FAIL] $name"
                FAILED+=("$name")
            fi
        fi
    fi
done

echo ""
echo "Modified: ${#MODIFIED[@]}  Failed: ${#FAILED[@]}"

# Verify
if $VERIFY && ! $DRY_RUN && [[ ${#MODIFIED[@]} -gt 0 ]]; then
    echo ""
    echo "=== Verifying modified wheels ==="
    bash "$REPO/tooling/scripts/test_all.sh" "${MODIFIED[@]}" 2>&1
fi
