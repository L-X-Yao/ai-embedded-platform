#!/bin/bash
# Flybox VSF: build on native WSL FS + flash via OpenOCD
# Usage: ./flybox_build_flash.sh [build|flash|both]
set -e

BUILD_DIR=~/flybox_build
SRC_DIR=/mnt/d/temp/project/ai-embedded-platform/projects/flybox/vsf_build
BIN=$BUILD_DIR/flybox_vsf.bin
ELF=$BUILD_DIR/flybox_vsf.elf
FLASH_ADDR=0x08010000

do_build() {
    if [ ! -f "$BUILD_DIR/build.ninja" ]; then
        echo "[BUILD] Configuring..."
        mkdir -p "$BUILD_DIR"
        cmake -GNinja -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja -DAPP_SLOT=A \
              -S "$SRC_DIR" -B "$BUILD_DIR"
    fi
    echo "[BUILD] Compiling..."
    time ninja -C "$BUILD_DIR"
    arm-none-eabi-objcopy -O binary "$ELF" "$BIN"
    echo "[BUILD] Binary: $(stat -c%s "$BIN") bytes"
}

do_flash() {
    echo "[FLASH] Programming $FLASH_ADDR..."
    openocd -f interface/cmsis-dap.cfg \
        -c "adapter speed 1000" \
        -f target/stm32f4x.cfg \
        -c "cortex_m reset_config vectreset" \
        -c "init; reset halt; stm32f4x unlock 0; reset halt; program $BIN $FLASH_ADDR; reset run; shutdown"
}

case "${1:-both}" in
    build) do_build ;;
    flash) do_flash ;;
    both)  do_build && do_flash ;;
    *) echo "Usage: $0 [build|flash|both]" ;;
esac
