# ai-embedded-platform

206 production-grade C99 components for embedded systems.

[![CI](https://github.com/L-X-Yao/ai-embedded-platform/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/L-X-Yao/ai-embedded-platform/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Components](https://img.shields.io/badge/components-206-green.svg)](wheels/README.md)

## Why this library

- **Zero dependencies** -- no OS, no heap, no libc beyond `<stdint.h>`. Every component is a pure C99 static library.
- **Any MCU** -- works on STM32, GD32, ESP32, nRF52, RISC-V, or anything with a C99 compiler. Hardware access is injected via function pointers, not `#ifdef` per platform.
- **Host-testable** -- every component compiles and runs on your desktop with `gcc` and `cmake`. No dev board needed to start integrating.
- **Battle-tested** -- 55-check automated audit, >=80% line coverage, AddressSanitizer in CI, property-based tests, fuzz harnesses.

## Quick start

Each component is a self-contained folder. To use one in your project:

```bash
# 1. Copy the component into your source tree (or add as a git subtree)
cp -r wheels/drivers/bmp280 your_project/lib/bmp280

# 2. Add to your CMakeLists.txt
add_subdirectory(lib/bmp280 bmp280)
target_link_libraries(your_app PRIVATE bmp280)
```

That's it. Include the header, inject your HAL callbacks, and call the API:

```c
#include "bmp280.h"

/* Provide your platform's I2C implementation */
static bool my_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val) {
    return hal_i2c_write(addr, reg, &val, 1);  /* your HAL here */
}
static bool my_read_regs(void *ctx, uint8_t addr, uint8_t reg,
                         uint8_t *buf, uint16_t len) {
    return hal_i2c_read(addr, reg, buf, len);
}

static const bmp280_i2c_t i2c = {
    .write_reg = my_write_reg,
    .read_regs = my_read_regs,
    .delay_ms  = NULL,  /* optional */
    .ctx       = NULL,
};

static const bmp280_cfg_t cfg = {
    .addr   = BMP280_ADDR_LOW,
    .osrs_t = BMP280_OVERSAMPLING_2,
    .osrs_p = BMP280_OVERSAMPLING_16,
};

void app_main(void) {
    bmp280_t sensor;
    bmp280_data_t data;

    if (bmp280_init(&sensor, &i2c, &cfg) != BMP280_OK) {
        /* handle error */
    }

    if (bmp280_read(&sensor, &data) == BMP280_OK) {
        printf("%.1f C, %.0f Pa\n", data.temperature_c, data.pressure_pa);
    }
}
```

Every component follows this same pattern: inject HAL via a struct of function pointers, call `_init`, then use the API.

## Component catalog

| Layer | Directory | Count | Examples |
|-------|-----------|------:|----------|
| L0 Primitives | `wheels/primitives/` | 63 | ring_buffer, crc32, state_machine, pid_controller, json_parser, aes128 |
| L1 Drivers | `wheels/drivers/` | 88 | bmp280, sht40, mpu6050, oled_ssd1306, w25qxx, ws2812, lora_sx1276 |
| L2 Protocols | `wheels/protocols/` | 35 | mqtt_packet, modbus_rtu, http_parser, nmea_parser, mavlink, coap_packet |
| L3 Modules | `wheels/modules/` | 20 | cli_shell, data_logger, ota_manager, sensor_hub, mqtt_client |

Full catalog with descriptions: [wheels/README.md](wheels/README.md)

## Architecture: HAL injection

Components never call hardware directly. Instead, you inject platform-specific functions at initialization:

```
 +-----------+     function pointers     +------------------+
 | Component | <------------------------ | Your HAL / RTOS  |
 | (pure C)  |  write_reg, read_regs,   | (STM32 HAL, ESP  |
 |           |  delay_ms, gpio_write... |  IDF, bare metal) |
 +-----------+                           +------------------+
       |
       | same component binary
       v
 +-----------+
 | Host test |  mock functions injected
 | (desktop) |  for simulation testing
 +-----------+
```

This means:

- Components have zero compile-time platform dependencies
- The same source compiles for ARM Cortex-M, RISC-V, Xtensa, or x86 desktop
- Tests run on your development machine with mock HAL implementations
- No `#ifdef STM32` / `#ifdef ESP32` inside component code

## Quality guarantees

Every component passes a 55-check automated audit that enforces:

- **Testing** -- host-simulation tests with >=80% line coverage
- **Safety** -- NULL parameter checks, error code returns, AddressSanitizer clean
- **Consistency** -- uniform API naming (`prefix_init`, `prefix_err_t`), SPDX headers, documentation
- **CI** -- incremental testing on every push; ARM cross-compilation smoke tests
- **Advanced** -- property-based tests, fuzz harnesses, and mutation testing where applicable

## Project structure

```
ai-embedded-platform/
├── wheels/
│   ├── primitives/       L0: pure software (CRC, buffers, math, crypto)
│   ├── drivers/          L1: sensor/peripheral drivers (I2C, SPI, GPIO)
│   ├── protocols/        L2: protocol parsers (MQTT, Modbus, HTTP)
│   └── modules/          L3: higher-level modules (CLI, OTA, logging)
├── examples/             Integration examples combining multiple components
├── tooling/
│   ├── harness/          Test framework headers
│   ├── codegen/          Test code generators
│   ├── test/             Test runners (coverage, fuzz, mutation)
│   └── audit/            Automated compliance checker
├── docs/
│   ├── wheel-contract.md Component specification
│   ├── api-conventions.md  Naming and error handling conventions
│   └── integrating-wheels.md  Build system integration guide
└── templates/            Starter templates for new projects
```

## Running tests

```bash
# Prerequisites: gcc, cmake, ninja, pytest

# Test everything
make test-parallel

# Test a single component
bash tooling/test/test_all.sh wheels/drivers/bmp280

# Run the 55-check audit
python tooling/audit/audit_wheels.py --quiet

# Coverage for one component
bash tooling/test/coverage_wheel.sh wheels/drivers/bmp280
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the quick-start guide, wheel contract, commit format, and testing requirements.

## License

[MIT](LICENSE)
