# Agent Wheel Implementation Prompt Template

Use this template when spawning subagents to implement L1 driver wheels.
Fill in the `{{placeholders}}` and pass as the agent prompt.

---

Implement the {{CHIP_NAME}} driver wheel at `wheels/drivers/{{wheel_name}}/`.

The scaffold already exists. You need to rewrite all source and test files.

## Chip Specs

- {{CHIP_FULL_NAME}}
- {{HAL_TYPE}} interface, address {{DEFAULT_ADDR}}
- ID register: {{ID_REG}}, expected value: {{ID_VALUE}}
- {{BRIEF_FUNCTIONALITY}}

## I2C HAL (standard pattern)

```c
typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} {{wheel_name}}_i2c_t;
```

> For 16-bit register devices (TMP117, INA3221): use `write_regs` instead of `write_reg`.
> For 16-bit register addresses (VL53L1X): use `uint16_t reg` parameter.
> For SPI devices: use cs_low/cs_high/xfer pattern.

## Requirements Checklist

### File Headers (MANDATORY in both .h AND .c)

```
// SPDX-License-Identifier: MIT
/*
 * {{wheel_name}} — {{one-line description}}
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      {{hal}}_hal (injected)
 * Instances: multi
 */
```

### Error Enum (standard pattern)

```c
typedef enum {
    {{UPPER}}_OK        =  0,
    {{UPPER}}_ERR_I2C   = -1,
    {{UPPER}}_ERR_ID    = -2,
    {{UPPER}}_ERR_PARAM = -3,
} {{wheel_name}}_err_t;
```

### API Functions

- `{{wheel_name}}_init(dev, i2c, cfg)` — verify chip ID, configure sensor
- `{{wheel_name}}_read(dev, &data)` — read sensor data
- `{{wheel_name}}_reset(dev)` — soft reset (if supported)

### hw_model (NOT mock_i2c.h)

- `uint8_t hw_regs[256]` (or `uint16_t` for 16-bit register devices)
- POR defaults from datasheet (especially chip ID)
- `hw_model_inject_*()` helpers for physical values
- Read-only register enforcement where applicable

### Tests (minimum coverage)

| Category | Tests |
|----------|-------|
| init     | OK, NULL (dev/i2c/cfg), zero HAL, wrong ID, I2C fail |
| read     | Known values (2-3 temperatures/pressures), negative, boundary |
| reset    | Command written, NULL |
| hw_model | POR defaults, roundtrip at multiple values, ID rejection |
| error    | Error code value assertions |

### Compile Flags

Must compile cleanly with: `-Wall -Wextra -Wpedantic -Werror -Wconversion`

### Self-Verification (MANDATORY before declaring done)

After implementing, run:
```bash
python3 tooling/audit/audit_wheels.py --wheel drivers/{{wheel_name}} --quiet
```
Verify output shows **0 warnings, 0 failures**. If any findings, fix them.

## Do NOT commit — just write the files.
