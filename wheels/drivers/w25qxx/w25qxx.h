/*
 * w25qxx — W25Q series SPI NOR flash driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    spi_hal
 */

#ifndef W25QXX_H
#define W25QXX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    W25QXX_OK = 0,
    W25QXX_ERR_SPI,
    W25QXX_ERR_JEDEC_ID,
    W25QXX_ERR_TIMEOUT,
    W25QXX_ERR_ALIGN,
} w25qxx_err_t;

/* ---------- SPI HAL (caller provides) ---------- */

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} w25qxx_spi_t;

/* ---------- constants ---------- */

#define W25QXX_PAGE_SIZE    256U
#define W25QXX_SECTOR_SIZE  4096U

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/** Read JEDEC ID and verify chip. @thread_safe: caller_must_lock */
w25qxx_err_t w25qxx_init(const w25qxx_spi_t *spi, uint32_t *jedec_id_out);

/** Read data from flash. @thread_safe: caller_must_lock */
w25qxx_err_t w25qxx_read(const w25qxx_spi_t *spi,
                          uint32_t addr, uint8_t *buf, uint32_t len);

/** Program up to one page (256 B). @thread_safe: caller_must_lock */
w25qxx_err_t w25qxx_page_program(const w25qxx_spi_t *spi,
                                  uint32_t addr, const uint8_t *data,
                                  uint16_t len);

/** Erase one 4 KB sector. @thread_safe: caller_must_lock */
w25qxx_err_t w25qxx_sector_erase(const w25qxx_spi_t *spi, uint32_t addr);

/** Erase entire chip. @thread_safe: caller_must_lock */
w25qxx_err_t w25qxx_chip_erase(const w25qxx_spi_t *spi);

/** Read status register 1. @thread_safe: caller_must_lock */
w25qxx_err_t w25qxx_read_status(const w25qxx_spi_t *spi, uint8_t *sr1);

#ifdef __cplusplus
}
#endif

#endif /* W25QXX_H */
