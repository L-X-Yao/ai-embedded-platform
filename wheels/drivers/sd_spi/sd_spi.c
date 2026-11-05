// SPDX-License-Identifier: MIT
/*
 * sd_spi -- SPI-mode SD card block read/write driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "sd_spi.h"
#include "contract.h"
#include <string.h>

/* ---------- SD SPI commands ---------- */

#define CMD0    0   /* GO_IDLE_STATE */
#define CMD8    8   /* SEND_IF_COND */
#define CMD13   13  /* SEND_STATUS */
#define CMD17   17  /* READ_SINGLE_BLOCK */
#define CMD24   24  /* WRITE_BLOCK */
#define CMD55   55  /* APP_CMD prefix */
#define CMD58   58  /* READ_OCR */
#define ACMD41  41  /* SD_SEND_OP_COND */

#define R1_ILLEGAL_CMD  0x04
#define DATA_TOKEN      0xFE
#define DATA_RESP_MASK  0x1F
#define DATA_RESP_OK    0x05

#ifndef SD_SPI_RETRY_LIMIT
#define SD_SPI_RETRY_LIMIT  8000U

/* Card waits are limited by time: a byte count shrinks as the clock speeds up.
 * Spec 3.01 section 4.6.2: 100 ms for reads, more than 500 ms for writes. */
#define SD_SPI_READ_TIMEOUT_MS   100U
#define SD_SPI_WRITE_TIMEOUT_MS  600U
#define SD_SPI_POLLS_PER_MS      16U
#endif

/* ---------- helpers ---------- */

static void spi_dummy(const sd_spi_hal_t *h, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        h->xfer(h->ctx, 0xFF);
}

static uint8_t send_cmd(const sd_spi_hal_t *h, uint8_t cmd,
                         uint32_t arg)
{
    h->cs_low(h->ctx);
    spi_dummy(h, 1);

    h->xfer(h->ctx, (uint8_t)(0x40U | cmd));
    h->xfer(h->ctx, (uint8_t)(arg >> 24));
    h->xfer(h->ctx, (uint8_t)(arg >> 16));
    h->xfer(h->ctx, (uint8_t)(arg >> 8));
    h->xfer(h->ctx, (uint8_t)(arg));

    /* CRC -- only CMD0 and CMD8 need valid CRC in SPI mode */
    uint8_t crc = 0xFF;
    if (cmd == CMD0)  crc = 0x95;
    if (cmd == CMD8)  crc = 0x87;
    h->xfer(h->ctx, crc);

    /* wait for response (R1: bit7 = 0) */
    uint8_t r1 = 0xFF;
    for (uint32_t i = 0; i < SD_SPI_RETRY_LIMIT; i++) {
        r1 = h->xfer(h->ctx, 0xFF);
        if (!(r1 & 0x80U)) break;
    }
    return r1;
}

static uint8_t send_acmd(const sd_spi_hal_t *h, uint8_t acmd,
                          uint32_t arg)
{
    send_cmd(h, CMD55, 0);
    h->cs_high(h->ctx);
    return send_cmd(h, acmd, arg);
}

static void deselect(const sd_spi_hal_t *h)
{
    h->cs_high(h->ctx);
    h->xfer(h->ctx, 0xFF);
}

static sd_spi_err_t wait_ready(const sd_spi_hal_t *h)
{
    for (uint32_t ms = 0; ms < SD_SPI_WRITE_TIMEOUT_MS; ms++) {
        for (uint32_t i = 0; i < SD_SPI_POLLS_PER_MS; i++) {
            if (h->xfer(h->ctx, 0xFF) == 0xFF)
                return SD_SPI_OK;
        }
        h->delay_ms(h->ctx, 1);
    }
    return SD_SPI_ERR_TIMEOUT;
}

/* First byte other than idle, or 0xFF when the card stays silent. */
static uint8_t wait_token(const sd_spi_hal_t *h)
{
    for (uint32_t ms = 0; ms < SD_SPI_READ_TIMEOUT_MS; ms++) {
        for (uint32_t i = 0; i < SD_SPI_POLLS_PER_MS; i++) {
            uint8_t token = h->xfer(h->ctx, 0xFF);
            if (token != 0xFF)
                return token;
        }
        h->delay_ms(h->ctx, 1);
    }
    return 0xFF;
}

/* ---------- API ---------- */

