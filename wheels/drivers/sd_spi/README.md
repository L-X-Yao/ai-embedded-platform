# sd_spi -- SPI-mode SD Card Block Driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic SPI-mode SD card driver for single-block read/write. Supports SDv1, SDv2 SDSC, and SDHC/SDXC cards.

## Origin

Proactive build for SD card data logging use cases (flight recorders, sensor loggers). Common in embedded projects where SDIO is unavailable and SPI is the only interface option.

## Why self-written?

Searched GitHub (2026-05-02). Referenced 1nv1/ulibSD (MIT) for register sequences and compensation algorithms. SD/SPI protocol complex; compare for correctness. Self-written for platform convention consistency; cross-validated against reference.

## Dependencies

None. Pure C99 with injected SPI HAL callbacks.

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
static void my_cs_low(void *ctx)  { (void)ctx; }
static void my_cs_high(void *ctx) { (void)ctx; }
static uint8_t my_xfer(void *ctx, uint8_t tx) { (void)ctx; return tx; }
static void my_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

static void example(void) {
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
}
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `SD_SPI_RETRY_LIMIT` | `8000U` | Iteration limit for response/busy polling; tune for SPI clock speed |

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- not yet verified

## Datasheet check

Checked on 2026-11-06 against the SD Physical Layer Simplified Specification v3.01. Not run on hardware.

- **Matched:** Command indexes, CMD0 and CMD8 CRC bytes, the 74 clocks before CMD0, CMD8 and ACMD41 arguments, data tokens, addressing by card type.
- **Fixed:** Card waits counted bytes, about 2.5 ms at 25 MHz, where the spec needs 100 ms for reads and over 500 ms for writes. They now count milliseconds. A write now checks CMD13. Only an illegal CMD8 means a legacy card. A data error token returns `SD_SPI_ERR_READ`.
- **Still open:** The model and mock do not check the command CRC, ACMD41's HCS bit, or the card state, and the model never goes busy.
- **Suspected:** `sd_spi_init` never waits before the first clocks. The timing between R1 and the data token is blank in the Simplified Specification.

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

## Features

- SDv1/SDv2/SDHC card type detection via CMD0/CMD8/ACMD41/CMD58 sequence
- Single 512-byte block read (CMD17)
- Single 512-byte block write (CMD24)
- Automatic SDHC block addressing vs SDSC byte addressing
- Bounded retry loops (8000 iterations) -- dead card returns error instead of hanging
- SPI HAL injection via callback struct -- zero RTOS / platform dependency

## API review

- **Date**: 2026-05-01
- **Key decisions**: HAL adds delay_ms callback (needed for SD init timing); device handle holds card_type and block_count; SDHC/SDSC addressing handled internally

## Responsible human

@user
