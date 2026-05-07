// SPDX-License-Identifier: MIT
/*
 * soft_spi --- GPIO bit-bang SPI master (modes 0-3, MSB/LSB)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef SOFT_SPI_H
#define SOFT_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SOFT_SPI_OK        =  0,
    SOFT_SPI_ERR_PARAM = -1,
} soft_spi_err_t;

/* ---------- GPIO HAL injection ---------- */

typedef struct {
    void (*sck_low)(void *ctx);
    void (*sck_high)(void *ctx);
    void (*mosi_low)(void *ctx);
    void (*mosi_high)(void *ctx);
    bool (*miso_read)(void *ctx);
    void (*delay_us)(void *ctx, uint16_t us);
    void *ctx;
} soft_spi_gpio_t;

/* ---------- configuration ---------- */

typedef struct {
    uint8_t mode;       /* 0-3 (CPOL/CPHA combinations) */
    bool    lsb_first;  /* false = MSB first (default)   */
} soft_spi_cfg_t;

/* ---------- instance ---------- */

typedef struct {
    const soft_spi_gpio_t *gpio; /* internal */
    soft_spi_cfg_t cfg;          /* internal */
} soft_spi_t;

/* ---------- API ---------- */

/** Initialize instance.
 *  @thread_safe: no (caller must serialize) @isr_safe: no */
soft_spi_err_t soft_spi_init(soft_spi_t *dev,
                             const soft_spi_gpio_t *gpio,
                             const soft_spi_cfg_t *cfg);

/** Exchange one byte (full-duplex). Returns received byte (0 on error).
 *  @thread_safe: no @isr_safe: no */
uint8_t soft_spi_xfer(soft_spi_t *dev, uint8_t tx);

/** Write len bytes (MISO ignored).
 *  @thread_safe: no @isr_safe: no */
soft_spi_err_t soft_spi_write(soft_spi_t *dev,
                              const uint8_t *data, uint16_t len);

/** Read len bytes (transmits 0xFF on MOSI).
 *  @thread_safe: no @isr_safe: no */
soft_spi_err_t soft_spi_read(soft_spi_t *dev,
                             uint8_t *buf, uint16_t len);

/** Full-duplex transfer of len bytes.
 *  @thread_safe: no @isr_safe: no */
soft_spi_err_t soft_spi_transfer(soft_spi_t *dev,
                                 const uint8_t *tx, uint8_t *rx,
                                 uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SOFT_SPI_H */
