#!/bin/bash
# Flybox VSF: build (WSL) + copy + flash (OpenOCD) + verify + serial check
# Runs from Windows bash (Git Bash / Claude Code Bash tool).
# Usage:
#   ./flybox_build_flash.sh              # build + flash + verify + serial check
#   ./flybox_build_flash.sh build        # build only (with size check)
#   ./flybox_build_flash.sh flash        # flash only
#   ./flybox_build_flash.sh check [sec]  # serial check only (reset + capture, default 30s)
set -euo pipefail

WIN_BIN="D:/temp/project/ai-embedded-platform/projects/flybox/vsf_build/flybox_vsf.bin"
FLASH_ADDR=0x08010000
SERIAL_LOG="D:/temp/flybox_serial.log"
SRAM_LIMIT=131072  # 128KB main SRAM

do_build() {
    echo "=== BUILD ==="
    wsl -e bash -c 'cd ~/flybox_build && ninja 2>&1' | tail -5
    wsl -e bash -c 'cp ~/flybox_build/flybox_vsf.bin /mnt/d/temp/project/ai-embedded-platform/projects/flybox/vsf_build/flybox_vsf.bin'

    echo "=== SIZE ==="
    local size_line
    size_line=$(wsl -e bash -c 'arm-none-eabi-size ~/flybox_build/flybox_vsf.elf' | tail -1)
    local text data bss
    text=$(echo "$size_line" | awk '{print $1}')
    data=$(echo "$size_line" | awk '{print $2}')
    bss=$(echo "$size_line" | awk '{print $3}')

    local main_bss=$((bss - 65536))
    if [ "$main_bss" -lt 0 ]; then main_bss=$bss; fi
    local sram_used=$((data + main_bss))
    local sram_free=$((SRAM_LIMIT - sram_used - 4096 - 2048))

    echo "  text=${text}  data=${data}  bss=${bss} (main ~${main_bss} + CCM 65536)"
    echo "  Main SRAM: used=${sram_used}  free=~${sram_free} (for thread stacks)"
    if [ "$sram_free" -lt 20480 ]; then
        echo "  [WARN] SRAM free < 20KB — thread stacks may not fit!"
    fi
}

do_flash() {
    echo "=== FLASH ==="
    local output
    output=$(openocd \
        -f interface/cmsis-dap.cfg \
        -c "adapter speed 1000" \
        -f target/stm32f4x.cfg \
        -c "cortex_m reset_config sysresetreq" \
        -c "init" \
        -c "reset halt" \
        -c "stm32f4x unlock 0" \
        -c "reset halt" \
        -c "flash write_image erase $WIN_BIN $FLASH_ADDR bin" \
        -c "verify_image $WIN_BIN $FLASH_ADDR bin" \
        -c "reset run" \
        -c "shutdown" 2>&1)

    if echo "$output" | grep -q "verified"; then
        echo "$output" | grep "verified"
        echo "[OK] Flash + verify done"
    else
        echo "[FAIL] Verify failed:"
        echo "$output" | tail -15
        return 1
    fi
}

do_check() {
    local wait_sec="${1:-30}"
    echo "=== SERIAL CHECK (${wait_sec}s) ==="

    # Capture is delegated to a standalone PowerShell script so bash heredoc
    # escaping doesn't mangle the dollar-sign syntax. Script opens COM25
    # before triggering reset to avoid losing the early-boot byte burst.
    local capture_ps="$(dirname "$0")/flybox_serial_capture.ps1"
    powershell -NoProfile -ExecutionPolicy Bypass -File "$capture_ps" \
        -WaitSec "$wait_sec" -LogFile "$SERIAL_LOG"
    do_verdict
}

do_verdict() {
    local last_boot
    local last_marker
    last_marker=$(grep -n "\[SYS\] Flybox" "$SERIAL_LOG" 2>/dev/null | tail -1 | cut -d: -f1)
    if [ -n "$last_marker" ]; then
        last_boot=$(tail -n +"$last_marker" "$SERIAL_LOG" 2>/dev/null || true)
    else
        last_boot=$(cat "$SERIAL_LOG" 2>/dev/null || true)
    fi

    echo "--- last boot output ---"
    echo "$last_boot"
    echo ""
    echo "--- verdict ---"

    local ok=1
    local log="$last_boot"

    if echo "$log" | grep -qi "HardFault\|BusFault\|MemManage\|UsageFault"; then
        echo "[FAIL] Fault handler triggered"
        ok=0
    fi
    if echo "$log" | grep -qi "\[WDG\]\[ERROR\]"; then
        echo "[FAIL] Watchdog reset detected"
        ok=0
    fi

    local tasks_ok=0
    echo "$log" | grep -q "\[IMU\] OK" && tasks_ok=$((tasks_ok + 1))
    echo "$log" | grep -q "\[BARO\] OK\|\[BARO\] Retry" && tasks_ok=$((tasks_ok + 1))
    echo "$log" | grep -q "\[LED\] task started" && tasks_ok=$((tasks_ok + 1))
    echo "$log" | grep -q "\[SYS\]\[INFO\] Starting NET" && tasks_ok=$((tasks_ok + 1))

    if [ "$tasks_ok" -ge 3 ]; then
        echo "[OK] Boot OK ($tasks_ok/4 task markers found)"
    else
        echo "[WARN] Only $tasks_ok/4 task markers — incomplete boot?"
        ok=0
    fi

    echo "$log" | grep -q "\[NET\]\[INFO\] SIM/network/PDP ready" && echo "[OK] Network ready"
    echo "$log" | grep -q "MQTT Session Established" && echo "[OK] MQTT connected"

    [ "$ok" -eq 1 ] && return 0 || return 1
}

case "${1:-all}" in
    build) do_build ;;
    flash) do_flash ;;
    check) do_check "${2:-30}" ;;
    all)   do_build && do_flash && do_check 30 ;;
    *) echo "Usage: $0 [build|flash|check [sec]|all]" ; exit 1 ;;
esac