sd_spi_err_t sd_spi_init(sd_spi_t *sd, const sd_spi_hal_t *hal)
{
    if (!sd || !hal || !hal->cs_low || !hal->cs_high ||
        !hal->xfer || !hal->delay_ms)
        return SD_SPI_ERR_PARAM;

    memset(sd, 0, sizeof(*sd));
    sd->hal = hal;

    /* 74+ clock pulses with CS high */
    hal->cs_high(hal->ctx);
    spi_dummy(hal, 10);

    /* CMD0: GO_IDLE_STATE */
    uint8_t r1 = send_cmd(hal, CMD0, 0);
    deselect(hal);
    if (r1 != 0x01)
        return SD_SPI_ERR_INIT;

    /* CMD8: SEND_IF_COND (voltage check) */
    r1 = send_cmd(hal, CMD8, 0x000001AAU);
    if (r1 == 0x01) {
        /* SDv2 -- read 4-byte R7 response */
        uint8_t r7[4];
        for (int i = 0; i < 4; i++)
            r7[i] = hal->xfer(hal->ctx, 0xFF);
        deselect(hal);

        if (r7[2] != 0x01 || r7[3] != 0xAA)
            return SD_SPI_ERR_INIT;

        /* ACMD41 with HCS bit until ready */
        for (uint32_t i = 0; i < SD_SPI_RETRY_LIMIT; i++) {
            r1 = send_acmd(hal, ACMD41, 0x40000000U);
            deselect(hal);
            if (r1 == 0x00) break;
            if (r1 != 0x01)
                return SD_SPI_ERR_INIT;
        }
        if (r1 != 0x00)
            return SD_SPI_ERR_TIMEOUT;

        /* CMD58: READ_OCR to check CCS bit */
        r1 = send_cmd(hal, CMD58, 0);
        if (r1 != 0x00) { deselect(hal); return SD_SPI_ERR_CMD; }
        uint8_t ocr[4];
        for (int i = 0; i < 4; i++)
            ocr[i] = hal->xfer(hal->ctx, 0xFF);
        deselect(hal);

        sd->card_type = (ocr[0] & 0x40U)
                        ? SD_SPI_TYPE_SDV2_HC
                        : SD_SPI_TYPE_SDV2_SC;
    } else if (r1 == 0xFF || !(r1 & R1_ILLEGAL_CMD)) {
        /* Only an illegal command marks a legacy card (spec 7.2.1). */
        deselect(hal);
        return SD_SPI_ERR_INIT;
    } else {
        /* SDv1: CMD8 returns illegal command (0x05 = idle + illegal) */
        deselect(hal);

        /* ACMD41 without HCS */
        for (uint32_t i = 0; i < SD_SPI_RETRY_LIMIT; i++) {
            r1 = send_acmd(hal, ACMD41, 0);
            deselect(hal);
            if (r1 == 0x00) break;
            if (r1 != 0x01)
                return SD_SPI_ERR_INIT;
        }
        if (r1 != 0x00)
            return SD_SPI_ERR_TIMEOUT;

        sd->card_type = SD_SPI_TYPE_SDV1;
    }

    return SD_SPI_OK;
}

sd_spi_err_t sd_spi_read_block(sd_spi_t *sd, uint32_t block_addr,
                                uint8_t *buf)
{
    if (!sd || !sd->hal || !buf)
        return SD_SPI_ERR_PARAM;

    const sd_spi_hal_t *h = sd->hal;

    /* SDSC uses byte addressing */
    uint32_t addr = (sd->card_type == SD_SPI_TYPE_SDV2_HC)
                    ? block_addr
                    : block_addr * SD_SPI_BLOCK_SIZE;

    uint8_t r1 = send_cmd(h, CMD17, addr);
    if (r1 != 0x00) { deselect(h); return SD_SPI_ERR_CMD; }

    /* wait for data token 0xFE */
    uint8_t token = wait_token(h);
    if (token != DATA_TOKEN) {
        deselect(h);
        /* The top four bits are clear in a data error token (spec 7.3.3.3). */
        return ((token & 0xF0U) == 0) ? SD_SPI_ERR_READ : SD_SPI_ERR_TIMEOUT;
    }

    /* read 512 bytes */
    for (uint32_t i = 0; i < SD_SPI_BLOCK_SIZE; i++)
        buf[i] = h->xfer(h->ctx, 0xFF);

    /* discard 2-byte CRC */
    h->xfer(h->ctx, 0xFF);
    h->xfer(h->ctx, 0xFF);

    deselect(h);
    return SD_SPI_OK;
}

sd_spi_err_t sd_spi_write_block(sd_spi_t *sd, uint32_t block_addr,
                                 const uint8_t *buf)
{
    if (!sd || !sd->hal || !buf)
        return SD_SPI_ERR_PARAM;

    const sd_spi_hal_t *h = sd->hal;

    /* SDSC uses byte addressing */
    uint32_t addr = (sd->card_type == SD_SPI_TYPE_SDV2_HC)
                    ? block_addr
                    : block_addr * SD_SPI_BLOCK_SIZE;

    uint8_t r1 = send_cmd(h, CMD24, addr);
    if (r1 != 0x00) { deselect(h); return SD_SPI_ERR_CMD; }

    /* send data token */
    h->xfer(h->ctx, DATA_TOKEN);

    /* send 512 bytes */
    for (uint32_t i = 0; i < SD_SPI_BLOCK_SIZE; i++)
        h->xfer(h->ctx, buf[i]);

    /* send 2 dummy CRC bytes */
    h->xfer(h->ctx, 0xFF);
    h->xfer(h->ctx, 0xFF);

    /* read data response */
    uint8_t resp = h->xfer(h->ctx, 0xFF);
    if ((resp & DATA_RESP_MASK) != DATA_RESP_OK) {
        deselect(h);
        return SD_SPI_ERR_WRITE;
    }

    /* wait for card to finish programming */
    sd_spi_err_t err = wait_ready(h);
    deselect(h);
    if (err != SD_SPI_OK) return err;

    /* Errors during programming only show in the status (R2 response). */
    r1 = send_cmd(h, CMD13, 0);
    uint8_t status = h->xfer(h->ctx, 0xFF);
    deselect(h);
    return (r1 == 0x00 && status == 0x00) ? SD_SPI_OK : SD_SPI_ERR_WRITE;
}

sd_spi_card_type_t sd_spi_get_type(const sd_spi_t *sd)
{
    if (!sd) return SD_SPI_TYPE_UNKNOWN;
    return sd->card_type;
}

uint32_t sd_spi_get_block_count(const sd_spi_t *sd)
{
    if (!sd) return 0;
    return sd->block_count;
}
