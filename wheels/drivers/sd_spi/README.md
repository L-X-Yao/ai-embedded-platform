# sd_spi -- SPI-mode SD Card Block Driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic SPI-mode SD card driver for single-block read/write. Supports SDv1, SDv2 SDSC, and SDHC/SDXC cards.
## Origin

Proactive build for SD card data logging use cases (flight recorders, sensor loggers). Common in embedded projects where SDIO is unavailable and SPI is the only interface option.


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Features

- SDv1/SDv2/SDHC card type detection via CMD0/CMD8/ACMD41/CMD58 sequence
- Single 512-byte block read (CMD17)
- Single 512-byte block write (CMD24)
- Automatic SDHC block addressing vs SDSC byte addressing
- Bounded retry loops (8000 iterations) -- dead card returns error instead of hanging
- SPI HAL injection via callback struct -- zero RTOS / platform dependency

## API

```c
sd_spi_err_t sd_spi_init(sd_spi_t *sd, const sd_spi_hal_t *hal);
sd_spi_err_t sd_spi_read_block(sd_spi_t *sd, uint32_t block_addr, uint8_t *buf);
sd_spi_err_t sd_spi_write_block(sd_spi_t *sd, uint32_t block_addr, const uint8_t *buf);
sd_spi_card_type_t sd_spi_get_type(const sd_spi_t *sd);
uint32_t sd_spi_get_block_count(const sd_spi_t *sd);
```

Caller provides SPI HAL via `sd_spi_hal_t` callbacks (CS control + byte transfer + delay).

## Usage

```c
static void my_cs_low(void *ctx)  { gpio_bit_reset(GPIOB, GPIO_PIN_12); }
static void my_cs_high(void *ctx) { gpio_bit_set(GPIOB, GPIO_PIN_12); }
static uint8_t my_xfer(void *ctx, uint8_t tx) { /* SPI full-duplex byte */ }
static void my_delay(void *ctx, uint32_t ms) { /* millisecond delay */ }

sd_spi_hal_t hal = {
    .cs_low = my_cs_low, .cs_high = my_cs_high,
    .xfer = my_xfer, .delay_ms = my_delay
};
sd_spi_t sd;
if (sd_spi_init(&sd, &hal) == SD_SPI_OK) {
    uint8_t buf[512];
    sd_spi_read_block(&sd, 0, buf);
    sd_spi_write_block(&sd, 1, buf);
}
```

## Dependencies

None. Pure C99 with injected SPI HAL callbacks.

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `SD_SPI_RETRY_LIMIT` | `8000U` | Iteration limit for response/busy polling; tune for SPI clock speed |

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- not yet verified

## Known issues

- No multi-block read/write (CMD18/CMD25); single-block only
- `block_count` is not auto-populated from CSD (CMD9); caller must set it if needed
- SPI clock must be <=400kHz during init, then can be raised; caller manages clock speed
- No CRC verification on read data (CRC bytes are discarded)

## How to test

```bash
cd wheels/drivers/sd_spi/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests (sim): PASS (18 tests -- init SDv1/SDv2-SC/SDv2-HC, init failure, ACMD41 retry, null safety x4, block read, read timeout, block write, write rejected, card type detection, block count, SDHC block addressing, SDSC byte addressing, read-write cycle, SPI failure)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: HAL adds delay_ms callback (needed for SD init timing); device handle holds card_type and block_count; SDHC/SDSC addressing handled internally

## Responsible human

@user
