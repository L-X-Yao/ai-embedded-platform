# GD32F4xx Debug Pack

Reusable debugging assets distilled from `flybox` Phase B + post-migration triage.
Use these on **any GD32F4xx project** (and most are generic enough for any ARM
Cortex-M4 SWD target — see "Genericity" column).

## Trigger scenarios → which asset

| Symptom | Tool | One-line invocation |
|---------|------|---------------------|
| Boot hangs / silent reset loop | `diag_halt.cfg` | `openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -f diag_halt.cfg` |
| Looking for where MCU is stuck | `diag_where.cfg` | `openocd -f ... -f diag_where.cfg` |
| HardFault / BusFault / UsageFault hit | `cortex_m4.gdbinit` → `fault_full` | `arm-none-eabi-gdb -x cortex_m4.gdbinit your.elf` then `monitor halt; fault_full` |
| Suspect stack overflow | `stack_analyzer.py` | `python stack_analyzer.py BUILD_DIR --task-config tasks.json` |
| Need clock/RCU/I2C reg dump | `gd32f4xx.gdbinit` → `rcu_clk` / `i2c_regs <base>` | `source gd32f4xx.gdbinit` inside an existing GDB session |
| Lose early boot output via PowerShell SerialPort | `serial_capture.ps1` | `pwsh -File serial_capture.ps1 -Port COM25` |

## Files

| File | Genericity | Description |
|------|-----------|-------------|
| `cortex_m4.gdbinit` | **Any Cortex-M4** | `fault_info` / `exc_frame [sp]` / `ret_lr` / `fault_full` — decode CFSR/HFSR + auto-pick PSP/MSP from EXC_RETURN |
| `gd32f4xx.gdbinit` | GD32F4xx | `rcu_clk` / `vtor` / `i2c_regs <base>` — chip-specific peripheral dumps |
| `diag_halt.cfg` | **Any Cortex-M** | OpenOCD one-shot: halt + dump CFSR/HFSR/MMFAR/BFAR/SHCSR/ICSR/VTOR + resume |
| `diag_where.cfg` | **Any Cortex-M** | OpenOCD one-shot: 3 PC samples 400 ms apart — useful for "where is it stuck?" |
| `stack_analyzer.py` | **Any GCC ARM project** | Static stack analysis from `-fstack-usage` + objdump callgraph; per-task budget check vs JSON config |
| `stack_tasks.example.json` | example | Format reference for `--task-config` |
| `serial_capture.ps1` | **Any STM32-family / GD32 SWD + USB-UART** | PowerShell SerialPort capture with CH340 DTR/RTS fix + boot-burst-friendly buffer |

## Why each asset exists (real flybox lessons)

- **`stack_analyzer.py`** — flybox HardFault during MQTT publish. Found `mqtt_task_entry` cumulative 6 KB > 4 KB allocated. Saved hours of GDB blind chasing.
- **`cortex_m4.gdbinit::fault_full`** — Cortex-M4 fault flow has 5 register groups + EXC_RETURN nibbles + auto-stacked frame. Decoding manually is 15 minutes per fault. `fault_full` is < 1 sec.
- **`diag_halt.cfg`** — Boot reboot-loop with no serial output. One OpenOCD invocation tells you "is this a fault, or stuck in `while`?" without a GDB session.
- **`serial_capture.ps1` DTR/RTS=true** — CH340 USB-UART silently latches into "stop forwarding RX" after first burst if DTR/RTS are deasserted. Diagnostic capture pipeline lied for 30 minutes before this was found.
- **`diag_halt.cfg` reset stuck loop** — W25Q64 SPI bus hung `spi_xfer` forever (chip went into bad state after many resets). `diag_halt.cfg` → `monitor halt` → `addr2line` → 5 min localization vs N reflash cycles.

## Layered GDB usage

For a custom project, write a `<project>.gdbinit` that sources both layers,
then adds project-specific commands:

```gdb
# Load both generic layers
source <path>/cortex_m4.gdbinit
source <path>/gd32f4xx.gdbinit

# Project-specific
target extended-remote :3333
file build/myproject.elf
break HardFault_Handler

define reflash
  monitor reset halt
  monitor program build/myproject.bin 0x08010000
  monitor reset run
end
```

See `projects/flybox/` (via `tooling/scripts/flybox.gdbinit`) for a real example.

## OpenOCD note on GD32F4

GD32F4xx is SWD-compatible with STM32F4 — `target/stm32f4x.cfg` works. Use
`cortex_m reset_config sysresetreq` (not `vectreset`) — see
`memory/feedback_sysresetreq_not_vectreset.md` for why. Use `flash write_image
erase` + `verify_image` (not `program ... verify`) — see
`memory/feedback_openocd_verify_stm32f4.md`.
