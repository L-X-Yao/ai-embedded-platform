#!/usr/bin/env python3
"""Generate a complete subagent prompt for an L1 driver wheel.

Usage:
    python3 gen_driver_prompt.py --name bma400 --chip "Bosch BMA400" \\
        --hal i2c --addr 0x14 --id-reg 0x00 --id-val 0x90 \\
        --brief "ultra-low-power accelerometer, 12-bit, 4 ranges"

Outputs a ready-to-use agent prompt to stdout.
"""
import argparse
import sys

TEMPLATE = """Implement the {chip} driver wheel at `wheels/drivers/{name}/`.

The scaffold already exists. Rewrite all source and test files.

## Chip Specs
- {chip}
- {hal_upper} interface, address {addr}
- ID register: {id_reg}, expected value: {id_val}
- {brief}

## {hal_upper} HAL
```c
typedef struct {{
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
}} {name}_{hal}_t;
```

## File headers (MANDATORY .h AND .c)
```
// SPDX-License-Identifier: MIT
/*
 * {name} — {chip_lower}
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      {hal}_hal (injected)
 * Instances: multi
 */
```

## Error enum: OK=0, ERR_I2C=-1, ERR_ID=-2, ERR_PARAM=-3

## API
- `{name}_init(dev, {hal}, cfg)` — verify chip ID, configure sensor
- `{name}_read(dev, &data)` — read sensor data
- `{name}_reset(dev)` — soft reset (if supported)

## hw_model
- Register array with POR defaults (chip ID at {id_reg} = {id_val})
- Inject helpers for physical values
- Read-only register enforcement
- Use hw_model, NOT mock_i2c.h

## Tests (minimum coverage)
| Category | Tests |
|----------|-------|
| init     | OK, NULL (dev/{hal}/cfg), zero HAL, wrong ID, I2C fail |
| read     | Known values (2-3 points), negative, boundary |
| reset    | Command written, NULL |
| hw_model | POR defaults, roundtrip, ID rejection |
| error    | Error code value assertions |

## Self-verification (MANDATORY)
After implementing, run:
```bash
python3 tooling/audit/audit_wheels.py --wheel drivers/{name} --quiet
```
Verify: **0 warnings, 0 failures**. Fix any findings before declaring done.

Compile: `-Wall -Wextra -Wpedantic -Werror -Wconversion`. Do NOT commit."""


def main():
    p = argparse.ArgumentParser(description="Generate L1 driver agent prompt")
    p.add_argument("--name", required=True, help="wheel name (e.g. bma400)")
    p.add_argument("--chip", required=True, help="full chip name (e.g. 'Bosch BMA400')")
    p.add_argument("--hal", default="i2c", choices=["i2c", "spi"],
                   help="HAL type (default: i2c)")
    p.add_argument("--addr", default="0x00", help="default I2C/SPI address")
    p.add_argument("--id-reg", default="0x00", help="chip ID register address")
    p.add_argument("--id-val", default="0x00", help="expected chip ID value")
    p.add_argument("--brief", default="", help="one-line functionality description")
    args = p.parse_args()

    prompt = TEMPLATE.format(
        name=args.name,
        chip=args.chip,
        chip_lower=args.chip.lower(),
        hal=args.hal,
        hal_upper=args.hal.upper(),
        addr=args.addr,
        id_reg=args.id_reg,
        id_val=args.id_val,
        brief=args.brief,
    )
    print(prompt)


if __name__ == "__main__":
    main()
