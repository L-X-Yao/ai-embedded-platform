# Flybox GDB init — GD32F405 via OpenOCD CMSIS-DAP.
# Generic Cortex-M4 + GD32F4xx helpers live in gd32f4xx_debug/. This file
# only carries flybox-specific connect / file / reflash / IMU bus shortcuts.
#
# Launch via flybox_gdb.sh — that script `cd`s into tooling/scripts/ first
# so the relative `source` paths below resolve.

set confirm off
set pagination off
set print pretty on

# Generic helpers (reusable across GD32F4xx projects)
source gd32f4xx_debug/cortex_m4.gdbinit
source gd32f4xx_debug/gd32f4xx.gdbinit

# Connect to OpenOCD GDB server
target extended-remote :3333

# Load symbols from ELF (build dir is ~/flybox_build/ on WSL native FS)
file ~/flybox_build/flybox_vsf.elf

# Cortex-M4 exception handlers — always break on these
break HardFault_Handler
break BusFault_Handler
break MemManage_Handler
break UsageFault_Handler

# ---- Flybox-specific helpers ----

define reset_halt
  monitor reset halt
  echo [GDB] Reset + halt done\n
end
document reset_halt
  Reset MCU and halt at reset vector.
end

define reflash
  monitor reset halt
  monitor stm32f4x unlock 0
  monitor reset halt
  monitor program ~/flybox_build/flybox_vsf.bin 0x08010000
  monitor reset run
  echo [GDB] Flash + run done\n
end
document reflash
  Program flybox slot B (0x08010000) and run.
end

define i2c1_scan
  echo I2C1 register dump (QMI8658A bus):\n
  i2c_regs 0x40005800
end
document i2c1_scan
  Dump I2C1 registers (flybox IMU bus: PB10/PB11).
end

echo [GDB] Flybox debug session ready.\n
echo   reset_halt   reflash    i2c1_scan\n
echo   (generic)    fault_full / fault_info / exc_frame / ret_lr / vtor / rcu_clk / i2c_regs <base>\n
echo Run `fault_full` after halting on a fault for a one-shot diagnostic.\n
