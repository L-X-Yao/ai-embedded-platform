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
  set $cfsr  = *(uint32_t*)0xE000ED28
  set $hfsr  = *(uint32_t*)0xE000ED2C
  set $mmfar = *(uint32_t*)0xE000ED34
  set $bfar  = *(uint32_t*)0xE000ED38
  set $shcsr = *(uint32_t*)0xE000ED24
  set $cfsr_mm = $cfsr & 0xFF
  set $cfsr_bf = ($cfsr >> 8) & 0xFF
  set $cfsr_uf = ($cfsr >> 16) & 0xFFFF

  printf "=== Cortex-M4 Fault Status ===\n"
  printf "CFSR  = 0x%08X  (MMFSR=0x%02X BFSR=0x%02X UFSR=0x%04X)\n", \
         $cfsr, $cfsr_mm, $cfsr_bf, $cfsr_uf
  printf "HFSR  = 0x%08X\n", $hfsr
  printf "MMFAR = 0x%08X\n", $mmfar
  printf "BFAR  = 0x%08X\n", $bfar
  printf "SHCSR = 0x%08X\n", $shcsr

  printf "\n--- MemManage faults (MMFSR) ---\n"
  if $cfsr_mm & 0x01
    printf "  IACCVIOL    instruction fetch from no-execute region\n"
  end
  if $cfsr_mm & 0x02
    printf "  DACCVIOL    data access violation\n"
  end
  if $cfsr_mm & 0x08
    printf "  MUNSTKERR   fault on exception return (unstacking)\n"
  end
  if $cfsr_mm & 0x10
    printf "  MSTKERR     fault on exception entry (stacking)\n"
  end
  if $cfsr_mm & 0x80
    printf "  MMARVALID   MMFAR holds the faulting address\n"
  end

  printf "\n--- BusFault (BFSR) ---\n"
  if $cfsr_bf & 0x01
    printf "  IBUSERR     instruction bus error\n"
  end
  if $cfsr_bf & 0x02
    printf "  PRECISERR   precise data bus error (BFAR valid)\n"
  end
  if $cfsr_bf & 0x04
    printf "  IMPRECISERR imprecise data bus error (BFAR invalid)\n"
  end
  if $cfsr_bf & 0x08
    printf "  UNSTKERR    bus fault on unstacking\n"
  end
  if $cfsr_bf & 0x10
    printf "  STKERR      bus fault on stacking\n"
  end
  if $cfsr_bf & 0x80
    printf "  BFARVALID   BFAR holds the faulting address\n"
  end

  printf "\n--- UsageFault (UFSR) ---\n"
  if $cfsr_uf & 0x0001
    printf "  UNDEFINSTR  undefined instruction\n"
  end
  if $cfsr_uf & 0x0002
    printf "  INVSTATE    invalid EPSR state (wrong T bit / IT)\n"
  end
  if $cfsr_uf & 0x0004
    printf "  INVPC       invalid PC load (bad EXC_RETURN?)\n"
  end
  if $cfsr_uf & 0x0008
    printf "  NOCP        coprocessor access (FPU disabled?)\n"
  end
  if $cfsr_uf & 0x0100
    printf "  UNALIGNED   unaligned access trap\n"
  end
  if $cfsr_uf & 0x0200
    printf "  DIVBYZERO   integer divide by zero\n"
  end

  printf "\n--- HardFault (HFSR) ---\n"
  if $hfsr & 0x00000002
    printf "  VECTTBL     bus fault on vector table read\n"
  end
  if $hfsr & 0x40000000
    printf "  FORCED      escalated from a configurable fault\n"
  end
  if $hfsr & 0x80000000
    printf "  DEBUGEVT    debug event (BKPT outside debug)\n"
  end

  printf "\n--- Current registers ---\n"
  info reg pc lr xpsr sp msp psp control
end
document fault_info
  Decode Cortex-M4 fault registers (CFSR/HFSR + MMFAR/BFAR), bit by bit.
  Run after a fault: `monitor halt` then `fault_info`.
end

define exc_frame
  # Decode the stacked exception frame. Supply PSP or MSP value as $arg0,
  # or omit to auto-pick based on EXC_RETURN low nibble of LR.
  if $argc == 0
    set $lr_full = $lr
    if ($lr_full & 0x4) != 0
      set $sp_use = $psp
      printf "(using PSP = 0x%08X)\n", $psp
    else
      set $sp_use = $msp
      printf "(using MSP = 0x%08X)\n", $msp
    end
  else
    set $sp_use = $arg0
  end

  printf "Stacked frame at 0x%08X:\n", $sp_use
  printf "  R0   = 0x%08X\n", *(uint32_t*)($sp_use + 0x00)
  printf "  R1   = 0x%08X\n", *(uint32_t*)($sp_use + 0x04)
  printf "  R2   = 0x%08X\n", *(uint32_t*)($sp_use + 0x08)
  printf "  R3   = 0x%08X\n", *(uint32_t*)($sp_use + 0x0C)
  printf "  R12  = 0x%08X\n", *(uint32_t*)($sp_use + 0x10)
  printf "  LR   = 0x%08X  (return addr before exception)\n", \
         *(uint32_t*)($sp_use + 0x14)
  printf "  PC   = 0x%08X  (faulting instruction)\n", \
         *(uint32_t*)($sp_use + 0x18)
  printf "  xPSR = 0x%08X\n", *(uint32_t*)($sp_use + 0x1C)

  # Pretty-print resolved symbols for PC and LR
  set $stk_pc = *(uint32_t*)($sp_use + 0x18)
  set $stk_lr = *(uint32_t*)($sp_use + 0x14)
  printf "\nFaulting PC -> "
  info symbol $stk_pc
  printf "Caller LR   -> "
  info symbol $stk_lr
end
document exc_frame
  Decode the auto-stacked exception frame.
  Usage: exc_frame              (auto-pick PSP/MSP from EXC_RETURN)
         exc_frame 0x20001000   (use a specific stack pointer)
end

define ret_lr
  # Decode EXC_RETURN value (the LR in handler mode)
  set $r = $lr
  printf "EXC_RETURN = 0x%08X\n", $r
  if ($r & 0x10) == 0
    printf "  FPU stacked (lazy/eager FPU state pushed)\n"
  else
    printf "  no FPU state\n"
  end
  if ($r & 0x08)
    printf "  return to thread mode\n"
  else
    printf "  return to handler mode\n"
  end
  if ($r & 0x04)
    printf "  use PSP after return\n"
  else
    printf "  use MSP after return\n"
  end
end
document ret_lr
  Decode current LR as an EXC_RETURN value (mode + stack used).
end

define fault_full
  # One-shot full fault diagnostic.
  printf "\n========== HARDFAULT DIAGNOSTIC ==========\n"
  fault_info
  printf "\n========== EXC_RETURN ==========\n"
  ret_lr
  printf "\n========== STACKED FRAME ==========\n"
  exc_frame
  printf "\n========== HANDLER BACKTRACE ==========\n"
  bt
end
document fault_full
  All-in-one fault diagnostic: CFSR/HFSR decode, EXC_RETURN, stacked frame, bt.
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

echo [GDB] Flybox debug session ready.\n
echo   reset_halt   reflash    vtor    rcu_clk   i2c_regs <base>\n
echo   fault_info   ret_lr     exc_frame [sp]    fault_full\n
echo Run `fault_full` after halting on a fault for a one-shot diagnostic.\n
