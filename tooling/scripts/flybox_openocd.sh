#!/bin/bash
# Start OpenOCD as GDB server for flybox (GD32F405 via CMSIS-DAP)
# GDB connects to :3333, Telnet to :4444
openocd \
    -f interface/cmsis-dap.cfg \
    -c "adapter speed 1000" \
    -f target/stm32f4x.cfg \
    -c "cortex_m reset_config vectreset" \
    -c "init" \
    -c "reset halt"
