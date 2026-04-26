# build_flash — generic build / flash / serial-verdict pipeline

Configurable pipeline for the standard "compile → flash → boot → check log"
loop on STM32-family / GD32 + CMSIS-DAP targets. Replaces project-specific
orchestration scripts (e.g. `flybox_build_flash.sh`) with a generic driver
fed by a `.conf` file per project.

## Quick start

```bash
# 1. Copy example.conf to <project>.conf and fill in BIN_PATH, FLASH_ADDR,
#    SERIAL_PORT, BUILD_CMD, FAIL_PATTERNS, PASS_PATTERNS.
cp example.conf myproject.conf

# 2. Run any subcommand
./build_flash_check.sh --conf myproject.conf            # build + flash + check
./build_flash_check.sh --conf myproject.conf build      # build only
./build_flash_check.sh --conf myproject.conf flash      # flash only
./build_flash_check.sh --conf myproject.conf check 60   # 60s soak
```

For per-project ergonomics, write a thin wrapper that hard-codes the conf:

```bash
# myproject_build_flash.sh
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$SCRIPT_DIR/build_flash/build_flash_check.sh" \
    --conf "$SCRIPT_DIR/build_flash/myproject.conf" "$@"
```

See `tooling/scripts/flybox_build_flash.sh` for a real example.

## Subcommands

| Subcommand | What it does |
|------------|--------------|
| `build`  | Run `BUILD_CMD`, then optional `COPY_CMD`, then optional size check vs `SRAM_LIMIT` |
| `flash`  | OpenOCD `write_image erase` + `verify_image` at `FLASH_ADDR` |
| `check [SECONDS]` | Reset + capture serial via `gd32f4xx_debug/serial_capture.ps1`, verdict by `FAIL_PATTERNS` / `PASS_PATTERNS` |
| `all`    | build + flash + check (default if no subcommand) |

## Verdict logic

- Any line matching any `FAIL_PATTERNS` → **FAIL**
- Lines matching `PASS_PATTERNS`: count how many match. Need ≥ `MIN_PASSED` → **PASS**
- `PASS_PATTERNS_INFO` patterns are printed if found, do not affect verdict

All patterns are ERE regex (`grep -E`) — escape `[` / `]` / `\` as needed.

## Conf file schema

See `example.conf` — every key is documented inline with required/optional
markers. Required: `BIN_PATH`, `FLASH_ADDR` (and `SERIAL_PORT` for `check`).

## Where to put the .conf

Two reasonable conventions:

1. **Co-located with tooling** (current flybox approach): `tooling/scripts/build_flash/myproject.conf`. Easy to grep across the repo.
2. **Co-located with project**: `projects/myproject/build_flash.conf`. Travels with the project if you copy it elsewhere.

Either works — `--conf` takes any path. Pick one and stay consistent across projects.

## Why generic, not per-project?

flybox's original `flybox_build_flash.sh` was 134 lines of bash with the
build dir, ELF name, SRAM layout, OpenOCD target, COM port, and verdict
keywords all hard-coded. Every new GD32F4 / STM32F4 project would copy and
edit that script — diverging fixes, inconsistent verdict logic, no shared
improvements. Genericized version is the same logic, ~250 lines, with the
project-specific bits in a 30-line `.conf`.
