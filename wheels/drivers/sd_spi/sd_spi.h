// SPDX-License-Identifier: MIT
/*
 * sd_spi -- SPI-mode SD card block read/write driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SD_SPI_OK          =  0,
    SD_SPI_ERR_SPI     = -1,
    SD_SPI_ERR_PARAM  = -2,
    SD_SPI_ERR_TIMEOUT = -3,
    SD_SPI_ERR_CRC     = -4,
    SD_SPI_ERR_CMD     = -5,
    SD_SPI_ERR_WRITE   = -6,
    SD_SPI_ERR_INIT    = -7,
    SD_SPI_ERR_READ    = -8,
} sd_spi_err_t;

/* ---------- card type ---------- */

typedef enum {
    SD_SPI_TYPE_UNKNOWN = 0,
    SD_SPI_TYPE_SDV1,           /* SD v1 */
    SD_SPI_TYPE_SDV2_SC,        /* SD v2 standard capacity (SDSC) */
    SD_SPI_TYPE_SDV2_HC,        /* SD v2 high capacity (SDHC/SDXC) */
} sd_spi_card_type_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*delay_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} sd_spi_hal_t;

/* ---------- device handle ---------- */

typedef struct {
    const sd_spi_hal_t *hal;
    sd_spi_card_type_t  card_type;
    uint32_t            block_count;    /* total blocks (each 512 bytes) */
} sd_spi_t;

/* ---------- constants ---------- */

#define SD_SPI_BLOCK_SIZE  512U

/* ---------- API (all @thread_safe: caller_must_lock) ---------- @isr_safe: yes */

/** Initialize card (CMD0->CMD8->ACMD41->CMD58). @thread_safe: no @isr_safe: no */
sd_spi_err_t sd_spi_init(sd_spi_t *sd, const sd_spi_hal_t *hal);

/** Read single 512-byte block. @thread_safe: caller_must_lock @isr_safe: no */
sd_spi_err_t sd_spi_read_block(sd_spi_t *sd, uint32_t block_addr,
                                uint8_t *buf);

/** Write single 512-byte block. @thread_safe: caller_must_lock @isr_safe: no */
sd_spi_err_t sd_spi_write_block(sd_spi_t *sd, uint32_t block_addr,
                                 const uint8_t *buf);

/** Get card type. @thread_safe: caller_must_lock @isr_safe: no */
sd_spi_card_type_t sd_spi_get_type(const sd_spi_t *sd);

/** Get capacity in blocks (each 512 bytes). @thread_safe: caller_must_lock @isr_safe: yes */
uint32_t sd_spi_get_block_count(const sd_spi_t *sd);

#ifdef __cplusplus
}
#endif

#endif /* SD_SPI_H */
