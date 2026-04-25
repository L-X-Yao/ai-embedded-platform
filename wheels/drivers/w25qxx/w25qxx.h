#ifndef W25QXX_H
#define W25QXX_H

#include <stdint.h>
#include <stdbool.h>

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

/* ---------- API ---------- */

w25qxx_err_t w25qxx_init(const w25qxx_spi_t *spi, uint32_t *jedec_id_out);

w25qxx_err_t w25qxx_read(const w25qxx_spi_t *spi,
                          uint32_t addr, uint8_t *buf, uint32_t len);

w25qxx_err_t w25qxx_page_program(const w25qxx_spi_t *spi,
                                  uint32_t addr, const uint8_t *data,
                                  uint16_t len);

w25qxx_err_t w25qxx_sector_erase(const w25qxx_spi_t *spi, uint32_t addr);

w25qxx_err_t w25qxx_chip_erase(const w25qxx_spi_t *spi);

w25qxx_err_t w25qxx_read_status(const w25qxx_spi_t *spi, uint8_t *sr1);

#endif /* W25QXX_H */
