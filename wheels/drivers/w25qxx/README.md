# W25Qxx — SPI NOR Flash Driver (L1)

Platform-agnostic driver for Winbond W25Q series (W25Q32/W25Q64/W25Q128 etc).

## Why not VSF native?

VSF has no SPI NOR flash component. Searched `vsf/source/{component,service,hal}/` — zero hits for `w25q`, `spi.*flash`, `nor.*flash`, `jedec.*flash`.

## API

```c
w25qxx_err_t w25qxx_init(const w25qxx_spi_t *spi, uint32_t *jedec_id_out);
w25qxx_err_t w25qxx_read(const w25qxx_spi_t *spi, uint32_t addr, uint8_t *buf, uint32_t len);
w25qxx_err_t w25qxx_page_program(const w25qxx_spi_t *spi, uint32_t addr, const uint8_t *data, uint16_t len);
w25qxx_err_t w25qxx_sector_erase(const w25qxx_spi_t *spi, uint32_t addr);
w25qxx_err_t w25qxx_chip_erase(const w25qxx_spi_t *spi);
w25qxx_err_t w25qxx_read_status(const w25qxx_spi_t *spi, uint8_t *sr1);
```

Caller provides SPI HAL via `w25qxx_spi_t` callbacks (CS control + byte transfer). No RTOS dependency.

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

## Origin

Extracted from `projects/flybox/` W25Q64 driver. HIL verified on GD32F405 + W25Q64 @ SPI 10.5MHz.

## Test

```bash
cd test && mkdir build && cd build
cmake .. && make && ./test_w25qxx
```
