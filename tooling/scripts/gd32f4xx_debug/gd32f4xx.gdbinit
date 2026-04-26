# GD32F4xx-specific GDB helpers — RCU clock + I2C peripheral register dumps.
# Source after cortex_m4.gdbinit from a project-specific gdbinit.
#
# Provides:
#   rcu_clk           — dump RCU clock enable + PLL config
#   i2c_regs <base>   — dump I2C peripheral registers (CTL/STAT/DATA/CKCFG)
#
# I2C base addresses (GD32F405):
#   I2C0 = 0x40005400   I2C1 = 0x40005800   I2C2 = 0x40005C00

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
  GD32F405 bases: I2C0=0x40005400  I2C1=0x40005800  I2C2=0x40005C00
end

echo [GDB] GD32F4xx helpers loaded: rcu_clk / i2c_regs <base>\n
