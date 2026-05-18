# ai-embedded-platform

264 production-grade C99 components for embedded systems.

[![CI](https://github.com/L-X-Yao/ai-embedded-platform/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/L-X-Yao/ai-embedded-platform/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Components](https://img.shields.io/badge/components-264-green.svg)](wheels/README.md)

## Why this library

- **Zero dependencies** -- no OS, no heap, no libc beyond `<stdint.h>`. Every component is a pure C99 static library.
- **Any MCU** -- works on STM32, GD32, ESP32, nRF52, RISC-V, or anything with a C99 compiler. Hardware access is injected via function pointers, not `#ifdef` per platform.
- **Host-testable** -- every component compiles and runs on your desktop with `gcc` and `cmake`. No dev board needed to start integrating.
- **Battle-tested** -- 63-check automated audit, >=80% line coverage, UBSan + AddressSanitizer, property-based tests, fuzz harnesses.

## Quick start

Each component is a `.h` + `.c` pair in a self-contained folder. To use one:

```bash
# 1. Copy the component files into your source tree
cp wheels/drivers/bmp280/bmp280.{c,h} your_project/lib/bmp280/

# 2. Add two lines to your CMakeLists.txt
target_include_directories(your_app PRIVATE lib/bmp280)
target_sources(your_app PRIVATE lib/bmp280/bmp280.c)
```

Also copy `tooling/harness/contract.h` (a single header with optional runtime assertions). Then include the component header, inject your HAL callbacks, and call the API:

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
| L0 Primitives | `wheels/primitives/` | 78 | ring_buffer, crc32, state_machine, pid_controller, json_parser, aes128, kalman_1d |
| L1 Drivers | `wheels/drivers/` | 116 | bme280, sht40, mpu6050, oled_ssd1306, w25qxx, ws2812, lora_sx1276, icm42688 |
| L2 Protocols | `wheels/protocols/` | 48 | mqtt_packet, modbus_rtu, http_parser, nmea_parser, mavlink, coap_packet, crsf |
| L3 Modules | `wheels/modules/` | 21 | cli_shell, data_logger, ota_manager, sensor_hub, mqtt_client, telemetry |

Full catalog with descriptions: [wheels/README.md](wheels/README.md)

**Find components for your hardware:** [MCU Compatibility Matrix](docs/mcu-compatibility.md) -- all 116 drivers organized by bus type (I2C/SPI/GPIO/PWM/OneWire) and use case.

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

Every component passes a 63-check automated audit that enforces:

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

## See it in action

Run the end-to-end telemetry demo -- builds and executes in ~3 seconds, no hardware needed:

```bash
make flybox-demo
```

This simulates a real drone telemetry device: sensor data generation → JSON formatting → MQTT publish (QoS 1 with PUBACK) → cloud command parsing → disconnect/reconnect with exponential backoff. Uses 3 wheels working together: `mqtt_client` + `json_parser` + `retry`.

### Flagship examples

Five runnable integration examples that cover the most common embedded use cases. Each compiles and runs on your desktop with zero hardware:

```bash
cd examples/<name>
cmake -B build -G "MinGW Makefiles" && cmake --build build && ./build/<name>
```

| Example | Wheels used | What it demonstrates |
|---------|------------|---------------------|
| **mqtt_telemetry_demo** | bmp280, sensor_hub, mqtt_packet, mqtt_client | IoT sensor → MQTT publish pipeline with state machine |
| **industrial_monitor** | pid_controller, rate_limiter, fixed_point, ina219, ads1115, canbus, telemetry, task_watchdog | PID control loop + CAN bus + power monitoring + watchdog |
| **gps_tracker** | kalman_1d, base64, uuid, ds3231, sd_spi, sntp_client, ubx_parser | GPS parsing → Kalman filtering → SD card logging → NTP sync |
| **display_hub** | sh1106, hd44780, tm1637, max7219, lcd_st7789, ili9341 | 6 display types (OLED, LCD, 7-seg, LED matrix, TFT) via mock HAL |
| **smart_home** | debounce, timer_wheel, led_pattern, bme280, bh1750, ws2812, cbor_lite, scheduler, event_bus | Event-driven IoT hub with sensor polling + CBOR encoding + LED effects |

All 27 examples in `examples/` compile on host. The five above produce the most informative output.

## Running tests

```bash
# Prerequisites: gcc, cmake, ninja, pytest

# Test everything
make test-parallel

# Test a single component
bash tooling/test/test_all.sh wheels/drivers/bmp280

# Run the 63-check audit
python3 tooling/audit/audit_wheels.py --quiet

# Coverage for one component
bash tooling/test/coverage_wheel.sh wheels/drivers/bmp280
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the quick-start guide, wheel contract, commit format, and testing requirements.

## License

[MIT](LICENSE)
