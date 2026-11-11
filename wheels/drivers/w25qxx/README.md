# W25Qxx — SPI NOR Flash Driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Winbond W25Q series (W25Q32/W25Q64/W25Q128 etc).

## Origin

Reimplemented for the needs of an internal project. SPI bring-up and any application-layer ring buffer stay in the consumer; this wheel holds the platform-agnostic command logic.

## Why self-written?

Searched GitHub (2026-05-02). Found libdriver/w25qxx, pellepl/spiflash_driver (MIT). Solid alternatives; our impl covers our subset. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

None. Pure C99 with injected SPI HAL callbacks.

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
static void my_cs_low(void *ctx)  { (void)ctx; }
static void my_cs_high(void *ctx) { (void)ctx; }
static uint8_t my_xfer(void *ctx, uint8_t tx) { (void)ctx; return tx; }

static void example(void) {
    w25qxx_spi_t spi = { .cs_low = my_cs_low, .cs_high = my_cs_high, .xfer = my_xfer };
    uint8_t data[64];
    uint8_t buf[64];
    uint32_t id;
    if (w25qxx_init(&spi, &id) == W25QXX_OK) {
        w25qxx_sector_erase(&spi, 0x1000);
        w25qxx_page_program(&spi, 0x1000, data, 64);
        w25qxx_read(&spi, 0x1000, buf, 64);
    }
}
```

## Configuration (compile-time)

| Macro | Default | Description |
|-------|---------|-------------|
| `W25QXX_BUSY_TIMEOUT` | `500000U` | Iteration limit for busy-wait polling; tune for SPI clock speed |

## Supported platforms

- [x] Host C99 (mock SPI) — 21 tests, 55 assertions, 2026-04-29
- [x] GD32F405 + W25Q64 @ SPI1 10.5 MHz — HIL verified 2026-04-26 (the code as of that date; later changes have not run on a board)

## Datasheet check

Checked on 2026-11-06 against the Winbond W25Q64JV manual, Revision J, March 27, 2018, and that part only: W25Q32, W25Q128 and the other sizes named above were not checked. This check was not run on hardware. The driver was not changed: the owner decided it is checked but not fixed, because no board is available to retest it.

- **Matched:** Instruction codes, JEDEC ID order and the W25Q64JV ID EF4017, Status Register-1 BUSY and WEL bits, 24-bit address order, 256-byte page, 4 KB sector, Write Enable before every program and erase, the page-wrap refusal in `w25qxx_page_program`.
- **Fixed:** The mock left WEL set after a program, and the model and mock never set BUSY, acted on data before /CS rose and ran past the page end. Both now follow the manual, so a driver that skips Write Enable or the busy wait, never raises /CS or overruns a page fails the tests. A busy cycle there lasts a few status reads, not the real time.
- **Still open:** `w25qxx_chip_erase` shares the 500000-poll limit of every other wait, about 0.4 s at 10.5 MHz, while the chip needs 20 s typical and 100 s at most. A healthy chip therefore gets `W25QXX_ERR_TIMEOUT` while still erasing, and it ignores everything but Read Status until it finishes. No project calls chip erase, so the flybox run never tried it. There is no unlock and no Release Power-down, so a protected page is not programmed while the driver returns `W25QXX_OK`, and a chip left in power-down fails init.
- **Suspected:** The same poll limit may be too short for a 400 ms sector erase at higher SPI clocks. Write Enable is never read back and init has no tPUW wait, so a write within 5 ms of power-up may do nothing and still return OK. Init does not wait for BUSY, so a reset during an erase may fail it with `W25QXX_ERR_JEDEC_ID` on a good chip. Init rejects only manufacturer bytes 0x00 and 0xFF, so noise or another maker's part passes.

## Known issues

- Page program `len` must be <= 256 and must not cross a 256-byte page boundary; caller is responsible for splitting
- Busy-wait upper bound (500k iterations) is tuned for typical SPI clocks; extremely slow bit-bang SPI may need adjustment

## How to test

```bash
cd wheels/drivers/w25qxx/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Host mock tests: PASS (21 tests, 55 assertions — init, JEDEC ID validation (valid/0xFF/0x00), read, program, full-page, page boundary, oversized, erase (aligned/non-aligned), chip erase, status, timeout, NOR semantics, NULL safety, re-init, end-of-page write)
- HIL: PASS on GD32F405 + W25Q64 (SPI1 PB12-15), 2026-04-26

## API convention deviations

None.

## Upgrade to stable

- [ ] 2nd project consumer (currently only flybox)
- [ ] Performance reference (erase/program/read timing at typical clock)
- [ ] Change history

## Features

- JEDEC ID read + validation
- Page program (up to 256 bytes)
- Sector erase (4 KB) and chip erase
- Continuous read (arbitrary length)
- Status register read (busy/WEL polling)
- Bounded busy-wait (500k iteration limit) — dead chip returns `W25QXX_ERR_TIMEOUT` instead of hanging
- SPI HAL injection via callback struct — zero RTOS / platform dependency

## API review

- Date: 2026-04-25
- Reviewer: @user
- Key decisions: generalized name from w25q64 to w25qxx (covers W25Q32/64/128 family); SPI HAL uses cs_low/cs_high/xfer callback triple

## Responsible human

@L-X-Yao
