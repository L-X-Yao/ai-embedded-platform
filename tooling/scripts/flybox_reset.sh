#!/bin/bash
# Quick MCU reset via OpenOCD (no reflash)
openocd \
    -f interface/cmsis-dap.cfg \
    -c "adapter speed 1000" \
    -f target/stm32f4x.cfg \
    -c "cortex_m reset_config sysresetreq" \
    -c "init" \
    -c "reset run" \
    -c "shutdown" 2>&1 | grep -v "^Info\|^Warn\|^Open\|^Licensed\|^For\|^$"
echo "[OK] MCU reset"
