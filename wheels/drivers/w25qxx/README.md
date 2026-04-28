# W25Qxx — SPI NOR Flash Driver

**STATUS: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Winbond W25Q series (W25Q32/W25Q64/W25Q128 etc).

## Why not VSF native?

VSF has no SPI NOR flash component. Searched `vsf/source/{component,service,hal}/` — zero hits for `w25q`, `spi.*flash`, `nor.*flash`, `jedec.*flash`.

## Features

- JEDEC ID read + validation
- Page program (up to 256 bytes)
- Sector erase (4 KB) and chip erase
- Continuous read (arbitrary length)
- Status register read (busy/WEL polling)
- Bounded busy-wait (500k iteration limit) — dead chip returns `W25QXX_ERR_TIMEOUT` instead of hanging
- SPI HAL injection via callback struct — zero RTOS / platform dependency

## API

```c
w25qxx_err_t w25qxx_init(const w25qxx_spi_t *spi, uint32_t *jedec_id_out);
w25qxx_err_t w25qxx_read(const w25qxx_spi_t *spi, uint32_t addr, uint8_t *buf, uint32_t len);
w25qxx_err_t w25qxx_page_program(const w25qxx_spi_t *spi, uint32_t addr, const uint8_t *data, uint16_t len);
w25qxx_err_t w25qxx_sector_erase(const w25qxx_spi_t *spi, uint32_t addr);
w25qxx_err_t w25qxx_chip_erase(const w25qxx_spi_t *spi);
w25qxx_err_t w25qxx_read_status(const w25qxx_spi_t *spi, uint8_t *sr1);
```

Caller provides SPI HAL via `w25qxx_spi_t` callbacks (CS control + byte transfer).

## Usage

```c
static void my_cs_low(void *ctx)  { gpio_bit_reset(GPIOB, GPIO_PIN_12); }
static void my_cs_high(void *ctx) { gpio_bit_set(GPIOB, GPIO_PIN_12); }
static uint8_t my_xfer(void *ctx, uint8_t tx) { /* SPI full-duplex byte */ }

w25qxx_spi_t spi = { .cs_low = my_cs_low, .cs_high = my_cs_high, .xfer = my_xfer };
uint32_t id;
if (w25qxx_init(&spi, &id) == W25QXX_OK) {
    w25qxx_sector_erase(&spi, 0x1000);
    w25qxx_page_program(&spi, 0x1000, data, 64);
    w25qxx_read(&spi, 0x1000, buf, 64);
}
```

## Supported platforms

- [x] Host C99 (mock SPI) — 29 tests, 2026-04-25
- [x] GD32F405 + W25Q64 @ SPI1 10.5 MHz — HIL verified 2026-04-26

## Dependencies

None. Pure C99 with injected SPI HAL callbacks.

## Known issues

- Page program `len` must be <= 256 and must not cross a 256-byte page boundary; caller is responsible for splitting
- Busy-wait upper bound (500k iterations) is tuned for typical SPI clocks; extremely slow bit-bang SPI may need adjustment

## Test status

- Host mock tests: PASS (29 tests covering init, read, program, erase, status, timeout, alignment, edge cases)
- HIL: PASS on GD32F405 + W25Q64 (SPI1 PB12-15), 2026-04-26

## API convention deviations

None.

## Responsible human

@user

## Origin

Extracted from `projects/flybox/src/drivers/w25q64/w25q64_driver.c`. Flybox retains GD32F4xx SPI1 bring-up + `flash_ring_*` application-layer ring buffer; platform-agnostic command logic moved to this wheel.

## API review

- Date: 2026-04-25
- Reviewer: @user
- Key decisions: generalized name from w25q64 to w25qxx (covers W25Q32/64/128 family); SPI HAL uses cs_low/cs_high/xfer callback triple

## Upgrade to stable

- [ ] 2nd project consumer (currently only flybox)
- [ ] Performance reference (erase/program/read timing at typical clock)
- [ ] Change history
