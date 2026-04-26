#!/bin/bash
# Launch GDB for flybox debugging via OpenOCD
# Prerequisites: OpenOCD running (use flybox_openocd.sh)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ELF=~/flybox_build/flybox_vsf.elf
GDBINIT="flybox.gdbinit"

if [ ! -f "$ELF" ]; then
    echo "[ERROR] ELF not found: $ELF — run flybox_build_flash.sh build first"
    exit 1
fi

# Pick whichever cross GDB is installed.
if command -v arm-none-eabi-gdb >/dev/null 2>&1; then
    GDB=arm-none-eabi-gdb
elif command -v gdb-multiarch >/dev/null 2>&1; then
    GDB="gdb-multiarch -ex 'set arch armv7e-m'"
else
    echo "[ERROR] No GDB found. Install gdb-multiarch:"
    echo "        sudo apt-get install gdb-multiarch"
    exit 1
fi

# cd into script dir so the relative `source gd32f4xx_debug/...` lines in
# flybox.gdbinit resolve correctly.
cd "$SCRIPT_DIR"
eval "$GDB -x \"\$GDBINIT\" \"\$@\""
