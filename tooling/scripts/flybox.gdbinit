# Flybox GDB init — GD32F405 via OpenOCD CMSIS-DAP
# Usage: arm-none-eabi-gdb -x flybox.gdbinit

set confirm off
set pagination off
set print pretty on

# Connect to OpenOCD GDB server
target extended-remote :3333

# Load symbols from ELF (adjust path if needed)
# Build dir is ~/flybox_build/ on WSL native FS
file ~/flybox_build/flybox_vsf.elf

# Cortex-M4 exception handlers — always break on these
break HardFault_Handler
break BusFault_Handler
break MemManage_Handler
break UsageFault_Handler

# ---- Helper commands ----

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
  Program flash at 0x08010000 and run. Calls OpenOCD directly.
end

define fault_info
  echo --- Cortex-M4 Fault Status ---\n
  echo CFSR:
  print/x *(uint32_t*)0xE000ED28
  echo HFSR:
  print/x *(uint32_t*)0xE000ED2C
  echo MMFAR:
  print/x *(uint32_t*)0xE000ED34
  echo BFAR:
  print/x *(uint32_t*)0xE000ED38
  echo SHCSR:
  print/x *(uint32_t*)0xE000ED24
  echo --- Exception frame (if in handler) ---\n
  echo xPSR:
  info reg xPSR
  echo PC:
  info reg pc
  echo LR:
  info reg lr
  echo SP:
  info reg sp
end
document fault_info
  Dump Cortex-M4 fault registers (CFSR/HFSR/MMFAR/BFAR) and current PC/LR.
end

define i2c_regs
  set $base = $arg0
  printf "I2C base 0x%08X:\n", $base
  printf "  CTL0  = 0x%04X\n", *(uint16_t*)($base + 0x00)
  printf "  CTL1  = 0x%04X\n", *(uint16_t*)($base + 0x04)
  printf "  SADDR0= 0x%04X\n", *(uint16_t*)($base + 0x08)
  printf "  SADDR1= 0x%04X\n", *(uint16_t*)($base + 0x0C)
  printf "  DATA  = 0x%04X\n", *(uint16_t*)($base + 0x10)
  printf "  STAT0 = 0x%04X\n", *(uint16_t*)($base + 0x14)
  printf "  STAT1 = 0x%04X\n", *(uint16_t*)($base + 0x18)
  printf "  CKCFG = 0x%04X\n", *(uint16_t*)($base + 0x1C)
  printf "  RT    = 0x%04X\n", *(uint16_t*)($base + 0x20)
end
document i2c_regs
  Dump I2C peripheral registers. Usage: i2c_regs 0x40005400
  I2C0=0x40005400  I2C1=0x40005800  I2C2=0x40005C00
end

define rcu_clk
  echo --- RCU Clock Enable ---\n
  printf "AHB1EN = 0x%08X\n", *(uint32_t*)0x40023830
  printf "AHB2EN = 0x%08X\n", *(uint32_t*)0x40023834
  printf "APB1EN = 0x%08X\n", *(uint32_t*)0x40023840
  printf "APB2EN = 0x%08X\n", *(uint32_t*)0x40023844
  echo --- RCU Clock Config ---\n
  printf "CFG0   = 0x%08X (SCSS=%d)\n", *(uint32_t*)0x40023808, (*(uint32_t*)0x40023808 >> 2) & 3
  printf "PLL    = 0x%08X\n", *(uint32_t*)0x40023804
end
document rcu_clk
  Dump RCU clock enable and PLL status registers.
end

define vtor
  printf "VTOR = 0x%08X\n", *(uint32_t*)0xE000ED08
end
document vtor
  Show current vector table offset register.
end

define i2c1_scan
  echo I2C1 register dump (QMI8658A bus):\n
  i2c_regs 0x40005800
end
document i2c1_scan
  Quick dump of I2C1 registers (flybox IMU bus: PB10/PB11).
end

echo [GDB] Flybox debug session ready. Commands: reset_halt, reflash, fault_info, i2c_regs, rcu_clk, vtor\n
