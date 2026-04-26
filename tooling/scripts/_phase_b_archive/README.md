# Phase B archive

Obsolete OpenOCD TCL probe scripts from flybox **Phase B**（VSF compat 移植期，
2026-04-23 → 04-25）, kept here for git history reference only. All have been
**superseded** by the `gd32f4xx_debug/` quick-pack:

| Archived script | Purpose at the time | Superseded by |
|-----------------|---------------------|---------------|
| `debug_fault.tcl`        | One-shot CFSR/HFSR/MMFAR/BFAR dump | `gd32f4xx_debug/diag_halt.cfg` (cleaner output) + `cortex_m4.gdbinit::fault_full` |
| `debug_vtor.tcl` / `debug_vtor2.tcl` | VTOR sanity check during early boot debugging | `cortex_m4.gdbinit::vtor` |
| `debug_nvic.tcl`         | NVIC enable/pending mask dump | (no replacement — write a generic NVIC dump in `cortex_m4.gdbinit` if needed again) |
| `debug_running.tcl`      | Spin-sample PC to see if MCU is alive | `gd32f4xx_debug/diag_where.cfg` |
| `debug_boot_trace.tcl`   | Step-by-step boot trace via halts | GDB breakpoints + `monitor` commands |
| `debug_user_main.tcl`    | Probe whether `user_main` was reached (early VSF Linux ENTRY debugging) | symbols visible via `info functions user_main` in GDB |
| `debug_linux_init.tcl`   | Probe whether `vsf_linux_init` was reached | same as above |
| `debug_flybox.tcl`       | Combined fault + VTOR + RCU dump | `cortex_m4.gdbinit::fault_full` + `gd32f4xx.gdbinit::rcu_clk` |
| `debug_kernel_thread.tcl` | Spin-sample PC under VSF kernel thread debug | `diag_where.cfg` |
| `debug_stuck.tcl`        | Halt + PC + lr dump for stuck-loop triage | `diag_halt.cfg` + GDB |
| `debug_check_state.tcl`  | One-shot processor state dump | `diag_halt.cfg` |
| `debug_tasks.tcl`        | List FreeRTOS tasks via memory peek | (no replacement — use VSF / FreeRTOS GDB Python helpers if needed again) |
| `read_regs.cfg`          | Generic memory peek wrapper | `diag_halt.cfg` |

These are kept (not deleted) so:
- Git blame / archaeology against Phase B commits still resolves
- If a similar bug recurs and one of these is closer to the right shape than the new generic pack, it can be revived

**Do not add to this directory.** New debug scripts go in `tooling/scripts/gd32f4xx_debug/` (or, for non-debug tooling, a sibling subdirectory under `tooling/scripts/`).
