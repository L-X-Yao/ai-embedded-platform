/*
 * w25qxx — W25Q series SPI NOR flash driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    spi_hal
 */

#include "w25qxx.h"

/* ---------- SPI flash commands ---------- */

#define CMD_WRITE_ENABLE    0x06
#define CMD_READ_STATUS1    0x05
#define CMD_READ_DATA       0x03
#define CMD_PAGE_PROGRAM    0x02
#define CMD_SECTOR_ERASE    0x20
#define CMD_CHIP_ERASE      0xC7
#define CMD_JEDEC_ID        0x9F

#define SR1_BUSY            0x01
#define SR1_WEL             0x02

#define BUSY_TIMEOUT        500000U

/* ---------- helpers ---------- */

static void cmd1(const w25qxx_spi_t *s, uint8_t cmd)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, cmd);
    s->cs_high(s->ctx);
}

static w25qxx_err_t wait_busy(const w25qxx_spi_t *s)
{
    uint32_t n = BUSY_TIMEOUT;
    s->cs_low(s->ctx);
    s->xfer(s->ctx, CMD_READ_STATUS1);
    while (n--) {
        uint8_t sr = s->xfer(s->ctx, 0xFF);
        if (!(sr & SR1_BUSY)) { s->cs_high(s->ctx); return W25QXX_OK; }
    }
    s->cs_high(s->ctx);
    return W25QXX_ERR_TIMEOUT;
}

static void write_enable(const w25qxx_spi_t *s)
{
    cmd1(s, CMD_WRITE_ENABLE);
}

static void send_addr(const w25qxx_spi_t *s, uint32_t addr)
{
    s->xfer(s->ctx, (uint8_t)(addr >> 16));
    s->xfer(s->ctx, (uint8_t)(addr >> 8));
    s->xfer(s->ctx, (uint8_t)(addr));
}

/* ---------- API ---------- */

w25qxx_err_t w25qxx_init(const w25qxx_spi_t *spi, uint32_t *jedec_id_out)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, CMD_JEDEC_ID);
    uint8_t mfr = spi->xfer(spi->ctx, 0xFF);
    uint8_t typ = spi->xfer(spi->ctx, 0xFF);
    uint8_t cap = spi->xfer(spi->ctx, 0xFF);
    spi->cs_high(spi->ctx);

    uint32_t id = ((uint32_t)mfr << 16) | ((uint32_t)typ << 8) | cap;
    if (jedec_id_out) *jedec_id_out = id;

    if (mfr == 0x00 || mfr == 0xFF) return W25QXX_ERR_JEDEC_ID;
    return W25QXX_OK;
}

w25qxx_err_t w25qxx_read(const w25qxx_spi_t *spi,
                          uint32_t addr, uint8_t *buf, uint32_t len)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, CMD_READ_DATA);
    send_addr(spi, addr);
    for (uint32_t i = 0; i < len; i++)
        buf[i] = spi->xfer(spi->ctx, 0xFF);
    spi->cs_high(spi->ctx);
    return W25QXX_OK;
}

w25qxx_err_t w25qxx_page_program(const w25qxx_spi_t *spi,
                                  uint32_t addr, const uint8_t *data,
                                  uint16_t len)
{
    if (len == 0 || len > W25QXX_PAGE_SIZE) return W25QXX_ERR_ALIGN;
    if (((addr & 0xFF) + len) > W25QXX_PAGE_SIZE) return W25QXX_ERR_ALIGN;

    write_enable(spi);
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, CMD_PAGE_PROGRAM);
    send_addr(spi, addr);
    for (uint16_t i = 0; i < len; i++)
        spi->xfer(spi->ctx, data[i]);
    spi->cs_high(spi->ctx);

    return wait_busy(spi);
}

w25qxx_err_t w25qxx_sector_erase(const w25qxx_spi_t *spi, uint32_t addr)
{
    write_enable(spi);
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, CMD_SECTOR_ERASE);
    send_addr(spi, addr);
    spi->cs_high(spi->ctx);

    return wait_busy(spi);
}

w25qxx_err_t w25qxx_chip_erase(const w25qxx_spi_t *spi)
{
    write_enable(spi);
    cmd1(spi, CMD_CHIP_ERASE);
    return wait_busy(spi);
}

w25qxx_err_t w25qxx_read_status(const w25qxx_spi_t *spi, uint8_t *sr1)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, CMD_READ_STATUS1);
    *sr1 = spi->xfer(spi->ctx, 0xFF);
    spi->cs_high(spi->ctx);
    return W25QXX_OK;
}
