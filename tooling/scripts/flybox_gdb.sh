#!/bin/bash
# Launch GDB for flybox debugging via OpenOCD
# Prerequisites: OpenOCD running (use flybox_openocd.sh)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ELF=~/flybox_build/flybox_vsf.elf
GDBINIT="$SCRIPT_DIR/flybox.gdbinit"

if [ ! -f "$ELF" ]; then
    echo "[ERROR] ELF not found: $ELF — run flybox_build_flash.sh build first"
    exit 1
fi

arm-none-eabi-gdb -x "$GDBINIT" "$@"
