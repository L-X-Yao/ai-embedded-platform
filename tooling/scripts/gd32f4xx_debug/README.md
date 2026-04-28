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
| `stack_tasks.example.json` | template | Clean schema reference — copy and edit per project |
| `stack_tasks.flybox.json` | flybox | Real-project config (9 tasks, VSF POSIX scale=2) — used by CI |
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

See `projects/flybox/` (via `projects/flybox/scripts/flybox.gdbinit`) for a real example.

## Porting to other MCUs

This pack was built layered so most of it survives an MCU swap. Two layers:

- **Generic Cortex-M layer** — fault decode, exception frame, PC sampling, register peek. Backed by ARM-standard SCB registers (CFSR / HFSR / MMFAR / BFAR / SHCSR / VTOR at fixed addresses 0xE000ED**xx**), ARM-standard exception stacking, and the `bl`/`b` opcode patterns GCC emits. Works on any Cortex-M4 unchanged; M3/M7/M33 with small adaptations.
- **Chip-specific layer** — RCC/RCU clock register addresses, peripheral register layouts, vendor-specific OpenOCD target. One file per chip family.

### Reuse matrix

| New MCU | Cortex-M layer (5 files) | GD32F4 layer (1 file) | Need to write | Effort |
|---------|--------------------------|------------------------|---------------|--------|
| **STM32F4** | ✅ direct | ❌ RCC layout differs but similar | `stm32f4xx.gdbinit` | ~30 min |
| **STM32H7** (M7) | ✅ M7 is M4 superset | ❌ different RCC | `stm32h7xx.gdbinit` | 1 h |
| **NXP K22 / ATSAM4** (M4) | ✅ direct | ❌ different vendor regs | `<chip>.gdbinit` | 1 h |
| **STM32F1** (M3) | ⚠️ trim FPU-related fault bits | ❌ different RCC | `cortex_m3.gdbinit` + chip layer | 2 h |
| **N32G003 / RP2040** (M0+) | ❌ M0+ has no CFSR/HFSR/MMFAR/BFAR (only HardFault, no breakdown) | ❌ | `cortex_m0plus.gdbinit` (simplified fault decode — read SHCSR + VECTACTIVE only) + chip | 3-4 h |
| **ESP32** (Xtensa) | ❌ different ISA, exception frame is Xtensa-specific | ❌ | full new architecture layer | 1-2 day |
| **CH32V / GD32V** (RISC-V) | ❌ RISC-V exception frame | ❌ | RISC-V layer + chip | 1 day |

### Tools that are MCU-agnostic (reuse on any platform)

- `stack_analyzer.py` — only needs GCC `-fstack-usage` output and an objdump that disassembles the ELF. Works on any GCC-built embedded target (ARM, RISC-V, Xtensa, AVR…)
- `serial_capture.ps1` — runs on the host, captures from any USB-UART. Pass a different `-OcdTarget` for non-stm32f4 chips
- `diag_halt.cfg` / `diag_where.cfg` — Cortex-M generic (M0/M3/M4/M7/M33). Just feed a different `-f target/<vendor>.cfg`

### Adding a new chip layer (recipe)

For a new Cortex-M chip on the same family (e.g. STM32F4):

1. Copy `gd32f4xx.gdbinit` to `<chip>.gdbinit` in this directory (or a sibling subdirectory if you want to keep packs separate).
2. Update register addresses in `rcu_clk` / `i2c_regs` (look up `RCC` and `I2C` base addresses in the chip's reference manual).
3. In your project's gdbinit, replace `source gd32f4xx.gdbinit` with `source <chip>.gdbinit`.

For a new core (M0+ / M3 / M7):

1. Copy `cortex_m4.gdbinit` to `cortex_m<X>.gdbinit`.
2. M0+ has no CFSR — drop `fault_info`'s MMFSR/BFSR/UFSR bit decoding; keep only HFSR + VECTACTIVE from ICSR.
3. M7 — keep everything; optionally add cache control register dumps (CCR/CCSIDR).
4. Update references in your project's gdbinit.

For a new architecture (Xtensa / RISC-V): start fresh — exception frame layout, fault registers, and call instruction patterns are all different. The `stack_analyzer.py` framework still applies (it's GCC-driven, ISA-agnostic).

## OpenOCD note on GD32F4

GD32F4xx is SWD-compatible with STM32F4 — `target/stm32f4x.cfg` works. Use
`cortex_m reset_config sysresetreq` (not `vectreset`) — see
`memory/feedback_sysresetreq_not_vectreset.md` for why. Use `flash write_image
erase` + `verify_image` (not `program ... verify`) — see
`memory/feedback_openocd_verify_stm32f4.md`.
