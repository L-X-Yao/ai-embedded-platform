# soft_i2c

**Status: candidate**
**Layer: L0 (primitives)**

GPIO bit-bang I2C master with bus recovery. All hardware access is via
injected function pointers (SCL/SDA set, SDA direction, SDA read, delay),
making it portable to any MCU. Supports register read/write helpers and
clock-stretching recovery. Pure C99, no heap, no OS.

## Why not VSF native?

VSF provides HAL I2C drivers tied to specific peripherals. This wheel
provides a software I2C master for MCUs with no available hardware I2C
or when extra I2C buses are needed on arbitrary GPIO pins.

## API

```c
#include "soft_i2c.h"

soft_i2c_err_t soft_i2c_init(soft_i2c_t *bus, const soft_i2c_gpio_t *gpio);
soft_i2c_err_t soft_i2c_write(soft_i2c_t *bus, uint8_t addr,
                              const uint8_t *data, uint16_t len);
soft_i2c_err_t soft_i2c_read(soft_i2c_t *bus, uint8_t addr,
                             uint8_t *buf, uint16_t len);
soft_i2c_err_t soft_i2c_write_reg(soft_i2c_t *bus, uint8_t addr,
                                  uint8_t reg, uint8_t val);
soft_i2c_err_t soft_i2c_read_regs(soft_i2c_t *bus, uint8_t addr,
                                  uint8_t reg, uint8_t *buf, uint16_t len);
soft_i2c_err_t soft_i2c_bus_recover(soft_i2c_t *bus);
```

## Usage

```c
static void scl_set(void *ctx, bool high) { gpio_write(SCL_PIN, high); }
static void sda_set(void *ctx, bool high) { gpio_write(SDA_PIN, high); }
static void sda_out(void *ctx) { gpio_dir_output(SDA_PIN); }
static void sda_in(void *ctx)  { gpio_dir_input(SDA_PIN); }
static bool sda_get(void *ctx) { return gpio_read(SDA_PIN); }
static void delay(void *ctx)   { delay_us(5); }

static const soft_i2c_gpio_t gpio = {
    .scl_set = scl_set, .sda_set = sda_set,
    .sda_dir_out = sda_out, .sda_dir_in = sda_in,
    .sda_get = sda_get, .delay = delay,
};

soft_i2c_t bus;
soft_i2c_init(&bus, &gpio);
soft_i2c_write_reg(&bus, 0x68, 0x6B, 0x00);  /* wake MPU6050 */
```

## Dependencies

None. GPIO operations injected via function pointers.

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Test status

- Unit tests (sim): PASS
- HIL: N/A
