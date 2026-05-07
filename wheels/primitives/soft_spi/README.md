# soft_spi --- GPIO bit-bang SPI master (modes 0-3, MSB/LSB)

**Status: candidate**
**Layer: L0 (primitives)**

Software SPI master using GPIO bit-banging. Supports all 4 SPI modes (CPOL/CPHA combinations) and configurable bit order (MSB or LSB first). Full-duplex: simultaneous read and write. CS management is the caller's responsibility, matching hardware SPI peripheral conventions.

## Origin

Proactive build for use cases where hardware SPI peripheral is unavailable or occupied. Complement to the existing `wheels/primitives/soft_i2c/` wheel.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-07):

- Most bitbang SPI implementations are tightly coupled to specific MCU HALs or RTOS.
- No suitable zero-dependency C99 implementation found with clean GPIO injection.

Reference: `wheels/primitives/soft_i2c/` (same GPIO injection pattern).

## Dependencies

None.

## API

```c
/* Error codes */
typedef enum {
    SOFT_SPI_OK        =  0,
    SOFT_SPI_ERR_PARAM = -1,
} soft_spi_err_t;

/* GPIO HAL injection */
typedef struct {
    void (*sck_low)(void *ctx);
    void (*sck_high)(void *ctx);
    void (*mosi_low)(void *ctx);
    void (*mosi_high)(void *ctx);
    bool (*miso_read)(void *ctx);
    void (*delay_us)(void *ctx, uint16_t us);
    void *ctx;
} soft_spi_gpio_t;

/* Configuration */
typedef struct {
    uint8_t mode;       /* 0-3 (CPOL/CPHA combinations) */
    bool    lsb_first;  /* false = MSB first (default)   */
} soft_spi_cfg_t;

/* Instance */
typedef struct {
    const soft_spi_gpio_t *gpio;
    soft_spi_cfg_t cfg;
} soft_spi_t;

soft_spi_err_t soft_spi_init(soft_spi_t *dev, const soft_spi_gpio_t *gpio, const soft_spi_cfg_t *cfg);
uint8_t        soft_spi_xfer(soft_spi_t *dev, uint8_t tx);
soft_spi_err_t soft_spi_write(soft_spi_t *dev, const uint8_t *data, uint16_t len);
soft_spi_err_t soft_spi_read(soft_spi_t *dev, uint8_t *buf, uint16_t len);
soft_spi_err_t soft_spi_transfer(soft_spi_t *dev, const uint8_t *tx, uint8_t *rx, uint16_t len);
```

### SPI modes

| Mode | CPOL | CPHA | SCK idle | Sample edge   |
|------|------|------|----------|---------------|
| 0    | 0    | 0    | Low      | Rising (lead) |
| 1    | 0    | 1    | Low      | Falling (trail)|
| 2    | 1    | 0    | High     | Falling (lead)|
| 3    | 1    | 1    | High     | Rising (trail) |

## Configuration (compile-time)

No compile-time defines. All configuration via `soft_spi_cfg_t` at runtime.

## Supported platforms

- [x] Host (sim) --- 2026-05-07
- [ ] Real hardware target --- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/soft_spi/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_soft_spi
./build/prop_soft_spi
./build/test_null_soft_spi
```

## Test status

- Unit tests (sim): 48 pass
- Property tests (sim): 5 pass (50,000 iterations total)
- Null param tests (sim): 11 pass
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-07
- **Key decisions**: CS (chip select) management excluded from wheel scope to match hardware SPI convention. `soft_spi_xfer` returns `uint8_t` (not error code) for ergonomic single-byte usage; returns 0 on NULL.

## Responsible human

@user
