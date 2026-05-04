#!/usr/bin/env bash
# Apply all patches in tooling/patches/ to vsf/ in order.
# Idempotent-ish: existing patches will show "already applied" but won't error.
#
# Usage:
#   bash tooling/patches/apply.sh           # apply all
#   bash tooling/patches/apply.sh --check   # dry run, check if all apply
#   bash tooling/patches/apply.sh --revert  # revert all patches

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PATCH_DIR="$ROOT/tooling/patches"
VSF_DIR="$ROOT/vsf"

mode="apply"
case "${1:-}" in
    --check) mode="check" ;;
    --revert) mode="revert" ;;
    "") mode="apply" ;;
    *) echo "Unknown arg: $1"; exit 2 ;;
esac

if [ ! -d "$VSF_DIR" ]; then
    echo "ERROR: vsf/ directory not found at $VSF_DIR"
    exit 1
fi

# Find all numbered patches in lex order (0001-xxx.patch < 0002-yyy.patch < ...)
patches=$(ls "$PATCH_DIR"/[0-9]*-*.patch 2>/dev/null | sort || true)

if [ -z "$patches" ]; then
    echo "No patches found in $PATCH_DIR"
    exit 0
fi

count=0
for p in $patches; do
    name=$(basename "$p")
    count=$((count + 1))
    case $mode in
        apply)
            # git apply handles both patch and git-diff formats
            if git -C "$ROOT" apply --check "$p" 2>/dev/null; then
                echo "[$count] Applying $name..."
                git -C "$ROOT" apply "$p"
            else
                # Check if already applied
                if git -C "$ROOT" apply --check --reverse "$p" 2>/dev/null; then
                    echo "[$count] $name: already applied, skipping"
                else
                    echo "[$count] $name: CONFLICT — manual resolution needed"
                    exit 1
                fi
            fi
            ;;
        check)
            if git -C "$ROOT" apply --check "$p" 2>/dev/null; then
                echo "[$count] $name: OK (would apply cleanly)"
            elif git -C "$ROOT" apply --check --reverse "$p" 2>/dev/null; then
                echo "[$count] $name: already applied"
            else
                echo "[$count] $name: CONFLICT"
            fi
            ;;
        revert)
            if git -C "$ROOT" apply --check --reverse "$p" 2>/dev/null; then
                echo "[$count] Reverting $name..."
                git -C "$ROOT" apply --reverse "$p"
            else
                echo "[$count] $name: not applied, skipping"
            fi
            ;;
    esac
done

echo "Done ($count patches)."
