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
        --sed)
            [[ $# -ge 2 ]] || { echo "ERROR: --sed requires a value"; exit 1; }
            SED_EXPR="$2"
            # Validate sed expression with a no-op test
            echo "" | sed "$SED_EXPR" > /dev/null 2>&1 || \
                { echo "ERROR: invalid sed expression: $SED_EXPR"; exit 1; }
            shift 2 ;;
        --py)
            [[ $# -ge 2 ]] || { echo "ERROR: --py requires a value"; exit 1; }
            PY_SCRIPT="$2"
            [[ -f "$PY_SCRIPT" ]] || { echo "ERROR: script not found: $PY_SCRIPT"; exit 1; }
            shift 2 ;;
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

# --- Collect files that will be affected (for backup/rollback) ---
AFFECTED_FILES=()

for wd in "${WHEEL_DIRS[@]}"; do
    name=$(basename "$wd")

    if [[ -n "$SED_EXPR" ]]; then
        pattern="${FILE_GLOB:-*.[ch]}"
        mapfile -t files < <(find "$wd" -maxdepth 1 -name "$pattern" 2>/dev/null)
        for f in "${files[@]}"; do
            AFFECTED_FILES+=("$f")
        done
    fi
done

# --- Create backup before applying changes ---
BACKUP_DIR=""
if ! $DRY_RUN && [[ ${#AFFECTED_FILES[@]} -gt 0 || -n "$PY_SCRIPT" ]]; then
    BACKUP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/batch_fix_backup.XXXXXX")
    echo "  backup: $BACKUP_DIR"
    for f in "${AFFECTED_FILES[@]}"; do
        rel="${f#$REPO/}"
        mkdir -p "$BACKUP_DIR/$(dirname "$rel")"
        cp "$f" "$BACKUP_DIR/$rel"
    done
fi

# Trap: clean up the backup dir on INT/TERM (interrupted before success).
# On normal exit the explicit rm-rf at the end of the script runs first;
# on interrupt we just remove the temp dir without rolling back changes
# because the rollback() function is reserved for verification failure.
_SCRIPT_DONE=false
cleanup_on_interrupt() {
    if [[ "$_SCRIPT_DONE" = false && -n "$BACKUP_DIR" && -d "$BACKUP_DIR" ]]; then
        echo "  Interrupted — rolling back changes from backup..."
        for f in $(find "$BACKUP_DIR" -type f 2>/dev/null); do
            rel="${f#$BACKUP_DIR/}"
            cp "$f" "$REPO/$rel" 2>/dev/null && echo "    restored: $rel"
        done
        rm -rf "$BACKUP_DIR"
    fi
}
trap cleanup_on_interrupt EXIT INT TERM

rollback() {
    if [[ -n "$BACKUP_DIR" && -d "$BACKUP_DIR" ]]; then
        echo ""
        echo "=== ROLLING BACK changes ==="
        for f in "${AFFECTED_FILES[@]}"; do
            rel="${f#$REPO/}"
            bak="$BACKUP_DIR/$rel"
            if [[ -f "$bak" ]]; then
                cp "$bak" "$f"
            fi
        done
        echo "  restored ${#AFFECTED_FILES[@]} file(s) from backup"
        echo "  backup retained at: $BACKUP_DIR"
    fi
}

for wd in "${WHEEL_DIRS[@]}"; do
    name=$(basename "$wd")

    if [[ -n "$SED_EXPR" ]]; then
        pattern="${FILE_GLOB:-*.[ch]}"
        mapfile -t files < <(find "$wd" -maxdepth 1 -name "$pattern" 2>/dev/null)
        [[ ${#files[@]} -eq 0 ]] && continue

        changed=false
        for f in "${files[@]}"; do
            if $DRY_RUN; then
                diff_out=$(diff <(cat "$f") <(sed "$SED_EXPR" "$f") 2>&1) || true
                if [[ -n "$diff_out" ]]; then
                    echo "  [would fix] $(basename "$f") in $name"
                    echo "$diff_out"
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

# Verify — rollback on failure
if $VERIFY && ! $DRY_RUN && [[ ${#MODIFIED[@]} -gt 0 ]]; then
    echo ""
    echo "=== Verifying modified wheels ==="
    if ! bash "$REPO/tooling/test/test_all.sh" "${MODIFIED[@]}" 2>&1; then
        echo ""
        echo "=== Verification FAILED ==="
        rollback
        exit 1
    fi
    echo ""
    echo "=== Verification passed — changes kept ==="
fi

# Clean up backup on success
_SCRIPT_DONE=true
if [[ -n "$BACKUP_DIR" && -d "$BACKUP_DIR" ]]; then
    rm -rf "$BACKUP_DIR"
fi
